#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <ctime>
#include <SDL_ttf.h>
#include <sstream>
#include <iomanip>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TREE_WIDTH 100
#define BRANCH_SPACING 150  // Vertical space between branches
#define EGG_SIZE_X 50
#define EGG_SIZE_Y 75
#define EGG_ANIMATION_SPEED 1.0f  // Adjust speed as needed, measured in seconds
#define EGG_SPRITE_COUNT 3
#define GRAVITY 0.5f
#define TERMINAL_VELOCITY 30.0f
#define STRENGTH_BAR_WIDTH 200
#define STRENGTH_BAR_HEIGHT 20
#define STRENGTH_BAR_X 20
#define STRENGTH_BAR_Y 550
#define ANGLE_BAR_WIDTH 20
#define ANGLE_BAR_HEIGHT 200
#define ANGLE_BAR_X 240
#define ANGLE_BAR_Y 370
#define ANGLE_SQUARE_SIZE 15
#define ANGLE_GRAVITY 0.5f
#define ANGLE_JUMP_POWER 8.0f
#define LAUNCH_POWER_SCALE 20.0f
#define PI 3.14159265359f

#define MIN_BRANCH_SPACING (WINDOW_HEIGHT * 0.1f)  // Minimum vertical gap between branches
#define MAX_BRANCH_SPACING (WINDOW_HEIGHT * 0.5f)  // Maximum vertical gap between branches
#define MIN_BRANCH_EXTENSION 50.0f   // Minimum distance branch extends from tree
#define MAX_BRANCH_EXTENSION 300.0f  // Maximum distance branch extends from tree
#define BRANCH_HEIGHT 30             // Height of branch texture

#define ARROW_WIDTH 40
#define ARROW_HEIGHT 15

const int SCREENS_HEIGHT = 15;
const float TOTAL_GAME_HEIGHT = WINDOW_HEIGHT * SCREENS_HEIGHT;
const int BRANCHES_PER_SCREEN = 3;  // Adjust this for desired branch density
const int TOTAL_BRANCHES = BRANCHES_PER_SCREEN * SCREENS_HEIGHT;

SDL_Window* g_Window = nullptr;
SDL_Renderer* g_Renderer = nullptr;
SDL_Texture* g_EggTexture = nullptr;
SDL_Texture* g_SquirrelTexture = nullptr;
SDL_Texture* g_TreeTexture = nullptr;
SDL_Texture* g_BranchTexture = nullptr;
SDL_Texture* g_ArrowTexture = nullptr;
TTF_Font* g_Font = nullptr;
Uint32 g_StartTime = 0;
bool g_TimerActive = false;
bool g_WinAchieved = false;
Uint32 g_lastElapsedTime = 0;

#define TIMER_X (WINDOW_WIDTH - 150)
#define TIMER_Y 20

#define SCORE_FILE "assets/scores.txt"
#define WIN_MESSAGE_X (WINDOW_WIDTH / 2)
#define WIN_MESSAGE_Y (WINDOW_HEIGHT / 2)

SDL_Texture* g_EggTextures[EGG_SPRITE_COUNT] = {nullptr};
float g_EggAnimationTime = 0.0f;

#define SQUIRREL_SPRITE_COUNT 2
SDL_Texture* g_SquirrelTextures[SQUIRREL_SPRITE_COUNT] = {nullptr};

#define SQUIRREL_SCALE 0.5f  // Adjust this value to scale sprites (1.0f = original size, 0.5f = half size, 2.0f = double size)

struct GameObject {
    float x, y;
    int width, height;
    SDL_Texture* texture;
    bool isLeftSide;  // Used for squirrels to determine which side they're on
    int currentSprite = 0;
    int spriteWidths[SQUIRREL_SPRITE_COUNT] = {0};
    int spriteHeights[SQUIRREL_SPRITE_COUNT] = {0};
};

struct GameState {
    GameObject egg;
    float eggVelocityY;  // Vertical velocity of egg
    float eggVelocityX;  // Add horizontal velocity
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
    GameObject floorSquirrel;  // New floor squirrel
    bool isLaunchingRight;  // Direction flag
    GameObject* activeSquirrel = nullptr;  // Pointer to squirrel currently holding egg
    float cameraY;  // Vertical camera offset
    float targetCameraY;  // Target position for smooth scrolling
    int currentEggSprite = 0;
} g_GameState;

