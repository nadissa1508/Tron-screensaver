# Anexo 2 — Catálogo de funciones

Para cada función: **entradas** (nombre, tipo, uso), **salidas** (nombre, tipo, uso) y **descripción** de su propósito. Se agrupan por archivo. Los comentarios in-line de cada función (en el código fuente) amplían el detalle de implementación.

## `src/config.h` / `config.cpp`

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `parseArgs` | `argc` (int, cantidad de argumentos), `argv` (char*[], argumentos crudos), `config` (Config&, se llena), `isParallelBinary` (bool, habilita flags exclusivas de `paralelo.exe`) | `ParseResult` (enum: OK / EXIT_OK / EXIT_ERROR) | Lee y valida todos los parámetros de línea de comandos de forma defensiva (rangos, formato numérico, flags desconocidas o sin valor); si N no vino y no es benchmark, lo solicita por consola. |
| `printUsage` | `programName` (const char*), `isParallelBinary` (bool) | — (imprime a stdout) | Imprime el mensaje de uso/ayuda (usado por `--help` y tras un error de argumentos). |
| `modeToString` | `mode` (ParallelMode) | `const char*` | Texto corto del modo (`v1-secuencial`, `v2`, `v3`, `v3-naive`, `v4`) para HUD, consola y CSV. |
| `scheduleToString` | `schedule` (Schedule) | `const char*` | Texto corto de la política de reparto (`static`, `dynamic`, `guided`). |
| `parseLong` / `parseFloat` (internas) | `text` (const char*), `out` (referencia numérica) | `bool` (formato válido) | Envoltorio defensivo sobre `strtol`/`strtof`: rechaza texto vacío, basura al final o desbordamiento. |
| `requestNFromConsole` (interna) | `n` (int&, se llena) | `bool` (false si hubo EOF) | Pide N por `std::cin`, reintenta ante entradas inválidas, sale limpio si la entrada estándar llega a EOF. |

## `src/neon_lines.h` / `neon_lines.cpp`

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `hsvToRgb` | `hueDegrees`, `saturation`, `value` (float) | `SDL_Color` | Convierte un color HSV a RGB de 8 bits; base de toda la paleta neón. |
| `pickTronHue` | `rng` (std::mt19937&) | `float` (grados) | Elige un tono siguiendo la paleta Tron (55% cian / 25% naranja / 13% azul eléctrico / 7% ámbar), con el generador con semilla (nunca `rand()`). |
| `thicknessForN` | `n` (int) | `float` (px) | Grosor de dibujo según N (más líneas, trazo más fino), acotado a [1.5, 6.0]. |
| `initBeams` | `n`, `width`, `height` (int), `length`, `speedMin`, `speedMax` (float), `rng` (std::mt19937&) | `std::vector<LightBeam>` | Crea N líneas con posición, ángulo, velocidad y color pseudoaleatorios; toda la aleatoriedad del programa ocurre aquí. |
| `makeDebugCornerBeam` | `width`, `height` (int), `length`, `speed` (float) | `LightBeam` | Crea una única línea apuntando a una esquina, para `--debug-corner`. |
| `updateBeam` | `beam` (LightBeam&), `dt` (float), `width`, `height` (int) | — (modifica `beam`) | Fase 1: integra la posición y llama a `keepInsideBounds`. Independiente por línea (segura para `omp parallel for`). |
| `keepInsideBounds` | `beam` (LightBeam&), `width`, `height` (int) | — (modifica `beam`) | Reacomoda una línea dentro de la ventana, invirtiendo la velocidad del eje correspondiente **solo si aún va hacia la pared**; recalcula ángulo, extremos y AABB. Se usa tras el movimiento y tras resolver un choque. |
| `segmentsIntersect` | 8 floats: extremos de dos segmentos (`x1,y1,x2,y2,x3,y3,x4,y4`) | `bool` | Test clásico de intersección de segmentos por orientación (producto cruz). |
| `beamsCollide` | `a`, `b` (const LightBeam&) | `bool` | Fase 2 (solo lectura): true si ninguna está en cooldown, sus AABB se solapan y sus segmentos se intersectan de verdad. |
| `resolveCollision` | `beamA`, `beamB` (LightBeam&), `width`, `height` (int), `speedMin`, `speedMax`, `cooldownSeconds` (float) | `CollisionOutcome` (struct: `happened`, `sparkX/Y`, `sparkColor`) | Aplica el choque elástico (intercambia la componente normal de velocidad), intercambia colores, separa y reacomoda las líneas a los bordes. Escribe en ambas líneas: debe llamarse en orden determinista y desde un solo hilo. |
| `computeChecksum` | `beams` (const std::vector<LightBeam>&) | `uint64_t` | Hash FNV-1a de posiciones (cuantizadas), id y color, ordenado por id. Sirve para comprobar que todas las versiones dan el mismo resultado. |
| `computeEndpoints`, `computeAabb`, `halfExtentX/Y`, `clampSpeed`, `orientation`, `onSegment` (internas) | — | — | Helpers matemáticos usados por las funciones de arriba (no se exponen fuera de `neon_lines.cpp`). |

