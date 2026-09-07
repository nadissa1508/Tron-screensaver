// paralelo.cpp
// Version PARALELA del screensaver "Neon Lines" (estetica Tron), con OpenMP.
// Comparte TODA la logica de neon_lines.cpp con secuencial.cpp (v1): lo unico que
// cambia entre archivos es COMO se recorre el arreglo de lineas en cada fase, para
// que el resultado (checksum) sea identico sin importar cuantos hilos se usen.
//
// Modos (ver Config::mode en config.h):
//   v2        : Fase 1 (movimiento) en paralelo; Fase 2 (colisiones) y el armado de
//               vertices de render se quedan secuenciales, como en v1.
//   v3        : Fase 1 en paralelo; Fase 2 con deteccion en paralelo (cada hilo junta
//               sus propios pares en un buffer local, que luego se fusiona con
//               #pragma omp critical) y resolucion secuencial y ordenada dentro de un
//               unico hilo (#pragma omp single); vertices de render en paralelo.
//   v3-naive  : igual que v3, pero SIN buffer local: cada par hallado entra a la
//               lista global con un #pragma omp critical individual. Se deja como
//               variante de comparacion en la bitacora, para medir el costo de
//               sincronizar de mas.
//   v4        : igual que v3, pero la deteccion usa una particion espacial (grilla
//               uniforme) en vez de probar las N*(N-1)/2 parejas.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <omp.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <utility>
#include <vector>

#include "bench_io.h"
#include "config.h"
#include "neon_lines.h"
#include "render.h"

namespace {

// Fase 2 secuencial (usada por v2, exactamente igual que en secuencial.cpp): revisa
// las N*(N-1)/2 parejas con un solo hilo.
void detectCollisionsSequential(const std::vector<LightBeam>& beams, std::vector<std::pair<int, int>>& pairs) {
    int n = static_cast<int>(beams.size());
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (beamsCollide(beams[static_cast<size_t>(i)], beams[static_cast<size_t>(j)])) {
                pairs.emplace_back(i, j);
            }
        }
    }
}

// Resuelve TODOS los pares ya detectados, en orden determinista (i,j) creciente. Es
// la unica funcion que aplica los choques (escribe velocidades y colores); se llama
// siempre desde un solo hilo -- directamente en v1/v2, o dentro de un
// `#pragma omp single` en v3/v3-naive/v4 -- para que el resultado no dependa de
// cuantos hilos se usaron ni del orden en que llegaron a la seccion critica.
void resolvePairs(std::vector<LightBeam>& beams, std::vector<std::pair<int, int>>& pairs,
                   const Config& config, std::vector<Spark>* sparks, long& collisionCount) {
    std::sort(pairs.begin(), pairs.end());
    for (const auto& pr : pairs) {
        CollisionOutcome outcome = resolveCollision(
            beams[static_cast<size_t>(pr.first)], beams[static_cast<size_t>(pr.second)],
            config.width, config.height, config.speedMin, config.speedMax, config.cooldown);
        if (outcome.happened) {
            collisionCount++;
            if (sparks != nullptr) {
                sparks->push_back(Spark{outcome.sparkX, outcome.sparkY, 0.45f, 0.45f, outcome.sparkColor});
            }
        }
    }
}

// Traduce el Schedule elegido por --schedule a la llamada de OpenMP correspondiente.
// Los `#pragma omp for` de este archivo usan `schedule(runtime)`, que toma la
// politica configurada aqui (se llama una sola vez, no en cada frame, porque no
// cambia durante la corrida).
void applyScheduleSetting(Schedule schedule) {
    switch (schedule) {
        case Schedule::STATIC:  omp_set_schedule(omp_sched_static, 0); break;
        case Schedule::DYNAMIC: omp_set_schedule(omp_sched_dynamic, 16); break;
        case Schedule::GUIDED:  omp_set_schedule(omp_sched_guided, 0); break;
    }
}

// --- Particion espacial (v4): grilla uniforme para acotar cuantos pares se prueban ---
// Formato CSR (compressed sparse row): cellStart[c]..cellStart[c+1] delimita, dentro
// de beamIndices, los indices de las lineas cuyo centro cae en la celda c.
struct SpatialGrid {
    int cols = 1, rows = 1;
    float cellSize = 1.0f;
    std::vector<int> cellStart;
    std::vector<int> beamIndices;
};