// forward declarations
void RenderControls();
void RenderTimer();
void ResetTimer();
void SaveScore(Uint32 time);
void RenderWinMessage();
void LoadSquirrelSpriteDimensions(GameObject& squirrel);

SDL_Texture* LoadTexture(const char* path)
{    
    SDL_Surface* surface = IMG_Load(path);
    if (!surface)
    {
        printf("Failed to load image %s!\n", path);
        printf("SDL_image Error: %s\n", IMG_GetError());
        return nullptr;
    }


    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_Renderer, surface);
    if (!texture)
    {
        printf("Failed to create texture from %s!\n", path);
        printf("SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return nullptr;
    }

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

    if (TTF_Init() < 0)
    {
        printf("SDL_ttf initialization failed! SDL_ttf Error: %s\n", TTF_GetError());
        return false;
    }

    g_Font = TTF_OpenFont("assets/VCR_OSD_MONO_1.001.ttf", 24);
    if (!g_Font)
    {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
        return false;
    }

    g_StartTime = SDL_GetTicks();
    g_TimerActive = false;
    g_WinAchieved = false;

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
    g_ArrowTexture = LoadTexture("assets/arrow.png");

    if (!g_EggTexture || !g_SquirrelTexture || !g_TreeTexture || !g_BranchTexture || !g_ArrowTexture) return false;

    // Load egg sprites
    g_EggTextures[0] = LoadTexture("assets/egg/egg_closed_1.png");
    g_EggTextures[1] = LoadTexture("assets/egg/egg_closing_2.png");
    g_EggTextures[2] = LoadTexture("assets/egg/egg_open_3.png");

    // Check if all textures loaded successfully
    for (int i = 0; i < EGG_SPRITE_COUNT; i++)
    {
        if (!g_EggTextures[i])
        {
            printf("Failed to load egg sprite %d\n", i + 1);
            return false;
        }
    }

    // Load squirrel sprites
    g_SquirrelTextures[0] = LoadTexture("assets/squirrel/squirrel_without_egg_1.png");
    g_SquirrelTextures[1] = LoadTexture("assets/squirrel/squirrel_launch_no_egg_2.png");

    // Check if all textures loaded successfully
    for (int i = 0; i < SQUIRREL_SPRITE_COUNT; i++)
    {
        if (!g_SquirrelTextures[i])
        {
            printf("Failed to load squirrel sprite %d\n", i + 1);
            return false;
        }
    }

    return true;
}

int GetDefaultSquirrelWidth() {
    int width, height;
    SDL_QueryTexture(g_SquirrelTextures[0], nullptr, nullptr, &width, &height);
    return static_cast<int>(width * SQUIRREL_SCALE);
}

int GetDefaultSquirrelHeight() {
    int width, height;
    SDL_QueryTexture(g_SquirrelTextures[0], nullptr, nullptr, &width, &height);
    return static_cast<int>(height * SQUIRREL_SCALE);
}

void GenerateBranchesAndSquirrels()
{
    // Clear existing branches and squirrels
    g_GameState.branches.clear();
    g_GameState.squirrels.clear();

    // Set random seed based on time
    srand(static_cast<unsigned>(time(nullptr)));
        // Get default squirrel dimensions
    int defaultWidth = GetDefaultSquirrelWidth();
    int defaultHeight = GetDefaultSquirrelHeight();

    float currentHeight = TOTAL_GAME_HEIGHT - WINDOW_HEIGHT*0.3f;  // Start above floor squirrel
    bool isLeft = false;  // always start with branch on the right

    while (currentHeight > 0)  // Generate until we reach the top
    {
        // Random height spacing for this branch
        float spacing = MIN_BRANCH_SPACING + 
            static_cast<float>(rand()) / RAND_MAX * (MAX_BRANCH_SPACING - MIN_BRANCH_SPACING);
        
        // Random extension from tree
        float extension = MIN_BRANCH_EXTENSION + 
            static_cast<float>(rand()) / RAND_MAX * (MAX_BRANCH_EXTENSION - MIN_BRANCH_EXTENSION);

        // Calculate branch position
        float branchX = isLeft ? TREE_WIDTH : WINDOW_WIDTH - TREE_WIDTH - extension;
        
        // Add branch
        GameObject branch = {
            branchX,
            currentHeight,
            static_cast<int>(extension),
            BRANCH_HEIGHT,
            g_BranchTexture,
            isLeft
        };
        g_GameState.branches.push_back(branch);

        // Add squirrel (positioned at end of branch)
        GameObject squirrel = {
            isLeft ? branchX + extension - defaultWidth : branchX,
            currentHeight - defaultHeight,
            defaultWidth,
            defaultHeight,
            g_SquirrelTexture,
            isLeft
        };
        LoadSquirrelSpriteDimensions(squirrel);
        g_GameState.squirrels.push_back(squirrel);

        // Update for next iteration
        currentHeight -= spacing;
        isLeft = !isLeft;  // Alternate sides
    }

    printf("Generated %zu branches and squirrels\n", g_GameState.branches.size());
}