## `src/render.h` / `render.cpp`

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `updateSparks` | `sparks` (std::vector<Spark>&), `dt` (float) | — | Resta vida a cada chispa y elimina las que ya expiraron. |
| `drawSparks` | `renderer` (SDL_Renderer*), `sparks` (const std::vector<Spark>&) | — | Dibuja el anillo de cada chispa activa (mezcla aditiva). |
| `drawGrid` | `renderer`, `width`, `height` (int), `globalTime` (float) | — | Dibuja la rejilla de fondo con un pulso sutil en el tiempo. |
| `BeamGeometry::resize` | `n` (size_t) | — (miembro de `BeamGeometry`) | Reserva los buffers de vértices/índices para n líneas y precalcula los índices de cada quad (no cambian entre frames). |
| `buildBeamQuad` | `geometry` (BeamGeometry&), `index` (size_t), `beam` (const LightBeam&) | — (escribe en `geometry`) | Escribe los 4 vértices (glow y núcleo) de una línea en su posición fija dentro del buffer. Pura escritura en memoria: segura para `omp parallel for` si cada hilo usa un `index` distinto. |
| `submitBeamGeometry` | `renderer` (SDL_Renderer*), `geometry` (const BeamGeometry&) | — | Emite las 2 llamadas a `SDL_RenderGeometry` (glow + núcleo) para TODAS las líneas de una vez. Solo hilo principal. |
| `drawBeams` | `renderer`, `geometry` (BeamGeometry&), `beams` (const std::vector<LightBeam>&) | — | Atajo secuencial: `buildBeamQuad` de todas las líneas + `submitBeamGeometry`. Usado por `secuencial.cpp`. |
| `Hud::Hud` (constructor) | `renderer` (SDL_Renderer*), `fontPath` (const std::string&), `windowHeight` (int) | — | Inicializa SDL2_ttf y abre la fuente; si falla, el HUD queda deshabilitado (con aviso por consola) sin detener el programa. |
| `Hud::isEnabled` | — | `bool` | true si la fuente se abrió correctamente. |
| `Hud::setText` | `text` (const std::string&) | — | Regenera la textura del HUD solo si el texto cambió desde la última llamada. |
| `Hud::draw` | `marginX`, `marginY` (int) | — | Dibuja la textura cacheada del HUD en pantalla. |

## `src/bench_io.h` / `bench_io.cpp`

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `createWindowAndRenderer` | `config` (const Config&), `windowTitle` (const char*), `window` (SDL_Window*&), `renderer` (SDL_Renderer*&) | `bool` (éxito) | Crea ventana y renderer con el tamaño/vsync de `config`, con fallback a renderer por software; limpia lo creado si falla. Compartida por `secuencial.cpp` y `paralelo.cpp` para que ambos binarios se comporten igual. |
| `appendCsvRow` | `path` (const std::string&), `versionLabel`, `scheduleLabel` (const char*), `n`, `threads`, `frames` (int), `seed` (unsigned int), `tUpdateMs`, `tCollisionMs`, `tRenderMs`, `tTotalMs`, `fpsAvg` (double), `checksum` (uint64_t) | — (escribe en `path`) | Agrega una fila de resultados al CSV de la bitácora de pruebas (con encabezado si el archivo es nuevo). |

## `src/secuencial.cpp` y `src/paralelo.cpp` (funciones internas relevantes)

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `main` (ambos archivos) | `argc` (int), `argv` (char*[]) | `int` (código de salida) | Punto de entrada: parsea argumentos, inicializa líneas y SDL (si aplica), corre el loop principal (fases 1–3 por frame), imprime resultados de benchmark si corresponde y libera todos los recursos. |
| `detectCollisionsSequential` (solo `paralelo.cpp`, también existe inline en `secuencial.cpp`) | `beams` (const std::vector<LightBeam>&), `pairs` (std::vector<std::pair<int,int>>&) | — | Fase 2 con un solo hilo: revisa las N·(N−1)/2 parejas y agrega las que colisionan. Usada por v1 y v2. |
| `resolvePairs` (interna) | `beams` (std::vector<LightBeam>&), `pairs` (std::vector<std::pair<int,int>>&), `config` (const Config&), `sparks` (std::vector<Spark>*, puede ser `nullptr` en modo headless), `collisionCount` (long&) | — | Ordena los pares por (i,j) y resuelve cada choque en ese orden. Es la única función que modifica las líneas al chocar; se llama desde un solo hilo siempre (directo en v1/v2, dentro de `omp single` en v3/v3-naive/v4). |
| `applyScheduleSetting` (interna, `paralelo.cpp`) | `schedule` (Schedule) | — | Traduce `--schedule` a `omp_set_schedule`, para que los `#pragma omp for schedule(runtime)` del archivo usen la política elegida. |
| `buildSpatialGrid` (interna, `paralelo.cpp`) | `beams` (const std::vector<LightBeam>&), `width`, `height` (int), `cellSize` (float), `grid` (SpatialGrid&) | — | Construye la grilla espacial (formato CSR) que usa v4 para acotar cuántos pares se prueban en la detección de choques. |
| `cellIndexFor` (interna, `paralelo.cpp`) | `cx`, `cy`, `cellSize` (float), `cols`, `rows` (int) | `int` (índice de celda) | Calcula a qué celda de la grilla pertenece un punto, acotado a los límites de la grilla. |
