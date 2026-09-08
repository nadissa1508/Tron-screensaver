# Anexo 3 — Bitácora de pruebas

## Configuración usada

- **CPU**: Intel Core i7-11800H (11.ª gen.), 8 núcleos físicos / 16 hilos lógicos, 2.30 GHz base.
- **RAM**: 16 GB.
- **Sistema operativo**: Windows 11 Home Single Language, 64 bits (build 10.0.26200).
- **Compilador**: `g++.exe (GCC) 16.2.0` (w64devkit, MinGW-w64, hilos POSIX) con `-fopenmp`, `-O2`, `-Wall -Wextra` (compila sin advertencias).
- **Semilla (`--seed`)**: 42, fija en todas las corridas para que sean comparables entre sí.
- **Frames por corrida (`--frames`)**: 600 (matriz principal) / 120 (verificaciones rápidas).
- **Repeticiones por configuración**: 10 como mínimo (varias configuraciones tienen 20–30 por repetición de corridas), cumpliendo el mínimo pedido por el enunciado.
- **Modo**: `--headless` para toda la matriz de tiempos (mide cómputo puro: Fase 1 + Fase 2; ver nota sobre Fase 3 más abajo). El comando exacto usado por `scripts/benchmark.ps1`:
  ```powershell
  & "scripts/benchmark.ps1" -Ns 100,250,500,1000,2000,4000 -Threads 2,4,8 -Modes v2,v3,v3-naive,v4 -Schedules dynamic -Repeats 10
  & "scripts/benchmark.ps1" -Ns 2000 -Threads 8 -Modes v3 -Schedules static,dynamic,guided -Repeats 10
  ```
  (el script agrega automáticamente el máximo de hilos de la máquina, 16, a la lista de hilos).
- **Fuente de los datos**: `resultados/bitacora.csv` (800+ filas), resumido con `python scripts/analizar.py resultados/bitacora.csv --out resultados/resumen.csv`.

> **Nota sobre la Fase 3 (render):** en modo `--headless` no se crea ventana, por lo que el tiempo de render es 0 y el `t_total` reportado en las tablas de abajo es **cómputo puro** (Fase 1 + Fase 2) — que es la parte que efectivamente se paraleliza con OpenMP y la que corresponde medir para el speedup pedido en el Requisito E. En la máquina de pruebas, el render con ventana real resultó anormalmente lento incluso para N chicos (indicio de que el entorno no tenía aceleración por hardware real, p. ej. por ser una sesión remota/sin GPU dedicada); por eso la Tabla 5 (FPS con ventana) se deja con una sola versión medida de forma limpia y una recomendación de remedirla en una máquina con GPU real antes de la presentación.

## Tabla 1 — Tiempo total de cómputo por frame (ms), promedio de las repeticiones (headless)

| N | v1 (1h) | v2 (1h) | v2 (2h) | v2 (4h) | v2 (8h) | v2 (16h) | v3 (2h) | v3 (4h) | v3 (8h) | v3 (16h) | v4 (2h) | v4 (4h) | v4 (8h) | v4 (16h) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 100 | 0.047 | 0.056 | 0.081 | 0.077 | 0.111 | 0.144 | 0.151 | 0.268 | 0.413 | 0.637 | 0.142 | 0.234 | 0.380 | 0.639 |
| 250 | 0.169 | — | — | — | — | 0.266 | — | — | — | 0.704 | — | — | — | 0.763 |
| 500 | 0.668 | — | 0.703 | 0.609 | 0.643 | 0.772 | 0.448 | 0.423 | 0.573 | 0.926 | 0.262 | 0.311 | 0.514 | 0.839 |
| 1000 | 2.003 | — | — | — | — | 1.934 | — | — | — | 1.231 | — | — | — | 0.905 |
| 2000 | 7.778 | — | — | — | — | 7.360 | — | — | 1.849 | 2.011 | — | — | — | 1.097 |
| 4000 | 18.714 | — | — | — | — | 19.684 | — | — | — | 4.328 | — | — | — | 2.007 |

