// neon_lines.cpp
// Implementacion de la logica pura del screensaver (ver neon_lines.h para el
// contrato de cada funcion). Aqui vive la fisica y la trigonometria que pide el
// enunciado: rebote especular en los bordes, choque elastico entre lineas e
// interseccion de segmentos.
#include "neon_lines.h"

#include <algorithm>
#include <cmath>

namespace {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Recalcula los extremos (x1,y1,x2,y2) de una linea a partir de su centro, angulo
// y longitud. Se llama cada vez que cx, cy o angle cambian.
void computeEndpoints(LightBeam& beam) {
    float dx = std::cos(beam.angle) * beam.length * 0.5f;
    float dy = std::sin(beam.angle) * beam.length * 0.5f;
    beam.x1 = beam.cx - dx;
    beam.y1 = beam.cy - dy;
    beam.x2 = beam.cx + dx;
    beam.y2 = beam.cy + dy;
}

// Semi-extension del segmento sobre cada eje (mitad del ancho/alto de su AABB).
// Un rebote especular invierte el signo de vx o vy pero no cambia |cos(angle)| ni
// |sin(angle)|, asi que estos valores no cambian al rebotar (solo al chocar, que
// si puede cambiar el angulo de verdad).
float halfExtentX(const LightBeam& beam) {
    return std::fabs(std::cos(beam.angle)) * beam.length * 0.5f + beam.thickness * 0.5f;
}
float halfExtentY(const LightBeam& beam) {
    return std::fabs(std::sin(beam.angle)) * beam.length * 0.5f + beam.thickness * 0.5f;
}

// Recalcula el AABB (minX/minY/maxX/maxY) a partir del centro y las semi-extensiones.
void computeAabb(LightBeam& beam) {
    float hx = halfExtentX(beam);
    float hy = halfExtentY(beam);
    beam.minX = beam.cx - hx;
    beam.maxX = beam.cx + hx;
    beam.minY = beam.cy - hy;
    beam.maxY = beam.cy + hy;
}

// Ajusta la rapidez de una linea para que quede en [speedMin*0.6, speedMax*1.4]:
// evita que un choque la deje casi quieta o que salga disparada sin control.
void clampSpeed(LightBeam& beam, float speedMin, float speedMax) {
    float speed = std::sqrt(beam.vx * beam.vx + beam.vy * beam.vy);
    if (speed < 0.0001f) return; // velocidad nula: no hay direccion que preservar
    float lo = speedMin * 0.6f;
    float hi = speedMax * 1.4f;
    float clamped = std::max(lo, std::min(hi, speed));
    float scale = clamped / speed;
    beam.vx *= scale;
    beam.vy *= scale;
}

// Orientacion de la terna (p, q, r): 0 colineales, 1 horario, 2 antihorario.
// Es el test clasico por producto cruz, portado de la version de Roberto.
int orientation(float px, float py, float qx, float qy, float rx, float ry) {
    float val = (qy - py) * (rx - qx) - (qx - px) * (ry - qy);
    if (std::fabs(val) < 1e-6f) return 0;
    return (val > 0) ? 1 : 2;
}

// true si q esta dentro del rectangulo delimitado por p y r (para el caso colineal).
bool onSegment(float px, float py, float qx, float qy, float rx, float ry) {
    return qx <= std::max(px, rx) && qx >= std::min(px, rx) &&
           qy <= std::max(py, ry) && qy >= std::min(py, ry);
}

} // namespace

SDL_Color hsvToRgb(float hueDegrees, float saturation, float value) {
    float c = value * saturation;
    float hp = hueDegrees / 60.0f;
    // El jitter de pickTronHue puede sacar el hue de [0,360); se normaliza a [0,6)
    // antes de clasificarlo, para que el color siga siendo valido.
    hp = std::fmod(hp, 6.0f);
    if (hp < 0.0f) hp += 6.0f;

    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = value - c;
    float rf = 0.0f, gf = 0.0f, bf = 0.0f;
    if (hp < 1.0f)      { rf = c; gf = x; bf = 0.0f; }
    else if (hp < 2.0f) { rf = x; gf = c; bf = 0.0f; }
    else if (hp < 3.0f) { rf = 0.0f; gf = c; bf = x; }
    else if (hp < 4.0f) { rf = 0.0f; gf = x; bf = c; }
    else if (hp < 5.0f) { rf = x; gf = 0.0f; bf = c; }
    else                { rf = c; gf = 0.0f; bf = x; }

    SDL_Color color;
    color.r = static_cast<Uint8>((rf + m) * 255.0f);
    color.g = static_cast<Uint8>((gf + m) * 255.0f);
    color.b = static_cast<Uint8>((bf + m) * 255.0f);
    color.a = 255;
    return color;
}

