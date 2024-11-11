#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <ctime>
#include <SDL_ttf.h>
#include <sstream>
#include <iomanip>
#include <SDL_mixer.h>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif


#define TARGET_FPS 60
#define FRAME_TIME (1000.0f / TARGET_FPS)

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TREE_WIDTH 75
#define BRANCH_SPACING 150  // Vertical space between branches
#define SQUIRREL_SCALE 0.6f  // Adjust this value to scale sprites (1.0f = original size, 0.5f = half size, 2.0f = double size)
#define EGG_SIZE_SCALE 1.0f
#define EGG_SIZE_X (int)(50*EGG_SIZE_SCALE)
#define EGG_SIZE_Y (int)(75*EGG_SIZE_SCALE)
#define EGG_ANIMATION_SPEED 0.7f  // Adjust speed as needed, measured in seconds
#define EGG_SPRITE_COUNT 3
#define GRAVITY 0.5f
#define TERMINAL_VELOCITY 30.0f
#define STRENGTH_BAR_WIDTH 10
#define STRENGTH_BAR_HEIGHT 75
#define STRENGTH_BAR_X 20
#define STRENGTH_BAR_Y 550
#define STRENGTH_CHARGE_RATE 0.03f
#define ANGLE_BAR_WIDTH 20
#define ANGLE_BAR_HEIGHT 200
#define ANGLE_BAR_X 240
#define ANGLE_BAR_Y 370
#define ANGLE_SQUARE_SIZE 15
#define ANGLE_GRAVITY 0.5f
#define ANGLE_JUMP_POWER 6.0f
#define LAUNCH_POWER_SCALE 20.0f
#define PI 3.14159265359f

#define MIN_BRANCH_SPACING (WINDOW_HEIGHT * 0.1f)  // Minimum vertical gap between branches
#define MAX_BRANCH_SPACING (WINDOW_HEIGHT * 0.5f)  // Maximum vertical gap between branches
#define MIN_BRANCH_EXTENSION 219.0f   // Minimum distance branch extends from tree
#define MAX_BRANCH_EXTENSION 350.0f  // Maximum distance branch extends from tree
#define BRANCH_HEIGHT 200             // Height of branch texture

#define NUM_BRANCH_TYPES 3
#define POSITIONS_PER_BRANCH 2

#define ARROW_WIDTH 40
#define ARROW_HEIGHT 15

#define TIMER_FONT_SIZE 32     // Base font size before scaling
#define WIN_MESSAGE_FONT_SIZE 32

#define INSTRUCTION_FONT_SIZE 20
#define INSTRUCTION_Y 50  // Adjust this value to position the text where you want

const int TOTAL_HEIGHT_IN_SCREENS = 10;
const float TOTAL_GAME_HEIGHT = WINDOW_HEIGHT * TOTAL_HEIGHT_IN_SCREENS;


const int NEST_SIZE = 64;
const SDL_Color NEST_COLOR = {34, 139, 34, 255};  // Forest green

#define TIMER_X (WINDOW_WIDTH - 200)
#define TIMER_Y 20

#define SCORE_FILE "assets/scores.txt"
#define WIN_MESSAGE_X (WINDOW_WIDTH / 2)
#define WIN_MESSAGE_Y (WINDOW_HEIGHT / 2)

#define NUM_LAUNCH_SOUNDS 4

// #define printf if(0) printf

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



SDL_Texture* g_EggTextures[EGG_SPRITE_COUNT] = {nullptr};
float g_EggAnimationTime = 0.0f;


enum {
    SPRITE_SQUIRREL_WITHOUT_EGG_0,
    SPRITE_SQUIRREL_WITH_EGG_1,
    SPRITE_SQUIRREL_TO_LAUNCH_2,
    SPRITE_SQUIRREL_TO_LAUNCH_3,
    SPRITE_SQUIRREL_TO_LAUNCH_4,
    SPRITE_SQUIRREL_MAX_VALUE // SHOULD ALWAYS BE THE LAST
};

SDL_Texture* g_SquirrelTextures[SPRITE_SQUIRREL_MAX_VALUE] = {nullptr};

// Add to global variables section
SDL_Texture* g_BackgroundBase = nullptr;
SDL_Texture* g_BackgroundModular = nullptr;
SDL_Texture* g_BackgroundTop = nullptr;
SDL_Texture* g_BranchTextures[NUM_BRANCH_TYPES] = {nullptr};

Mix_Chunk* g_CrunchSound = nullptr;
Mix_Chunk* g_WinSound = nullptr;
Mix_Music* g_BackgroundMusic = nullptr;
Mix_Chunk* g_LaunchSounds[NUM_LAUNCH_SOUNDS] = {nullptr};

struct GameObject {
    float x, y;
    int width, height;
    SDL_Texture* texture;
    bool isLeftSide;  // Used for squirrels to determine which side they're on
    int currentSprite = 0;
    int spriteWidths[SPRITE_SQUIRREL_MAX_VALUE] = {0};
    int spriteHeights[SPRITE_SQUIRREL_MAX_VALUE] = {0};
    int branchType;      // For branches: which type of branch (0-2)
    int positionIndex;   // For squirrels: which position on the branch (0-1)
    float animationTimer;  // Track animation duration, measured in seconds
    bool hasEgg;          // Track if squirrel has egg
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
    GameObject nest;
    bool isInNest;  // New flag to track if egg is in starting position
    bool isFirstFall = 1;
} g_GameState;

// Define the relative positions for squirrels on each branch type
struct BranchPosition {
    float x;  // Relative X position from branch start
    float y;  // Relative Y position from branch top
};

