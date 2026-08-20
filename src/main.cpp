// PoC: Neon Lines Screensaver — version secuencial (base para paralelizacion con OpenMP)
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <vector>
#include <algorithm>

const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 600;

// Cada entidad es un "haz de luz" con origen FIJO (fuera de la pantalla) que sigue una
// trayectoria CURVA precalculada (arco de curvatura constante). El haz no se traslada:
// a medida que pasa el tiempo se "revela" mas tramo de la curva desde el origen hacia
// la punta, como un rayo continuo que se dibuja solo, sin cortes. Cuando la punta ya
// recorrio toda la curva (cruzo la pantalla) el haz se recicla con una curva nueva, lo
// que mantiene siempre N haces activos y evita saturar la pantalla.
struct LightBeam {
    std::vector<SDL_FPoint> path; // curva precalculada del origen hasta el final del recorrido
    float step;                    // distancia (px) entre puntos consecutivos de la curva
    float length;                  // longitud ya revelada del recorrido (crece con el tiempo)
    float growSpeed;               // velocidad de crecimiento a lo largo de la curva (px/seg)
    float maxLength;               // longitud total de la curva (path.size() * step)
    float thickness;               // grosor del haz
    SDL_Color color;                // color neon (celeste o naranja, estilo Tron)
    int id;                         // identificador unico
};

// Paleta Tron: celeste (programas) y naranja (rivales), los dos tonos iconicos de la pelicula
static const SDL_Color NEON_PALETTE[] = {
    {60, 220, 255, 255},  // celeste
    {255, 130, 20, 255}   // naranja
};
static const int NEON_PALETTE_SIZE = sizeof(NEON_PALETTE) / sizeof(NEON_PALETTE[0]);

// Extrae y valida N desde argv. Devuelve -1 si los argumentos son invalidos.
int parseArgs(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Uso: %s <N_lineas>\n", argv[0]);
        return -1;
    }
    char* endPtr = nullptr;
    long n = std::strtol(argv[1], &endPtr, 10);
    if (endPtr == argv[1] || *endPtr != '\0') {
        std::fprintf(stderr, "Error: N debe ser un entero valido. Recibido: '%s'\n", argv[1]);
        return -1;
    }
    if (n <= 0 || n > 100000) {
        std::fprintf(stderr, "Error: N debe estar en el rango (0, 100000]. Recibido: %ld\n", n);
        return -1;
    }
    return static_cast<int>(n);
}

SDL_Color randomNeonColor() {
    return NEON_PALETTE[std::rand() % NEON_PALETTE_SIZE];
}

// Cantidad de puntos que forman cada curva. Se reduce cuando N crece para mantener
// acotado el trabajo total de renderizado (N * muestras) y no perder FPS.
int computeCurveSamples(int n) {
    int samples = 4000 / n;
    samples = std::max(samples, 12);
    samples = std::min(samples, 200);
    return samples;
}