float pickTronHue(std::mt19937& rng) {
    // Misma distribucion que elegirHueTron() de la version de Roberto, pero con un
    // generador con semilla explicita en vez de rand() (reproducible con --seed).
    std::uniform_real_distribution<float> familyDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> jitter20(0, 19); // equivalente a rand() % 20
    std::uniform_int_distribution<int> jitter14(0, 13); // equivalente a rand() % 14
    std::uniform_int_distribution<int> jitter10(0, 9);  // equivalente a rand() % 10

    float r = familyDist(rng);
    if (r < 0.55f) {
        return 188.0f + static_cast<float>(jitter20(rng) - 10); // cian
    } else if (r < 0.80f) {
        return 22.0f + static_cast<float>(jitter14(rng) - 7);   // naranja
    } else if (r < 0.93f) {
        return 200.0f + static_cast<float>(jitter14(rng) - 7);  // azul electrico
    } else {
        return 45.0f + static_cast<float>(jitter10(rng) - 5);   // ambar
    }
}

float thicknessForN(int n) {
    // Formula de Roberto: mas lineas, trazo mas fino, acotado a [1.5, 6.0] px.
    float g = 6.0f - 0.01f * static_cast<float>(n);
    return std::max(1.5f, std::min(6.0f, g));
}

std::vector<LightBeam> initBeams(int n, int width, int height, float length,
                                  float speedMin, float speedMax, std::mt19937& rng) {
    std::vector<LightBeam> beams(static_cast<size_t>(n));
    float thickness = thicknessForN(n);

    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * static_cast<float>(M_PI));
    std::uniform_real_distribution<float> speedDist(speedMin, speedMax);

    // Margen para que ninguna linea nazca ya "fuera" de la ventana por su propia
    // longitud/grosor (si la ventana es mas chica que el margen, se centra en 0..dim).
    float marginX = length * 0.5f + thickness * 0.5f + 1.0f;
    float marginY = marginX;
    float loX = std::min(marginX, static_cast<float>(width) - marginX);
    float hiX = std::max(marginX, static_cast<float>(width) - marginX);
    float loY = std::min(marginY, static_cast<float>(height) - marginY);
    float hiY = std::max(marginY, static_cast<float>(height) - marginY);
    std::uniform_real_distribution<float> xDist(loX, hiX);
    std::uniform_real_distribution<float> yDist(loY, hiY);

    for (int i = 0; i < n; i++) {
        LightBeam& beam = beams[static_cast<size_t>(i)];
        beam.id = i;
        beam.length = length;
        beam.thickness = thickness;
        beam.cx = xDist(rng);
        beam.cy = yDist(rng);

        float angle = angleDist(rng);
        float speed = speedDist(rng);
        beam.vx = std::cos(angle) * speed;
        beam.vy = std::sin(angle) * speed;
        beam.angle = angle;

        beam.hue = pickTronHue(rng);
        beam.color = hsvToRgb(beam.hue, 0.85f, 1.0f);
        beam.cooldown = 0.0f;

        computeEndpoints(beam);
        computeAabb(beam);
    }
    return beams;
}

LightBeam makeDebugCornerBeam(int width, int height, float length, float speed) {
    LightBeam beam{};
    beam.id = 0;
    beam.length = length;
    beam.thickness = thicknessForN(1);
    beam.cx = static_cast<float>(width) * 0.5f;
    beam.cy = static_cast<float>(height) * 0.5f;
    // Rumbo directo a la esquina superior izquierda: el peor caso para el bug de
    // "temblor" que reporto el equipo (ambos ejes rebotando en el mismo frame).
    beam.vx = -speed;
    beam.vy = -speed;
    beam.angle = std::atan2(beam.vy, beam.vx);
    beam.hue = 188.0f; // cian, para que sea facil de ver en pantalla
    beam.color = hsvToRgb(beam.hue, 0.85f, 1.0f);
    beam.cooldown = 0.0f;
    computeEndpoints(beam);
    computeAabb(beam);
    return beam;
}