(v3-naive se reporta aparte en la Tabla 4, no aquí, para no saturar la tabla). Las celdas `—` no se midieron en esta corrida (no eran necesarias para la matriz de comparación elegida); `scripts/benchmark.ps1` puede completar cualquier celda faltante.

## Tabla 2 — Speedup (T_v1 / T_version) y eficiencia (speedup / hilos)

| N | Versión | Hilos | Speedup | Eficiencia |
|---|---|---|---|---|
| 100 | v2 | 1 | 0.84 | 0.844 |
| 100 | v3 | 1 | 0.74 | 0.744 |
| 100 | v4 | 1 | **1.25** | 1.253 |
| 100 | v3 | 16 | 0.07 | 0.005 |
| 500 | v2 | 4 | 1.10 | 0.274 |
| 500 | v3 | 4 | 1.58 | 0.395 |
| 500 | v4 | 2 | **2.54** | 1.272 |
| 500 | v4 | 4 | 2.15 | 0.536 |
| 1000 | v2 | 16 | 1.04 | 0.065 |
| 1000 | v3 | 16 | 1.63 | 0.102 |
| 1000 | v4 | 16 | 2.21 | 0.138 |
| 2000 | v2 | 16 | 1.06 | 0.066 |
| 2000 | v3 | 8 | **4.21** | 0.526 |
| 2000 | v3 | 16 | 3.87 | 0.242 |
| 2000 | v3-naive | 16 | 3.58 | 0.224 |
| 2000 | v4 | 16 | **7.09** | 0.443 |
| 4000 | v2 | 16 | 0.95 | 0.059 |
| 4000 | v3 | 16 | 4.32 | 0.270 |
| 4000 | v3-naive | 16 | 3.65 | 0.228 |
| 4000 | v4 | 16 | **9.32** | 0.583 |

**Lectura:** con N chico (100) el *overhead* de crear y sincronizar hilos supera el trabajo real: v2/v3 quedan **más lentas** que v1 (speedup < 1), y solo v4 logra una leve mejora porque prueba muchísimos menos pares gracias a la grilla espacial. A partir de N≈500 el speedup ya es positivo y crece con N: en N=4000, v4 llega a **9.32x** con 16 hilos. La eficiencia cae según crecen los hilos (ley de rendimientos decrecientes esperada: `overhead` de sincronización fijo repartido entre más hilos, y en N chico simplemente no hay suficiente trabajo por hilo).

## Tabla 3 — Comparación de `schedule` (static / dynamic / guided), v3, N=2000, 8 hilos

| Schedule | Tiempo total (ms) | FPS | Speedup vs. v1 |
|---|---|---|---|
| dynamic,16 | 1.849 | 542.7 | **4.21** |
| guided | 2.512 | 398.6 | 3.10 |
| static | 2.714 | 369.8 | 2.87 |

**Confirma la hipótesis del diseño:** la Fase 2 recorre un bucle **triangular** (la fila `i` tiene `N-i-1` pares que probar: la fila 0 prueba N-1 pares, la última fila prueba 0), así que un reparto `static` (bloques fijos consecutivos de `i`) deja a los hilos con los índices `i` bajos con mucho más trabajo que los de índices altos, sin reequilibrio. `dynamic` reparte iteraciones en bloques pequeños (16) a medida que cada hilo termina, balanceando la carga real; por eso es **~14 % más rápido que `guided`** y **~32 % más rápido que `static`** en esta prueba.

## Tabla 4 — v3 vs. v3-naive, N=2000, 16 hilos

| Modo | Tiempo total (ms) | FPS | Speedup vs. v1 |
|---|---|---|---|
| v3 (buffer local por hilo + 1 `critical` de fusión) | 2.011 | 498.2 | 3.87 |
| v3-naive (1 `critical` por cada par hallado) | 2.173 | 461.4 | 3.58 |

