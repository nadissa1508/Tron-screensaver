// config.h
// Configuracion del screensaver: valores parametrizables por linea de comandos
// (o solicitados por consola cuando falta N), y su validacion defensiva.
#pragma once

#include <string>

// Modos de la version paralela (paralelo.cpp). La version secuencial siempre
// corre con un solo hilo y no usa este enum para tomar decisiones, pero se
// comparte el tipo para poder imprimir el modo con el mismo texto en el HUD,
// la consola y el CSV de ambos binarios.
enum class ParallelMode {
    SEQUENTIAL, // v1: un solo hilo (el que corre secuencial.exe)
    V2,         // v2: omp parallel for solo en la fase de movimiento (Fase 1)
    V3,         // v3: v2 + deteccion de colisiones en paralelo (buffers locales por hilo)
    V3_NAIVE,   // v3 con "critical" por cada par hallado (variante de comparacion, mas lenta)
    V4          // v4: v3 + particion espacial (grilla uniforme) para las colisiones
};

// Politica de reparto de iteraciones de OpenMP para los bucles paralelos.
enum class Schedule { STATIC, DYNAMIC, GUIDED };

// Resultado de intentar leer la configuracion desde argv (y, si hace falta, desde stdin).
enum class ParseResult {
    OK,         // configuracion valida, se puede iniciar el screensaver
    EXIT_OK,    // se pidio --help: ya se imprimio el uso, terminar con exito
    EXIT_ERROR  // argumento invalido o entrada cancelada: terminar con error
};

// Agrupa todos los parametros configurables del screensaver. Ningun valor queda
// "hardcodeado" en la logica: todo lo que el enunciado permite variar (N, tamano
// de ventana, velocidades, hilos, etc.) vive aqui y se pasa explicitamente.
struct Config {
    // --- parametros del escenario ---
    int          n          = 60;     // N: cantidad de lineas a renderizar (unico obligatorio)
    int          width      = 800;    // ancho del canvas en pixeles (minimo 640 segun enunciado)
    int          height     = 600;    // alto del canvas en pixeles (minimo 480 segun enunciado)
    float        length     = 40.0f;  // longitud fija de cada linea, en pixeles
    float        speedMin   = 90.0f;  // rapidez minima de una linea recien creada, px/s
    float        speedMax   = 220.0f; // rapidez maxima de una linea recien creada, px/s
    float        cooldown   = 0.15f;  // segundos sin poder volver a chocar tras un choque
    unsigned int seed       = 0;      // semilla del generador pseudoaleatorio; parseArgs la fija a
                                       // time(nullptr) si --seed no se dio explicitamente (0 SI es una
                                       // semilla valida y reproducible si el usuario la pide con --seed 0)

    // --- paralelizacion (solo los usa paralelo.exe; secuencial.exe los ignora) ---
    int          threads    = 0;      // hilos OpenMP a usar; 0 => omp_get_max_threads()
    ParallelMode mode       = ParallelMode::V3;
    Schedule     schedule   = Schedule::DYNAMIC;

    // --- modo benchmark (para medir tiempos sin depender de la tasa de refresco) ---
    int          frames     = 0;      // 0 => modo interactivo (ventana, loop infinito hasta ESC/cerrar)
    bool         headless   = false;  // true => no crea ventana; solo mide fases 1 y 2
    std::string  csvPath;             // si no esta vacio, se agrega una fila de resultados aqui

    // --- render ---
    bool         trail      = true;   // estela neon que se desvanece (efecto light-cycle)
    bool         grid       = true;   // rejilla de fondo sutil
    bool         vsync      = false;  // limitar la presentacion a la tasa de refresco del monitor
    std::string  fontPath   = "assets/fonts/Orbitron-Regular.ttf"; // fuente TTF del HUD

    // --- depuracion ---
    bool         debugCorner = false; // fuerza N=1 apuntando a una esquina, para validar el rebote
};

// Lee los argumentos de linea de comandos, valida cada uno de forma defensiva
// (rangos, formato numerico, flags desconocidos o sin valor) y, si N no vino en
// argv y no estamos en modo benchmark (--frames > 0), lo solicita por consola,
// reintentando hasta recibir un entero valido o saliendo limpio si hay EOF.
// isParallelBinary indica si quien llama es paralelo.exe (habilita -t/-m/--schedule);
// en secuencial.exe esos flags se rechazan con un mensaje explicando por que.
ParseResult parseArgs(int argc, char* argv[], Config& config, bool isParallelBinary);

// Imprime el mensaje de uso (usado por --help y como ayuda tras un error de argumentos).
void printUsage(const char* programName, bool isParallelBinary);

// Texto corto para el HUD, la consola y el CSV.
const char* modeToString(ParallelMode mode);
const char* scheduleToString(Schedule schedule);
