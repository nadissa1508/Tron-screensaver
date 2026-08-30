# Makefile del screensaver "Neon Lines" (Tron). Genera dos ejecutables:
#   secuencial.exe -> version v1 (un solo hilo)
#   paralelo.exe   -> versiones v2/v3/v3-naive/v4 (OpenMP), elegidas con --mode
# Requisito del enunciado: NO se entregan ejecutables (.gitignore ya los excluye);
# este Makefile es la forma de reconstruirlos en una maquina limpia con MinGW-w64.

CXX = g++
# SDL2_ttf.h hace `#include "SDL.h"` (sin el prefijo SDL2/), asi que ademas de
# libs/SDL2/include (para nuestro propio `#include <SDL2/SDL.h>`) hace falta agregar
# libs/SDL2/include/SDL2 al include path para que esa referencia interna se resuelva.
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fopenmp \
           -I libs/SDL2/include -I libs/SDL2/include/SDL2 -I libs/SDL2_ttf/include
LDFLAGS = -L libs/SDL2/lib -L libs/SDL2_ttf/lib \
          -lmingw32 -lSDL2main -lSDL2_ttf -lSDL2

# Modulos que comparten secuencial.exe y paralelo.exe: configuracion (parseo de
# argumentos), logica pura del screensaver (movimiento, choques, paleta), render
# (SDL_RenderGeometry, rejilla, chispas, HUD) y utilidades de benchmark (ventana,
# CSV). Cada uno se compila una sola vez a .o y se enlaza en ambos binarios.
COMMON_SRC = src/config.cpp src/neon_lines.cpp src/render.cpp src/bench_io.cpp
COMMON_OBJ = $(COMMON_SRC:.cpp=.o)

SEQ_BIN = secuencial.exe
PAR_BIN = paralelo.exe

all: $(SEQ_BIN) $(PAR_BIN)

secuencial: $(SEQ_BIN)
paralelo: $(PAR_BIN)

$(SEQ_BIN): src/secuencial.o $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@cp -n libs/SDL2/bin/SDL2.dll . 2>/dev/null || true
	@cp -n libs/SDL2_ttf/bin/SDL2_ttf.dll . 2>/dev/null || true

$(PAR_BIN): src/paralelo.o $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@cp -n libs/SDL2/bin/SDL2.dll . 2>/dev/null || true
	@cp -n libs/SDL2_ttf/bin/SDL2_ttf.dll . 2>/dev/null || true

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Corrida corta (headless, sin ventana) para verificar que ambos binarios compilan
# y corren correctamente antes de una sesion de medicion larga (ver scripts/benchmark.ps1).
bench: all
	./$(SEQ_BIN) 200 --headless --frames 120 --seed 42 --csv resultados/smoke_test.csv
	./$(PAR_BIN) 200 --headless --frames 120 --seed 42 --mode v3 --csv resultados/smoke_test.csv

clean:
	rm -f $(SEQ_BIN) $(PAR_BIN) src/*.o

.PHONY: all clean secuencial paralelo bench