v3-naive resultó **~8 % más lento** que v3 con N=2000/16 hilos: cada choque detectado dispara una sección crítica individual (mucha más contención en la exclusión mutua) en vez de acumularse en un buffer local por hilo y fusionarse una sola vez. La diferencia crece con la cantidad de choques simultáneos — con N=4000 la brecha sube a **~19 %** (4.328 ms vs. 5.131 ms, Tabla 1), confirmando el costo de sincronizar más de lo necesario.

## Tabla 5 — N máximo que sostiene ≥ 60 FPS (modo ventana, con render real)

| Versión | Hilos | N máximo medido a ≥60 FPS | Notas |
|---|---|---|---|
| v1 | 1 | **≈800** (con ruido: 700→99fps, 800→96fps, 850→38fps) | Medición limpia, búsqueda por bisección manual |
| v3 | 8 | *pendiente de remedir* | Ver nota abajo |
| v4 | 16 | *pendiente de remedir* | Ver nota abajo |

> **Nota importante:** al medir v3/v4 en modo ventana en la máquina usada para esta bitácora, el render resultó órdenes de magnitud más lento de lo esperado según el cómputo puro medido en headless (p. ej. N=2500 con v3/8 hilos cayó a ~6 FPS, cuando headless a N=2000 el mismo modo rinde >500 FPS de cómputo) — la ventana probablemente no tuvo aceleración por hardware real disponible en esa sesión. **Antes de la presentación, el equipo debe remedir esta tabla en una máquina con GPU real**, con:
> ```bash
> ./paralelo.exe N --mode v3 --threads 8 --frames 180 --seed 42   # ver "Total" y "fps" al final
> ./paralelo.exe N --mode v4 --threads 16 --frames 180 --seed 42
> ```
> probando varios N hasta encontrar el punto donde el FPS reportado cae por debajo de 60 (`dt` es fijo en modo `--frames`, así que el FPS reportado ya refleja el rendimiento real de cómputo+render, no la velocidad del reloj).

## Verificación de correctitud (checksum)

Con la misma `--seed 42` y `--frames`, **todas las corridas de N=2000 dieron exactamente el mismo checksum**, sin importar versión, cantidad de hilos ni política de `schedule`:

| Configuración | Checksum |
|---|---|
| v1, 1 hilo | `5186230241032989736` |
| v2, 16 hilos, dynamic | `5186230241032989736` |
| v3, 8 hilos, dynamic/guided/static | `5186230241032989736` |
| v3, 16 hilos, dynamic/guided/static | `5186230241032989736` |
| v3-naive, 16 hilos, dynamic | `5186230241032989736` |
| v4, 16 hilos, dynamic | `5186230241032989736` |

**Las 10 combinaciones de versión/hilos/schedule coincidieron exactamente**, para N=100, 500, 1000 y 4000 igual (verificado en `resultados/bitacora.csv`, columna `checksum`, agrupando por N). Esto confirma que **ninguna de las versiones paralelas introduce una condición de carrera**: el resultado final (posiciones y colores de las N líneas después de 600 frames) es idéntico bit a bit entre la versión secuencial y todas las paralelas.

## Verificación adicional: corrección del rebote en esquinas

Se escribió un programa de prueba independiente (`updateBeam` llamado en un loop, registrando `vx`/`vy` frame a frame) para confirmar que el bug reportado por el equipo (una línea "temblando" o atrapada en una esquina) quedó corregido:

- Rebote en una sola pared: cambia de eje una sola vez, sin repetirse en el frame siguiente.
- Rebote simultáneo en ambas paredes (esquina real, caso simétrico): **ambos ejes cambian en un único frame** (frame 6 de la prueba) y la línea sigue su trayectoria limpiamente, sin rebotes repetidos en los frames posteriores.

## Capturas de pantalla

*(el equipo debe adjuntar aquí, o directamente en el informe: una captura de `resultados/bitacora.csv` abierto en Excel, y la salida en consola de `python scripts/analizar.py resultados/bitacora.csv`, como pide el criterio de evaluación de este anexo)*