void InitGameObjects()
{
    // Setup trees
    g_GameState.leftTree = {
        0.0f,
        0.0f,
        TREE_WIDTH,
        static_cast<int>(TOTAL_GAME_HEIGHT),
        g_TreeTexture,
        true
    };

    g_GameState.rightTree = {
        static_cast<float>(WINDOW_WIDTH - TREE_WIDTH),
        0.0f,
        TREE_WIDTH,
        static_cast<int>(TOTAL_GAME_HEIGHT),
        g_TreeTexture,
        false
    };

    // Get default squirrel dimensions
    int defaultWidth = GetDefaultSquirrelWidth();
    int defaultHeight = GetDefaultSquirrelHeight();

    // Position floor squirrel at the bottom of the total height
    g_GameState.floorSquirrel = {
        static_cast<float>(WINDOW_WIDTH / 2 - defaultWidth / 2),
        TOTAL_GAME_HEIGHT - defaultHeight,
        defaultWidth,
        defaultHeight,
        g_SquirrelTexture,
        true
    };
    LoadSquirrelSpriteDimensions(g_GameState.floorSquirrel);

    // Generate branches and squirrels
    GenerateBranchesAndSquirrels();

    // Initialize egg position relative to floor squirrel
    g_GameState.egg = {
        g_GameState.floorSquirrel.x + (defaultWidth - EGG_SIZE_X) / 2,
        g_GameState.floorSquirrel.y - EGG_SIZE_Y,
        EGG_SIZE_X,
        EGG_SIZE_Y,
        g_EggTexture,
        false
    };

    g_GameState.eggVelocityY = 0.0f;
    g_GameState.eggIsHeld = false;
    g_GameState.strengthCharge = 0.0f;
    g_GameState.isCharging = false;
    g_GameState.isDepletingCharge = false;
    g_GameState.angleSquareY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
    g_GameState.angleSquareVelocity = 0.0f;
    g_GameState.eggVelocityX = 0.0f;
    g_GameState.isLaunchingRight = true;  // Default to right direction
    g_GameState.activeSquirrel = &g_GameState.floorSquirrel;  // Start with floor squirrel
    g_GameState.cameraY = 0.0f;
    g_GameState.targetCameraY = 0.0f;
}

