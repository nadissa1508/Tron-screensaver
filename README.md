# Neon Lines — Tron Screensaver

Screensaver de haces de luz neon inspirado en **Tron**, hecho en C++ con **SDL2** (render) y **OpenMP** (paralelizacion), para el Proyecto 1 del curso  CC3069-Computación Paralela y Distribuida.

N líneas rectas rígidas se mueven por una ventana oscura, rebotan en los bordes y, al chocar entre sí, **cambian de dirección (choque físico real) e intercambian sus colores**, en una paleta cian / naranja / azul eléctrico / ámbar. Se incluye una versión **secuencial (v1)** y varias **versiones paralelas con OpenMP (v2, v3, v3-naive, v4)** para medir speedup y eficiencia.

## Autores

- Angie Nadissa Vela López, 23764
- Javier Alexander Linares Chang, 231135
- Roberto Emiliano Camposeco Torres, 23968

## Requisitos

- **Compilador**: MinGW-w64 (g++) con soporte de OpenMP (modelo de hilos posix o posix-seh). Probado con g++ 16.1.0.
- **make** o **mingw32-make**.
- Windows (el proyecto usa las DLL de SDL2/SDL2_ttf para Windows, vendorizadas en `libs/`; no requiere instalar nada aparte).
- No se necesita instalar SDL2 ni SDL2_ttf por separado: ya están incluidas en `libs/SDL2/` y `libs/SDL2_ttf/` (headers, `.a`/`.dll.a` para enlazar y `.dll` para ejecutar), igual que se documentó en la Entrega 2.

## Compilación

Con `make` (recomendado):

```bash
make            # compila secuencial.exe y paralelo.exe
make secuencial # compila solo secuencial.exe
make paralelo   # compila solo paralelo.exe
make clean      # borra los .exe y los .o
```

Equivalente manual con g++ (por si no se tiene `make`), para `secuencial.exe`:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -fopenmp \
    -I libs/SDL2/include -I libs/SDL2_ttf/include \
    src/secuencial.cpp src/config.cpp src/neon_lines.cpp src/render.cpp src/bench_io.cpp \
    -o secuencial.exe \
    -L libs/SDL2/lib -L libs/SDL2_ttf/lib -lmingw32 -lSDL2main -lSDL2_ttf -lSDL2
```

Para `paralelo.exe`, el mismo comando cambiando `src/secuencial.cpp` por `src/paralelo.cpp` y el nombre de salida a `paralelo.exe`.

Después de compilar, copia `libs/SDL2/bin/SDL2.dll` y `libs/SDL2_ttf/bin/SDL2_ttf.dll` junto al `.exe` (el Makefile ya lo hace automáticamente). **No se entregan los `.exe` en el repositorio** (están en `.gitignore`, como pide el enunciado); hay que compilarlos.

## Ejecución

```bash
./secuencial.exe 500                       # v1, N=500 líneas
./paralelo.exe 500 --mode v3 --threads 8    # v3, N=500, 8 hilos
./secuencial.exe                            # si se omite N, el programa lo solicita por consola
```

Controles en la ventana: **ESC** o cerrar la ventana para salir.

### Parámetros de línea de comandos

| Flag | Aplica a | Default | Descripción |
|---|---|---|---|
| `N` (posicional) | ambos | se solicita por consola si falta | Cantidad de líneas, entero 1..100000 |
| `-w`, `--width` | ambos | 800 | Ancho de ventana en px (mínimo 640) |
| `-h`, `--height` | ambos | 600 | Alto de ventana en px (mínimo 480) |
| `-l`, `--length` | ambos | 40 | Longitud fija de cada línea en px (5..200) |
| `--speed-min` / `--speed-max` | ambos | 90 / 220 | Rapidez mínima/máxima al nacer, px/s |
| `--cooldown` | ambos | 0.15 | Segundos sin poder rechocar tras un choque |
| `-s`, `--seed` | ambos | hora actual | Semilla del generador pseudoaleatorio (reproducibilidad) |
| `-t`, `--threads` | solo `paralelo.exe` | todos los disponibles | Hilos OpenMP a usar |
| `-m`, `--mode` | solo `paralelo.exe` | `v3` | `v2` \| `v3` \| `v3-naive` \| `v4` |
| `--schedule` | solo `paralelo.exe` | `dynamic` | `static` \| `dynamic` \| `guided` |
| `--frames F` | ambos | 0 (interactivo) | Corre exactamente F frames con dt fijo y termina (modo benchmark) |
| `--headless` | ambos | apagado | No crea ventana; solo mide las fases de cómputo |
| `--csv archivo` | ambos | — | Agrega una fila de resultados a ese CSV |
| `--trail` / `--no-trail` | ambos | activado | Estela neón que se desvanece |
| `--grid` / `--no-grid` | ambos | activado | Rejilla de fondo |
| `--vsync` / `--no-vsync` | ambos | desactivado | Sincronizar con el monitor |
| `--font ruta.ttf` | ambos | `assets/fonts/Orbitron-Regular.ttf` | Fuente del HUD en pantalla |
| `--debug-corner` | ambos | apagado | Fuerza N=1 apuntando a una esquina (prueba de rebote) |
| `--help` | ambos | — | Muestra la ayuda y termina |

### Ejemplos

```bash
# Ventana interactiva, 300 líneas, semilla fija para reproducir la misma corrida
./secuencial.exe 300 --seed 42

# Comparar v2 vs v3 con 4 hilos, mismos parametros
./paralelo.exe 1000 --mode v2 --threads 4
./paralelo.exe 1000 --mode v3 --threads 4

