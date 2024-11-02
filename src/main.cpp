#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <vector>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TREE_WIDTH 100
#define BRANCH_SPACING 150  // Vertical space between branches
#define SQUIRREL_SIZE 60
#define EGG_SIZE 30

SDL_Window* g_Window = nullptr;
SDL_Renderer* g_Renderer = nullptr;
SDL_Texture* g_EggTexture = nullptr;
SDL_Texture* g_SquirrelTexture = nullptr;
SDL_Texture* g_TreeTexture = nullptr;
SDL_Texture* g_BranchTexture = nullptr;

struct GameObject {
    float x, y;
    int width, height;
    SDL_Texture* texture;
    bool isLeftSide;  // Used for squirrels to determine which side they're on
};

struct GameState {
    GameObject egg;
    std::vector<GameObject> squirrels;
    std::vector<GameObject> branches;
    GameObject leftTree;
    GameObject rightTree;
} g_GameState;

SDL_Texture* LoadTexture(const char* path)
{
    printf("Attempting to load texture: %s\n", path);
    
    SDL_Surface* surface = IMG_Load(path);
    if (!surface)
    {
        printf("Failed to load image %s!\n", path);
        printf("SDL_image Error: %s\n", IMG_GetError());
        printf("Current working directory might be wrong.\n");
        return nullptr;
    }

    printf("Successfully loaded surface: %s (Width: %d, Height: %d)\n", 
           path, surface->w, surface->h);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_Renderer, surface);
    if (!texture)
    {
        printf("Failed to create texture from %s!\n", path);
        printf("SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return nullptr;
    }

    printf("Successfully created texture from %s\n", path);
    
    // Get texture information
    Uint32 format;
    int access, width, height;
    SDL_QueryTexture(texture, &format, &access, &width, &height);
    printf("Texture details - Width: %d, Height: %d\n", width, height);

    SDL_FreeSurface(surface);
    return texture;
}

bool InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL initialization failed! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        printf("SDL_image initialization failed! SDL_image Error: %s\n", IMG_GetError());
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

    if (!g_Window) return false;

    g_Renderer = SDL_CreateRenderer(
        g_Window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!g_Renderer) return false;

    // Load textures
    g_EggTexture = LoadTexture("assets/egg.png");
    g_SquirrelTexture = LoadTexture("assets/squirrel.png");
    g_TreeTexture = LoadTexture("assets/tree.png");
    g_BranchTexture = LoadTexture("assets/branch.png");

    if (!g_EggTexture || !g_SquirrelTexture || !g_TreeTexture || !g_BranchTexture) return false;

    return true;
}

void InitGameObjects()
{
    // Setup trees
    g_GameState.leftTree = {0, 0, TREE_WIDTH, WINDOW_HEIGHT, g_TreeTexture, true};
    g_GameState.rightTree = {WINDOW_WIDTH - TREE_WIDTH, 0, TREE_WIDTH, WINDOW_HEIGHT, g_TreeTexture, false};

    // Setup initial egg position (top center)
    g_GameState.egg = {
        WINDOW_WIDTH / 2.0f - EGG_SIZE / 2.0f,
        50.0f,
        EGG_SIZE,
        EGG_SIZE,
        g_EggTexture,
        true
    };

    // Create squirrels on alternating sides
    int numBranches = (WINDOW_HEIGHT - 100) / BRANCH_SPACING;
    
    // Create branches and squirrels on alternating sides
    for (int i = 0; i < numBranches; i++)
    {
        bool isLeft = i % 2 == 0;
        
        // Add branch
        GameObject branch = {
            isLeft ? TREE_WIDTH : WINDOW_WIDTH - TREE_WIDTH - TREE_WIDTH*2,
            100.0f + i * BRANCH_SPACING,
            TREE_WIDTH*2,
            30,  // Branch height
            g_BranchTexture,
            isLeft
        };
        g_GameState.branches.push_back(branch);

        // Add squirrel at the edge of the branch
        GameObject squirrel = {
            isLeft ? TREE_WIDTH + TREE_WIDTH*2 - SQUIRREL_SIZE : WINDOW_WIDTH - TREE_WIDTH - TREE_WIDTH*2,
            100.0f + i * BRANCH_SPACING - SQUIRREL_SIZE,
            SQUIRREL_SIZE,
            SQUIRREL_SIZE,
            g_SquirrelTexture,
            isLeft
        };
        g_GameState.squirrels.push_back(squirrel);
    }
}

void RenderGameObject(const GameObject& obj)
{
    SDL_Rect dest = {
        static_cast<int>(obj.x),
        static_cast<int>(obj.y),
        obj.width,
        obj.height
    };
    
    // Flip squirrel texture based on side
    SDL_RendererFlip flip = (obj.texture == g_SquirrelTexture && !obj.isLeftSide) 
        ? SDL_FLIP_HORIZONTAL 
        : SDL_FLIP_NONE;
        
    SDL_RenderCopyEx(g_Renderer, obj.texture, nullptr, &dest, 0, nullptr, flip);
}

void Render()
{
    SDL_SetRenderDrawColor(g_Renderer, 135, 206, 235, 255);  // Sky blue background
    SDL_RenderClear(g_Renderer);

    // Render trees
    RenderGameObject(g_GameState.leftTree);
    RenderGameObject(g_GameState.rightTree);

    // Render branches first (behind squirrels)
    for (const auto& branch : g_GameState.branches)
    {
        RenderGameObject(branch);
    }

    // Render squirrels
    for (const auto& squirrel : g_GameState.squirrels)
    {
        RenderGameObject(squirrel);
    }

    // Render egg
    RenderGameObject(g_GameState.egg);

    SDL_RenderPresent(g_Renderer);
}

void CleanUp()
{
    SDL_DestroyTexture(g_EggTexture);
    SDL_DestroyTexture(g_SquirrelTexture);
    SDL_DestroyTexture(g_TreeTexture);
    SDL_DestroyTexture(g_BranchTexture);
    SDL_DestroyRenderer(g_Renderer);
    SDL_DestroyWindow(g_Window);
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char* argv[])
{
    if (!InitSDL())
    {
        printf("Failed to initialize!\n");
        return -1;
    }

    InitGameObjects();

    bool quit = false;
    SDL_Event e;

    while (!quit)
    {
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

        Render();
    }

    CleanUp();
    return 0;
}