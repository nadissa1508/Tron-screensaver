// render.h
// Todo lo que llama a SDL para dibujar (glow, rejilla, chispas y el HUD de texto).
// Estas funciones SIEMPRE corren en el hilo principal: SDL no es thread-safe, y
// SDL2_ttf tampoco. La unica parte de este modulo pensada para llamarse en paralelo
// es buildBeamQuad (construccion de vertices en memoria, sin tocar SDL).
#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

#include "neon_lines.h"

// Una chispa breve en el punto medio de un choque, para hacer visible el
// intercambio de color ademas del cambio de color mismo.
struct Spark {
    float x, y;
    float life;     // segundos restantes de vida
    float maxLife;  // duracion total (para calcular el progreso de la animacion)
    SDL_Color color;
};

void updateSparks(std::vector<Spark>& sparks, float dt);
void drawSparks(SDL_Renderer* renderer, const std::vector<Spark>& sparks);

// Rejilla de fondo sutil con un ligero pulso en el tiempo (estetica de grid digital).
void drawGrid(SDL_Renderer* renderer, int width, int height, float globalTime);

// Buffers reutilizables para dibujar todas las lineas en un solo batch (2 llamadas a
// SDL_RenderGeometry por frame -- glow y nucleo -- en vez de 2*N). Los indices de
// cada quad (4 vertices, 2 triangulos) son fijos y se calculan una sola vez en
// resize(); en cada frame solo se reescriben las posiciones y colores de los
// vertices, lo cual puede hacerse en paralelo (ver buildBeamQuad).
struct BeamGeometry {
    std::vector<SDL_Vertex> glowVerts, coreVerts;
    std::vector<int> glowIdx, coreIdx;

    // Reserva/ajusta los buffers para n lineas y precalcula los indices de cada quad.
    // Se llama una vez al iniciar (o si N cambia); no hace falta llamarla cada frame.
    void resize(size_t n);
};

// Escribe en geometry los 4 vertices (glow y nucleo) de la linea en la posicion
// "index" (0-based), usando solo esa posicion del arreglo. Es pura escritura en
// memoria (no llama a SDL) en indices disjuntos entre lineas distintas, por lo que
// es segura dentro de un `#pragma omp parallel for` mientras cada hilo procese un
// "index" distinto.
void buildBeamQuad(BeamGeometry& geometry, size_t index, const LightBeam& beam);

// Emite las dos llamadas a SDL_RenderGeometry (glow + nucleo) para todo lo que haya
// en geometry. Debe llamarse siempre desde el hilo principal.
void submitBeamGeometry(SDL_Renderer* renderer, const BeamGeometry& geometry);

// Atajo secuencial: construye la geometria de todas las lineas con un for comun y la
// dibuja. Lo usa secuencial.cpp; paralelo.cpp llama buildBeamQuad dentro de un
// `omp for` y luego submitBeamGeometry por separado (ver paralelo.cpp).
void drawBeams(SDL_Renderer* renderer, BeamGeometry& geometry, const std::vector<LightBeam>& beams);

// HUD de texto (SDL2_ttf): FPS, N, hilos, modo y choques acumulados, dibujados en
// pantalla con la fuente Orbitron. Encapsula una textura cacheada que solo se vuelve
// a rasterizar cuando el texto cambia (cada ~500ms, junto con el calculo de FPS), no
// en cada frame, para no volverse un cuello de botella.
class Hud {
public:
    // Intenta inicializar SDL2_ttf y abrir la fuente en fontPath. Si cualquiera de
    // los dos falla, isEnabled() queda en false, se imprime el motivo por consola
    // (TTF_GetError()) y el screensaver sigue funcionando sin HUD en pantalla (los
    // FPS se siguen mostrando en el titulo de la ventana y en la consola).
    Hud(SDL_Renderer* renderer, const std::string& fontPath, int windowHeight);
    ~Hud();
    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    bool isEnabled() const { return font_ != nullptr; }

    // Regenera la textura del HUD solo si el texto cambio desde la ultima llamada.
    void setText(const std::string& text);

    // Dibuja la textura cacheada en la esquina superior izquierda, con el margen dado.
    void draw(int marginX, int marginY);

private:
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int textureW_ = 0;
    int textureH_ = 0;
    std::string lastText_;
};
