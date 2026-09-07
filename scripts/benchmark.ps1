# benchmark.ps1
# Corre la matriz de mediciones del proyecto (N x hilos x version x schedule),
# 10 repeticiones por configuracion, y deja todo en un solo CSV (resultados/).
#
# Uso tipico (matriz completa, puede tardar bastante -- correrla de un tiron o en
# tandas usando los parametros para acotarla):
#   powershell -File scripts/benchmark.ps1
#
# Acotar la corrida (por ejemplo, solo N chicos mientras se prueba el script):
#   powershell -File scripts/benchmark.ps1 -Ns 100,250 -Threads 1,4 -Modes v3 -Repeats 3
#
# Requiere que `make all` ya se haya corrido (secuencial.exe y paralelo.exe deben
# existir en la raiz del proyecto).

param(
    [int[]]$Ns = @(100, 250, 500, 1000, 2000, 4000),
    [int[]]$Threads = @(1, 2, 4, 8),          # se agrega automaticamente el maximo de la maquina
    [string[]]$Modes = @("v2", "v3", "v3-naive", "v4"),
    [string[]]$Schedules = @("dynamic"),       # agregar "static","guided" para comparar politicas
    [int]$Repeats = 10,
    [int]$Frames = 600,
    [int]$Seed = 42,
    [switch]$IncludeWindowed,                  # ademas de --headless, correr tambien con ventana
    [string]$OutCsv = "resultados/bitacora.csv",
    [string]$SeqBin = "./secuencial.exe",
    [string]$ParBin = "./paralelo.exe"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SeqBin)) {
    Write-Error "No se encontro $SeqBin. Corre 'make all' (o 'mingw32-make all') antes de este script."
    exit 1
}
if (-not (Test-Path $ParBin)) {
    Write-Error "No se encontro $ParBin. Corre 'make all' (o 'mingw32-make all') antes de este script."
    exit 1
}

$resultsDir = Split-Path -Parent $OutCsv
if ($resultsDir -and -not (Test-Path $resultsDir)) {
    New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null
}

# Hilos maximos de esta maquina (se agrega a la lista si no estaba ya).
$maxThreads = [Environment]::ProcessorCount
if ($Threads -notcontains $maxThreads) {
    $Threads += $maxThreads
}

$headlessOptions = @($true)
if ($IncludeWindowed) { $headlessOptions += $false }

$totalRuns = $Ns.Count * (1 + ($Threads.Count * $Modes.Count * $Schedules.Count)) * $Repeats * $headlessOptions.Count
$runIndex = 0
$startedAt = Get-Date
Write-Output "Total de corridas planeadas: $totalRuns"

foreach ($headless in $headlessOptions) {
    $headlessFlag = if ($headless) { "--headless" } else { "" }
    $headlessLabel = if ($headless) { "headless" } else { "ventana" }

    foreach ($n in $Ns) {
        # --- v1 (secuencial): una sola configuracion de hilos (1), sin --mode/--schedule ---
        for ($rep = 1; $rep -le $Repeats; $rep++) {
            $runIndex++
            Write-Output "[$runIndex/$totalRuns] v1 secuencial N=$n rep=$rep ($headlessLabel)"
            $args = @($n, "--frames", $Frames, "--seed", $Seed, "--csv", $OutCsv)
            if ($headlessFlag) { $args += $headlessFlag }
            & $SeqBin @args | Out-Null
        }

        # --- v2/v3/v3-naive/v4 (paralelo): todas las combinaciones de hilos/schedule ---
        foreach ($threads in $Threads) {
            foreach ($mode in $Modes) {
                foreach ($schedule in $Schedules) {
                    for ($rep = 1; $rep -le $Repeats; $rep++) {
                        $runIndex++
                        Write-Output "[$runIndex/$totalRuns] $mode N=$n hilos=$threads schedule=$schedule rep=$rep ($headlessLabel)"
                        $args = @($n, "--frames", $Frames, "--seed", $Seed, "--csv", $OutCsv,
                                   "--threads", $threads, "--mode", $mode, "--schedule", $schedule)
                        if ($headlessFlag) { $args += $headlessFlag }
                        & $ParBin @args | Out-Null
                    }
                }
            }
        }
    }
}

$elapsed = (Get-Date) - $startedAt
Write-Output ""
Write-Output "Listo. $totalRuns corridas en $($elapsed.ToString('hh\:mm\:ss'))."
Write-Output "Resultados acumulados en: $OutCsv"
Write-Output "Siguiente paso: python scripts/analizar.py $OutCsv"
