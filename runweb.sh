#!/bin/bash

# Compile the web version with optimizations
emcc src/main.cpp -I./include/SDL2 -Wall \
-O3 \
-flto \
-s USE_SDL=2 \
-s USE_SDL_IMAGE=2 \
-s USE_SDL_TTF=2 \
-s USE_SDL_MIXER=2 \
-s SDL2_IMAGE_FORMATS='["png"]' \
-s SDL2_MIXER_FORMATS='["wav","mp3"]' \
--preload-file assets \
-s ALLOW_MEMORY_GROWTH=1 \
-s INITIAL_MEMORY=67108864 \
--shell-file web/shell.html \
-o web/index.html

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful! Starting server..."
    echo "Press 'q' to quit the server"
    # Start server and save PID
    emrun --no_browser --port 8000 . & 
    SERVER_PID=$!
    # Wait for 'q' key
    read -n 1 key
    if [[ $key = "q" ]]; then
        kill $SERVER_PID
    fi
else
    echo "Compilation failed with error code $?"
    read -p "Press enter to continue..."
fi
