
#include <SDL2/SDL.h>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <iostream>
#include <string>

const int   ANCHO_VENTANA     = 800;
const int   ALTO_VENTANA      = 600;
const int   N_DEFECTO         = 60;
const float VEL_MIN           = 90.0f;  
const float VEL_MAX           = 220.0f;  
const float LONGITUD_LINEA    = 46.0f;   
const float ALFA_DESVANECIDO  = 8;       
const float DURACION_CHISPA   = 0.45f;   
const int   ESPACIADO_REJILLA = 40;      
const float PERIODO_BARRIDO   = 4.5f;    

struct Linea {
    float cx, cy;      
    float angulo;      
    float velocidad;   
    float hue;         
    float cooldown;    
};

struct Chispa {
    float x, y;
    float vida;
    Uint8 r, g, b;
};

void hsvARgb(float h, float s, float v, Uint8& r, Uint8& g, Uint8& b) {
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;
    float rf = 0, gf = 0, bf = 0;
    if (hp < 1)      { rf = c; gf = x; bf = 0; }
    else if (hp < 2) { rf = x; gf = c; bf = 0; }
    else if (hp < 3) { rf = 0; gf = c; bf = x; }
    else if (hp < 4) { rf = 0; gf = x; bf = c; }
    else if (hp < 5) { rf = x; gf = 0; bf = c; }
    else             { rf = c; gf = 0; bf = x; }
    r = static_cast<Uint8>((rf + m) * 255);
    g = static_cast<Uint8>((gf + m) * 255);
    b = static_cast<Uint8>((bf + m) * 255);
}

void calcularExtremos(const Linea& l, float& x1, float& y1, float& x2, float& y2) {
    float dx = cosf(l.angulo) * LONGITUD_LINEA * 0.5f;
    float dy = sinf(l.angulo) * LONGITUD_LINEA * 0.5f;
    x1 = l.cx - dx; y1 = l.cy - dy;
    x2 = l.cx + dx; y2 = l.cy + dy;
}

float grosorSegunN(int n) {
    float g = 6.0f - 0.01f * static_cast<float>(n);
    return fmaxf(1.5f, fminf(6.0f, g));
}

float elegirHueTron() {
    float r = static_cast<float>(rand()) / RAND_MAX;
    if (r < 0.55f)      return 188.0f + (rand() % 20 - 10); // cian
    else if (r < 0.80f) return  22.0f + (rand() % 14 - 7);  // naranja
    else if (r < 0.93f) return 200.0f + (rand() % 14 - 7);  // azul electrico
    else                return  45.0f + (rand() % 10 - 5);  // ambar
}

std::vector<Linea> generarLineas(int n) {
    std::vector<Linea> lineas;
    lineas.reserve(n);
    for (int i = 0; i < n; i++) {
        Linea l;
        l.cx = LONGITUD_LINEA + static_cast<float>(rand() % (int)(ANCHO_VENTANA - 2 * LONGITUD_LINEA));
        l.cy = LONGITUD_LINEA + static_cast<float>(rand() % (int)(ALTO_VENTANA - 2 * LONGITUD_LINEA));
        l.angulo = static_cast<float>(rand()) / RAND_MAX * 2.0f * static_cast<float>(M_PI);
        l.velocidad = VEL_MIN + static_cast<float>(rand()) / RAND_MAX * (VEL_MAX - VEL_MIN);
        l.hue = elegirHueTron();
        l.cooldown = 0.0f;
        lineas.push_back(l);
    }
    return lineas;
}
void actualizarMovimiento(std::vector<Linea>& lineas, float dt) {
    for (auto& l : lineas) {
        l.cx += cosf(l.angulo) * l.velocidad * dt;
        l.cy += sinf(l.angulo) * l.velocidad * dt;

        float x1, y1, x2, y2;
        calcularExtremos(l, x1, y1, x2, y2);

        if (std::min(x1, x2) < 0 || std::max(x1, x2) > ANCHO_VENTANA) {
            l.angulo = static_cast<float>(M_PI) - l.angulo;
        }
        if (std::min(y1, y2) < 0 || std::max(y1, y2) > ALTO_VENTANA) {
            l.angulo = -l.angulo;
        }

        l.cx = fmaxf(0.0f, fminf(static_cast<float>(ANCHO_VENTANA), l.cx));
        l.cy = fmaxf(0.0f, fminf(static_cast<float>(ALTO_VENTANA), l.cy));
        l.cooldown = fmaxf(0.0f, l.cooldown - dt);
    }
}

