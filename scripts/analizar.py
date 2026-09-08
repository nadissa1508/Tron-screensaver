#!/usr/bin/env python3
# analizar.py
# Lee el CSV que genera scripts/benchmark.ps1 (o cualquier corrida individual con
# --csv) y calcula, por cada configuracion (N, modo, hilos, schedule):
#   - promedio y maximo de t_total_ms (y de cada fase)
#   - speedup  = t_total_promedio(v1, mismo N) / t_total_promedio(esta config)
#   - eficiencia = speedup / hilos
# Solo usa la libreria estandar de Python (csv, statistics), para no depender de
# tener pandas instalado.
#
# Uso:
#   python scripts/analizar.py resultados/bitacora.csv
#   python scripts/analizar.py resultados/bitacora.csv --out resultados/resumen.csv
import argparse
import csv
import statistics
import sys
from collections import defaultdict


def leer_filas(ruta):
    with open(ruta, newline="", encoding="utf-8") as archivo:
        lector = csv.DictReader(archivo)
        filas = []
        for fila in lector:
            fila["n"] = int(fila["n"])
            fila["threads"] = int(fila["threads"])
            fila["frames"] = int(fila["frames"])
            for campo in ("t_update_ms", "t_collision_ms", "t_render_ms", "t_total_ms", "fps_avg"):
                fila[campo] = float(fila[campo])
            filas.append(fila)
        return filas


def agrupar(filas):
    # Clave: (n, version, schedule, threads). Todas las filas con la misma clave son
    # repeticiones de la misma configuracion (--seed distinta idealmente no importa,
    # pero aqui se agrupan tal cual vienen en el CSV).
    grupos = defaultdict(list)
    for fila in filas:
        clave = (fila["n"], fila["version"], fila["schedule"], fila["threads"])
        grupos[clave].append(fila)
    return grupos


def resumen_por_grupo(grupos):
    resumen = {}
    for clave, filas in grupos.items():
        totales = [f["t_total_ms"] for f in filas]
        resumen[clave] = {
            "reps": len(filas),
            "t_update_avg": statistics.mean(f["t_update_ms"] for f in filas),
            "t_collision_avg": statistics.mean(f["t_collision_ms"] for f in filas),
            "t_render_avg": statistics.mean(f["t_render_ms"] for f in filas),
            "t_total_avg": statistics.mean(totales),
            "t_total_max": max(totales),
            "fps_avg": statistics.mean(f["fps_avg"] for f in filas),
            "checksums": {f["checksum"] for f in filas},
        }
    return resumen


def main():
    parser = argparse.ArgumentParser(description="Calcula speedup y eficiencia a partir del CSV de mediciones.")
    parser.add_argument("csv_entrada", help="CSV generado por --csv (secuencial.exe / paralelo.exe / benchmark.ps1)")
    parser.add_argument("--out", help="Si se indica, tambien escribe el resumen a este CSV")
    args = parser.parse_args()

    filas = leer_filas(args.csv_entrada)
    if not filas:
        print("El CSV esta vacio.", file=sys.stderr)
        sys.exit(1)

    grupos = agrupar(filas)
    resumen = resumen_por_grupo(grupos)

    # El baseline de cada N es la version secuencial (v1) con ese mismo N.
    baseline_por_n = {}
    for (n, version, _schedule, threads), datos in resumen.items():
        if version == "v1-secuencial" and threads == 1:
            baseline_por_n[n] = datos["t_total_avg"]

    filas_salida = []
    encabezado = ["n", "version", "schedule", "threads", "reps", "t_update_avg_ms", "t_collision_avg_ms",
                  "t_render_avg_ms", "t_total_avg_ms", "t_total_max_ms", "fps_avg", "speedup", "eficiencia",
                  "checksum_consistente"]

    print(f"{'N':>6} {'version':<10} {'schedule':<8} {'hilos':>5} {'t_total(ms)':>12} "
          f"{'fps':>8} {'speedup':>8} {'eficiencia':>10} {'checksum_ok':>12}")

    for clave in sorted(resumen.keys()):
        n, version, schedule, threads = clave
        datos = resumen[clave]
        baseline = baseline_por_n.get(n)
        speedup = (baseline / datos["t_total_avg"]) if baseline and datos["t_total_avg"] > 0 else float("nan")
        eficiencia = (speedup / threads) if threads > 0 else float("nan")
        checksum_ok = len(datos["checksums"]) == 1  # todas las repeticiones dieron el mismo resultado

        print(f"{n:>6} {version:<10} {schedule:<8} {threads:>5} {datos['t_total_avg']:>12.3f} "
              f"{datos['fps_avg']:>8.1f} {speedup:>8.2f} {eficiencia:>10.3f} {str(checksum_ok):>12}")

        filas_salida.append([
            n, version, schedule, threads, datos["reps"],
            f"{datos['t_update_avg']:.4f}", f"{datos['t_collision_avg']:.4f}", f"{datos['t_render_avg']:.4f}",
            f"{datos['t_total_avg']:.4f}", f"{datos['t_total_max']:.4f}", f"{datos['fps_avg']:.2f}",
            f"{speedup:.4f}" if speedup == speedup else "", f"{eficiencia:.4f}" if eficiencia == eficiencia else "",
            checksum_ok,
        ])

    print("\nNota: 'checksum_ok' compara el checksum entre las repeticiones de UNA MISMA config;")
    print("para validar que la paralelizacion es correcta, compara ademas el checksum de cada")
    print("version contra v1 con el mismo --seed y --frames (deben coincidir exactamente).")

    if args.out:
        with open(args.out, "w", newline="", encoding="utf-8") as salida:
            escritor = csv.writer(salida)
            escritor.writerow(encabezado)
            escritor.writerows(filas_salida)
        print(f"\nResumen guardado en: {args.out}")


if __name__ == "__main__":
    main()
