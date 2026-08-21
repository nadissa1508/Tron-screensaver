CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -fopenmp -I libs/SDL2/include
LDFLAGS = -L libs/SDL2/lib -lmingw32 -lSDL2main -lSDL2

SRC = src/main.cpp
BIN = screensaver.exe

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)
	@cp -n libs/SDL2/bin/SDL2.dll . 2>/dev/null || true

clean:
	rm -f $(BIN)

.PHONY: all clean
