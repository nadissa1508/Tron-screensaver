// render.cpp
// Implementacion de las funciones de dibujo (ver render.h). Todo lo que llama a
// SDL_Render* corre en el hilo principal; solo buildBeamQuad esta pensada para
// llamarse tambien desde un `omp for` (no toca SDL, solo escribe en memoria).
#include "render.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void updateSparks(std::vector<Spark>& sparks, float dt) {
    for (auto& spark : sparks) spark.life -= dt;
    sparks.erase(std::remove_if(sparks.begin(), sparks.end(),
                                 [](const Spark& spark) { return spark.life <= 0.0f; }),
                 sparks.end());
}

void drawSparks(SDL_Renderer* renderer, const std::vector<Spark>& sparks) {
    if (sparks.empty()) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    const int segments = 16;
    for (const auto& spark : sparks) {
        float progress = 1.0f - (spark.life / spark.maxLife);
        int ringRadius = static_cast<int>(4 + progress * 26);
        Uint8 ringAlpha = static_cast<Uint8>(200.0f * (1.0f - progress));
        SDL_SetRenderDrawColor(renderer, spark.color.r, spark.color.g, spark.color.b, ringAlpha);
        for (int k = 0; k < segments; k++) {
            float a0 = (2.0f * static_cast<float>(M_PI) * k) / segments;
            float a1 = (2.0f * static_cast<float>(M_PI) * (k + 1)) / segments;
            int x0 = static_cast<int>(spark.x + std::cos(a0) * ringRadius);
            int y0 = static_cast<int>(spark.y + std::sin(a0) * ringRadius);
            int x1 = static_cast<int>(spark.x + std::cos(a1) * ringRadius);
            int y1 = static_cast<int>(spark.y + std::sin(a1) * ringRadius);
            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        }
    }
}

void drawGrid(SDL_Renderer* renderer, int width, int height, float globalTime) {
    Uint8 pulse = static_cast<Uint8>(10 + 8 * (0.5f + 0.5f * std::sin(globalTime * 0.6f)));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(renderer, 25, 80, 130, pulse);
    const int spacing = 40;
    for (int x = 0; x <= width; x += spacing) {
        SDL_RenderDrawLine(renderer, x, 0, x, height);
    }
    for (int y = 0; y <= height; y += spacing) {
        SDL_RenderDrawLine(renderer, 0, y, width, y);
    }
}

void BeamGeometry::resize(size_t n) {
    glowVerts.assign(n * 4, SDL_Vertex{});
    coreVerts.assign(n * 4, SDL_Vertex{});
    glowIdx.resize(n * 6);
    coreIdx.resize(n * 6);
    for (size_t i = 0; i < n; i++) {
        size_t base = i * 4;
        size_t ib = i * 6;
        // Dos triangulos por quad: (0,1,2) y (1,3,2). Este patron es el mismo para
        // glow y nucleo, y no depende de los datos de la linea, asi que se calcula
        // una sola vez aqui (no en cada frame).
        int idx[6] = {
            static_cast<int>(base + 0), static_cast<int>(base + 1), static_cast<int>(base + 2),
            static_cast<int>(base + 1), static_cast<int>(base + 3), static_cast<int>(base + 2)
        };
        for (int k = 0; k < 6; k++) {
            glowIdx[ib + static_cast<size_t>(k)] = idx[k];
            coreIdx[ib + static_cast<size_t>(k)] = idx[k];
        }
    }
}

void buildBeamQuad(BeamGeometry& geometry, size_t index, const LightBeam& beam) {
    float dx = beam.x2 - beam.x1;
    float dy = beam.y2 - beam.y1;
    float len = std::sqrt(dx * dx + dy * dy);
    float nx = 0.0f, ny = 0.0f;
    if (len > 0.0001f) {
        nx = -dy / len;
        ny = dx / len;
    }

    SDL_Color glowColor = beam.color;
    glowColor.a = 60;
    SDL_Color coreColor = beam.color;
    coreColor.a = 255;

    float glowHalf = (beam.thickness + 14.0f) * 0.5f;
    float coreHalf = beam.thickness * 0.5f;

    size_t base = index * 4;
    SDL_FPoint tex{0.0f, 0.0f};

    geometry.glowVerts[base + 0] = SDL_Vertex{ {beam.x1 + nx * glowHalf, beam.y1 + ny * glowHalf}, glowColor, tex };
    geometry.glowVerts[base + 1] = SDL_Vertex{ {beam.x1 - nx * glowHalf, beam.y1 - ny * glowHalf}, glowColor, tex };
    geometry.glowVerts[base + 2] = SDL_Vertex{ {beam.x2 + nx * glowHalf, beam.y2 + ny * glowHalf}, glowColor, tex };
    geometry.glowVerts[base + 3] = SDL_Vertex{ {beam.x2 - nx * glowHalf, beam.y2 - ny * glowHalf}, glowColor, tex };

    geometry.coreVerts[base + 0] = SDL_Vertex{ {beam.x1 + nx * coreHalf, beam.y1 + ny * coreHalf}, coreColor, tex };
    geometry.coreVerts[base + 1] = SDL_Vertex{ {beam.x1 - nx * coreHalf, beam.y1 - ny * coreHalf}, coreColor, tex };
    geometry.coreVerts[base + 2] = SDL_Vertex{ {beam.x2 + nx * coreHalf, beam.y2 + ny * coreHalf}, coreColor, tex };
    geometry.coreVerts[base + 3] = SDL_Vertex{ {beam.x2 - nx * coreHalf, beam.y2 - ny * coreHalf}, coreColor, tex };
}