int orientacion(float px, float py, float qx, float qy, float rx, float ry) {
    float val = (qy - py) * (rx - qx) - (qx - px) * (ry - qy);
    if (fabsf(val) < 1e-6f) return 0; // colineales
    return (val > 0) ? 1 : 2;
}

bool enSegmento(float px, float py, float qx, float qy, float rx, float ry) {
    return qx <= fmaxf(px, rx) && qx >= fminf(px, rx) &&
           qy <= fmaxf(py, ry) && qy >= fminf(py, ry);
}

bool segmentosSeIntersectan(float x1, float y1, float x2, float y2,
                             float x3, float y3, float x4, float y4) {
    int o1 = orientacion(x1, y1, x2, y2, x3, y3);
    int o2 = orientacion(x1, y1, x2, y2, x4, y4);
    int o3 = orientacion(x3, y3, x4, y4, x1, y1);
    int o4 = orientacion(x3, y3, x4, y4, x2, y2);

    if (o1 != o2 && o3 != o4) return true;
    if (o1 == 0 && enSegmento(x1, y1, x3, y3, x2, y2)) return true;
    if (o2 == 0 && enSegmento(x1, y1, x4, y4, x2, y2)) return true;
    if (o3 == 0 && enSegmento(x3, y3, x1, y1, x4, y4)) return true;
    if (o4 == 0 && enSegmento(x3, y3, x2, y2, x4, y4)) return true;
    return false;
}

