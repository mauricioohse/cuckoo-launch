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
#define GRAVITY 0.5f
#define TERMINAL_VELOCITY 10.0f
#define STRENGTH_BAR_WIDTH 200
#define STRENGTH_BAR_HEIGHT 20
#define STRENGTH_BAR_X 20
#define STRENGTH_BAR_Y 550
#define ANGLE_BAR_WIDTH 20
#define ANGLE_BAR_HEIGHT 200
#define ANGLE_BAR_X 240
#define ANGLE_BAR_Y 370
#define ANGLE_SQUARE_SIZE 15
#define ANGLE_GRAVITY 0.3f
#define ANGLE_JUMP_POWER 8.0f

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
    float eggVelocityY;  // Vertical velocity of egg
    bool eggIsHeld;      // Whether a squirrel is holding the egg
    std::vector<GameObject> squirrels;
    std::vector<GameObject> branches;
    GameObject leftTree;
    GameObject rightTree;
    float strengthCharge;     // 0.0 to 1.0
    bool isCharging;         // Is left mouse being held
    bool isDepletingCharge;  // Has charge maxed out
    float angleSquareY;      // Position in the angle bar
    float angleSquareVelocity;
} g_GameState;

// Add this forward declaration near the top of the file, after the GameState struct
void RenderControls();

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
        
        // Add branch with float casting
        GameObject branch = {
            static_cast<float>(isLeft ? TREE_WIDTH : WINDOW_WIDTH - TREE_WIDTH - TREE_WIDTH*2),
            100.0f + i * BRANCH_SPACING,
            TREE_WIDTH*2,
            30,  // Branch height
            g_BranchTexture,
            isLeft
        };
        g_GameState.branches.push_back(branch);

        // Add squirrel with float casting
        GameObject squirrel = {
            static_cast<float>(isLeft ? TREE_WIDTH + TREE_WIDTH*2 - SQUIRREL_SIZE : WINDOW_WIDTH - TREE_WIDTH - TREE_WIDTH*2),
            100.0f + i * BRANCH_SPACING - SQUIRREL_SIZE,
            SQUIRREL_SIZE,
            SQUIRREL_SIZE,
            g_SquirrelTexture,
            isLeft
        };
        g_GameState.squirrels.push_back(squirrel);
    }

    g_GameState.eggVelocityY = 0.0f;
    g_GameState.eggIsHeld = false;
    g_GameState.strengthCharge = 0.0f;
    g_GameState.isCharging = false;
    g_GameState.isDepletingCharge = false;
    g_GameState.angleSquareY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
    g_GameState.angleSquareVelocity = 0.0f;
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

    RenderControls();

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

bool CheckCollision(const SDL_Rect& a, const SDL_Rect& b)
{
    return (a.x < b.x + b.w &&
            a.x + a.w > b.x &&
            a.y < b.y + b.h &&
            a.y + a.h > b.y);
}

void UpdatePhysics()
{
    if (!g_GameState.eggIsHeld)
    {
        // Apply gravity
        g_GameState.eggVelocityY += GRAVITY;
        
        // Limit fall speed
        if (g_GameState.eggVelocityY > TERMINAL_VELOCITY)
            g_GameState.eggVelocityY = TERMINAL_VELOCITY;
            
        // Update egg position
        g_GameState.egg.y += g_GameState.eggVelocityY;

        // Create egg collision rect
        SDL_Rect eggRect = {
            static_cast<int>(g_GameState.egg.x),
            static_cast<int>(g_GameState.egg.y),
            g_GameState.egg.width,
            g_GameState.egg.height
        };

        // Check collision with trees
        SDL_Rect leftTreeRect = {
            static_cast<int>(g_GameState.leftTree.x),
            static_cast<int>(g_GameState.leftTree.y),
            g_GameState.leftTree.width,
            g_GameState.leftTree.height
        };
        
        SDL_Rect rightTreeRect = {
            static_cast<int>(g_GameState.rightTree.x),
            static_cast<int>(g_GameState.rightTree.y),
            g_GameState.rightTree.width,
            g_GameState.rightTree.height
        };

        // If egg hits trees, bounce it slightly
        if (CheckCollision(eggRect, leftTreeRect) || 
            CheckCollision(eggRect, rightTreeRect))
        {
            g_GameState.eggVelocityY = -g_GameState.eggVelocityY * 0.5f; // Bounce with 50% force
        }

        // Check collision with squirrels
        for (auto& squirrel : g_GameState.squirrels)
        {
            SDL_Rect squirrelRect = {
                static_cast<int>(squirrel.x),
                static_cast<int>(squirrel.y),
                squirrel.width,
                squirrel.height
            };

            if (CheckCollision(eggRect, squirrelRect))
            {
                g_GameState.eggIsHeld = true;
                g_GameState.eggVelocityY = 0;
                // Position egg slightly above squirrel
                g_GameState.egg.y = squirrel.y - g_GameState.egg.height;
                g_GameState.egg.x = squirrel.x + (squirrel.width - g_GameState.egg.width) / 2;
                break;
            }
        }

        // Reset if egg falls off screen
        if (g_GameState.egg.y > WINDOW_HEIGHT)
        {
            g_GameState.egg.y = 50.0f;
            g_GameState.egg.x = WINDOW_WIDTH / 2.0f - EGG_SIZE / 2.0f;
            g_GameState.eggVelocityY = 0;
        }
    }
}