void submitBeamGeometry(SDL_Renderer* renderer, const BeamGeometry& geometry) {
    // Capa de glow: mezcla aditiva, ancha y translucida. Una sola llamada para
    // TODAS las lineas (no una por linea), que es lo que evita que el render se
    // vuelva el cuello de botella cuando N crece.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_RenderGeometry(renderer, nullptr, geometry.glowVerts.data(),
                        static_cast<int>(geometry.glowVerts.size()),
                        geometry.glowIdx.data(), static_cast<int>(geometry.glowIdx.size()));

    // Nucleo solido y opaco encima, tambien en una sola llamada.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, nullptr, geometry.coreVerts.data(),
                        static_cast<int>(geometry.coreVerts.size()),
                        geometry.coreIdx.data(), static_cast<int>(geometry.coreIdx.size()));
}

void drawBeams(SDL_Renderer* renderer, BeamGeometry& geometry, const std::vector<LightBeam>& beams) {
    for (size_t i = 0; i < beams.size(); i++) {
        buildBeamQuad(geometry, i, beams[i]);
    }
    submitBeamGeometry(renderer, geometry);
}

Hud::Hud(SDL_Renderer* renderer, const std::string& fontPath, int windowHeight)
    : renderer_(renderer) {
    if (TTF_WasInit() == 0 && TTF_Init() != 0) {
        std::cerr << "Aviso: TTF_Init fallo (" << TTF_GetError()
                   << "). El HUD en pantalla queda desactivado; los FPS se siguen mostrando"
                   << " en el titulo de la ventana y en la consola.\n";
        return;
    }
    int fontSize = std::max(14, windowHeight / 30);
    font_ = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (font_ == nullptr) {
        std::cerr << "Aviso: no se pudo abrir la fuente '" << fontPath << "' (" << TTF_GetError()
                   << "). El HUD en pantalla queda desactivado; los FPS se siguen mostrando"
                   << " en el titulo de la ventana y en la consola.\n";
    }
}

Hud::~Hud() {
    if (texture_ != nullptr) SDL_DestroyTexture(texture_);
    if (font_ != nullptr) TTF_CloseFont(font_);
    // TTF_Quit() se llama una sola vez desde el main correspondiente (secuencial.cpp /
    // paralelo.cpp), despues de que el Hud se destruye, no aqui.
}

void Hud::setText(const std::string& text) {
    if (!isEnabled() || text == lastText_) return;
    lastText_ = text;

    SDL_Color textColor{0, 230, 255, 255}; // cian neon, mismo tono base que la paleta Tron
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font_, text.c_str(), textColor);
    if (surface == nullptr) {
        std::cerr << "Aviso: TTF_RenderUTF8_Blended fallo (" << TTF_GetError() << ").\n";
        return;
    }
    SDL_Texture* newTexture = SDL_CreateTextureFromSurface(renderer_, surface);
    int width = surface->w;
    int height = surface->h;
    SDL_FreeSurface(surface);

    if (newTexture == nullptr) {
        std::cerr << "Aviso: SDL_CreateTextureFromSurface fallo (" << SDL_GetError() << ").\n";
        return;
    }
    if (texture_ != nullptr) SDL_DestroyTexture(texture_);
    texture_ = newTexture;
    textureW_ = width;
    textureH_ = height;
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
}

void Hud::draw(int marginX, int marginY) {
    if (!isEnabled() || texture_ == nullptr) return;
    SDL_Rect dst{marginX, marginY, textureW_, textureH_};
    SDL_RenderCopy(renderer_, texture_, nullptr, &dst);
}
