// secuencial.cpp
// Version SECUENCIAL (v1) del screensaver "Neon Lines" (estetica Tron). Un solo
// hilo procesa, en cada frame, las tres fases del algoritmo:
//   Fase 1: mover cada linea e implementar el rebote en los bordes.
//   Fase 2: detectar que pares de lineas chocan y resolver el choque (fisica +
//           intercambio de color) en orden determinista.
//   Fase 3: renderizar todo (siempre secuencial: SDL no es thread-safe).
// Esta es la version base (baseline) contra la que se compara el speedup de
// paralelo.cpp. Comparten toda la logica de neon_lines.cpp: la unica diferencia
// entre este archivo y paralelo.cpp es COMO se recorre el arreglo de lineas.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <omp.h> // solo se usa omp_get_wtime() para medir tiempos; no hay pragmas en este archivo

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

int main(int argc, char* argv[]) {
    Config config;
    ParseResult parseResult = parseArgs(argc, argv, config, /*isParallelBinary=*/false);
    if (parseResult == ParseResult::EXIT_OK) return EXIT_SUCCESS;
    if (parseResult == ParseResult::EXIT_ERROR) return EXIT_FAILURE;

    std::cout << "Iniciando screensaver SECUENCIAL (Tron - Neon Lines) con N=" << config.n
               << ", semilla=" << config.seed << ".\n";

    // Toda la aleatoriedad (posiciones, angulos, velocidades y colores iniciales) se
    // genera aqui, una sola vez, con un generador sembrado por --seed. Nada de rand()
    // ni de generadores nuevos dentro del loop principal: asi la corrida es
    // reproducible y comparable frame a frame contra la version paralela.
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
        if (!createWindowAndRenderer(config, "Neon Lines (Tron) - Secuencial", window, renderer)) {
            SDL_Quit();
            return EXIT_FAILURE;
        }

        // Lienzo persistente para la estela neon: en vez de limpiar a negro puro cada
        // frame, se rellena con negro semitransparente, lo que deja un rastro que se
        // va desvaneciendo (efecto "light-cycle trail" de la version de Roberto).
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
        hud = new Hud(renderer, config.fontPath, config.height); // si la fuente falla, sigue sin HUD
    }

    std::vector<Spark> sparks;
    // Buffer de pares en colision, reutilizado entre frames (sin reservar memoria
    // nueva en el heap en cada uno).
    std::vector<std::pair<int, int>> collisionPairs;

    bool running = true;
    SDL_Event event;
    Uint64 prevTicks = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    float globalTime = 0.0f;
    long collisionCount = 0;

    int frameCount = 0;
    float fpsTimer = 0.0f;
    double currentFps = 0.0;
    char titleBuffer[192];
    char hudBuffer[192];

    // Acumuladores de tiempo por fase (para el modo benchmark: --frames / --csv).
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
            dt = 1.0f / 60.0f; // dt fijo: el benchmark mide computo puro, no la velocidad real de la maquina
        } else {
            Uint64 nowTicks = SDL_GetPerformanceCounter();
            dt = static_cast<float>(nowTicks - prevTicks) / static_cast<float>(freq);
            prevTicks = nowTicks;
            if (dt > 0.05f) dt = 0.05f; // evita saltos grandes tras una pausa (p. ej. mover la ventana)
        }
        globalTime += dt;

        const int n = static_cast<int>(beams.size());

        // --- Fase 1: movimiento (un solo hilo: for comun) ---
        double t0 = omp_get_wtime();
        for (int i = 0; i < n; i++) {
            updateBeam(beams[static_cast<size_t>(i)], dt, config.width, config.height);
        }
        double t1 = omp_get_wtime();

        // --- Fase 2: deteccion (solo lectura) + resolucion (en orden i<j) de choques ---
        collisionPairs.clear();
        for (int i = 0; i < n - 1; i++) {
            for (int j = i + 1; j < n; j++) {
                if (beamsCollide(beams[static_cast<size_t>(i)], beams[static_cast<size_t>(j)])) {
                    collisionPairs.emplace_back(i, j);
                }
            }
        }
        for (const auto& pair : collisionPairs) {
            CollisionOutcome outcome = resolveCollision(
                beams[static_cast<size_t>(pair.first)], beams[static_cast<size_t>(pair.second)],
                config.width, config.height, config.speedMin, config.speedMax, config.cooldown);
            if (outcome.happened) {
                collisionCount++;
                if (renderer != nullptr) {
                    sparks.push_back(Spark{outcome.sparkX, outcome.sparkY, 0.45f, 0.45f, outcome.sparkColor});
                }
            }
        }
        double t2 = omp_get_wtime();

        // --- Contador de FPS y textos del HUD/titulo/consola (no en modo benchmark: con
        // dt fijo el "FPS" no significa nada, ahi se reporta el promedio real al final) ---
        frameCount++;
        fpsTimer += dt;
        if (!benchmarkMode && fpsTimer >= 0.5f) {
            currentFps = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
            std::snprintf(titleBuffer, sizeof(titleBuffer),
                          "Neon Lines (Tron) - Secuencial | N=%d | FPS=%.2f | Choques=%ld",
                          config.n, currentFps, collisionCount);
            std::cout << titleBuffer << "\n";
            if (renderer != nullptr) {
                SDL_SetWindowTitle(window, titleBuffer);
                std::snprintf(hudBuffer, sizeof(hudBuffer), "FPS: %.1f  N: %d  MODO: v1  CHOQUES: %ld",
                              currentFps, config.n, collisionCount);
                if (hud != nullptr) hud->setText(hudBuffer);
            }
        }

        // --- Fase 3: render (siempre secuencial; no ocurre en modo headless) ---
        if (renderer != nullptr) {
            updateSparks(sparks, dt);

            SDL_SetRenderTarget(renderer, canvas);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, config.trail ? 8 : 255);
            SDL_RenderFillRect(renderer, nullptr);

            if (config.grid) drawGrid(renderer, config.width, config.height, globalTime);
            drawBeams(renderer, geometry, beams);
            drawSparks(renderer, sparks);

            SDL_SetRenderTarget(renderer, nullptr);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_RenderCopy(renderer, canvas, nullptr, nullptr);
            if (hud != nullptr) hud->draw(18, 14); // se dibuja SOBRE la copia del lienzo, antes de Present
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
                   << "  Fase 1 (movimiento):  " << tUpdateMs << " ms\n"
                   << "  Fase 2 (colisiones):  " << tCollisionMs << " ms\n"
                   << "  Fase 3 (render):      " << tRenderMs << " ms\n"
                   << "  Total:                " << tTotalMs << " ms  (" << fpsAvg << " fps)\n"
                   << "  Choques acumulados:   " << collisionCount << "\n"
                   << "  Checksum:             " << checksum << "\n";

        if (!config.csvPath.empty()) {
            appendCsvRow(config.csvPath, modeToString(ParallelMode::SEQUENTIAL), "n/a",
                         config.n, 1, framesRun, config.seed,
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