void RenderGameObject(const GameObject& obj)
{
    // Use the current sprite's dimensions for squirrels
    int renderWidth = obj.width;
    int renderHeight = obj.height;
    
    if (&obj == &g_GameState.floorSquirrel || 
        (&obj >= &g_GameState.squirrels.front() && &obj <= &g_GameState.squirrels.back()))
    {
        renderWidth = obj.spriteWidths[obj.currentSprite];
        renderHeight = obj.spriteHeights[obj.currentSprite];
    }

    SDL_Rect destRect = {
        static_cast<int>(obj.x),
        static_cast<int>(obj.y - g_GameState.cameraY),
        renderWidth,
        renderHeight
    };

    if (&obj == &g_GameState.egg)
    {
        SDL_RenderCopy(g_Renderer, 
                      g_EggTextures[g_GameState.currentEggSprite], 
                      nullptr, 
                      &destRect);
    }
    else if (&obj == &g_GameState.floorSquirrel || 
             (&obj >= &g_GameState.squirrels.front() && &obj <= &g_GameState.squirrels.back()))
    {
        // Add flip based on isLeftSide
        SDL_RendererFlip flip = (obj.isLeftSide) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_RenderCopyEx(g_Renderer,
                        g_SquirrelTextures[obj.currentSprite],
                        nullptr,
                        &destRect,
                        0,      // no rotation
                        nullptr, // rotate around center
                        flip);  // flip horizontally if needed
    }
    else
    {
        SDL_Rect dest = {
            static_cast<int>(obj.x),
            static_cast<int>(obj.y - g_GameState.cameraY),  // Subtract camera offset
            obj.width,
            obj.height
        };
        
        if (dest.y + dest.h >= 0 && dest.y <= WINDOW_HEIGHT)
        {
            SDL_RendererFlip flip = (obj.texture == g_SquirrelTexture && !obj.isLeftSide) 
                ? SDL_FLIP_HORIZONTAL 
                : SDL_FLIP_NONE;
                
            SDL_RenderCopyEx(g_Renderer, obj.texture, nullptr, &dest, 0, nullptr, flip);
        }
    }
}

void RenderArrow()
{
    if (!g_GameState.eggIsHeld) return;

    // Calculate angle based on angle square position
    float normalizedY = (ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE - g_GameState.angleSquareY) 
                     / (ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE);
    float angle = -normalizedY * 90.0f;  // Convert to degrees (0 to 90), negative to point upward

    // If launching left, mirror the angle
    if (!g_GameState.isLaunchingRight) {
        angle = 180.0f - angle;
    }

    // Position arrow with its left edge at egg's center
    SDL_Rect arrowRect = {
        static_cast<int>(g_GameState.egg.x + g_GameState.egg.width/2),  // Start at egg's center
        static_cast<int>(g_GameState.egg.y + g_GameState.egg.height/2 - ARROW_HEIGHT/2 - g_GameState.cameraY),  // Vertically centered
        ARROW_WIDTH,
        ARROW_HEIGHT
    };

    // Create a point for rotation (at the left edge of the arrow)
    SDL_Point rotationPoint = {
        0,                    // Pivot at left edge (x=0)
        ARROW_HEIGHT/2        // Vertically centered
    };

    // Render the arrow with rotation around its left edge
    SDL_RenderCopyEx(
        g_Renderer,
        g_ArrowTexture,
        nullptr,
        &arrowRect,
        angle,         // Rotation angle in degrees
        &rotationPoint, // Rotate around left edge
        SDL_FLIP_NONE  // No flipping
    );
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

    // Render floor squirrel
    RenderGameObject(g_GameState.floorSquirrel);

    // Render egg
    RenderGameObject(g_GameState.egg);

    // Render arrow
    RenderArrow();

    // Render controls
    RenderControls();

    // Add before SDL_RenderPresent
    RenderTimer();
    RenderWinMessage();

    SDL_RenderPresent(g_Renderer);
}

void CleanUp()
{
    SDL_DestroyTexture(g_EggTexture);
    SDL_DestroyTexture(g_SquirrelTexture);
    SDL_DestroyTexture(g_TreeTexture);
    SDL_DestroyTexture(g_BranchTexture);
    SDL_DestroyTexture(g_ArrowTexture);
    SDL_DestroyRenderer(g_Renderer);
    SDL_DestroyWindow(g_Window);
    IMG_Quit();
    SDL_Quit();
    TTF_CloseFont(g_Font);
    TTF_Quit();

    // Cleanup egg textures
    for (int i = 0; i < EGG_SPRITE_COUNT; i++)
    {
        SDL_DestroyTexture(g_EggTextures[i]);
    }

    // Cleanup squirrel textures
    for (int i = 0; i < SQUIRREL_SPRITE_COUNT; i++)
    {
        SDL_DestroyTexture(g_SquirrelTextures[i]);
    }
}

bool CheckCollision(const SDL_Rect& a, const SDL_Rect& b)
{
    return (a.x < b.x + b.w &&
            a.x + a.w > b.x &&
            a.y < b.y + b.h &&
            a.y + a.h > b.y);
}

