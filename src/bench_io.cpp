// bench_io.cpp
// Implementacion de las utilidades compartidas (ver bench_io.h).
#include "bench_io.h"

#include <fstream>
#include <iostream>

bool createWindowAndRenderer(const Config& config, const char* windowTitle,
                              SDL_Window*& window, SDL_Renderer*& renderer) {
    window = SDL_CreateWindow(
        windowTitle,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config.width, config.height, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "Error al crear la ventana: " << SDL_GetError() << "\n";
        return false;
    }

    Uint32 flags = SDL_RENDERER_ACCELERATED | (config.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    renderer = SDL_CreateRenderer(window, -1, flags);
    if (renderer == nullptr) {
        std::cerr << "Renderer acelerado no disponible (" << SDL_GetError()
                   << "), probando con renderer por software...\n";
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        std::cerr << "Error al crear el renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        window = nullptr;
        return false;
    }
    return true;
}

void appendCsvRow(const std::string& path, const char* versionLabel, const char* scheduleLabel,
                   int n, int threads, int frames, unsigned int seed,
                   double tUpdateMs, double tCollisionMs, double tRenderMs, double tTotalMs,
                   double fpsAvg, uint64_t checksum) {
    bool needsHeader = false;
    {
        std::ifstream test(path);
        needsHeader = !test.good();
    }
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) {
        std::cerr << "Aviso: no se pudo abrir --csv '" << path << "' para escribir; se omite esta fila.\n";
        return;
    }
    if (needsHeader) {
        out << "version,schedule,n,threads,frames,seed,t_update_ms,t_collision_ms,t_render_ms,"
               "t_total_ms,fps_avg,checksum\n";
    }
    out << versionLabel << "," << scheduleLabel << "," << n << "," << threads << ","
        << frames << "," << seed << "," << tUpdateMs << "," << tCollisionMs << "," << tRenderMs
        << "," << tTotalMs << "," << fpsAvg << "," << checksum << "\n";
}