// Genera (o recicla) un haz: calcula un punto objetivo dentro de la pantalla, una
// direccion inicial aleatoria y una curvatura aleatoria (positiva o negativa), luego
// integra la trayectoria curva desde un origen retrocedido "margin" px para garantizar
// que nace fuera del area visible y la atraviesa formando un arco al crecer.
void spawnBeam(LightBeam& b, int width, int height, int samples) {
    float diag = std::sqrt(static_cast<float>(width) * width + static_cast<float>(height) * height);
    float margin = diag * 0.55f; // distancia fuera de pantalla donde nace el haz

    float targetX = static_cast<float>(std::rand() % width);
    float targetY = static_cast<float>(std::rand() % height);
    float aimAngle = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f * static_cast<float>(M_PI);

    float ox = targetX - std::cos(aimAngle) * margin;
    float oy = targetY - std::sin(aimAngle) * margin;

    float maxLength = margin * 2.2f; // suficiente para nacer fuera, cruzar la pantalla y salir del otro lado
    float step = maxLength / static_cast<float>(samples);

    // Curvatura aleatoria (rad por pixel recorrido): el signo define si curva a la
    // izquierda o a la derecha, la magnitud que tan cerrado es el arco.
    float curvature = ((static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f) * 0.0022f;

    b.path.resize(samples + 1);
    float angle = aimAngle;
    float x = ox, y = oy;
    b.path[0] = SDL_FPoint{x, y};
    for (int i = 1; i <= samples; ++i) {
        angle += curvature * step;
        x += std::cos(angle) * step;
        y += std::sin(angle) * step;
        b.path[i] = SDL_FPoint{x, y};
    }

    b.step = step;
    b.maxLength = maxLength;
    b.length = 0.0f;
    b.growSpeed = 220.0f + static_cast<float>(std::rand() % 220); // px/seg
    b.color = randomNeonColor();
}

// Inicializa N haces de luz. Se escalona la longitud inicial de cada uno (en vez de 0)
// para que la pantalla no arranque vacia y los haces no crezcan todos sincronizados.
std::vector<LightBeam> initBeams(int n, int width, int height, int samples) {
    std::vector<LightBeam> beams(n);
    float thickness = (n <= 100) ? 9.0f : (n <= 500 ? 6.0f : 4.0f);

    for (int i = 0; i < n; ++i) {
        LightBeam& b = beams[i];
        b.id = i;
        b.thickness = thickness;
        spawnBeam(b, width, height, samples);
        b.length = (static_cast<float>(std::rand()) / RAND_MAX) * b.maxLength;
    }
    return beams;
}

// Revela mas tramo de la curva ya precalculada. El origen y la forma de la curva NUNCA
// cambian mientras el haz esta activo: lo que crece es cuanta curva ya se dibujo (el
// "movimiento" se traduce en tamano, no en traslacion). Al completar el recorrido se
// recicla como un haz nuevo con una curva distinta, manteniendo siempre N haces activos.
void updateBeam(LightBeam& b, float dt, int width, int height, int samples) {
    b.length += b.growSpeed * dt;
    if (b.length > b.maxLength) {
        spawnBeam(b, width, height, samples);
    }
}

// Dibuja el nucleo del haz como una sola cinta solida (triangle strip via
// SDL_RenderGeometry) en vez de varios trazos paralelos: los tramos consecutivos
// comparten vertices en cada union, por lo que el resultado es un trazo ancho y
// continuo, sin que se vea como multiples lineas separadas en las curvas.
void drawSolidRibbon(SDL_Renderer* renderer, const std::vector<SDL_FPoint>& path,
                      int revealed, float thickness, SDL_Color color) {
    int pointCount = revealed + 1;
    float halfThick = thickness / 2.0f;

    // Buffers reutilizados entre llamadas (el renderizado es secuencial, un solo hilo)
    // para evitar reservar memoria en el heap en cada cuadro.
    static std::vector<SDL_Vertex> verts;
    static std::vector<int> indices;
    verts.clear();
    indices.clear();
    verts.reserve(pointCount * 2);
    SDL_Color vc = color;

    for (int i = 0; i <= revealed; ++i) {
        float dx, dy;
        if (i == 0) {
            dx = path[1].x - path[0].x;
            dy = path[1].y - path[0].y;
        } else if (i == revealed) {
            dx = path[i].x - path[i - 1].x;
            dy = path[i].y - path[i - 1].y;
        } else {
            dx = path[i + 1].x - path[i - 1].x; // promedio de segmentos vecinos -> union suave
            dy = path[i + 1].y - path[i - 1].y;
        }
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.0001f) len = 1.0f;
        float nx = -dy / len;
        float ny = dx / len;

        const SDL_FPoint& p = path[i];
        SDL_Vertex left{ {p.x + nx * halfThick, p.y + ny * halfThick}, vc, {0, 0} };
        SDL_Vertex right{ {p.x - nx * halfThick, p.y - ny * halfThick}, vc, {0, 0} };
        verts.push_back(left);
        verts.push_back(right);
    }

    indices.reserve((pointCount - 1) * 6);
    for (int i = 0; i < pointCount - 1; ++i) {
        int i0 = 2 * i, i1 = 2 * i + 1, i2 = 2 * i + 2, i3 = 2 * i + 3;
        indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
        indices.push_back(i1); indices.push_back(i3); indices.push_back(i2);
    }

    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                        indices.data(), static_cast<int>(indices.size()));
}