void HandleCollision(GameObject *squirrel)
{
    g_GameState.eggIsHeld = true;
    g_GameState.eggVelocityX = 0;
    g_GameState.eggVelocityY = 0;
    g_GameState.egg.y = squirrel->y - g_GameState.egg.height;
    g_GameState.egg.x = squirrel->x + 
        (squirrel->spriteWidths[squirrel->currentSprite] - g_GameState.egg.width) / 2;
    g_GameState.activeSquirrel = squirrel;
    g_GameState.isLaunchingRight = g_GameState.activeSquirrel->isLeftSide;
    
    // Reset control states
    g_GameState.strengthCharge = 0.0f;
    g_GameState.isCharging = false;
    g_GameState.isDepletingCharge = false;
    g_GameState.angleSquareY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
    g_GameState.angleSquareVelocity = 0.0f;

    
    g_GameState.currentEggSprite = 0;  // Reset animation
    g_EggAnimationTime = 0.0f;
    squirrel->currentSprite = 1;  // Change to launch sprite

}

void UpdatePhysics()
{
    if (!g_GameState.eggIsHeld)
    {
        // Apply gravity
        g_GameState.eggVelocityY += GRAVITY;
        if (g_GameState.eggVelocityY > TERMINAL_VELOCITY)
            g_GameState.eggVelocityY = TERMINAL_VELOCITY;

        // Calculate new position
        float newX = g_GameState.egg.x + g_GameState.eggVelocityX;
        float newY = g_GameState.egg.y + g_GameState.eggVelocityY;

        // printf("Physics Update - Pos: (%.2f, %.2f), New Pos: (%.2f, %.2f), Vel: (%.2f, %.2f)\n",
        //        g_GameState.egg.x, g_GameState.egg.y, 
        //        newX, newY,
        //        g_GameState.eggVelocityX, g_GameState.eggVelocityY);

        // Create egg collision rect at the new position
        SDL_Rect eggRect = {
            static_cast<int>(newX),
            static_cast<int>(newY),
            g_GameState.egg.width,
            g_GameState.egg.height
        };

        // Tree collisions
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

        // Handle tree collisions
        bool collided = false;
        if (CheckCollision(eggRect, leftTreeRect))
        {
            printf("Left Tree Collision\n");
            newX = g_GameState.leftTree.x + g_GameState.leftTree.width;
            g_GameState.eggVelocityX = fabs(g_GameState.eggVelocityX) * 0.5f;
            collided = true;
        }
        else if (CheckCollision(eggRect, rightTreeRect))
        {
            printf("Right Tree Collision\n");
            newX = g_GameState.rightTree.x - g_GameState.egg.width;
            g_GameState.eggVelocityX = -fabs(g_GameState.eggVelocityX) * 0.5f;
            collided = true;
        }

        if (collided)
        {
            g_GameState.eggVelocityY *= 0.5f; // Reduce vertical velocity on collision
            printf("Tree Collision - Adjusting Velocities: VelX=%.2f, VelY=%.2f\n",
                   g_GameState.eggVelocityX, g_GameState.eggVelocityY);
            g_GameState.eggVelocityY *= 0.5f;
        }

        // Finally update the actual position
        g_GameState.egg.x = newX;
        g_GameState.egg.y = newY;

        // Check collision with squirrels
        for (auto& squirrel : g_GameState.squirrels)
        {
            SDL_Rect squirrelRect = {
                static_cast<int>(squirrel.x),
                static_cast<int>(squirrel.y),
                squirrel.spriteWidths[squirrel.currentSprite],
                squirrel.spriteHeights[squirrel.currentSprite]
            };

            if (CheckCollision(eggRect, squirrelRect) && g_GameState.activeSquirrel != &squirrel)
            {
                HandleCollision(&squirrel);
                printf("Egg caught by squirrel!\n");
                break;
            }
        }

        // Reset if egg goes off screen (left, right, or bottom) or hits bottom
        if (g_GameState.egg.y > TOTAL_GAME_HEIGHT - EGG_SIZE_Y ||
            g_GameState.egg.x < -g_GameState.egg.width ||
            g_GameState.egg.x > WINDOW_WIDTH)
        {
            printf("Egg missed - giving to floor squirrel\n");
            HandleCollision(&g_GameState.floorSquirrel);

            // reset timer
            ResetTimer();
        }


        SDL_Rect floorSquirrelRect = {
            static_cast<int>(g_GameState.floorSquirrel.x),
            static_cast<int>(g_GameState.floorSquirrel.y),
            g_GameState.floorSquirrel.spriteWidths[g_GameState.floorSquirrel.currentSprite],
            g_GameState.floorSquirrel.spriteHeights[g_GameState.floorSquirrel.currentSprite]
        };

        if (CheckCollision(eggRect, floorSquirrelRect))
        {
            HandleCollision(&g_GameState.floorSquirrel);
            printf("Egg caught by floor squirrel!\n");
        }


        // Check if egg reached the top - win condition
        if (g_GameState.egg.y <= 0)
        {
            if (g_TimerActive)  // Only save score once
            {
                printf("win condition achieved\n");
                g_TimerActive = false;  // Stop the timer
                SaveScore(SDL_GetTicks() - g_StartTime);  // Save the score
                g_WinAchieved = true;
                // teleport to floor squirrel
                g_GameState.egg.x = g_GameState.floorSquirrel.x + 
                    (g_GameState.floorSquirrel.spriteWidths[g_GameState.floorSquirrel.currentSprite] - g_GameState.egg.width) / 2;
                g_GameState.egg.y = g_GameState.floorSquirrel.y - g_GameState.egg.height;
                g_GameState.eggVelocityX = 0;
                g_GameState.eggVelocityY = 0;
                g_GameState.eggIsHeld = true;
                g_GameState.activeSquirrel = &g_GameState.floorSquirrel;
            }
        }

    }
}

