#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <vector>
#include <cmath>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TREE_WIDTH 100
#define BRANCH_SPACING 150  // Vertical space between branches
#define SQUIRREL_SIZE 60
#define EGG_SIZE 30
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
#define ANGLE_GRAVITY 0.3f
#define ANGLE_JUMP_POWER 8.0f
#define MAX_LAUNCH_POWER 20.0f   // Maximum launch velocity
#define PI 3.14159265359f

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

struct GameObject {
    float x, y;
    int width, height;
    SDL_Texture* texture;
    bool isLeftSide;  // Used for squirrels to determine which side they're on
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
    GameObject* activeSquirrel;  // Pointer to squirrel currently holding egg
    float cameraY;  // Vertical camera offset
    float targetCameraY;  // Target position for smooth scrolling
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

    // Position floor squirrel at the bottom of the total height
    g_GameState.floorSquirrel = {
        static_cast<float>(WINDOW_WIDTH / 2 - SQUIRREL_SIZE / 2),
        TOTAL_GAME_HEIGHT - SQUIRREL_SIZE,  // Position at bottom of total height
        SQUIRREL_SIZE,
        SQUIRREL_SIZE,
        g_SquirrelTexture,
        true
    };

    // Start distributing branches from bottom to top, but above floor squirrel
    float branchSpacing = TOTAL_GAME_HEIGHT / (TOTAL_BRANCHES + 1);  // +1 to leave space at bottom
    
    for (int i = 0; i < TOTAL_BRANCHES; i++)
    {
        bool isLeft = i % 2 == 0;
        
        // Calculate Y position starting from above floor squirrel
        float branchY = TOTAL_GAME_HEIGHT - ((i + 2) * branchSpacing);  // +2 to start above floor
        
        // Add branch
        GameObject branch = {
            static_cast<float>(isLeft ? TREE_WIDTH : WINDOW_WIDTH - TREE_WIDTH - TREE_WIDTH*2),
            branchY,
            TREE_WIDTH*2,
            30,  // Branch height
            g_BranchTexture,
            isLeft
        };
        g_GameState.branches.push_back(branch);

        // Add squirrel
        GameObject squirrel = {
            static_cast<float>(isLeft ? TREE_WIDTH + TREE_WIDTH*2 - SQUIRREL_SIZE : WINDOW_WIDTH - TREE_WIDTH - TREE_WIDTH*2),
            branchY - SQUIRREL_SIZE,
            SQUIRREL_SIZE,
            SQUIRREL_SIZE,
            g_SquirrelTexture,
            !isLeft
        };
        g_GameState.squirrels.push_back(squirrel);
    }

    // Initial egg position should be with floor squirrel
    g_GameState.egg = {
        g_GameState.floorSquirrel.x + (g_GameState.floorSquirrel.width - EGG_SIZE) / 2,
        EGG_SIZE,
        EGG_SIZE,
        EGG_SIZE,
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
    SDL_Rect dest = {
        static_cast<int>(obj.x),
        static_cast<int>(obj.y - g_GameState.cameraY),  // Subtract camera offset
        obj.width,
        obj.height
    };
    
    // Only render if object is visible on screen
    if (dest.y + dest.h >= 0 && dest.y <= WINDOW_HEIGHT)
    {
        SDL_RendererFlip flip = (obj.texture == g_SquirrelTexture && !obj.isLeftSide) 
            ? SDL_FLIP_HORIZONTAL 
            : SDL_FLIP_NONE;
            
        SDL_RenderCopyEx(g_Renderer, obj.texture, nullptr, &dest, 0, nullptr, flip);
    }
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
        if (g_GameState.eggVelocityY > TERMINAL_VELOCITY)
            g_GameState.eggVelocityY = TERMINAL_VELOCITY;

        // Calculate new position
        float newX = g_GameState.egg.x + g_GameState.eggVelocityX;
        float newY = g_GameState.egg.y + g_GameState.eggVelocityY;

        printf("Physics Update - Pos: (%.2f, %.2f), New Pos: (%.2f, %.2f), Vel: (%.2f, %.2f)\n",
               g_GameState.egg.x, g_GameState.egg.y, 
               newX, newY,
               g_GameState.eggVelocityX, g_GameState.eggVelocityY);

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
                squirrel.width,
                squirrel.height
            };

