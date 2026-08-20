# Selección de Herramienta Gráfica — Screensaver "Neon Lines"

## 1. Descripción breve de la herramienta seleccionada

Se seleccionó **SDL2 (Simple DirectMedia Layer 2)** como librería gráfica para el desarrollo del screensaver.

SDL2 es una librería multiplataforma escrita en C que provee una capa de abstracción sobre el sistema operativo para la creación de ventanas, manejo de eventos de entrada (teclado, mouse, cierre de ventana), temporización de alta resolución y renderizado 2D acelerado por hardware a través de su API `SDL_Renderer`. A diferencia de un framework de bajo nivel como OpenGL, SDL2 no requiere gestionar manualmente un pipeline gráfico (shaders, buffers de vértices, matrices de proyección); expone primitivas de dibujo directas como `SDL_RenderDrawLine`, `SDL_RenderDrawPoint`, `SDL_RenderFillRect`, junto con soporte de *blending* (mezcla de colores/transparencias) mediante `SDL_BlendMode`.

## 2. Justificación de su uso para el screensaver

1. **Alineación con el problema a resolver**: la propuesta *Neon Lines* consiste en primitivas 2D simples (líneas rectas) sobre un fondo oscuro. SDL2 provee estas primitivas de forma directa, sin necesidad de construir un pipeline 3D.
2. **Recomendación explícita del enunciado**: el documento del Proyecto 1 sugiere SDL o OpenGL y utiliza SDL2 en su propio ejemplo de referencia (screensaver de N círculos rebotando).
3. **Coherencia con el diseño de paralelización propuesto**: la propuesta identifica correctamente que **SDL no es thread-safe**, lo cual encaja con el patrón de paralelización planificado — Fase 1 (actualización de posiciones) y Fase 2 (detección de colisiones) son paralelizables con OpenMP, mientras que la Fase 3 (renderizado) permanece secuencial en el hilo principal. Esto es exactamente el patrón recomendado para cualquier librería gráfica de contexto único (SDL u OpenGL comparten esta restricción), por lo que no se gana simplicidad usando OpenGL en su lugar.
4. **Menor complejidad de configuración**: OpenGL requiere gestión explícita de contexto gráfico, extensiones, shaders GLSL y buffers de vértices — sobrecarga innecesaria para un proyecto cuyo objetivo de evaluación central es la paralelización con OpenMP (60% del rubro de programa), no el renderizado.
5. **Extensibilidad para el efecto neon**: el efecto de *glow* opcional descrito en la propuesta puede lograrse en SDL2 dibujando cada línea varias veces con grosor decreciente y modo de mezcla aditivo (`SDL_BLENDMODE_ADD`), sin necesidad de shaders personalizados.
6. **Medición de rendimiento simple**: SDL2 expone temporizadores de alta resolución (`SDL_GetTicks()`, `SDL_GetPerformanceCounter()`) que facilitan el cálculo de FPS y tiempos de ejecución requeridos para las métricas de speedup y eficiencia del proyecto.

## 3. Requisitos básicos para su instalación y uso con C/C++

**Entorno de desarrollo del equipo**: Windows con MinGW-w64 (g++), sin MSYS2/pacman disponible.

Pasos seguidos para este proyecto:

1. **Compilador**: MinGW-w64 (GCC/G++) con soporte de OpenMP verificado (`g++ --version` → 16.1.0, modelo de hilos `posix-seh`, compatible con OpenMP 4.5+).
2. **Librería SDL2**: se descargó el paquete de desarrollo precompilado para MinGW desde la página oficial de releases de SDL (`SDL2-devel-<version>-mingw.tar.gz`, https://www.libsdl.org/ / https://github.com/libsdl-org/SDL/releases), específicamente la carpeta `x86_64-w64-mingw32/`, que contiene:
   - `include/SDL2/*.h` — cabeceras
   - `lib/libSDL2.a`, `lib/libSDL2main.a`, `lib/libSDL2.dll.a` — librerías de enlace
   - `bin/SDL2.dll` — librería dinámica en tiempo de ejecución
3. **Ubicación en el proyecto**: los archivos se colocaron en `libs/SDL2/` dentro del repositorio del proyecto para mantener el build autocontenido y reproducible en cualquier máquina con el mismo compilador.
4. **Compilación**: se enlaza el ejecutable contra SDL2 con las banderas:
   ```
   g++ src/main.cpp -o screensaver.exe -I libs/SDL2/include -L libs/SDL2/lib -lmingw32 -lSDL2main -lSDL2 -fopenmp
   ```
5. **Ejecución**: `SDL2.dll` debe copiarse junto al ejecutable (o estar en el PATH) para que el programa cargue en tiempo de ejecución.
6. **Alternativas de instalación** (no usadas, mencionadas por completitud):
   - Linux/WSL2: `sudo apt install libsdl2-dev` + `g++ -fopenmp`
   - Windows con MSYS2: `pacman -S mingw-w64-x86_64-SDL2`
   - Windows con Visual Studio: `vcpkg install sdl2` (OpenMP limitado a la versión 2.0 en MSVC, por lo que se descartó esta ruta)

Ambos requisitos (compilador con OpenMP y SDL2 enlazada) fueron verificados mediante pruebas de humo (*smoke tests*) antes de iniciar la implementación: un programa mínimo con `#pragma omp parallel` confirmó 8 hilos disponibles, y un programa mínimo con `SDL_Init(SDL_INIT_VIDEO)` confirmó el enlace correcto de la librería.

## 4. Ejemplo de referencia de una aplicación gráfica desarrollada con SDL2

El propio enunciado del Proyecto 1 (Universidad del Valle de Guatemala, Computación Paralela y Distribuida, Semestre 2 2026) incluye como ejemplo de referencia un screensaver desarrollado en C++ con SDL2 y OpenMP que genera **N círculos que se mueven y rebotan** en los bordes de la ventana, mostrando el contador de FPS tanto en consola como en el título de la ventana (Imagen 1 del enunciado, ejecutado sobre Ubuntu/Linux).

Este ejemplo es directamente análogo al screensaver propuesto por el equipo (*Neon Lines*): comparte la misma arquitectura de N elementos independientes con movimiento, rebote en bordes y necesidad de renderizado secuencial sobre lógica potencialmente paralela, cambiando únicamente la primitiva geométrica (círculos → líneas) y añadiendo la mecánica de intercambio de color en colisiones.

Adicionalmente, la documentación oficial de SDL2 (https://wiki.libsdl.org/) provee ejemplos de referencia equivalentes, como `SDL_RenderDrawLine` y el patrón estándar de *game loop* (inicialización → poll de eventos → actualización de estado → limpieza de pantalla → renderizado → presentación de buffer) utilizado como base para la estructura de este proyecto.
