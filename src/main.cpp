#include <SDL.h>
#include <iostream>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

SDL_Window* g_Window = nullptr;
SDL_Renderer* g_Renderer = nullptr;

bool InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL initialization failed! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    g_Window = SDL_CreateWindow(
        "Cucko Launch",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!g_Window)
    {
        printf("Window creation failed! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    g_Renderer = SDL_CreateRenderer(
        g_Window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!g_Renderer)
    {
        printf("Renderer creation failed! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void CleanUp()
{
    if (g_Renderer) SDL_DestroyRenderer(g_Renderer);
    if (g_Window) SDL_DestroyWindow(g_Window);
    SDL_Quit();
}

void ClearScreen()
{
    SDL_SetRenderDrawColor(g_Renderer, 64, 64, 64, 255); // Dark gray background
    SDL_RenderClear(g_Renderer);
}

int main(int argc, char* argv[])
{
    if (!InitSDL())
    {
        printf("Failed to initialize!\n");
        return -1;
    }

    bool quit = false;
    SDL_Event e;

    // Main game loop
    while (!quit)
    {
        // Handle events
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                }
            }
        }

        // Clear screen
        ClearScreen();

        // Present renderer
        SDL_RenderPresent(g_Renderer);
    }

    CleanUp();
    return 0;
}