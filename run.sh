emcc src/main.cpp -I./include/SDL2 -Wall -g \
-s USE_SDL=2 \
-s USE_SDL_IMAGE=2 \
-s USE_SDL_TTF=2 \
-s USE_SDL_MIXER=2 \
-s SDL2_IMAGE_FORMATS='["png"]' \
-s SDL2_MIXER_FORMATS='["wav","mp3"]' \
--preload-file assets \
-s ALLOW_MEMORY_GROWTH=1 \
-s INITIAL_MEMORY=67108864 \
-o web/index.html

emrun --no_browser --port 8000 .
