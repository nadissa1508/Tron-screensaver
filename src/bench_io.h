// bench_io.h
// Utilidades compartidas por secuencial.cpp y paralelo.cpp para que ambos binarios
// creen la ventana/renderer exactamente igual y escriban el CSV de mediciones con
// el mismo formato (indispensable para poder comparar speedup entre versiones).
#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <string>

#include "config.h"

// Crea la ventana y el renderer con el tamano y el modo vsync de config, con
// fallback a renderer por software si el acelerado no esta disponible (por ejemplo,
// en una maquina virtual sin GPU). Devuelve false en cualquier error, ya con el
// mensaje impreso por SDL_GetError() y sin dejar recursos a medio crear (todo lo
// que se llego a crear antes del fallo se destruye antes de devolver false).
bool createWindowAndRenderer(const Config& config, const char* windowTitle,
                              SDL_Window*& window, SDL_Renderer*& renderer);

// Agrega una fila de resultados al CSV en path (creando el archivo y su encabezado
// si todavia no existe). Se usa desde el modo benchmark (--csv) de ambos binarios.
void appendCsvRow(const std::string& path, const char* versionLabel, const char* scheduleLabel,
                   int n, int threads, int frames, unsigned int seed,
                   double tUpdateMs, double tCollisionMs, double tRenderMs, double tTotalMs,
                   double fpsAvg, uint64_t checksum);