void UpdateControls()
{
    // Update strength bar
    if (g_GameState.isCharging && !g_GameState.isDepletingCharge)
    {
        g_GameState.strengthCharge += 0.02f;  // Adjust speed as needed
        if (g_GameState.strengthCharge >= 1.0f)
        {
            g_GameState.strengthCharge = 1.0f;
            g_GameState.isDepletingCharge = true;
        }
    }
    else if (g_GameState.isDepletingCharge)
    {
        g_GameState.strengthCharge -= 0.02f;  // Adjust speed as needed
        if (g_GameState.strengthCharge <= 0.0f)
        {
            g_GameState.strengthCharge = 0.0f;
            g_GameState.isCharging = false;
            g_GameState.isDepletingCharge = false;
        }
    }

    // Update angle square physics
    if (g_GameState.eggIsHeld)
    {
        // Apply gravity to angle square
        g_GameState.angleSquareVelocity += ANGLE_GRAVITY;
        g_GameState.angleSquareY += g_GameState.angleSquareVelocity;

        // Constrain to bar bounds
        float maxY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
        if (g_GameState.angleSquareY > maxY)
        {
            g_GameState.angleSquareY = maxY;
            g_GameState.angleSquareVelocity = 0;
        }
        else if (g_GameState.angleSquareY < ANGLE_BAR_Y)
        {
            g_GameState.angleSquareY = ANGLE_BAR_Y;
            g_GameState.angleSquareVelocity = 0;
        }
    }
}

void RenderControls()
{
    if (g_GameState.eggIsHeld)
    {
        // Draw strength bar background
        SDL_Rect strengthBarBg = {
            STRENGTH_BAR_X,
            STRENGTH_BAR_Y,
            STRENGTH_BAR_WIDTH,
            STRENGTH_BAR_HEIGHT
        };
        SDL_SetRenderDrawColor(g_Renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(g_Renderer, &strengthBarBg);

        // Draw strength bar fill
        SDL_Rect strengthBarFill = {
            STRENGTH_BAR_X,
            STRENGTH_BAR_Y,
            static_cast<int>(STRENGTH_BAR_WIDTH * g_GameState.strengthCharge),
            STRENGTH_BAR_HEIGHT
        };
        SDL_SetRenderDrawColor(g_Renderer, 
            g_GameState.isDepletingCharge ? 255 : 0,  // Red if depleting
            g_GameState.isDepletingCharge ? 0 : 255,  // Green if charging
            0, 
            255);
        SDL_RenderFillRect(g_Renderer, &strengthBarFill);

        // Draw angle bar background
        SDL_Rect angleBarBg = {
            ANGLE_BAR_X,
            ANGLE_BAR_Y,
            ANGLE_BAR_WIDTH,
            ANGLE_BAR_HEIGHT
        };
        SDL_SetRenderDrawColor(g_Renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(g_Renderer, &angleBarBg);

        // Draw angle square
        SDL_Rect angleSquare = {
            ANGLE_BAR_X + (ANGLE_BAR_WIDTH - ANGLE_SQUARE_SIZE) / 2,
            static_cast<int>(g_GameState.angleSquareY),
            ANGLE_SQUARE_SIZE,
            ANGLE_SQUARE_SIZE
        };
        SDL_SetRenderDrawColor(g_Renderer, 255, 255, 0, 255);  // Yellow square
        SDL_RenderFillRect(g_Renderer, &angleSquare);
    }
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
                    case SDLK_SPACE:
                        if (g_GameState.eggIsHeld)
                        {
                            g_GameState.eggIsHeld = false;
                            g_GameState.eggVelocityY = 0;
                        }
                        break;
                    case SDLK_i:  // New debug teleport
                        if (!g_GameState.squirrels.empty())
                        {
                            // Teleport above the first squirrel
                            const auto& squirrel = g_GameState.squirrels[0];
                            g_GameState.egg.x = squirrel.x + (squirrel.width - g_GameState.egg.width) / 2;
                            g_GameState.egg.y = squirrel.y - g_GameState.egg.height - 50;  // 50 pixels above
                            g_GameState.eggVelocityY = 0;
                            g_GameState.eggIsHeld = false;
                        }
                        break;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                if (e.button.button == SDL_BUTTON_LEFT && g_GameState.eggIsHeld)
                {
                    g_GameState.isCharging = true;
                    g_GameState.isDepletingCharge = false;
                    g_GameState.strengthCharge = 0.0f;
                }
                else if (e.button.button == SDL_BUTTON_RIGHT && g_GameState.eggIsHeld)
                {
                    // Jump the angle square with strength based on current charge
                    float jumpPower = ANGLE_JUMP_POWER * g_GameState.strengthCharge;
                    g_GameState.angleSquareVelocity = -jumpPower;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP)
            {
                if (e.button.button == SDL_BUTTON_LEFT && g_GameState.eggIsHeld)
                {
                    // Launch the egg here (we'll implement this in the next milestone)
                    g_GameState.isCharging = false;
                    printf("Launch with power: %f and angle: %f\n", 
                           g_GameState.strengthCharge,
                           (g_GameState.angleSquareY - ANGLE_BAR_Y) / ANGLE_BAR_HEIGHT);
                }
            }
        }

        UpdatePhysics();
        UpdateControls();
        Render();
    }

    CleanUp();
    return 0;
}