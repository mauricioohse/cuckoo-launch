# Common variables
CXX_WINDOWS = g++
CXX_WEB = emcc
CXXFLAGS = -Wall -g
INCLUDES = -I./include/SDL2

# Windows-specific
WINDOWS_LIBS = -L./lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lwinmm -lusp10 -lgdi32 -static -static-libgcc -static-libstdc++ \
    -lole32 -loleaut32 -limm32 -lversion -lsetupapi -lcfgmgr32 -lrpcrt4 \
    #-mwindows # removes terminal window

# Web-specific
WEB_FLAGS = -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 \
    -s SDL2_IMAGE_FORMATS='["png"]' \
    -s SDL2_MIXER_FORMATS='["wav","mp3"]' \
    --preload-file assets \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=67108864

# Source files
SOURCES = src/main.cpp
WINDOWS_TARGET = bin/out.exe
WEB_TARGET = web/index.html

# Default target
all: windows web

# Windows build
windows: $(WINDOWS_TARGET)

$(WINDOWS_TARGET): $(SOURCES)
	$(CXX_WINDOWS) $(SOURCES) $(INCLUDES) $(CXXFLAGS) $(WINDOWS_LIBS) -o $(WINDOWS_TARGET)

# Web build
web: $(WEB_TARGET)

$(WEB_TARGET): $(SOURCES)
	$(CXX_WEB) $(SOURCES) $(INCLUDES) $(CXXFLAGS) $(WEB_FLAGS) -o $(WEB_TARGET)

clean:
	rm -f $(WINDOWS_TARGET) $(WEB_TARGET) web/index.js web/index.wasm web/index.data

.PHONY: all windows web clean