// Array to store possible positions for each branch type
// You'll need to fill these values based on your branch PNGs
BranchPosition g_BranchPositions[NUM_BRANCH_TYPES][POSITIONS_PER_BRANCH] = {
    // Branch Type 1 positions
    {
        {.0f, 40.0f},    // Fill with actual values for position 1
        {100.0f, 45.0f}     // Fill with actual values for position 2
    },
    // Branch Type 2 positions
    {
        {40.0f, 40.0f},    // Fill with actual values for position 1
        {140.0f, 50.0f}     // Fill with actual values for position 2
    },
    // Branch Type 3 positions
    {
        {60.0f, 30.0f},    // Fill with actual values for position 1
        {80.0f, 70.0f}     // Fill with actual values for position 2
    }
};


// forward declarations
void RenderControls();
void RenderTimer();
void ResetTimer();
void SaveScore(Uint32 time);
void RenderWinMessage();
void LoadSquirrelSpriteDimensions(GameObject& squirrel);
void RenderText(const char* text, int x, int y, int fontSize);
void RenderInstructions();
void RenderLastScores();

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

    g_Font = TTF_OpenFont("assets/VCR_OSD_MONO_1.001.ttf", TIMER_FONT_SIZE);
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
    g_SquirrelTextures[1] = LoadTexture("assets/squirrel/sprite_esquilo-holding_egg.png");
    g_SquirrelTextures[2] = LoadTexture("assets/squirrel/sprite_esquilo-launch_1.png");
    g_SquirrelTextures[3] = LoadTexture("assets/squirrel/sprite_esquilo-launch_2.png");
    g_SquirrelTextures[4] = LoadTexture("assets/squirrel/sprite_esquilo-launch_3.png");

    // Check if all textures loaded successfully
    for (int i = 0; i < SPRITE_SQUIRREL_MAX_VALUE; i++)
    {
        if (!g_SquirrelTextures[i])
        {
            printf("Failed to load squirrel sprite %d\n", i + 1);
            return false;
        }
    }

    // Load background textures
    g_BackgroundBase = LoadTexture("assets/background/bg_base_1.png");
    g_BackgroundModular = LoadTexture("assets/background/bg_modular_2.png");
    g_BackgroundTop = LoadTexture("assets/background/bg_top_3.png");

    if (!g_BackgroundBase || !g_BackgroundModular || !g_BackgroundTop) {
        printf("Failed to load background textures!\n");
        return false;
    }

    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 8, 2048) < 0)
    {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        return false;
    }

    // Load sound effect
    g_CrunchSound = Mix_LoadWAV("assets/audio/82318-iedlabs-cruch-eggshells-medium.mp3");
    if (g_CrunchSound == nullptr)
    {
        printf("Failed to load crunch sound effect! SDL_mixer Error: %s\n", Mix_GetError());
        return false;
    }

    g_WinSound = Mix_LoadWAV("assets/audio/242501__gabrielaraujo__powerupsuccess.wav");
    if (g_WinSound == nullptr)
    {
        printf("Failed to load g_WinSound sound effect! SDL_mixer Error: %s\n", Mix_GetError());
        return false;
    }

    // Load launch sounds
    const char* launchSoundPaths[NUM_LAUNCH_SOUNDS] = {
        "assets/audio/zapsplat_animals_bird_ringneck_parakeet_says_what_you_doing_109613.mp3",
        "assets/audio/zapsplat_animals_bird_ringneck_parakeet_single_excited_chirp_squeak_109616.mp3",
        "assets/audio/zapsplat_animals_budgies_chirping_happy_001_75627.mp3",
        "assets/audio/zapsplat_animals_budgies_chirping_happy_005_75538.mp3"
    };

    for (int i = 0; i < NUM_LAUNCH_SOUNDS; i++)
    {
        g_LaunchSounds[i] = Mix_LoadWAV(launchSoundPaths[i]);
        if (g_LaunchSounds[i] == nullptr)
        {
            printf("Failed to load launch sound %d! SDL_mixer Error: %s\n", i, Mix_GetError());
            return false;
        }
    }

    g_BackgroundMusic = Mix_LoadMUS("assets/audio/music.mpeg");
    if (g_BackgroundMusic == nullptr)
    {
        printf("Failed to load background music! SDL_mixer Error: %s\n", Mix_GetError());
        return false;
    }

    // Start playing the music and loop indefinitely (-1)
    if (Mix_PlayMusic(g_BackgroundMusic, -1) == -1)
    {
        printf("Failed to play background music! SDL_mixer Error: %s\n", Mix_GetError());
        return false;
    }

    // Optionally adjust music volume (0-128)
    Mix_VolumeMusic(MIX_MAX_VOLUME / 6);  // 50% volume - adjust as needed

    // Load branch textures
    g_BranchTextures[0] = LoadTexture("assets/branch/branch1.png");
    g_BranchTextures[1] = LoadTexture("assets/branch/branch2.png");
    g_BranchTextures[2] = LoadTexture("assets/branch/branch3.png");

    // Check if all branch textures loaded successfully
    for (int i = 0; i < NUM_BRANCH_TYPES; i++) {
        if (!g_BranchTextures[i]) {
            printf("Failed to load branch texture %d\n", i + 1);
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
    srand(2); //static_cast<unsigned>(time(nullptr)));
        // Get default squirrel dimensions
    int defaultWidth = GetDefaultSquirrelWidth();
    int defaultHeight = GetDefaultSquirrelHeight();

    float currentHeight = TOTAL_GAME_HEIGHT - WINDOW_HEIGHT*0.5f;  // Start above floor squirrel
    bool isLeft = false;  // always start with branch on the right

    while (currentHeight > 100)  // Generate until we reach the top
    {
        // Random height spacing for this branch
        float spacing = MIN_BRANCH_SPACING + 
            static_cast<float>((float)rand()) / (float)RAND_MAX * (MAX_BRANCH_SPACING - MIN_BRANCH_SPACING);
        
        // Random extension from tree
        float extension = MIN_BRANCH_EXTENSION + 
            static_cast<float>((float)rand()) / (float)RAND_MAX * (MAX_BRANCH_EXTENSION - MIN_BRANCH_EXTENSION);

        // Calculate branch position
        float branchX = !isLeft ? TREE_WIDTH : WINDOW_WIDTH - TREE_WIDTH - extension;
        
        // Randomly select branch type and position
        int branchType = rand() % NUM_BRANCH_TYPES;
        int positionIndex = rand() % POSITIONS_PER_BRANCH;

        // Add branch
        GameObject branch = {
            branchX,
            currentHeight,
            static_cast<int>(extension),
            BRANCH_HEIGHT,
            g_BranchTextures[branchType],
            isLeft,
            branchType  // Add branch type
        };
        g_GameState.branches.push_back(branch);

        // Calculate squirrel position based on branch type and chosen position
        float squirrelX = branchX + g_BranchPositions[branchType][positionIndex].x;
        float squirrelY = currentHeight + g_BranchPositions[branchType][positionIndex].y;

        if (!isLeft) {
            // Adjust X position for right-side branches
            squirrelX = branchX + extension - defaultWidth - g_BranchPositions[branchType][positionIndex].x;
        }

        // Add squirrel
        GameObject squirrel = {
            squirrelX,
            squirrelY,
            defaultWidth,
            defaultHeight,
            g_SquirrelTexture,
            !isLeft,
            SPRITE_SQUIRREL_WITHOUT_EGG_0,  // currentSprite
            {0},  // spriteWidths
            {0},  // spriteHeights
            -1,   // branchType (not used for squirrels)
            positionIndex,
            0.0f,           // animationTimer
            false           // hasEgg
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

    // Position floor squirrel at the bottom of the total height plus an offset
    g_GameState.floorSquirrel = {
        static_cast<float>(WINDOW_WIDTH / 2 - defaultWidth / 2) - 40,
        TOTAL_GAME_HEIGHT - defaultHeight - 60,
        defaultWidth,
        defaultHeight,
        g_SquirrelTexture,
        true
    };
    LoadSquirrelSpriteDimensions(g_GameState.floorSquirrel);

    // Generate branches and squirrels
    GenerateBranchesAndSquirrels();


    // Initialize nest position at the top-middle of the screen
    g_GameState.nest.x = (WINDOW_WIDTH - NEST_SIZE) / 2;
    g_GameState.nest.y = 74;
    g_GameState.nest.width = NEST_SIZE;
    g_GameState.nest.height = NEST_SIZE;

    // Initialize egg position on nest
    g_GameState.egg = {
        g_GameState.nest.x + (NEST_SIZE - EGG_SIZE_X) / 2,  // Center egg on nest
        g_GameState.nest.y, 
        EGG_SIZE_X,
        EGG_SIZE_Y,
        g_EggTexture,
        false
    };

    g_GameState.eggVelocityY = 0.0f;
    g_GameState.eggVelocityX = 0.0f;
    g_GameState.isInNest = true;  // Start in nest

    g_GameState.strengthCharge = 0.0f;
    g_GameState.isCharging = false;
    g_GameState.isDepletingCharge = false;
    g_GameState.angleSquareY = ANGLE_BAR_Y + ANGLE_BAR_HEIGHT - ANGLE_SQUARE_SIZE;
    g_GameState.angleSquareVelocity = 0.0f;
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
        // printf("Rendering egg at x:%d y:%d w:%d h:%d (sprite:%d)\n", 
        //        destRect.x, 
        //        destRect.y, 
        //        destRect.w, 
        //        destRect.h,
        //        g_GameState.currentEggSprite);

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
    else if (&obj >= &g_GameState.branches.front() && &obj <= &g_GameState.branches.back()) {
        // It's a branch, use the appropriate texture
        SDL_RendererFlip flip = (obj.isLeftSide) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_RenderCopyEx(g_Renderer, g_BranchTextures[obj.branchType], nullptr, &destRect, 0, nullptr, flip);
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
    if (!g_GameState.eggIsHeld || g_GameState.isInNest ) return;

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

void RenderBackground()
{
    // Calculate how many screens are visible based on camera position
    int startScreen = static_cast<int>(g_GameState.cameraY / WINDOW_HEIGHT);
    int endScreen = static_cast<int>((g_GameState.cameraY + WINDOW_HEIGHT) / WINDOW_HEIGHT) + 1;

    // Clamp to valid range
    startScreen = std::max(0, startScreen);
    endScreen = std::min(TOTAL_HEIGHT_IN_SCREENS - 1, endScreen);

    for (int screen = startScreen; screen <= endScreen; screen++)
    {
        // screen 0 is the topmost screen, endScreen is the bottom screen
        SDL_Rect destRect = {
            0,
            screen * WINDOW_HEIGHT - static_cast<int>(g_GameState.cameraY),
            WINDOW_WIDTH,
            WINDOW_HEIGHT
        };

        // Choose which background to render based on screen position
        SDL_Texture* bgTexture;
        if (screen == 0) {
            // First screen uses base background
            bgTexture = g_BackgroundTop;
        }
        else if (screen == TOTAL_HEIGHT_IN_SCREENS - 1) {
            // Last screen uses top background
            bgTexture = g_BackgroundBase;
        }
        else {
            // All other screens use modular background
            bgTexture = g_BackgroundModular;
        }

        SDL_RenderCopy(g_Renderer, bgTexture, nullptr, &destRect);
    }
}

void RenderNest()
{
    if (0) // for debug
    {
        SDL_SetRenderDrawColor(g_Renderer, NEST_COLOR.r, NEST_COLOR.g, NEST_COLOR.b, NEST_COLOR.a);
        SDL_Rect nestRect = {
            static_cast<int>(g_GameState.nest.x),
            static_cast<int>(g_GameState.nest.y - g_GameState.cameraY), // Account for camera position
            g_GameState.nest.width,
            g_GameState.nest.height};
        SDL_RenderFillRect(g_Renderer, &nestRect);
    }
}

void Render()
{
    // Clear with a color (can be kept as fallback)
    SDL_SetRenderDrawColor(g_Renderer, 135, 206, 235, 255);  // Sky blue background
    SDL_RenderClear(g_Renderer);

    // Render background first
    RenderBackground();

    // Render trees - currently not drawing, used only for debug
   // RenderGameObject(g_GameState.leftTree);
    //RenderGameObject(g_GameState.rightTree);

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

    // Render the nest
    RenderNest();

    // Render floor squirrel
    RenderGameObject(g_GameState.floorSquirrel);

    // Render egg
    RenderGameObject(g_GameState.egg);

    // Render arrow
    RenderArrow();

    // Render controls
    RenderControls();

    // Render instructions
    RenderInstructions();

    RenderTimer();
    
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
    for (int i = 0; i < SPRITE_SQUIRREL_MAX_VALUE; i++)
    {
        SDL_DestroyTexture(g_SquirrelTextures[i]);
    }

    SDL_DestroyTexture(g_BackgroundBase);
    SDL_DestroyTexture(g_BackgroundModular);
    SDL_DestroyTexture(g_BackgroundTop);

    if (g_CrunchSound != nullptr)
    {
        Mix_FreeChunk(g_CrunchSound);
        g_CrunchSound = nullptr;
    }

    if (g_WinSound != nullptr)
    {
        Mix_FreeChunk(g_WinSound);
        g_WinSound = nullptr;
    }
    
    for (int i = 0; i < NUM_LAUNCH_SOUNDS; i++)
    {
        if (g_LaunchSounds[i] != nullptr)
        {
            Mix_FreeChunk(g_LaunchSounds[i]);
            g_LaunchSounds[i] = nullptr;
        }
    }
    
    Mix_Quit();

    for (int i = 0; i < NUM_BRANCH_TYPES; i++) {
        SDL_DestroyTexture(g_BranchTextures[i]);
    }
}

void PlayRandomLaunchSound()
{
    int randomIndex = rand() % NUM_LAUNCH_SOUNDS;
    Mix_PlayChannel(-1, g_LaunchSounds[randomIndex], 0);
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
    // hacks for adjusting the egg on the tail
    int offset_y=-110, offset_x = 50;
    if (squirrel->isLeftSide)        { offset_y=-110; offset_x=0;}
    if (squirrel == &g_GameState.floorSquirrel) {offset_y=-110;offset_x=0;}

    g_GameState.eggIsHeld = true;
    g_GameState.eggVelocityX = 0;
    g_GameState.eggVelocityY = 0;
    g_GameState.egg.y = squirrel->y + g_GameState.egg.height + offset_y;
    g_GameState.egg.x = squirrel->x  + offset_x +
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

    squirrel->hasEgg = true;
    squirrel->currentSprite = SPRITE_SQUIRREL_WITH_EGG_1;     // Switch to catching animation
    squirrel->animationTimer = 0.5f; // Set animation duration to 0.5 seconds
    // g_GameState.egg.width = 0;      // Make egg invisible
    // g_GameState.egg.height = 0;
}

void UpdatePhysics(float deltaTime)
{
    // Skip physics if egg is still in nest
    if (g_GameState.isInNest) {
        return;
    }

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
            Mix_PlayChannel(-1, g_CrunchSound, 0);
        }
        else if (CheckCollision(eggRect, rightTreeRect))
        {
            printf("Right Tree Collision\n");
            newX = g_GameState.rightTree.x - g_GameState.egg.width;
            g_GameState.eggVelocityX = -fabs(g_GameState.eggVelocityX) * 0.5f;
            collided = true;
            Mix_PlayChannel(-1, g_CrunchSound, 0);
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

            if (CheckCollision(eggRect, squirrelRect) && 
                g_GameState.activeSquirrel != &squirrel &&
                !g_GameState.isFirstFall) // does not check collision on first fall
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
            Mix_PlayChannel(-1, g_CrunchSound, 0);

            // reset timer
            ResetTimer();
            g_GameState.isFirstFall = 0; // clears after falling the first time
        }


        SDL_Rect floorSquirrelRect = {
            static_cast<int>(g_GameState.floorSquirrel.x),
            static_cast<int>(g_GameState.floorSquirrel.y),
            g_GameState.floorSquirrel.spriteWidths[g_GameState.floorSquirrel.currentSprite],
            g_GameState.floorSquirrel.spriteHeights[g_GameState.floorSquirrel.currentSprite]
        };

        if (CheckCollision(eggRect, floorSquirrelRect) && g_GameState.activeSquirrel != &g_GameState.floorSquirrel)
        {
            HandleCollision(&g_GameState.floorSquirrel);
            printf("Egg caught by floor squirrel!\n");

            g_GameState.floorSquirrel.currentSprite = 2;
            g_GameState.egg.width = EGG_SIZE_X; // Make egg visible again
            g_GameState.egg.height = EGG_SIZE_Y;
            g_GameState.floorSquirrel.animationTimer = 0.0;
        }

        SDL_Rect nestRect = {
            static_cast<int>(g_GameState.nest.x),
            static_cast<int>(g_GameState.nest.y),
            g_GameState.nest.width,
            g_GameState.nest.height
        };

        // Check if egg reached the top - win condition
        if (CheckCollision(eggRect, nestRect) )// g_GameState.egg.y <= 0)
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

                Mix_PlayChannel(-1, g_WinSound, 0);
            }
        }

    }
}

void UpdateControls()
{
    // Update strength bar
    if (g_GameState.isCharging && !g_GameState.isDepletingCharge)
    {
        g_GameState.strengthCharge += STRENGTH_CHARGE_RATE;  // Adjust speed as needed
        if (g_GameState.strengthCharge >= 1.0f)
        {
            g_GameState.strengthCharge = 1.0f;
            g_GameState.isDepletingCharge = true;
        }
    }
    else if (g_GameState.isDepletingCharge)
    {
        g_GameState.strengthCharge -= STRENGTH_CHARGE_RATE;  // Adjust speed as needed
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
        // Calculate strength bar position relative to egg
        int x_offset = g_GameState.activeSquirrel->isLeftSide ? -10 : EGG_SIZE_X + 20;
        int strengthBarX = static_cast<int>(g_GameState.egg.x) - STRENGTH_BAR_WIDTH + x_offset; // 10 pixels gap
        int strengthBarY = static_cast<int>(g_GameState.egg.y) - g_GameState.cameraY - STRENGTH_BAR_HEIGHT/2 + g_GameState.egg.height/2;

        // Draw strength bar background
        SDL_Rect strengthBarBg = {
            strengthBarX,
            strengthBarY,
            STRENGTH_BAR_WIDTH,
            STRENGTH_BAR_HEIGHT
        };
        SDL_SetRenderDrawColor(g_Renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(g_Renderer, &strengthBarBg);

        // Draw strength bar fill (from bottom to top)
        SDL_Rect strengthBarFill = {
            strengthBarX,
            static_cast<int>(strengthBarY + STRENGTH_BAR_HEIGHT * (1.0f - g_GameState.strengthCharge)),  // Start from bottom
            STRENGTH_BAR_WIDTH,
            static_cast<int>(STRENGTH_BAR_HEIGHT * g_GameState.strengthCharge)
        };
        SDL_SetRenderDrawColor(g_Renderer, 
            g_GameState.isDepletingCharge ? 255 : 0,  // Red if depleting
            g_GameState.isDepletingCharge ? 0 : 255,  // Green if charging
            0, 
            255);
        SDL_RenderFillRect(g_Renderer, &strengthBarFill);

        // Draw angle bar background
        // SDL_Rect angleBarBg = {
        //     ANGLE_BAR_X,
        //     ANGLE_BAR_Y,
        //     ANGLE_BAR_WIDTH,
        //     ANGLE_BAR_HEIGHT
        // };
        // SDL_SetRenderDrawColor(g_Renderer, 100, 100, 100, 255);
        //SDL_RenderFillRect(g_Renderer, &angleBarBg);

        // Draw angle square
        // SDL_Rect angleSquare = {
        //     ANGLE_BAR_X + (ANGLE_BAR_WIDTH - ANGLE_SQUARE_SIZE) / 2,
        //     static_cast<int>(g_GameState.angleSquareY),
        //     ANGLE_SQUARE_SIZE,
        //     ANGLE_SQUARE_SIZE
        // };
        // SDL_SetRenderDrawColor(g_Renderer, 255, 255, 0, 255);  // Yellow square
        //SDL_RenderFillRect(g_Renderer, &angleSquare);
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

    PlayRandomLaunchSound();
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
    

            g_GameState.floorSquirrel.currentSprite = 2;
            g_GameState.egg.width = EGG_SIZE_X; // Make egg visible again
            g_GameState.egg.height = EGG_SIZE_Y;
            g_GameState.floorSquirrel.animationTimer = 0.0;
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
       << std::setfill('0') << std::setw(2) << (milliseconds / 10);
    
    // Set outline thickness (1-5 pixels)
    TTF_SetFontOutline(g_Font, 0);

    // Create surface with outline
    SDL_Color textColor = {255, 255, 255, 255};  // White text
    SDL_Color outlineColor = {135, 206, 235, 255};     // Light blue outline
    SDL_Surface* surface = TTF_RenderText_Shaded(g_Font, ss.str().c_str(), textColor, outlineColor);
    if (!surface) return;

    // Render timer
    SDL_Rect timerRect = {
        TIMER_X,
        TIMER_Y,
        surface->w,
        surface->h
    };

    // Create texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_Renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;

    SDL_RenderCopy(g_Renderer, texture, nullptr, &timerRect);
    SDL_DestroyTexture(texture);
    
    // Reset outline for other text rendering
    TTF_SetFontOutline(g_Font, 0);
    
    // Debug texture info
    // printf("Timer texture info - S %s, Position: (%d, %d), Size: (%d, %d)\n",
    //        ss.str().c_str(), timerRect.x, timerRect.y, timerRect.w, timerRect.h);
}
 
void SaveScore(Uint32 time)
{
    #ifdef __EMSCRIPTEN__
    // Web version - use localStorage
    int minutes = (time / 1000) / 60;
    int seconds = (time / 1000) % 60;
    int milliseconds = time % 1000;
    
    char scoreStr[32];
    snprintf(scoreStr, sizeof(scoreStr), "%02d:%02d.%03d", minutes, seconds, milliseconds);
    EM_ASM({
        // Get existing scores
        var scores = localStorage.getItem('scores') || "";
        // Add new score
        scores += UTF8ToString($0) + '\n';
        // Keep only last 5 scores
        var scoresArray = scores.trim().split('\n');
        if (scoresArray.length > 5) {
            scoresArray = scoresArray.slice(-5);
            scores = scoresArray.join('\n') + '\n';
        }
        // Save back to localStorage
        localStorage.setItem('scores', scores);
    }, scoreStr);
    
    #else
    // Desktop version - use file
    FILE* file = fopen(SCORE_FILE, "a");  // Open in append mode
    if (file) {
        int minutes = (time / 1000) / 60;
        int seconds = (time / 1000) % 60;
        int milliseconds = time % 1000;
        
        fprintf(file, "%02d:%02d.%03d\n", minutes, seconds, milliseconds);
        fclose(file);
    }
    #endif
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
       << ":" << std::setfill('0') << std::setw(2) << seconds  << "."
       << std::setfill('0') << std::setw(2) << (milliseconds / 10);

    // Create surface
    SDL_Color textColor = {255, 255, 255, 255};  // White color

    SDL_Color outlineColor = {135, 206, 235, 255}; // Light blue outline
    SDL_Surface *surface = TTF_RenderText_Shaded(g_Font, ss.str().c_str(), textColor, outlineColor);
    if (!surface)
        return;

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
    for (int i = 0; i < SPRITE_SQUIRREL_MAX_VALUE; i++) {
        int width, height;
        SDL_QueryTexture(g_SquirrelTextures[i], nullptr, nullptr, &width, &height);
        squirrel.spriteWidths[i] = static_cast<int>(width * SQUIRREL_SCALE);
        squirrel.spriteHeights[i] = static_cast<int>(height * SQUIRREL_SCALE);
    }
}

void RenderInstructions()
{
    // Change condition to show instructions when egg is in nest
    if (g_GameState.isInNest)
    {
        // Calculate the background rectangle dimensions
        int textWidth = 280;  // Adjust this value to fit your text
        int textHeight = 120; // Covers 3 lines of text
        int textX = WINDOW_WIDTH/2 - textWidth/2;
        int textY = 400;  // Move text to top of screen
        
        // Draw background rectangle
        SDL_SetRenderDrawColor(g_Renderer, 135, 206, 235, 180);  // Light blue with some transparency
        SDL_Rect bgRect = {
            textX - 10,
            textY - 10,
            textWidth + 20,
            textHeight + 20
        };
        SDL_RenderFillRect(g_Renderer, &bgRect);

        // Change instruction text for initial nest release
        RenderText("Press ENTER or SPACE", 
                  textX, 
                  textY, 
                  INSTRUCTION_FONT_SIZE);
                  
        RenderText("to release the egg", 
                  textX, 
                  textY + 40, 
                  INSTRUCTION_FONT_SIZE);
                  
        RenderText("from the nest", 
                  textX, 
                  textY + 80, 
                  INSTRUCTION_FONT_SIZE);

        RenderLastScores();
    }
    // Keep existing instructions for when floor squirrel has the egg
    else if (g_GameState.eggIsHeld && g_GameState.activeSquirrel == &g_GameState.floorSquirrel)
    {
        // Calculate the background rectangle dimensions
        int textWidth = 340;  // Adjust this value to fit your text
        int textHeight = 120; // Covers 3 lines of text (adjust as needed)
        int textX = WINDOW_WIDTH/2 - textWidth/2;
        int textY = INSTRUCTION_Y;
        
        // Draw background rectangle
        SDL_SetRenderDrawColor(g_Renderer, 135, 206, 235, 180);  // Light blue with some transparency
        SDL_Rect bgRect = {
            textX - 10,           // Add some padding
            textY - 10,           // Add some padding
            textWidth ,       // Add padding on both sides
            textHeight + 2       // Add padding on both sides
        };
        SDL_RenderFillRect(g_Renderer, &bgRect);
        

        // Render the text
        RenderText("SPACE - Hold to charge power", 
                  textX, 
                  textY, 
                  INSTRUCTION_FONT_SIZE);
                  
        RenderText("Mouse Click - Adjust angle", 
                  textX, 
                  textY + 40, 
                  INSTRUCTION_FONT_SIZE);
                  
        RenderText("RELEASE SPACE - Launch egg", 
                  textX, 
                  textY + 80, 
                  INSTRUCTION_FONT_SIZE);
        RenderLastScores();
    }
}

void RenderText(const char* text, int x, int y, int fontSize)
{
    // Set font size
    TTF_Font* sizedFont = TTF_OpenFont("assets/VCR_OSD_MONO_1.001.ttf", fontSize);
    if (!sizedFont) {
        printf("Failed to load font for size %d! SDL_ttf Error: %s\n", fontSize, TTF_GetError());
        return;
    }

    // Create surface
    SDL_Color textColor = {255, 255, 255, 255};  // White text
    SDL_Color outlineColor = {135, 206, 235, 255};  // Light blue outline
    SDL_Surface* surface = TTF_RenderText_Shaded(sizedFont, text, textColor, outlineColor);
    if (!surface) {
        TTF_CloseFont(sizedFont);
        return;
    }

    // Create texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_Renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        TTF_CloseFont(sizedFont);
        return;
    }

    // Render text
    SDL_Rect textRect = {
        x,
        y,
        surface->w,
        surface->h
    };

    SDL_RenderCopy(g_Renderer, texture, nullptr, &textRect);

    // Clean up
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    TTF_CloseFont(sizedFont);
}

// Add this function to read and store the last 5 scores
std::vector<std::string> GetLastFiveScores() {
    std::vector<std::string> scores;
    
    #ifdef __EMSCRIPTEN__
    // Web version - use localStorage
    char* scoresStr = (char*)EM_ASM_INT({
        var scores = localStorage.getItem('scores') || "";
        var lengthBytes = lengthBytesUTF8(scores) + 1;
        var stringOnWasmHeap = _malloc(lengthBytes);
        stringToUTF8(scores, stringOnWasmHeap, lengthBytes);
        return stringOnWasmHeap;
    });
    
    if (scoresStr) {
        std::stringstream ss(scoresStr);
        std::string line;
        std::vector<std::string> allScores;
        
        while (std::getline(ss, line)) {
            allScores.push_back(line);
        }
        
        free(scoresStr);
        
        // Get last 5 scores
        int startIdx = std::max(0, static_cast<int>(allScores.size()) - 5);
        for (int i = startIdx; i < allScores.size(); i++) {
            scores.push_back(allScores[i]);
        }
    }
    #else
    // Desktop version - use file
    FILE* file = fopen(SCORE_FILE, "r");
    if (file) {
        char line[32];
        std::vector<std::string> allScores;
        
        // Read all scores
        while (fgets(line, sizeof(line), file)) {
            // Remove newline character
            line[strcspn(line, "\n")] = 0;
            allScores.push_back(line);
        }
        fclose(file);

        // Get last 5 scores (or less if there aren't 5 yet)
        int startIdx = std::max(0, static_cast<int>(allScores.size()) - 5);
        for (int i = startIdx; i < (int)allScores.size(); i++) {
            scores.push_back(allScores[i]);
        }
    }
    #endif
    
    // printf("Number of scores loaded: %zu\n", scores.size());
    // for (size_t i = 0; i < scores.size(); i++) {
    //     printf("Score %zu: %s\n", i, scores[i].c_str());
    // }

    return scores;
}

// Add this function to render the scores box
void RenderLastScores() {
    // printf("isInNest: %d, eggIsHeld: %d, activeSquirrel is floor squirrel: %d\n", 
    //        g_GameState.isInNest, 
    //        g_GameState.eggIsHeld, 
    //        g_GameState.activeSquirrel == &g_GameState.floorSquirrel);


    std::vector<std::string> lastScores = GetLastFiveScores();
    if (lastScores.empty()) return;

    // Calculate dimensions for the score box
    int textWidth = 200;  // Width of the box
    int lineHeight = 25;  // Height per line
    int textHeight = (lastScores.size() + 1) * lineHeight;  // +1 for title
    int padding = 10;
    
    // Position in bottom right
    int boxX = WINDOW_WIDTH - textWidth - padding;
    int boxY = WINDOW_HEIGHT - textHeight - padding;

    // Draw background rectangle
    SDL_SetRenderDrawColor(g_Renderer, 135, 206, 235, 180);  // Light blue with transparency
    SDL_Rect bgRect = {
        boxX - padding,
        boxY - padding,
        textWidth,
        textHeight +5
    };
    SDL_RenderFillRect(g_Renderer, &bgRect);

    // Render title
    RenderText("Last Scores:", 
               boxX, 
               boxY, 
               INSTRUCTION_FONT_SIZE);

    // Render each score
    for (size_t i = 0; i < lastScores.size(); i++) {
        RenderText(lastScores[i].c_str(),
                  boxX,
                  boxY + lineHeight * (i + 1),
                  INSTRUCTION_FONT_SIZE);
    }
}

void UpdateSquirrelAnimations(float deltaTime) 
{
    for (auto& squirrel : g_GameState.squirrels) 
    {
        // handle squirrel with egg
        if ( squirrel.hasEgg && squirrel.animationTimer > 0) {
            squirrel.currentSprite=SPRITE_SQUIRREL_WITH_EGG_1;
            squirrel.animationTimer -= deltaTime;
            
            // Animation finished
            if (squirrel.animationTimer <= 0) {
                squirrel.currentSprite = SPRITE_SQUIRREL_TO_LAUNCH_2;  // Return to default sprite

                g_GameState.egg.width = EGG_SIZE_X;   // Make egg visible again
                g_GameState.egg.height = EGG_SIZE_Y;
                squirrel.hasEgg = false;
            }

        }

            if (squirrel.currentSprite >= SPRITE_SQUIRREL_TO_LAUNCH_2 &&  squirrel.currentSprite <= SPRITE_SQUIRREL_TO_LAUNCH_4)
            {
                squirrel.animationTimer = 10;
                static float animTimer = 0;
                // Update animation every 400ms
                animTimer += deltaTime;
                
                // printf("Animation Timer: %.2f, Delta Time: %.2f, Current Sprite: %d\n", 
                //      animTimer, deltaTime, squirrel.currentSprite);
                
                if (animTimer >= 0.4F) {
                    squirrel.currentSprite++;
                    if (squirrel.currentSprite > SPRITE_SQUIRREL_TO_LAUNCH_4) {
                        squirrel.currentSprite = SPRITE_SQUIRREL_TO_LAUNCH_2;
                    }
                    animTimer = 0;
                }
            }

        
    }
}

struct MainLoopData {
    bool quit;
    SDL_Event e;
    Uint32 lastTime;
} g_MainLoopData;


void UpdateGame(float deltaTime)
{

    UpdatePhysics(deltaTime);
    UpdateControls();
    UpdateCamera(); // Add camera update
    UpdateEggAnimation(deltaTime);
    UpdateSquirrelAnimations(deltaTime);
}


void main_loop_iteration() {
    Uint32 frameStart = SDL_GetTicks();
    Uint32 currentTime = frameStart;
    float deltaTime = static_cast<float>(currentTime - g_MainLoopData.lastTime) / 1000.0f;

    g_MainLoopData.lastTime = currentTime;

    while (SDL_PollEvent(&g_MainLoopData.e)) {
        if (g_MainLoopData.e.type == SDL_QUIT) {
            g_MainLoopData.quit = true;
            #ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
            #endif
        }
        else if (g_MainLoopData.e.type == SDL_KEYDOWN)
        {
            switch (g_MainLoopData.e.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                g_MainLoopData.quit = true;
                break;
            case SDLK_SPACE:
                // note: keyboard keys events are sent continuously
                if (g_GameState.eggIsHeld)
                {
                    StartStrengthCharge();
                }
                break;
            case SDLK_i: // New debug teleport
                if (!g_GameState.squirrels.empty())
                {
                    // Teleport above the first squirrel
                    const auto &squirrel = g_GameState.squirrels[0];
                    g_GameState.egg.x = squirrel.x + (squirrel.spriteWidths[squirrel.currentSprite] - g_GameState.egg.width) / 2;
                    g_GameState.egg.y = squirrel.y - g_GameState.egg.height - 50; // 50 pixels above
                    g_GameState.eggVelocityY = 0;
                    g_GameState.eggIsHeld = false;
                }
                break;
            case SDLK_a:
                if (g_GameState.eggIsHeld && g_GameState.activeSquirrel)
                {
                    g_GameState.isLaunchingRight = false;
                    g_GameState.activeSquirrel->isLeftSide = false; // Make active squirrel face left
                }
                break;
            case SDLK_d:
                if (g_GameState.eggIsHeld && g_GameState.activeSquirrel)
                {
                    g_GameState.isLaunchingRight = true;
                    g_GameState.activeSquirrel->isLeftSide = true; // Make active squirrel face right
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
            case SDLK_RETURN: // Enter key
                if (g_GameState.isInNest)
                {
                    g_GameState.isInNest = false;
                    g_GameState.eggIsHeld = false;
                    printf("Egg released from nest\n");
                }
                break;
            }
        }
        else if (g_MainLoopData.e.type == SDL_KEYUP)
        {
            // if (g_MainLoopData.e.key.keysym.sym == SDLK_k && g_GameState.eggIsHeld && g_GameState.isCharging)
            // {
            //     LaunchEgg();
            // }
            if (g_MainLoopData.e.key.keysym.sym == SDLK_SPACE && g_GameState.eggIsHeld && g_GameState.isCharging)
            {
                LaunchEgg();
            }

            if (g_MainLoopData.e.key.keysym.sym == SDLK_SPACE)
            {

                if (g_GameState.isInNest)
                {
                    g_GameState.isInNest = false;
                    g_GameState.eggIsHeld = false;
                    printf("Egg released from nest\n");
                }
            }
        }
        else if (g_MainLoopData.e.type == SDL_MOUSEBUTTONDOWN)
        {
            if (g_MainLoopData.e.button.button == SDL_BUTTON_LEFT && g_GameState.eggIsHeld)
            {
                HitAngleSquare();
                // StartStrengthCharge();
            }
            else if (g_MainLoopData.e.button.button == SDL_BUTTON_RIGHT && g_GameState.eggIsHeld)
            {
                HitAngleSquare();
            } 
        }
        else if (g_MainLoopData.e.type == SDL_MOUSEBUTTONUP)
        {
            // if (e.button.button == SDL_BUTTON_LEFT && g_GameState.eggIsHeld)
            // {
            //     LaunchEgg();
            // }
        }
    }

    // Update game state
    UpdateGame(deltaTime);
    
    // Render
    Render();

    // Cap frame rate
    Uint32 frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < FRAME_TIME) {
        SDL_Delay(FRAME_TIME - frameTime);
    }
}

int main(int argc, char* argv[]) {
    if (!InitSDL()) {
        printf("Failed to initialize!\n");
        return -1;
    }

    InitGameObjects();

    g_MainLoopData.quit = false;
    g_MainLoopData.lastTime = SDL_GetTicks();

    #ifdef __EMSCRIPTEN__
    // Web version - use emscripten_set_main_loop
    emscripten_set_main_loop(main_loop_iteration, 0, 1);
    #else
    // Desktop version - use while loop
    while (!g_MainLoopData.quit) {
        main_loop_iteration();
    }
    #endif

    // Cleanup
    CleanUp();
    return 0;
}

void Update(Uint32 deltaTime)
{

    UpdatePhysics(deltaTime);
    UpdateControls();
    UpdateCamera(); // Add camera update
    UpdateEggAnimation(deltaTime);
    UpdateSquirrelAnimations(deltaTime);
}