            if (CheckCollision(eggRect, squirrelRect))
            {
                g_GameState.eggIsHeld = true;
                g_GameState.eggVelocityX = 0;
                g_GameState.eggVelocityY = 0;
                g_GameState.egg.y = squirrel.y - g_GameState.egg.height;
                g_GameState.egg.x = squirrel.x + (squirrel.width - g_GameState.egg.width) / 2;
                g_GameState.activeSquirrel = &squirrel;
                g_GameState.isLaunchingRight = g_GameState.activeSquirrel->isLeftSide;  // Sync launch direction
                
                // Reset control states when caught
                g_GameState.strengthCharge = 0.0f;
                g_GameState.isCharging = false;
                g_GameState.isDepletingCharge = false;
                g_GameState.angleSquareY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
                g_GameState.angleSquareVelocity = 0.0f;
                
                printf("Egg caught by squirrel!\n");
                break;
            }
        }

        // Reset if egg goes off screen (left, right, or bottom) or hits bottom
        if (g_GameState.egg.y > TOTAL_GAME_HEIGHT - EGG_SIZE ||
            g_GameState.egg.x < -g_GameState.egg.width ||
            g_GameState.egg.x > WINDOW_WIDTH)
        {
            printf("Egg missed - giving to floor squirrel\n");
            
            // Position egg with floor squirrel
            g_GameState.egg.x = g_GameState.floorSquirrel.x + 
                (g_GameState.floorSquirrel.width - g_GameState.egg.width) / 2;
            g_GameState.egg.y = g_GameState.floorSquirrel.y - g_GameState.egg.height;
            
            g_GameState.eggVelocityX = 0;
            g_GameState.eggVelocityY = 0;
            g_GameState.eggIsHeld = true;
            
            // Reset control states
            g_GameState.strengthCharge = 0.0f;
            g_GameState.isCharging = false;
            g_GameState.isDepletingCharge = false;
            g_GameState.angleSquareY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
            g_GameState.angleSquareVelocity = 0.0f;
        }


        SDL_Rect floorSquirrelRect = {
            static_cast<int>(g_GameState.floorSquirrel.x),
            static_cast<int>(g_GameState.floorSquirrel.y),
            g_GameState.floorSquirrel.width,
            g_GameState.floorSquirrel.height
        };

        if (CheckCollision(eggRect, floorSquirrelRect))
        {
            g_GameState.eggIsHeld = true;
            g_GameState.eggVelocityX = 0;
            g_GameState.eggVelocityY = 0;
            g_GameState.egg.y = g_GameState.floorSquirrel.y - g_GameState.egg.height;
            g_GameState.egg.x = g_GameState.floorSquirrel.x + 
                (g_GameState.floorSquirrel.width - g_GameState.egg.width) / 2;
            g_GameState.activeSquirrel = &g_GameState.floorSquirrel;
            g_GameState.isLaunchingRight = g_GameState.activeSquirrel->isLeftSide;  // Sync launch direction
            
            // Reset control states
            g_GameState.strengthCharge = 0.0f;
            g_GameState.isCharging = false;
            g_GameState.isDepletingCharge = false;
            g_GameState.angleSquareY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
            g_GameState.angleSquareVelocity = 0.0f;
            
            printf("Egg caught by floor squirrel!\n");
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

void LaunchEgg()
{
    // Calculate angle (0 at bottom, PI/2 at top)
    float normalizedY = (ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE - g_GameState.angleSquareY) 
                     / (ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE);
    float angle = normalizedY * PI / 2;  // Convert to radians (0 to PI/2)

    // Calculate launch power
    float power = MAX_LAUNCH_POWER * g_GameState.strengthCharge;

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
                    LaunchEgg();
                }
            }
        }

        UpdatePhysics();
        UpdateControls();
        UpdateCamera();  // Add camera update
        Render();

        // SDL_Delay(100);  // Sleep for 100ms (0.1 seconds)
    }

    CleanUp();
    return 0;
}