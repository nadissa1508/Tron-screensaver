// config.cpp
// Implementacion de la lectura y validacion de parametros (ver config.h).
#include "config.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>

namespace {

// --- Entradas: cadena de argv. Salidas: valor numerico via referencia y un bool
// que indica si el formato era valido (sin basura al final, sin overflow). ---

bool parseLong(const char* text, long& out) {
    if (text == nullptr || *text == '\0') return false;
    char* endPtr = nullptr;
    errno = 0;
    long value = std::strtol(text, &endPtr, 10);
    if (endPtr == text || *endPtr != '\0' || errno == ERANGE) return false;
    out = value;
    return true;
}

bool parseFloat(const char* text, float& out) {
    if (text == nullptr || *text == '\0') return false;
    char* endPtr = nullptr;
    errno = 0;
    float value = std::strtof(text, &endPtr);
    if (endPtr == text || *endPtr != '\0' || errno == ERANGE) return false;
    out = value;
    return true;
}

// Pide el siguiente argv como valor de una bandera. Devuelve false (con mensaje
// de error) si no hay mas argumentos, en cuyo caso la bandera quedo "sin valor".
bool takeValue(int argc, char* argv[], int& i, const char* flagName, std::string& out) {
    if (i + 1 >= argc) {
        std::cerr << "Error: la bandera " << flagName << " requiere un valor.\n";
        return false;
    }
    out = argv[++i];
    return true;
}

bool takeIntValue(int argc, char* argv[], int& i, const char* flagName, long& out) {
    std::string raw;
    if (!takeValue(argc, argv, i, flagName, raw)) return false;
    if (!parseLong(raw.c_str(), out)) {
        std::cerr << "Error: valor invalido para " << flagName << ": '" << raw << "' (se esperaba un entero).\n";
        return false;
    }
    return true;
}

bool takeFloatValue(int argc, char* argv[], int& i, const char* flagName, float& out) {
    std::string raw;
    if (!takeValue(argc, argv, i, flagName, raw)) return false;
    if (!parseFloat(raw.c_str(), out)) {
        std::cerr << "Error: valor invalido para " << flagName << ": '" << raw << "' (se esperaba un numero).\n";
        return false;
    }
    return true;
}

// Entrada: N leido de argv o de consola. Salida: true si esta en el rango permitido.
// Se valida aqui en un solo lugar para que argv y la solicitud interactiva usen
// exactamente la misma regla.
bool nInRange(long n) {
    return n >= 1 && n <= 100000;
}

// Solicita N por consola de forma defensiva: reintenta ante entradas no numericas
// o fuera de rango, y sale limpio (EXIT_ERROR) si la entrada estandar llega a EOF
// (por ejemplo, el proceso corre sin terminal interactiva).
bool requestNFromConsole(int& n) {
    std::cout << "No se especifico N por linea de comandos.\n";
    for (;;) {
        std::cout << "Ingrese la cantidad de lineas a renderizar (N, entero entre 1 y 100000): ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cerr << "\nError: no se pudo leer N desde la entrada estandar (EOF).\n";
            return false;
        }
        long value;
        if (!parseLong(line.c_str(), value) || !nInRange(value)) {
            std::cerr << "Entrada invalida: '" << line << "'. Debe ser un entero entre 1 y 100000.\n";
            continue;
        }
        n = static_cast<int>(value);
        return true;
    }
}

} // namespace

const char* modeToString(ParallelMode mode) {
    switch (mode) {
        case ParallelMode::SEQUENTIAL: return "v1-secuencial";
        case ParallelMode::V2:         return "v2";
        case ParallelMode::V3:         return "v3";
        case ParallelMode::V3_NAIVE:   return "v3-naive";
        case ParallelMode::V4:         return "v4";
    }
    return "desconocido";
}

const char* scheduleToString(Schedule schedule) {
    switch (schedule) {
        case Schedule::STATIC:  return "static";
        case Schedule::DYNAMIC: return "dynamic";
        case Schedule::GUIDED:  return "guided";
    }
    return "desconocido";
}