void keepInsideBounds(LightBeam& beam, int width, int height) {
    float hx = halfExtentX(beam);
    float hy = halfExtentY(beam);

    // Eje X: solo se invierte vx si la linea todavia va HACIA la pared que toco.
    // Esto es lo que corrige el bug reportado: la version anterior invertia el
    // angulo en cada frame en que un extremo seguia fuera, sin importar si la
    // linea ya iba de regreso, lo que la dejaba "temblando" pegada a la pared
    // (y atrapada en las esquinas, donde ambos ejes rebotaban a la vez).
    if (beam.cx - hx < 0.0f) {
        beam.cx = hx;
        if (beam.vx < 0.0f) beam.vx = -beam.vx;
    }
    if (beam.cx + hx > static_cast<float>(width)) {
        beam.cx = static_cast<float>(width) - hx;
        if (beam.vx > 0.0f) beam.vx = -beam.vx;
    }

    // Eje Y: misma logica, independiente del eje X (por eso una esquina rebota
    // correctamente en ambos ejes en un solo frame, sin quedar atrapada).
    if (beam.cy - hy < 0.0f) {
        beam.cy = hy;
        if (beam.vy < 0.0f) beam.vy = -beam.vy;
    }
    if (beam.cy + hy > static_cast<float>(height)) {
        beam.cy = static_cast<float>(height) - hy;
        if (beam.vy > 0.0f) beam.vy = -beam.vy;
    }

    beam.angle = std::atan2(beam.vy, beam.vx);
    computeEndpoints(beam);
    computeAabb(beam);
}

void updateBeam(LightBeam& beam, float dt, int width, int height) {
    beam.cx += beam.vx * dt;
    beam.cy += beam.vy * dt;
    keepInsideBounds(beam, width, height);
    beam.cooldown = std::max(0.0f, beam.cooldown - dt);
}

bool segmentsIntersect(float x1, float y1, float x2, float y2,
                        float x3, float y3, float x4, float y4) {
    int o1 = orientation(x1, y1, x2, y2, x3, y3);
    int o2 = orientation(x1, y1, x2, y2, x4, y4);
    int o3 = orientation(x3, y3, x4, y4, x1, y1);
    int o4 = orientation(x3, y3, x4, y4, x2, y2);

    if (o1 != o2 && o3 != o4) return true;

    // Casos especiales: puntos colineales que caen dentro del otro segmento.
    if (o1 == 0 && onSegment(x1, y1, x3, y3, x2, y2)) return true;
    if (o2 == 0 && onSegment(x1, y1, x4, y4, x2, y2)) return true;
    if (o3 == 0 && onSegment(x3, y3, x1, y1, x4, y4)) return true;
    if (o4 == 0 && onSegment(x3, y3, x2, y2, x4, y4)) return true;
    return false;
}

bool beamsCollide(const LightBeam& a, const LightBeam& b) {
    if (a.cooldown > 0.0f || b.cooldown > 0.0f) return false;
    // Filtro rapido por AABB: si no se solapan, ni vale la pena probar los segmentos.
    if (a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY) {
        return false;
    }
    return segmentsIntersect(a.x1, a.y1, a.x2, a.y2, b.x1, b.y1, b.x2, b.y2);
}