// Renderiza la porcion ya revelada de la curva (origen -> punta actual): una cinta de
// glow ancha y translucida (blending aditivo) detras, mas el nucleo solido y opaco
// encima. Ambas capas se dibujan como una sola malla (SDL_RenderGeometry) en vez de
// muchos trazos paralelos, por lo que el haz se ve como un solo trazo continuo y ancho.
void renderBeam(SDL_Renderer* renderer, const LightBeam& b) {
    int samples = static_cast<int>(b.path.size()) - 1;
    int revealed = static_cast<int>(b.length / b.step);
    revealed = std::min(revealed, samples);
    if (revealed < 1) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_Color glow = b.color; glow.a = 60;
    drawSolidRibbon(renderer, b.path, revealed, b.thickness + 14.0f, glow);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Color core = b.color; core.a = 255;
    drawSolidRibbon(renderer, b.path, revealed, b.thickness, core);
}

int main(int argc, char* argv[]) {
    int n = parseArgs(argc, argv);
    if (n < 0) {
        return EXIT_FAILURE;
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    std::printf("SDL video driver en uso: %s\n", SDL_GetCurrentVideoDriver());
    std::fflush(stdout);

    SDL_Window* window = SDL_CreateWindow(
        "Neon Lines Screensaver - PoC",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "Error al crear ventana: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::printf("Ventana creada OK (id=%u)\n", SDL_GetWindowID(window));
    std::fflush(stdout);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::fprintf(stderr, "Renderer acelerado fallo (%s), probando con software...\n", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::fprintf(stderr, "Error al crear renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    std::printf("Renderer creado OK\n");
    std::fflush(stdout);

    int samples = computeCurveSamples(n);
    std::vector<LightBeam> beams = initBeams(n, WINDOW_WIDTH, WINDOW_HEIGHT, samples);

    bool running = true;
    Uint64 prevTicks = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    Uint32 fpsTimerMs = SDL_GetTicks();
    int frameCount = 0;
    double currentFps = 0.0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        Uint64 nowTicks = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(nowTicks - prevTicks) / static_cast<float>(freq);
        prevTicks = nowTicks;
        if (dt > 0.05f) dt = 0.05f; // evita saltos grandes si hubo pausa (ej. mover ventana)

        // Fase 1: revelado de curva de cada haz y reciclaje de los que ya la completaron
        // (candidata a paralelizar con OpenMP en v2)
        for (int i = 0; i < n; ++i) {
            updateBeam(beams[i], dt, WINDOW_WIDTH, WINDOW_HEIGHT, samples);
        }

        // Fase 3: renderizado (secuencial, SDL no es thread-safe)
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (int i = 0; i < n; ++i) {
            renderBeam(renderer, beams[i]);
        }
        SDL_RenderPresent(renderer);

        // Calculo y despliegue de FPS (cada ~500ms se actualiza el titulo)
        frameCount++;
        Uint32 nowMs = SDL_GetTicks();
        if (nowMs - fpsTimerMs >= 500) {
            currentFps = frameCount * 1000.0 / (nowMs - fpsTimerMs);
            frameCount = 0;
            fpsTimerMs = nowMs;

            char title[128];
            std::snprintf(title, sizeof(title), "Neon Lines Screensaver - PoC | N=%d | FPS=%.2f", n, currentFps);
            SDL_SetWindowTitle(window, title);
            std::printf("FPS=%.2f\n", currentFps);
            std::fflush(stdout);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