bool resolverColisionFisica(Linea& a, Linea& b) {
    float dx = b.cx - a.cx, dy = b.cy - a.cy;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.01f) dist = 0.01f;
    float nx = dx / dist, ny = dy / dist;

    float vxa = cosf(a.angulo) * a.velocidad, vya = sinf(a.angulo) * a.velocidad;
    float vxb = cosf(b.angulo) * b.velocidad, vyb = sinf(b.angulo) * b.velocidad;

    float vNormal = (vxa - vxb) * nx + (vya - vyb) * ny;
    if (vNormal <= 0) return false; 

    vxa -= vNormal * nx; vya -= vNormal * ny;
    vxb += vNormal * nx; vyb += vNormal * ny;

    a.angulo = atan2f(vya, vxa);
    a.velocidad = fmaxf(VEL_MIN * 0.6f, fminf(VEL_MAX * 1.4f, sqrtf(vxa * vxa + vya * vya)));
    b.angulo = atan2f(vyb, vxb);
    b.velocidad = fmaxf(VEL_MIN * 0.6f, fminf(VEL_MAX * 1.4f, sqrtf(vxb * vxb + vyb * vyb)));

    a.angulo += (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.2f;
    b.angulo += (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.2f;

    const float separacionMinima = LONGITUD_LINEA * 0.7f;
    if (dist < separacionMinima) {
        float empuje = (separacionMinima - dist) * 0.5f + 0.5f;
        a.cx -= nx * empuje; a.cy -= ny * empuje;
        b.cx += nx * empuje; b.cy += ny * empuje;
    }

    a.cooldown = 0.15f;
    b.cooldown = 0.15f;

    return true;
}

void detectarColisiones(std::vector<Linea>& lineas, std::vector<Chispa>& chispas,
                         long& contadorColisiones) {
    int n = static_cast<int>(lineas.size());
    for (int i = 0; i < n - 1; i++) {
        if (lineas[i].cooldown > 0.0f) continue; 

        float x1, y1, x2, y2;
        calcularExtremos(lineas[i], x1, y1, x2, y2);

        for (int j = i + 1; j < n; j++) {
            if (lineas[j].cooldown > 0.0f) continue; 

            float x3, y3, x4, y4;
            calcularExtremos(lineas[j], x3, y3, x4, y4);

            if (segmentosSeIntersectan(x1, y1, x2, y2, x3, y3, x4, y4)) {
                bool huboImpulso = resolverColisionFisica(lineas[i], lineas[j]);
                if (huboImpulso) {
                    std::swap(lineas[i].hue, lineas[j].hue);

                    Chispa c;
                    c.x = (lineas[i].cx + lineas[j].cx) * 0.5f;
                    c.y = (lineas[i].cy + lineas[j].cy) * 0.5f;
                    c.vida = DURACION_CHISPA;
                    hsvARgb(lineas[i].hue, 0.25f, 1.0f, c.r, c.g, c.b);
                    chispas.push_back(c);

                    contadorColisiones++;
                }
            }
        }
    }
}

void dibujarSegmentoNeon(SDL_Renderer* renderer, float x1, float y1, float x2, float y2,
                          Uint8 cr, Uint8 cg, Uint8 cb, float grosor) {
    float dx = x2 - x1, dy = y2 - y1;
    float largo = sqrtf(dx * dx + dy * dy);
    float nx = 0, ny = 0;
    if (largo > 0.001f) { nx = -dy / largo; ny = dx / largo; }

    float factor = grosor / 3.0f;
    const float pasosOffset[3] = {3.0f * factor, 2.0f * factor, 1.0f * factor};
    const Uint8 alfasGlow[3]   = {35, 70, 130};

    for (int k = 0; k < 3; k++) {
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, alfasGlow[k]);
        float off = pasosOffset[k];
        SDL_RenderDrawLine(renderer, (int)(x1 + nx * off), (int)(y1 + ny * off),
                                       (int)(x2 + nx * off), (int)(y2 + ny * off));
        SDL_RenderDrawLine(renderer, (int)(x1 - nx * off), (int)(y1 - ny * off),
                                       (int)(x2 - nx * off), (int)(y2 - ny * off));
    }

    SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);
    SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)x2, (int)y2);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    for (int dy = -2; dy <= 2; dy++) {
        int dx = static_cast<int>(sqrtf(static_cast<float>(4 - dy * dy)));
        SDL_RenderDrawLine(renderer, (int)x2 - dx, (int)y2 + dy, (int)x2 + dx, (int)y2 + dy);
    }
}


void dibujarChispa(SDL_Renderer* renderer, const Chispa& c) {
    float progreso = 1.0f - (c.vida / DURACION_CHISPA); 

    int radioAnillo = static_cast<int>(4 + progreso * 26);
    Uint8 alfaAnillo = static_cast<Uint8>(200 * (1.0f - progreso));
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, alfaAnillo);
    const int segmentosAnillo = 20;
    for (int k = 0; k < segmentosAnillo; k++) {
        float a0 = (2.0f * static_cast<float>(M_PI) * k) / segmentosAnillo;
        float a1 = (2.0f * static_cast<float>(M_PI) * (k + 1)) / segmentosAnillo;
        int x0 = static_cast<int>(c.x + cosf(a0) * radioAnillo);
        int y0 = static_cast<int>(c.y + sinf(a0) * radioAnillo);
        int x1 = static_cast<int>(c.x + cosf(a1) * radioAnillo);
        int y1 = static_cast<int>(c.y + sinf(a1) * radioAnillo);
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }

    // Esquirlas radiales (efecto de "shatter" digital)
    const int nEsquirlas = 6;
    float largoEsquirla = 6.0f + progreso * 12.0f;
    Uint8 alfaEsquirla = static_cast<Uint8>(230 * (1.0f - progreso));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alfaEsquirla);
    for (int k = 0; k < nEsquirlas; k++) {
        float ang = (2.0f * static_cast<float>(M_PI) * k) / nEsquirlas + progreso * 1.5f;
        float rInterior = radioAnillo * 0.5f;
        int x0 = static_cast<int>(c.x + cosf(ang) * rInterior);
        int y0 = static_cast<int>(c.y + sinf(ang) * rInterior);
        int x1 = static_cast<int>(c.x + cosf(ang) * (rInterior + largoEsquirla));
        int y1 = static_cast<int>(c.y + sinf(ang) * (rInterior + largoEsquirla));
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }

    if (progreso < 0.3f) {
        int radioNucleo = static_cast<int>(3 + (0.3f - progreso) * 10);
        Uint8 alfaNucleo = static_cast<Uint8>(255 * (1.0f - progreso / 0.3f));
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, alfaNucleo);
        for (int dy = -radioNucleo; dy <= radioNucleo; dy++) {
            int dx = static_cast<int>(sqrtf(static_cast<float>(radioNucleo * radioNucleo - dy * dy)));
            SDL_RenderDrawLine(renderer, (int)c.x - dx, (int)c.y + dy, (int)c.x + dx, (int)c.y + dy);
        }
    }
}