CollisionOutcome resolveCollision(LightBeam& beamA, LightBeam& beamB, int width, int height,
                                   float speedMin, float speedMax, float cooldownSeconds) {
    CollisionOutcome outcome{false, 0.0f, 0.0f, SDL_Color{0, 0, 0, 0}};

    float dx = beamB.cx - beamA.cx;
    float dy = beamB.cy - beamA.cy;
    float dist = std::sqrt(dx * dx + dy * dy);
    float nx, ny;
    if (dist < 0.01f) {
        // Centros casi coincidentes (caso degenerado): se usa la perpendicular del
        // segmento A como normal, para no dividir por un numero casi cero.
        float segDx = beamA.x2 - beamA.x1;
        float segDy = beamA.y2 - beamA.y1;
        float segLen = std::sqrt(segDx * segDx + segDy * segDy);
        if (segLen < 0.0001f) segLen = 1.0f;
        nx = -segDy / segLen;
        ny = segDx / segLen;
        dist = 0.01f;
    } else {
        nx = dx / dist;
        ny = dy / dist;
    }

    // Velocidad relativa proyectada sobre la normal. Si es <= 0, las lineas ya se
    // estan separando (por ejemplo, ya resolvimos este mismo cruce el frame
    // anterior y los segmentos todavia se solapan): no se aplica ningun impulso,
    // lo que evita que el choque "parpadee" (swap, swap-back) en varios frames.
    float vRel = (beamA.vx - beamB.vx) * nx + (beamA.vy - beamB.vy) * ny;
    if (vRel <= 0.0f) {
        return outcome;
    }

    // Choque elastico entre masas iguales: se intercambia la componente de
    // velocidad sobre la normal del choque; la componente tangencial no cambia.
    beamA.vx -= vRel * nx;
    beamA.vy -= vRel * ny;
    beamB.vx += vRel * nx;
    beamB.vy += vRel * ny;

    clampSpeed(beamA, speedMin, speedMax);
    clampSpeed(beamB, speedMin, speedMax);

    // Si quedaron muy encimadas, separarlas un poco a lo largo de la normal.
    const float minSeparation = std::max(beamA.length, beamB.length) * 0.7f;
    if (dist < minSeparation) {
        float push = (minSeparation - dist) * 0.5f + 0.5f;
        beamA.cx -= nx * push;
        beamA.cy -= ny * push;
        beamB.cx += nx * push;
        beamB.cy += ny * push;
    }

    // Recalcula angulo/extremos/AABB de ambas Y las reacomoda si el empuje de
    // separacion las metio contra un borde (corrige ese bug de la version anterior).
    keepInsideBounds(beamA, width, height);
    keepInsideBounds(beamB, width, height);

    // El comportamiento que pidio el equipo: intercambian color al chocar.
    std::swap(beamA.hue, beamB.hue);
    std::swap(beamA.color, beamB.color);

    beamA.cooldown = cooldownSeconds;
    beamB.cooldown = cooldownSeconds;

    outcome.happened = true;
    outcome.sparkX = (beamA.cx + beamB.cx) * 0.5f;
    outcome.sparkY = (beamA.cy + beamB.cy) * 0.5f;
    outcome.sparkColor = beamA.color; // ya intercambiado: coherente con el nuevo color de A
    return outcome;
}

uint64_t computeChecksum(const std::vector<LightBeam>& beams) {
    const uint64_t FNV_OFFSET = 1469598103934665603ULL;
    const uint64_t FNV_PRIME  = 1099511628211ULL;
    uint64_t hash = FNV_OFFSET;

    auto mix = [&hash, FNV_PRIME](uint32_t value) {
        for (int b = 0; b < 4; b++) {
            hash ^= (value >> (b * 8)) & 0xFFu;
            hash *= FNV_PRIME;
        }
    };

    // Se ordena por id (no por indice en el vector) para que el checksum no dependa
    // de como cada version reordene internamente el arreglo de lineas.
    std::vector<const LightBeam*> ordered;
    ordered.reserve(beams.size());
    for (const auto& beam : beams) ordered.push_back(&beam);
    std::sort(ordered.begin(), ordered.end(),
              [](const LightBeam* a, const LightBeam* b) { return a->id < b->id; });

    for (const LightBeam* beam : ordered) {
        // Cuantizar a 1e-3 px: asi pequenas diferencias de redondeo por distinto
        // orden de operaciones en punto flotante (paralelo vs. secuencial) no
        // producen un checksum distinto para un resultado que es, en la practica,
        // el mismo.
        int32_t qx = static_cast<int32_t>(std::lround(beam->cx * 1000.0f));
        int32_t qy = static_cast<int32_t>(std::lround(beam->cy * 1000.0f));
        mix(static_cast<uint32_t>(qx));
        mix(static_cast<uint32_t>(qy));
        mix(static_cast<uint32_t>(beam->id));
        mix(static_cast<uint32_t>(beam->color.r));
        mix(static_cast<uint32_t>(beam->color.g));
        mix(static_cast<uint32_t>(beam->color.b));
    }
    return hash;
}
