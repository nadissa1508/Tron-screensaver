// neon_lines.h
// Estructura de datos y logica pura (sin llamadas a SDL de render) del screensaver:
// creacion de lineas, movimiento con rebote, deteccion e intercambio en choques, y
// paleta de colores estilo Tron. Este modulo NO sabe nada de ventanas ni de OpenMP:
// las versiones secuencial y paralela (secuencial.cpp / paralelo.cpp) llaman a estas
// mismas funciones, cada una recorriendo el arreglo de lineas a su manera, para que
// el resultado (checksum) sea identico entre versiones.
#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <random>
#include <vector>

// Un "haz de luz" (light beam): segmento recto y rigido de longitud fija que se
// mueve, rebota en los bordes de la ventana y, al chocar con otro, intercambia
// velocidades (componente normal) y color. Es la struct "Line" de la propuesta.
struct LightBeam {
    float x1, y1, x2, y2;         // extremos del segmento (cola y punta), derivados de cx,cy,angle,length
    float cx, cy;                 // centro del segmento; es la posicion que se integra frame a frame
    float vx, vy;                 // velocidad en px/s
    float angle;                  // atan2(vy, vx); se recalcula cada vez que vx/vy cambian
    float length;                 // longitud fija del segmento, en px
    float thickness;              // grosor de dibujo, en px (depende de N, ver thicknessForN)
    float minX, minY, maxX, maxY; // bounding box (AABB) para el filtro rapido de colisiones
    float hue;                    // tono (grados) dentro de la paleta Tron
    SDL_Color color;               // color RGBA actual, derivado de hue (se intercambia junto con hue)
    float cooldown;                // segundos restantes sin poder participar en un nuevo choque
    int   id;                      // identificador unico y estable (no cambia si el vector se reordena)
};

// Convierte un color HSV (hue en grados, saturacion y valor en [0,1]) a RGB de 8 bits.
// Se usa S=0.85, V=1.0 para todas las lineas, siguiendo el aspecto neon de la version
// de Roberto (colores vivos pero no blancos puros).
SDL_Color hsvToRgb(float hueDegrees, float saturation, float value);

// Elige un tono (grados) siguiendo la paleta Tron: ~55% cian, ~25% naranja (coherencia
// con la Entrega 2), ~13% azul electrico, ~7% ambar, cada familia con variacion
// pseudoaleatoria (jitter). Usa el generador que se le pasa (nunca rand()), para que
// las corridas sean reproducibles con --seed.
float pickTronHue(std::mt19937& rng);

// Grosor de dibujo segun N: mas lineas, trazo mas fino, para no saturar la pantalla.
float thicknessForN(int n);

// Crea N lineas con posicion, angulo, velocidad y color pseudoaleatorios (con rng,
// sembrado por --seed). Todo el trabajo de randomizacion ocurre aqui, una sola vez,
// nunca dentro de las fases que se paralelizan.
std::vector<LightBeam> initBeams(int n, int width, int height, float length,
                                  float speedMin, float speedMax, std::mt19937& rng);

// Crea una unica linea apuntando hacia la esquina superior izquierda, usada por
// --debug-corner para verificar visualmente que el rebote en esquinas no "tiembla".
LightBeam makeDebugCornerBeam(int width, int height, float length, float speed);

// --- Fase 1: movimiento (independiente por linea, segura para omp parallel for) ---

// Integra la posicion de una linea y la mantiene dentro de la ventana (ver
// keepInsideBounds). Tambien hace avanzar su cooldown hacia 0.
void updateBeam(LightBeam& beam, float dt, int width, int height);

// Reacomoda una linea para que quede dentro de [0,width]x[0,height], invirtiendo la
// componente de velocidad que la llevaba hacia afuera SOLO si aun apunta hacia la
// pared (evita el "temblor" de invertir el angulo en cada frame). Tambien recalcula
// angle, los extremos (x1,y1,x2,y2) y el AABB. Se usa tanto tras integrar el
// movimiento como tras resolver un choque (que puede reposicionar el centro).
void keepInsideBounds(LightBeam& beam, int width, int height);

// --- Fase 2: deteccion de choques (solo lectura sobre las lineas -> paralelizable) ---

// Test clasico de interseccion de dos segmentos por orientacion (producto cruz).
bool segmentsIntersect(float x1, float y1, float x2, float y2,
                        float x3, float y3, float x4, float y4);

// true si a y b deberian chocar este frame: ninguna esta en cooldown, sus AABB se
// solapan (filtro rapido) y sus segmentos realmente se intersectan.
bool beamsCollide(const LightBeam& a, const LightBeam& b);

// --- Resolucion de un choque ya detectado (escribe en ambas lineas) ---

// Resultado de intentar resolver un choque: si "happened" es false, las lineas ya se
// estaban separando (por ejemplo, un choque detectado el frame anterior que todavia
// se solapa) y no se debe crear chispa ni contar el choque.
struct CollisionOutcome {
    bool happened;
    float sparkX, sparkY;
    SDL_Color sparkColor;
};

// Aplica un choque elastico entre masas iguales (se intercambia la componente de
// velocidad sobre la normal del choque), intercambia colores, separa las lineas si
// quedaron muy encimadas y las reacomoda dentro de los bordes. Debe llamarse de forma
// secuencial y en orden determinista cuando varios pares comparten una misma linea
// (ver paralelo.cpp), porque escribe en beamA y beamB.
CollisionOutcome resolveCollision(LightBeam& beamA, LightBeam& beamB, int width, int height,
                                   float speedMin, float speedMax, float cooldownSeconds);

// FNV-1a sobre posiciones (cuantizadas a 1e-3 px), id y color de cada linea, ordenado
// por id para no depender del orden interno del arreglo. Sirve para comprobar que
// v1, v2, v3 y v4 producen exactamente el mismo resultado con la misma --seed.
uint64_t computeChecksum(const std::vector<LightBeam>& beams);