void dibujarRejilla(SDL_Renderer* renderer, float tiempoGlobal) {
    Uint8 pulso = static_cast<Uint8>(10 + 8 * (0.5f + 0.5f * sinf(tiempoGlobal * 0.6f)));
    SDL_SetRenderDrawColor(renderer, 25, 80, 130, pulso);
    for (int x = 0; x <= ANCHO_VENTANA; x += ESPACIADO_REJILLA)
        SDL_RenderDrawLine(renderer, x, 0, x, ALTO_VENTANA);
    for (int y = 0; y <= ALTO_VENTANA; y += ESPACIADO_REJILLA)
        SDL_RenderDrawLine(renderer, 0, y, ANCHO_VENTANA, y);
}


void dibujarBarridoRadar(SDL_Renderer* renderer, float tiempoGlobal) {
    float progreso = fmodf(tiempoGlobal, PERIODO_BARRIDO) / PERIODO_BARRIDO;
    int y = static_cast<int>(progreso * ALTO_VENTANA);
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 22);
    SDL_RenderDrawLine(renderer, 0, y, ANCHO_VENTANA, y);
    for (int k = 1; k <= 5; k++) {
        int yy = y - k * 4;
        if (yy < 0) break;
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, static_cast<Uint8>(18 - k * 3));
        SDL_RenderDrawLine(renderer, 0, yy, ANCHO_VENTANA, yy);
    }
}

void dibujarMarcosHUD(SDL_Renderer* renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 210, 230, 130);
    const int m = 18; // margen desde el borde
    const int l = 28; // longitud de cada trazo del marco

    SDL_RenderDrawLine(renderer, m, m, m + l, m);
    SDL_RenderDrawLine(renderer, m, m, m, m + l);

    SDL_RenderDrawLine(renderer, ANCHO_VENTANA - m, m, ANCHO_VENTANA - m - l, m);
    SDL_RenderDrawLine(renderer, ANCHO_VENTANA - m, m, ANCHO_VENTANA - m, m + l);

    SDL_RenderDrawLine(renderer, m, ALTO_VENTANA - m, m + l, ALTO_VENTANA - m);
    SDL_RenderDrawLine(renderer, m, ALTO_VENTANA - m, m, ALTO_VENTANA - m - l);

    SDL_RenderDrawLine(renderer, ANCHO_VENTANA - m, ALTO_VENTANA - m, ANCHO_VENTANA - m - l, ALTO_VENTANA - m);
    SDL_RenderDrawLine(renderer, ANCHO_VENTANA - m, ALTO_VENTANA - m, ANCHO_VENTANA - m, ALTO_VENTANA - m - l);
}