# Benchmark headless (sin ventana), 600 frames, guardando resultados en CSV
./secuencial.exe 1000 --headless --frames 600 --seed 42 --csv resultados/bitacora.csv
./paralelo.exe   1000 --headless --frames 600 --seed 42 --threads 8 --mode v3 --csv resultados/bitacora.csv

# Verificar visualmente que el rebote en una esquina no "tiembla"
./secuencial.exe --debug-corner
```

## Estructura del proyecto

```
Tron-screensaver/
├── Makefile
├── README.md
├── PLAN_DE_CORRECCION.md        # plan de correccion que guio esta version del proyecto
├── Propuesta.txt                # Propuesta 1 (ya entregada)
├── Proyecto 1 - Entrega 2.pdf
├── docs/
│   ├── instrucciones_proyecto.md
│   ├── propuesta_entrega2.md
│   ├── 01_Seleccion_Herramienta_Grafica.md
│   ├── 02_Diagrama_de_flujo.md      # Anexo 1
│   ├── 03_Catalogo_de_funciones.md  # Anexo 2
│   └── 04_Bitacora_de_pruebas.md    # Anexo 3
├── libs/
│   ├── SDL2/       # vendorizado (headers, libs de enlace, DLL)
│   └── SDL2_ttf/   # vendorizado (headers, libs de enlace, DLL)
├── assets/fonts/   # Orbitron (OFL) para el HUD
├── scripts/
│   ├── benchmark.ps1   # corre la matriz de mediciones (N x hilos x version), 10 reps
│   └── analizar.py     # calcula speedup y eficiencia a partir del CSV
├── resultados/     # CSV de mediciones (se genera al correr benchmark.ps1)
└── src/
    ├── config.h/.cpp        # parametros por linea de comandos + validacion defensiva
    ├── neon_lines.h/.cpp    # logica pura: movimiento, rebote, choques, paleta (sin SDL render)
    ├── render.h/.cpp        # dibujo con SDL2 (batch, rejilla, chispas) y HUD con SDL2_ttf
    ├── bench_io.h/.cpp      # creacion de ventana/renderer y escritura del CSV (compartido)
    ├── secuencial.cpp       # main de la version v1
    └── paralelo.cpp         # main de las versiones v2/v3/v3-naive/v4
```

`secuencial.cpp` y `paralelo.cpp` llaman exactamente a las mismas funciones de `neon_lines.cpp`; lo único que cambia entre ambos es **cómo se recorre** el arreglo de líneas en cada fase (con o sin `#pragma omp`). Esto es lo que permite comparar el resultado (checksum) entre versiones para verificar que la paralelización es correcta.

## Versiones

| Versión | Fase 1 (movimiento) | Fase 2 (choques: detección) | Resolución | Vértices de render |
|---|---|---|---|---|
| **v1** secuencial | `for` | doble `for` | secuencial | secuencial |
| **v2** paralelo simple | `omp parallel for` | secuencial | secuencial | secuencial |
| **v3** paralelo mejorado | `omp parallel for` | `omp for` + buffer local por hilo + `critical` de fusión | `omp single`, orden determinista | `omp parallel for` |
| **v3-naive** | igual a v3 | igual a v3 pero con un `critical` por cada par hallado (más contención) | igual a v3 | igual a v3 |
| **v4** | igual a v3 | partición espacial (grilla uniforme): solo se prueban pares dentro de la misma celda y sus 8 vecinas | igual a v3 | igual a v3 |

El render (llamadas reales a SDL) **siempre es secuencial**: SDL no es thread-safe. Por eso el speedup del frame completo es menor que el de las fases de cómputo — es el comportamiento esperado según la Ley de Amdahl, no un error.

## Medición de rendimiento (speedup, eficiencia)

1. Compilar: `make all`.
2. Correr la matriz completa de mediciones (10 repeticiones por configuración, como pide el enunciado):
   ```powershell
   powershell -File scripts/benchmark.ps1
   ```
   (acepta parámetros para acotar la corrida: `-Ns`, `-Threads`, `-Modes`, `-Repeats`, ver comentarios en el script).
3. Calcular speedup y eficiencia:
   ```bash
   python scripts/analizar.py resultados/bitacora.csv --out resultados/resumen.csv
   ```

Cada fila del CSV incluye un **checksum** (hash de posiciones y colores al final de la corrida): con la misma `--seed` y `--frames`, v1, v2, v3, v3-naive y v4 deben dar exactamente el mismo checksum. Si no coincide, hay una condición de carrera en la versión paralela correspondiente.

## Notas de diseño

- **Líneas rectas**: volvimos a la propuesta original del diseño.
- **Paleta**: ~55% cian, ~25% naranja, ~13% azul eléctrico, ~7% ámbar, con variación pseudoaleatoria de tono por línea.
- **Choques**: elásticos entre masas iguales (se intercambia la componente de velocidad normal al choque) Y intercambio de color. Solo se aplican si las líneas todavía se están acercando (evita que el color parpadee en el mismo cruce).
- **Rebote en bordes/esquinas**: solo se invierte la velocidad si la línea todavía va hacia la pared que tocó (corrige el bug de la versión anterior, donde una línea quedaba temblando o atrapada en una esquina).
- **HUD** (FPS, N, hilos, modo, choques) con SDL2_ttf y la fuente Orbitron; si la fuente no carga, el programa avisa por consola y sigue funcionando sin HUD en pantalla (los FPS se siguen viendo en el título de la ventana y en la consola).