int cellIndexFor(float cx, float cy, float cellSize, int cols, int rows) {
    int gx = std::max(0, std::min(cols - 1, static_cast<int>(cx / cellSize)));
    int gy = std::max(0, std::min(rows - 1, static_cast<int>(cy / cellSize)));
    return gy * cols + gx;
}

// Construye la grilla en O(N). El conteo por celda se deja en un solo hilo porque es
// muy barato frente al filtrado de pares que reemplaza: en v1-v3 esa parte es
// O(N^2), y aqui se reduce a O(N * vecinos por celda) -- esa SI se recorre en
// paralelo (ver la rama V4 mas abajo). cellSize debe ser >= la mayor extension
// posible del AABB de una linea (longitud + grosor), para garantizar que dos
// lineas que se pueden tocar siempre caen en celdas vecinas (adyacentes o la misma).
void buildSpatialGrid(const std::vector<LightBeam>& beams, int width, int height,
                       float cellSize, SpatialGrid& grid) {
    grid.cellSize = cellSize;
    grid.cols = std::max(1, static_cast<int>(std::ceil(static_cast<float>(width) / cellSize)));
    grid.rows = std::max(1, static_cast<int>(std::ceil(static_cast<float>(height) / cellSize)));
    int numCells = grid.cols * grid.rows;
    int n = static_cast<int>(beams.size());

    std::vector<int> cellOf(static_cast<size_t>(n));
    grid.cellStart.assign(static_cast<size_t>(numCells) + 1, 0);
    for (int i = 0; i < n; i++) {
        int cell = cellIndexFor(beams[static_cast<size_t>(i)].cx, beams[static_cast<size_t>(i)].cy,
                                 cellSize, grid.cols, grid.rows);
        cellOf[static_cast<size_t>(i)] = cell;
        grid.cellStart[static_cast<size_t>(cell) + 1]++;
    }
    for (int c = 0; c < numCells; c++) {
        grid.cellStart[static_cast<size_t>(c) + 1] += grid.cellStart[static_cast<size_t>(c)];
    }
    std::vector<int> cursor(grid.cellStart.begin(), grid.cellStart.end() - 1);
    grid.beamIndices.assign(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; i++) {
        int cell = cellOf[static_cast<size_t>(i)];
        grid.beamIndices[static_cast<size_t>(cursor[static_cast<size_t>(cell)]++)] = i;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    Config config;
    ParseResult parseResult = parseArgs(argc, argv, config, /*isParallelBinary=*/true);
    if (parseResult == ParseResult::EXIT_OK) return EXIT_SUCCESS;
    if (parseResult == ParseResult::EXIT_ERROR) return EXIT_FAILURE;

    int maxThreads = (config.threads > 0) ? config.threads : omp_get_max_threads();
    omp_set_num_threads(maxThreads);
    applyScheduleSetting(config.schedule);

    std::cout << "Iniciando screensaver PARALELO (Tron - Neon Lines) con N=" << config.n
               << ", modo=" << modeToString(config.mode) << ", hilos=" << maxThreads
               << ", schedule=" << scheduleToString(config.schedule)
               << ", semilla=" << config.seed << ".\n";

    // Toda la aleatoriedad se genera aqui, antes de entrar al loop, con el mismo
    // generador y la misma semilla que usaria secuencial.cpp: por eso los checksums
    // deben coincidir entre versiones para los mismos --seed y --frames.
    std::mt19937 rng(config.seed);
    std::vector<LightBeam> beams = config.debugCorner
        ? std::vector<LightBeam>{ makeDebugCornerBeam(config.width, config.height, config.length,
                                                        (config.speedMin + config.speedMax) * 0.5f) }
        : initBeams(config.n, config.width, config.height, config.length,
                     config.speedMin, config.speedMax, rng);

    const bool benchmarkMode = config.frames > 0;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* canvas = nullptr;
    Hud* hud = nullptr;
    BeamGeometry geometry;

    if (!config.headless) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::cerr << "Error al inicializar SDL: " << SDL_GetError() << "\n";
            return EXIT_FAILURE;
        }
        if (!createWindowAndRenderer(config, "Neon Lines (Tron) - Paralelo", window, renderer)) {
            SDL_Quit();
            return EXIT_FAILURE;
        }
        canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                    SDL_TEXTUREACCESS_TARGET, config.width, config.height);
        if (canvas == nullptr) {
            std::cerr << "Error al crear el lienzo: " << SDL_GetError() << "\n";
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return EXIT_FAILURE;
        }
        SDL_SetTextureBlendMode(canvas, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(renderer, canvas);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, nullptr);

        geometry.resize(beams.size());
        hud = new Hud(renderer, config.fontPath, config.height);
    }

    std::vector<Spark> sparks;
    std::vector<std::pair<int, int>> globalPairs;
    // Un buffer de pares por hilo (memoria protegida: cada hilo solo lee/escribe su
    // propia posicion en este vector, indexada por omp_get_thread_num(), asi que no
    // hace falta ningun candado para llenarlo).
    std::vector<std::vector<std::pair<int, int>>> localPairsPerThread(static_cast<size_t>(maxThreads));

    SpatialGrid grid;
    // La grilla necesita celdas al menos tan grandes como el AABB mas grande posible
    // de una linea, para que dos lineas que puedan tocarse siempre queden en celdas
    // vecinas (ver buildSpatialGrid).
    const float gridCellSize = config.length + thicknessForN(config.n) + 2.0f;

    bool running = true;
    SDL_Event event;
    Uint64 prevTicks = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    float globalTime = 0.0f;
    long collisionCount = 0;
    long pairsTestedTotal = 0;

    int frameCount = 0;
    float fpsTimer = 0.0f;
    double currentFps = 0.0;
    char titleBuffer[192];
    char hudBuffer[192];

    double tUpdateAccum = 0.0, tCollisionAccum = 0.0, tRenderAccum = 0.0;
    int framesRun = 0;

    while (running) {
        if (renderer != nullptr) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
            }
        }

        float dt;
        if (benchmarkMode) {
            dt = 1.0f / 60.0f;
        } else {
            Uint64 nowTicks = SDL_GetPerformanceCounter();
            dt = static_cast<float>(nowTicks - prevTicks) / static_cast<float>(freq);
            prevTicks = nowTicks;
            if (dt > 0.05f) dt = 0.05f;
        }
        globalTime += dt;

        const int n = static_cast<int>(beams.size());

        // --- Fase 1: movimiento, igual en v2/v3/v3-naive/v4 (independiente por
        //     linea: cada hilo procesa un subconjunto de indices sin escribirle a
        //     nadie mas, asi que no hace falta ningun mecanismo de exclusion mutua). ---
        double t0 = omp_get_wtime();
        #pragma omp parallel for schedule(runtime)
        for (int i = 0; i < n; i++) {
            updateBeam(beams[static_cast<size_t>(i)], dt, config.width, config.height);
        }
        double t1 = omp_get_wtime();

        // --- Fase 2: deteccion + resolucion de choques ---
        globalPairs.clear();
        if (config.mode == ParallelMode::V4) {
            buildSpatialGrid(beams, config.width, config.height, gridCellSize, grid);
        }

        if (config.mode == ParallelMode::V2) {
            // v2: solo la Fase 1 se paralelizo; esta fase se queda tal cual v1.
            detectCollisionsSequential(beams, globalPairs);
            resolvePairs(beams, globalPairs, config, (renderer != nullptr) ? &sparks : nullptr, collisionCount);
        } else {
            long pairsTested = 0; // demuestra `reduction`: cada hilo suma su propio conteo
            #pragma omp parallel default(shared)
            {
                int tid = omp_get_thread_num();
                std::vector<std::pair<int, int>>& localPairs = localPairsPerThread[static_cast<size_t>(tid)];
                localPairs.clear();

                if (config.mode == ParallelMode::V3_NAIVE) {
                    // Variante de comparacion: SIN buffer local, un critical POR CADA
                    // par hallado (mucha mas contencion que v3; se deja para medir en
                    // la bitacora el costo de sincronizar de mas).
                    #pragma omp for schedule(runtime) reduction(+:pairsTested)
                    for (int i = 0; i < n - 1; i++) {
                        for (int j = i + 1; j < n; j++) {
                            pairsTested++;
                            if (beamsCollide(beams[static_cast<size_t>(i)], beams[static_cast<size_t>(j)])) {
                                #pragma omp critical(naive_pairs)
                                { globalPairs.emplace_back(i, j); }
                            }
                        }
                    }
                } else if (config.mode == ParallelMode::V4) {
                    int numCells = grid.cols * grid.rows;
                    #pragma omp for schedule(runtime) reduction(+:pairsTested)
                    for (int cell = 0; cell < numCells; cell++) {
                        int gx = cell % grid.cols;
                        int gy = cell / grid.cols;
                        int startA = grid.cellStart[static_cast<size_t>(cell)];
                        int endA = grid.cellStart[static_cast<size_t>(cell) + 1];
                        for (int idxA = startA; idxA < endA; idxA++) {
                            int i = grid.beamIndices[static_cast<size_t>(idxA)];
                            // Solo se revisan la propia celda y sus 8 vecinas (3x3):
                            // como cellSize >= AABB maximo, ninguna colision real
                            // puede quedar fuera de este vecindario.
                            for (int ddy = -1; ddy <= 1; ddy++) {
                                int ny = gy + ddy;
                                if (ny < 0 || ny >= grid.rows) continue;
                                for (int ddx = -1; ddx <= 1; ddx++) {
                                    int nx = gx + ddx;
                                    if (nx < 0 || nx >= grid.cols) continue;
                                    int neighborCell = ny * grid.cols + nx;
                                    int startB = grid.cellStart[static_cast<size_t>(neighborCell)];
                                    int endB = grid.cellStart[static_cast<size_t>(neighborCell) + 1];
                                    for (int idxB = startB; idxB < endB; idxB++) {
                                        int j = grid.beamIndices[static_cast<size_t>(idxB)];
                                        pairsTested++;
                                        // j <= i evita duplicar el par (se probaria
                                        // dos veces, una desde la celda de i y otra
                                        // desde la de j) y evita auto-comparar i==j.
                                        if (j <= i) continue;
                                        if (beamsCollide(beams[static_cast<size_t>(i)], beams[static_cast<size_t>(j)])) {
                                            localPairs.emplace_back(i, j);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    #pragma omp critical(merge_pairs)
                    { globalPairs.insert(globalPairs.end(), localPairs.begin(), localPairs.end()); }
                } else {
                    // V3 (modo por defecto): buffer local por hilo + una sola fusion
                    // por hilo al final, mucha menos contencion que v3-naive.
                    #pragma omp for schedule(runtime) reduction(+:pairsTested)
                    for (int i = 0; i < n - 1; i++) {
                        for (int j = i + 1; j < n; j++) {
                            pairsTested++;
                            if (beamsCollide(beams[static_cast<size_t>(i)], beams[static_cast<size_t>(j)])) {
                                localPairs.emplace_back(i, j);
                            }
                        }
                    }
                    #pragma omp critical(merge_pairs)
                    { globalPairs.insert(globalPairs.end(), localPairs.begin(), localPairs.end()); }
                }

                // Barrera explicita: garantiza que TODOS los hilos ya terminaron su
                // fusion (critical) antes de que el hilo unico de abajo se ponga a
                // resolver globalPairs. Sin esta barrera, un hilo rapido podria
                // llegar al `single` mientras otro todavia no fusiono su buffer.
                #pragma omp barrier

                // Resolucion: un solo hilo, en orden determinista. Cada choque
                // escribe en DOS lineas (beamA y beamB) y una misma linea puede
                // aparecer en varios pares del mismo frame, asi que resolverlos en
                // paralelo exigiria candados por linea y perderia el orden
                // determinista; con N tipico el numero de pares es pequeno frente a
                // las N*(N-1)/2 pruebas de la deteccion, asi que no vale la pena.
                #pragma omp single
                {
                    resolvePairs(beams, globalPairs, config, (renderer != nullptr) ? &sparks : nullptr,
                                 collisionCount);
                }
            } // fin de la region paralela (barrera implicita de salida)

            pairsTestedTotal += pairsTested;
        }
        double t2 = omp_get_wtime();

        // --- Contador de FPS y textos del HUD/titulo/consola ---
        frameCount++;
        fpsTimer += dt;
        if (!benchmarkMode && fpsTimer >= 0.5f) {
            currentFps = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
            std::snprintf(titleBuffer, sizeof(titleBuffer),
                          "Neon Lines (Tron) - %s (%d hilos) | N=%d | FPS=%.2f | Choques=%ld",
                          modeToString(config.mode), maxThreads, config.n, currentFps, collisionCount);
            std::cout << titleBuffer << "\n";
            if (renderer != nullptr) {
                SDL_SetWindowTitle(window, titleBuffer);
                std::snprintf(hudBuffer, sizeof(hudBuffer), "FPS: %.1f  N: %d  HILOS: %d  MODO: %s  CHOQUES: %ld",
                              currentFps, config.n, maxThreads, modeToString(config.mode), collisionCount);
                if (hud != nullptr) hud->setText(hudBuffer);
            }
        }

        // --- Fase 3: render. La construccion de los vertices (buildBeamQuad) es
        //     calculo puro en memoria, en posiciones disjuntas por linea, asi que en
        //     v3/v3-naive/v4 se hace con un omp parallel for; las llamadas reales a
        //     SDL (submitBeamGeometry, drawSparks, RenderPresent...) se quedan en el
        //     hilo principal siempre, porque SDL no es thread-safe. ---
        if (renderer != nullptr) {
            updateSparks(sparks, dt);

            if (config.mode == ParallelMode::V2) {
                for (int i = 0; i < n; i++) {
                    buildBeamQuad(geometry, static_cast<size_t>(i), beams[static_cast<size_t>(i)]);
                }
            } else {
                #pragma omp parallel for schedule(runtime)
                for (int i = 0; i < n; i++) {
                    buildBeamQuad(geometry, static_cast<size_t>(i), beams[static_cast<size_t>(i)]);
                }
            }

            SDL_SetRenderTarget(renderer, canvas);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, config.trail ? 8 : 255);
            SDL_RenderFillRect(renderer, nullptr);

            if (config.grid) drawGrid(renderer, config.width, config.height, globalTime);
            submitBeamGeometry(renderer, geometry);
            drawSparks(renderer, sparks);

            SDL_SetRenderTarget(renderer, nullptr);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_RenderCopy(renderer, canvas, nullptr, nullptr);
            if (hud != nullptr) hud->draw(18, 14);
            SDL_RenderPresent(renderer);
        }
        double t3 = omp_get_wtime();

        tUpdateAccum += (t1 - t0);
        tCollisionAccum += (t2 - t1);
        tRenderAccum += (t3 - t2);
        framesRun++;

        if (benchmarkMode && framesRun >= config.frames) running = false;
    }

    uint64_t checksum = computeChecksum(beams);

    if (benchmarkMode) {
        double tUpdateMs = (tUpdateAccum / framesRun) * 1000.0;
        double tCollisionMs = (tCollisionAccum / framesRun) * 1000.0;
        double tRenderMs = (tRenderAccum / framesRun) * 1000.0;
        double tTotalMs = tUpdateMs + tCollisionMs + tRenderMs;
        double fpsAvg = (tTotalMs > 0.0) ? (1000.0 / tTotalMs) : 0.0;

        std::cout << "--- Resultados del benchmark (promedio por frame, " << framesRun << " frames) ---\n"
                   << "  Modo:                 " << modeToString(config.mode) << "\n"
                   << "  Hilos:                " << maxThreads << "\n"
                   << "  Schedule:             " << scheduleToString(config.schedule) << "\n"
                   << "  Fase 1 (movimiento):  " << tUpdateMs << " ms\n"
                   << "  Fase 2 (colisiones):  " << tCollisionMs << " ms\n"
                   << "  Fase 3 (render):      " << tRenderMs << " ms\n"
                   << "  Total:                " << tTotalMs << " ms  (" << fpsAvg << " fps)\n"
                   << "  Choques acumulados:   " << collisionCount << "\n"
                   << "  Pares probados (acum.): " << pairsTestedTotal << "\n"
                   << "  Checksum:             " << checksum << "\n";

        if (!config.csvPath.empty()) {
            appendCsvRow(config.csvPath, modeToString(config.mode), scheduleToString(config.schedule),
                         config.n, maxThreads, framesRun, config.seed,
                         tUpdateMs, tCollisionMs, tRenderMs, tTotalMs, fpsAvg, checksum);
        }
    }

    if (renderer != nullptr) {
        delete hud;
        if (TTF_WasInit() != 0) TTF_Quit();
        SDL_DestroyTexture(canvas);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    return EXIT_SUCCESS;
}