int main(int argc, char* argv[]) {
    int n = N_DEFECTO;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-n" && i + 1 < argc) {
            try {
                n = std::stoi(argv[i + 1]);
                if (n <= 0) {
                    std::cerr << "Error: N debe ser positivo. Usando " << N_DEFECTO << ".\n";
                    n = N_DEFECTO;
                }
            } catch (...) {
                std::cerr << "Error: valor invalido para -n. Usando " << N_DEFECTO << ".\n";
                n = N_DEFECTO;
            }
            i++;
        } else {
            std::cerr << "Uso: " << argv[0] << " -n <cantidad_de_lineas>\n";
        }
    }

    std::cout << "Iniciando screensaver SECUENCIAL (Tron - Neon Lines) con N = " << n << " lineas.\n";

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Error al inicializar SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* ventana = SDL_CreateWindow(
        "Screensaver Secuencial (Tron) - FPS: --",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ANCHO_VENTANA, ALTO_VENTANA, SDL_WINDOW_SHOWN);
    if (!ventana) {
        std::cerr << "Error al crear ventana: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        ventana, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if (!renderer) {
        std::cerr << "Error al crear renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(ventana);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* lienzo = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, ANCHO_VENTANA, ALTO_VENTANA);
    SDL_SetTextureBlendMode(lienzo, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer, lienzo);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);

    srand(static_cast<unsigned int>(time(nullptr)));
    std::vector<Linea> lineas = generarLineas(n);
    std::vector<Chispa> chispas;
    float grosor = grosorSegunN(n);

    bool corriendo = true;
    SDL_Event evento;
    Uint64 tickAnterior = SDL_GetPerformanceCounter();
    Uint64 frecuencia = SDL_GetPerformanceFrequency();
    long contadorColisiones = 0;
    float tiempoGlobal = 0.0f;

    int contadorFrames = 0;
    float tiempoAcumulado = 0.0f;
    char tituloBuffer[160];

    while (corriendo) {
        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_QUIT) corriendo = false;
            if (evento.type == SDL_KEYDOWN && evento.key.keysym.sym == SDLK_ESCAPE) corriendo = false;
        }

        Uint64 tickActual = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(tickActual - tickAnterior) / static_cast<float>(frecuencia);
        tickAnterior = tickActual;
        if (dt > 0.05f) dt = 0.05f;
        tiempoGlobal += dt;

        actualizarMovimiento(lineas, dt);

        detectarColisiones(lineas, chispas, contadorColisiones);

        for (auto& c : chispas) c.vida -= dt;
        chispas.erase(std::remove_if(chispas.begin(), chispas.end(),
                       [](const Chispa& c) { return c.vida <= 0.0f; }), chispas.end());

        SDL_SetRenderTarget(renderer, lienzo);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)ALFA_DESVANECIDO);
        SDL_RenderFillRect(renderer, nullptr);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        dibujarRejilla(renderer, tiempoGlobal);
        dibujarBarridoRadar(renderer, tiempoGlobal);

        for (const auto& l : lineas) {
            float x1, y1, x2, y2;
            calcularExtremos(l, x1, y1, x2, y2);
            Uint8 r, g, b;
            hsvARgb(l.hue, 0.85f, 1.0f, r, g, b);
            dibujarSegmentoNeon(renderer, x1, y1, x2, y2, r, g, b, grosor);
        }

        for (const auto& c : chispas) dibujarChispa(renderer, c);

        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderCopy(renderer, lienzo, nullptr, nullptr);
        dibujarMarcosHUD(renderer);
        SDL_RenderPresent(renderer);

        contadorFrames++;
        tiempoAcumulado += dt;
        if (tiempoAcumulado >= 1.0f) {
            float fps = contadorFrames / tiempoAcumulado;
            snprintf(tituloBuffer, sizeof(tituloBuffer),
                     "Screensaver Secuencial (Tron) - N=%d - FPS: %.2f", n, fps);
            SDL_SetWindowTitle(ventana, tituloBuffer);
            std::cout << "FPS= " << fps << " | colisiones acumuladas= " << contadorColisiones << "\n";
            contadorFrames = 0;
            tiempoAcumulado = 0.0f;
        }
    }

    SDL_DestroyTexture(lienzo);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
    return 0;
}