void UpdateControls()
{
    // Update strength bar
    if (g_GameState.isCharging && !g_GameState.isDepletingCharge)
    {
        g_GameState.strengthCharge += 0.01f;  // Adjust speed as needed
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
            // Instead of dropping the egg, restart the charge cycle
            g_GameState.strengthCharge = 0.0f;
            g_GameState.isDepletingCharge = false;  // Switch back to charging mode
            // Note: We keep isCharging true to continue the cycle
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

void LaunchEgg()
{
    if (g_GameState.activeSquirrel)
    {
        g_GameState.activeSquirrel->currentSprite = 0;  // Change back to normal sprite
    }
    // Calculate angle (0 at bottom, PI/2 at top)
    float normalizedY = (ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE - g_GameState.angleSquareY) 
                     / (ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE);
    float angle = normalizedY * PI / 2;  // Convert to radians (0 to PI/2)

    // Calculate launch power
    float power = LAUNCH_POWER_SCALE * g_GameState.strengthCharge;

    // Calculate velocities using trigonometry
    if (g_GameState.isLaunchingRight) {
        g_GameState.eggVelocityX = power * cos(angle);
    } else {
        g_GameState.eggVelocityX = -power * cos(angle);  // Negative for left direction
    }
    g_GameState.eggVelocityY = -power * sin(angle);

    // Release the egg
    g_GameState.eggIsHeld = false;
    g_GameState.isCharging = false;
    
    printf("Launch - Power: %.2f, Angle: %.2f degrees, Direction: %s, VelX: %.2f, VelY: %.2f\n",
           power, angle * 180 / PI, g_GameState.isLaunchingRight ? "Right" : "Left", 
           g_GameState.eggVelocityX, g_GameState.eggVelocityY);


    // if timer had not started yet, start it
    if (!g_TimerActive)
        {
            g_StartTime = SDL_GetTicks();
            g_TimerActive = true;
            g_WinAchieved = false;
        }
    //g_GameState.activeSquirrel = nullptr;  // Clear active squirrel
    g_GameState.currentEggSprite = 0;  // Reset to closed sprite when launching
}

void UpdateCamera()
{
    // Calculate target camera position (center egg vertically)
    float screenCenterY = WINDOW_HEIGHT / 2.0f;
    g_GameState.targetCameraY = g_GameState.egg.y - screenCenterY;

    // Clamp camera to game bounds
    g_GameState.targetCameraY = std::max(0.0f, g_GameState.targetCameraY);
    g_GameState.targetCameraY = std::min(g_GameState.targetCameraY, TOTAL_GAME_HEIGHT - WINDOW_HEIGHT);
    
    // Smooth camera movement (lerp)
    float smoothSpeed = 0.1f;
    g_GameState.cameraY += (g_GameState.targetCameraY - g_GameState.cameraY) * smoothSpeed;
}

void ResetLevel()
{
    GenerateBranchesAndSquirrels();
    
    // Reset egg to floor squirrel
    g_GameState.egg.x = g_GameState.floorSquirrel.x + 
        (g_GameState.floorSquirrel.spriteWidths[g_GameState.floorSquirrel.currentSprite] - g_GameState.egg.width) / 2;
    g_GameState.egg.y = g_GameState.floorSquirrel.y - g_GameState.egg.height;
    g_GameState.eggVelocityX = 0;
    g_GameState.eggVelocityY = 0;
    g_GameState.eggIsHeld = true;
    g_GameState.activeSquirrel = &g_GameState.floorSquirrel;
}

void StartStrengthCharge()
{
    if (g_GameState.eggIsHeld && !g_GameState.isCharging)
    {
        g_GameState.isCharging = true;
        g_GameState.isDepletingCharge = false;
        g_GameState.strengthCharge = 0.0f;
    }
}

void HitAngleSquare()
{
    // Jump the angle square with strength based on current charge
    // float jumpPower = ANGLE_JUMP_POWER * g_GameState.strengthCharge;
    float jumpPower = ANGLE_JUMP_POWER;
    g_GameState.angleSquareVelocity = -jumpPower;
}

void ResetTimer()
{
    g_StartTime = SDL_GetTicks();
    g_TimerActive = false;
}

void RenderTimer()
{
    if (!g_TimerActive) return;

    // Calculate elapsed time
    Uint32 currentTime = SDL_GetTicks();
    Uint32 elapsedTime = currentTime - g_StartTime;
    g_lastElapsedTime = elapsedTime;
    
    // Convert to minutes:seconds.milliseconds
    int minutes = (elapsedTime / 1000) / 60;
    int seconds = (elapsedTime / 1000) % 60;
    int milliseconds = elapsedTime % 1000;

    // Format time string
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << seconds << "."
       << std::setfill('0') << std::setw(3) << milliseconds;
    
    // Create surface
    SDL_Color textColor = {255, 255, 255, 255};  // White color
    SDL_Surface* surface = TTF_RenderText_Solid(g_Font, ss.str().c_str(), textColor);
    if (!surface) return;

    // Create texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_Renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;

    // Render timer
    SDL_Rect timerRect = {
        TIMER_X,
        TIMER_Y,
        surface->w,
        surface->h
    };
    SDL_RenderCopy(g_Renderer, texture, nullptr, &timerRect);
    SDL_DestroyTexture(texture);
}

void SaveScore(Uint32 time)
{
    FILE* file = fopen(SCORE_FILE, "a");  // Open in append mode
    if (file)
    {
        // Convert time to minutes:seconds.milliseconds format
        int minutes = (time / 1000) / 60;
        int seconds = (time / 1000) % 60;
        int milliseconds = time % 1000;
        
        fprintf(file, "%02d:%02d.%03d\n", minutes, seconds, milliseconds);
        fclose(file);
    }
}

void RenderWinMessage()
{
    if (g_TimerActive || !g_WinAchieved) return;  // Only show when game is won

    // Calculate final time
    int minutes = (g_lastElapsedTime / 1000) / 60;
    int seconds = (g_lastElapsedTime / 1000) % 60;
    int milliseconds = g_lastElapsedTime % 1000;

    // Format win message
    std::stringstream ss;
    ss << "Final Time: " << std::setfill('0') << std::setw(2) << minutes 
       << ":" << std::setfill('0') << std::setw(2) << seconds 
       << "." << std::setfill('0') << std::setw(3) << milliseconds;

    // Create surface
    SDL_Color textColor = {255, 255, 255, 255};  // White color
    SDL_Surface* surface = TTF_RenderText_Solid(g_Font, ss.str().c_str(), textColor);
    if (!surface) return;

    // Create texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_Renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;

    // Center the text
    SDL_Rect messageRect = {
        WIN_MESSAGE_X - surface->w / 2,  // Center horizontally
        WIN_MESSAGE_Y - surface->h / 2,  // Center vertically
        surface->w,
        surface->h
    };

    SDL_RenderCopy(g_Renderer, texture, nullptr, &messageRect);
    SDL_DestroyTexture(texture);
}

void UpdateEggAnimation(float deltaTime)
{
    // printf("deltaTime: %.4f, eggAnimationTime: %.4f, EGG_ANIMATION_SPEED: %.4f\n",
    //        deltaTime, g_EggAnimationTime, EGG_ANIMATION_SPEED);

    int oldSprite = g_GameState.currentEggSprite;
    
    if (!g_GameState.eggIsHeld)
    {
        // When flying/idle, use closed egg sprite
        g_GameState.currentEggSprite = 0;
        g_EggAnimationTime = 0.0f;
    }
    else
    {
        // When held, cycle through sprites
        g_EggAnimationTime += deltaTime;
        if (g_EggAnimationTime >= EGG_ANIMATION_SPEED)
        {
            g_EggAnimationTime = 0.0f;
            if (g_GameState.currentEggSprite!=2)
                g_GameState.currentEggSprite++; // clamps at the third animation
        }
    }

    if (oldSprite != g_GameState.currentEggSprite) {
        printf("Egg sprite changed from %d to %d (%s)\n", 
               oldSprite, 
               g_GameState.currentEggSprite,
               g_GameState.eggIsHeld ? "held" : "not held");
    }
}

// First, move the sprite dimension loading to a separate function
void LoadSquirrelSpriteDimensions(GameObject& squirrel) {
    for (int i = 0; i < SQUIRREL_SPRITE_COUNT; i++) {
        int width, height;
        SDL_QueryTexture(g_SquirrelTextures[i], nullptr, nullptr, &width, &height);
        squirrel.spriteWidths[i] = static_cast<int>(width * SQUIRREL_SCALE);
        squirrel.spriteHeights[i] = static_cast<int>(height * SQUIRREL_SCALE);
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

    Uint32 lastTime = SDL_GetTicks();

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
                            g_GameState.egg.x = squirrel.x + (squirrel.spriteWidths[squirrel.currentSprite] - g_GameState.egg.width) / 2;
                            g_GameState.egg.y = squirrel.y - g_GameState.egg.height - 50;  // 50 pixels above
                            g_GameState.eggVelocityY = 0;
                            g_GameState.eggIsHeld = false;
                        }
                        break;
                    case SDLK_a:
                        if (g_GameState.eggIsHeld && g_GameState.activeSquirrel) {
                            g_GameState.isLaunchingRight = false;
                            g_GameState.activeSquirrel->isLeftSide = false;  // Make active squirrel face left
                        }
                        break;
                    case SDLK_d:
                        if (g_GameState.eggIsHeld && g_GameState.activeSquirrel) {
                            g_GameState.isLaunchingRight = true;
                            g_GameState.activeSquirrel->isLeftSide = true;  // Make active squirrel face right
                        }
                        break;
                    case SDLK_k:
                        // note: keyboard keys events are sent continuously
                        if (g_GameState.eggIsHeld)
                        {
                            StartStrengthCharge();
                        }

                        break;
                    case SDLK_l:
                        if (g_GameState.eggIsHeld)
                        {
                            HitAngleSquare();
                        }
                        break;
                }
            }
            else if (e.type == SDL_KEYUP)
            {
                if (e.key.keysym.sym == SDLK_k && g_GameState.eggIsHeld && g_GameState.isCharging)
                {
                    LaunchEgg();
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                if (e.button.button == SDL_BUTTON_LEFT && g_GameState.eggIsHeld)
                {
                    StartStrengthCharge();
                }
                else if (e.button.button == SDL_BUTTON_RIGHT && g_GameState.eggIsHeld)
                {
                    HitAngleSquare();
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP)
            {
                if (e.button.button == SDL_BUTTON_LEFT && g_GameState.eggIsHeld)
                {
                    LaunchEgg();
                }
            }
        }

        // used for sprite cycling
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        UpdatePhysics();
        UpdateControls();
        UpdateCamera();  // Add camera update
        UpdateEggAnimation(deltaTime);  // Add this line
        Render();

        // SDL_Delay(50);  // Sleep for x ms
    }

    CleanUp();
    return 0;
}