void printUsage(const char* programName, bool isParallelBinary) {
    std::cout <<
        "Uso: " << programName << " [N] [opciones]\n"
        "\n"
        "  N                       Cantidad de lineas a renderizar (1..100000).\n"
        "                          Si se omite y no es modo benchmark, se solicita por consola.\n"
        "\n"
        "Opciones del escenario:\n"
        "  -w, --width ANCHO       Ancho de la ventana en px (minimo 640). Defecto: 800.\n"
        "  -h, --height ALTO       Alto de la ventana en px (minimo 480). Defecto: 600.\n"
        "  -l, --length LONG       Longitud fija de cada linea en px (5..200). Defecto: 40.\n"
        "      --speed-min V       Rapidez minima al nacer, px/s (>0). Defecto: 90.\n"
        "      --speed-max V       Rapidez maxima al nacer, px/s (>= speed-min). Defecto: 220.\n"
        "      --cooldown S        Segundos sin poder rechocar tras un choque (0..2). Defecto: 0.15.\n"
        "  -s, --seed N            Semilla del generador pseudoaleatorio. Defecto: hora actual.\n";
    if (isParallelBinary) {
        std::cout <<
            "\n"
            "Opciones de paralelizacion (solo en la version paralela):\n"
            "  -t, --threads N         Hilos OpenMP a usar (1..256). Defecto: todos los disponibles.\n"
            "  -m, --mode MODO         v2 | v3 | v3-naive | v4. Defecto: v3.\n"
            "      --schedule POL      static | dynamic | guided. Defecto: dynamic.\n";
    }
    std::cout <<
        "\n"
        "Opciones de medicion (modo benchmark):\n"
        "      --frames F          Corre exactamente F frames con dt fijo (1/60s) y termina.\n"
        "                          Con F > 0 no se solicita N por consola: si falta, es error.\n"
        "      --headless          No crea ventana; solo mide las fases de computo.\n"
        "      --csv ARCHIVO       Agrega una fila de resultados a ARCHIVO (se crea si no existe).\n"
        "\n"
        "Opciones de render:\n"
        "      --trail / --no-trail   Estela neon que se desvanece. Defecto: activada.\n"
        "      --grid / --no-grid     Rejilla de fondo sutil. Defecto: activada.\n"
        "      --vsync / --no-vsync   Sincronizar con el monitor. Defecto: desactivada.\n"
        "      --font RUTA.ttf     Fuente TTF para el HUD. Defecto: assets/fonts/Orbitron-Regular.ttf\n"
        "\n"
        "      --debug-corner      Fuerza N=1 apuntando a una esquina (prueba de rebote).\n"
        "      --help              Muestra esta ayuda y termina.\n"
        "\n"
        "Controles en ventana: ESC o cerrar la ventana para salir.\n";
}

ParseResult parseArgs(int argc, char* argv[], Config& config, bool isParallelBinary) {
    bool nGiven = false;
    bool seedGiven = false; // para poder aceptar --seed 0 explicito (0 no es "sin semilla")
    long tmpLong = 0;
    float tmpFloat = 0.0f;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage(argv[0], isParallelBinary);
            return ParseResult::EXIT_OK;
        } else if (arg == "-w" || arg == "--width") {
            if (!takeIntValue(argc, argv, i, arg.c_str(), tmpLong)) return ParseResult::EXIT_ERROR;
            if (tmpLong < 640) {
                std::cerr << "Error: --width debe ser >= 640 (minimo del enunciado). Recibido: " << tmpLong << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.width = static_cast<int>(tmpLong);
        } else if (arg == "-h" || arg == "--height") {
            if (!takeIntValue(argc, argv, i, arg.c_str(), tmpLong)) return ParseResult::EXIT_ERROR;
            if (tmpLong < 480) {
                std::cerr << "Error: --height debe ser >= 480 (minimo del enunciado). Recibido: " << tmpLong << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.height = static_cast<int>(tmpLong);
        } else if (arg == "-l" || arg == "--length") {
            if (!takeFloatValue(argc, argv, i, arg.c_str(), tmpFloat)) return ParseResult::EXIT_ERROR;
            if (tmpFloat < 5.0f || tmpFloat > 200.0f) {
                std::cerr << "Error: --length debe estar en [5, 200]. Recibido: " << tmpFloat << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.length = tmpFloat;
        } else if (arg == "--speed-min") {
            if (!takeFloatValue(argc, argv, i, arg.c_str(), tmpFloat)) return ParseResult::EXIT_ERROR;
            if (tmpFloat <= 0.0f) {
                std::cerr << "Error: --speed-min debe ser > 0. Recibido: " << tmpFloat << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.speedMin = tmpFloat;
        } else if (arg == "--speed-max") {
            if (!takeFloatValue(argc, argv, i, arg.c_str(), tmpFloat)) return ParseResult::EXIT_ERROR;
            if (tmpFloat <= 0.0f) {
                std::cerr << "Error: --speed-max debe ser > 0. Recibido: " << tmpFloat << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.speedMax = tmpFloat;
        } else if (arg == "--cooldown") {
            if (!takeFloatValue(argc, argv, i, arg.c_str(), tmpFloat)) return ParseResult::EXIT_ERROR;
            if (tmpFloat < 0.0f || tmpFloat > 2.0f) {
                std::cerr << "Error: --cooldown debe estar en [0, 2]. Recibido: " << tmpFloat << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.cooldown = tmpFloat;
        } else if (arg == "-s" || arg == "--seed") {
            if (!takeIntValue(argc, argv, i, arg.c_str(), tmpLong)) return ParseResult::EXIT_ERROR;
            if (tmpLong < 0) {
                std::cerr << "Error: --seed debe ser >= 0. Recibido: " << tmpLong << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.seed = static_cast<unsigned int>(tmpLong);
            seedGiven = true;
        } else if (arg == "-t" || arg == "--threads") {
            if (!isParallelBinary) {
                std::cerr << "Error: " << arg << " solo aplica a la version paralela (paralelo.exe).\n";
                return ParseResult::EXIT_ERROR;
            }
            if (!takeIntValue(argc, argv, i, arg.c_str(), tmpLong)) return ParseResult::EXIT_ERROR;
            if (tmpLong < 1 || tmpLong > 256) {
                std::cerr << "Error: --threads debe estar en [1, 256]. Recibido: " << tmpLong << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.threads = static_cast<int>(tmpLong);
        } else if (arg == "-m" || arg == "--mode") {
            if (!isParallelBinary) {
                std::cerr << "Error: " << arg << " solo aplica a la version paralela (paralelo.exe).\n";
                return ParseResult::EXIT_ERROR;
            }
            std::string raw;
            if (!takeValue(argc, argv, i, arg.c_str(), raw)) return ParseResult::EXIT_ERROR;
            if (raw == "v2") config.mode = ParallelMode::V2;
            else if (raw == "v3") config.mode = ParallelMode::V3;
            else if (raw == "v3-naive") config.mode = ParallelMode::V3_NAIVE;
            else if (raw == "v4") config.mode = ParallelMode::V4;
            else {
                std::cerr << "Error: --mode invalido: '" << raw << "'. Use v2, v3, v3-naive o v4.\n";
                return ParseResult::EXIT_ERROR;
            }
        } else if (arg == "--schedule") {
            if (!isParallelBinary) {
                std::cerr << "Error: --schedule solo aplica a la version paralela (paralelo.exe).\n";
                return ParseResult::EXIT_ERROR;
            }
            std::string raw;
            if (!takeValue(argc, argv, i, arg.c_str(), raw)) return ParseResult::EXIT_ERROR;
            if (raw == "static") config.schedule = Schedule::STATIC;
            else if (raw == "dynamic") config.schedule = Schedule::DYNAMIC;
            else if (raw == "guided") config.schedule = Schedule::GUIDED;
            else {
                std::cerr << "Error: --schedule invalido: '" << raw << "'. Use static, dynamic o guided.\n";
                return ParseResult::EXIT_ERROR;
            }
        } else if (arg == "--frames") {
            if (!takeIntValue(argc, argv, i, arg.c_str(), tmpLong)) return ParseResult::EXIT_ERROR;
            if (tmpLong < 0) {
                std::cerr << "Error: --frames debe ser >= 0. Recibido: " << tmpLong << ".\n";
                return ParseResult::EXIT_ERROR;
            }
            config.frames = static_cast<int>(tmpLong);
        } else if (arg == "--headless") {
            config.headless = true;
        } else if (arg == "--csv") {
            if (!takeValue(argc, argv, i, arg.c_str(), config.csvPath)) return ParseResult::EXIT_ERROR;
        } else if (arg == "--trail") {
            config.trail = true;
        } else if (arg == "--no-trail") {
            config.trail = false;
        } else if (arg == "--grid") {
            config.grid = true;
        } else if (arg == "--no-grid") {
            config.grid = false;
        } else if (arg == "--vsync") {
            config.vsync = true;
        } else if (arg == "--no-vsync") {
            config.vsync = false;
        } else if (arg == "--font") {
            if (!takeValue(argc, argv, i, arg.c_str(), config.fontPath)) return ParseResult::EXIT_ERROR;
        } else if (arg == "--debug-corner") {
            config.debugCorner = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: bandera desconocida: '" << arg << "'.\n";
            printUsage(argv[0], isParallelBinary);
            return ParseResult::EXIT_ERROR;
        } else {
            // Argumento posicional: solo se acepta uno, y es N.
            if (nGiven) {
                std::cerr << "Error: argumento posicional inesperado: '" << arg << "' (N ya se recibio).\n";
                return ParseResult::EXIT_ERROR;
            }
            long value;
            if (!parseLong(arg.c_str(), value) || !nInRange(value)) {
                std::cerr << "Error: N debe ser un entero entre 1 y 100000. Recibido: '" << arg << "'.\n";
                return ParseResult::EXIT_ERROR;
            }
            config.n = static_cast<int>(value);
            nGiven = true;
        }
    }

    // Programacion defensiva sobre combinaciones de rangos (no solo valores sueltos).
    if (config.speedMin > config.speedMax) {
        std::cerr << "Error: --speed-min (" << config.speedMin << ") no puede ser mayor que --speed-max ("
                   << config.speedMax << ").\n";
        return ParseResult::EXIT_ERROR;
    }

    // --debug-corner fuerza N=1 sin importar lo demas: ni se pide por consola ni se
    // exige por argv, porque su unico proposito es la linea de prueba de la esquina.
    if (config.debugCorner) {
        config.n = 1;
    } else if (!nGiven && config.frames == 0) {
        // Solicitud de ingreso de datos: si N no vino por argv y esto no es una
        // corrida de benchmark desatendida (--frames > 0), se pide por consola.
        int n = config.n;
        if (!requestNFromConsole(n)) return ParseResult::EXIT_ERROR;
        config.n = n;
    } else if (!nGiven && config.frames > 0) {
        std::cerr << "Error: en modo benchmark (--frames > 0) N es obligatorio por linea de comandos.\n";
        return ParseResult::EXIT_ERROR;
    }

    if (!seedGiven) {
        config.seed = static_cast<unsigned int>(std::time(nullptr));
    }

    return ParseResult::OK;
}
