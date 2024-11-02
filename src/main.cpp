#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <math.h>

#define PADDLE_WIDTH 64
#define PADDLE_HEIGHT 64
#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 480
#define BALL_SIZE 16
#define BLOCK_HEIGHT 64
#define BLOCK_WIDTH 32
#define BLOCK_SPACING 8
#define PADDLE_SPEED 4

#define GRAVITY 0.8f
#define JUMP_SPEED 10

//#define DEBUG_INVINCIBLE


SDL_Renderer *g_Renderer;
TTF_Font* g_font;
int g_score = 0;
int g_frame_count = 0;

Mix_Chunk *gHitSound = NULL;

enum game_state_e
{
    GAME_STATE_MENU,
    GAME_STATE_PLAYING
};

game_state_e g_game_state = GAME_STATE_MENU;
game_state_e game_state_current = GAME_STATE_MENU;

void DoGameLost()
{
#ifndef DEBUG_INVINCIBLE
    g_game_state = GAME_STATE_MENU;
    printf("last game score: %d",g_score);
#endif //DEBUG_INVINCIBLE
}

void DoGameStart()
{
    g_game_state = GAME_STATE_PLAYING;
    g_score = 0;
}

void PrintSDL_Rect(SDL_Rect * rect)
{
    printf("SDL_rect (%d,%d)(%d,%d)\n",rect->x,rect->y,rect->w,rect->h);
}

struct GameScene 
{
    virtual ~GameScene() = default;
    virtual void Init() = 0;
    virtual void Update() = 0;
    virtual void Render() = 0;
    virtual void DeleteAllObjects() = 0;
    virtual void HandleEvents(SDL_Event* e) = 0;
};

bool CheckAABBCollision( SDL_Rect r1, SDL_Rect r2 )
{
    bool collided = false;

    if (
        (r2.x + r2.w > r1.x && r2.x < r1.x + r1.w) // collision in x
        &&
        (r2.y + r2.h > r1.y && r2.y < r1.y + r1.h) // collision in y
       )
    {
        collided = true;
    }
        return collided;
}

SDL_Texture* loadTexture (char* str)
{
    SDL_Texture* newTexture = NULL;

    SDL_Surface* loadedSurface = IMG_Load(str);

    if (loadedSurface != NULL)
    {
        newTexture = SDL_CreateTextureFromSurface(g_Renderer, loadedSurface);
        if (newTexture == NULL)
        {
            printf("Unable to create texture from %s! SDL Error: %s\n", str, SDL_GetError());
        }
        SDL_FreeSurface(loadedSurface);
    }
    else
    {
        printf("Unable to load image %s! SDL_image Error: %s\n", str, IMG_GetError());
    }

    return newTexture;
}

int g_background_x = 0;

struct Background_t
{
#define BACKGROUND_IMAGE_TOTAL_WIDTH 1500 // 3x500 pattern, see below
#define BACKGROUND_IMAGE_REPEATING_WIDTH 500
#define BACKGROUND_IMAGE_HEIGHT WINDOW_HEIGHT
#define BACKGROUND_SCROLL_SPEED 1;

    SDL_Rect srcRect;
    SDL_Rect destRect;
    SDL_Texture * texture;

    Background_t(){
        //printf("size of backround_t %ld", sizeof(Background_t));
        destRect = {g_background_x,0,BACKGROUND_IMAGE_TOTAL_WIDTH,BACKGROUND_IMAGE_HEIGHT};
        texture = loadTexture("assets/bg_trees.png");
    }

    void Render()
    {
        // printf("background render called %d \n",g_background_x);
        SDL_RenderCopy(g_Renderer, texture, NULL, &destRect);
    }
    void UpdatePos()
    {
        g_background_x -= BACKGROUND_SCROLL_SPEED;
        if (g_background_x<-BACKGROUND_IMAGE_REPEATING_WIDTH)
            g_background_x+=BACKGROUND_IMAGE_REPEATING_WIDTH;

        destRect = {g_background_x,0,BACKGROUND_IMAGE_TOTAL_WIDTH,BACKGROUND_IMAGE_HEIGHT};
        // printf("g_background_x %d \n",g_background_x);
    }
};

struct Eve_t
{
    SDL_Rect rect;
    float y_pos = WINDOW_HEIGHT/2; // y pos is float for calculations
    float y_speed = 0;
    bool move_right = 0, move_left = 0;
    double angle = 0;
    SDL_Texture * texture;

    Eve_t(int x, int y) { rect = {x, y, PADDLE_WIDTH, PADDLE_HEIGHT};
        texture = loadTexture("assets/eve.png");
    }
    void Render(){
        // SDL_RenderCopy(g_Renderer, texture, NULL, &rect);
        SDL_RenderCopyEx(g_Renderer, texture, NULL, &rect, angle,NULL, SDL_FLIP_NONE);
    }
    void UpdatePos(){
        y_speed -= -GRAVITY;
        y_pos += y_speed;
        if (y_pos < 0)
        {   // hit ceiling
            y_pos = 0;
            y_speed = 0;
        }
        if (y_pos > WINDOW_HEIGHT-PADDLE_HEIGHT)
            y_pos = WINDOW_HEIGHT-PADDLE_HEIGHT; 
        rect.x += move_right*PADDLE_SPEED - move_left*PADDLE_SPEED;
        rect.y = y_pos;


        // update rendering angle
        double rad = atan(y_speed/3);
        angle = rad*180/(3.14*3); // limits angle to -30 to 30 degrees
    }

    void StartOrStopMovingRight(bool StartOrStop){
        move_right=StartOrStop; // start is 1, stop is 0
    }
    void StartOrStopMovingLeft(bool StartOrStop){
        move_left=StartOrStop; // start is 1, stop is 0
    }

};

struct Pillar_t
{
    #define PILLAR_WIDTH 150
    #define PILLAR_HEIGHT 300

    float x,y; // bottom left of pillars
    SDL_Rect top_rect;
    SDL_Rect bottom_rect;

    float top_h;  // determines where the open space between pillars start. top_h [0,WINDOW_HEIGHT-open_h], 0 topmost 
    float open_h;
    float speed;

    bool was_scored = 0;

    SDL_Texture* TubeTexture;
    SDL_Texture* CapTexture;

    void SetScored()
    {
        if (!was_scored)
            {
                was_scored = 1;
                g_score++;
            }
    }

    void Init( int s_top_h,int s_open_h, int s_speed)
    {
        //printf("initted pillar object with s_top_h %d, s_open_h %d,  s_speed %d\n",s_top_h, s_open_h,  s_speed);
        TubeTexture = loadTexture("assets/tube.png");
        top_h = s_top_h; 
        open_h = s_open_h; 
        speed = s_speed;
        x=WINDOW_WIDTH+10; // slightly to the right of the screen
        y=0;
        // was_scored = 0;
        
    }

    void Update()
    {
        x -= speed / 60;
        top_rect = SDL_Rect{(int)x,(int)y-PILLAR_HEIGHT+(int)top_h,PILLAR_WIDTH,PILLAR_HEIGHT};
        int bottom_rect_y = y + (int)top_h + (int)open_h;
        bottom_rect = SDL_Rect{(int)x,bottom_rect_y,PILLAR_WIDTH,PILLAR_HEIGHT};

        // PrintSDL_Rect(&top_rect);
    }

    void Render()
    {
        SDL_RenderCopyEx(g_Renderer, TubeTexture, NULL, &top_rect, 0, 0, SDL_FLIP_VERTICAL );
        SDL_RenderCopy(g_Renderer, TubeTexture, NULL, &bottom_rect);
    }

    void Cleanup()
    {
        if(this)
            memset(this,0,sizeof(Pillar_t));
    }

    ~Pillar_t()
    {
        Cleanup();
    }
};


struct DifficultyParameters
{
  float height_distance = 1.0f / 3.0f; // measured per window height 
  float opening_height = 1.0f / 3.0f; // measured per window height 
  float pillar_per_second = 1.0f / 3.0f;  
  float speed = 1.0f / 3.0f; // window width per second
};

DifficultyParameters g_params; 

float CalculateNextPillarHeight(float last_pillar_height, float next_pillar_open_size)
{
    float next_pillar_height = 0;
    bool valid_height=0;
    do
    {
        float r = ((float)rand() / (float) RAND_MAX)-0.5f; // -1 to 1
        next_pillar_height = last_pillar_height+(r*g_params.height_distance*WINDOW_HEIGHT);

        if (next_pillar_height>WINDOW_HEIGHT-next_pillar_open_size || next_pillar_height<0)
            valid_height=0;
        else
            valid_height=1;    
    } while (!valid_height);

    return next_pillar_height;

}

struct PillarFactory_t
{
    #define MAX_PILLARS 100
    Pillar_t arr_pillars[MAX_PILLARS];
    bool allocated_pillars[MAX_PILLARS] = {0};

    int last_top_h= WINDOW_HEIGHT / 4; // first defaults
    int last_open_h=WINDOW_HEIGHT/2.5;
    int last_speed=0;

    void CreatePillar()
    {
        // seed(3);
        float s_open_h = g_params.opening_height* 480.0f;
        float s_top_h = CalculateNextPillarHeight(last_top_h, s_open_h);
        last_top_h = (int)s_top_h;

        int i = 0;
        while (allocated_pillars[i])
            i++;
        // found first index that was not allocated yet

        arr_pillars[i].Init((int)s_top_h, (int)s_open_h, g_params.speed*WINDOW_WIDTH); // todo: make dynamic values for the position
        allocated_pillars[i] = true;
        printf("created pillar i %d s_top_h %f s_open_h %f \n", i, s_top_h, s_open_h);
        printf("g_params.height_distance %f ,g_params.opening_height %f ,g_params.pillar_per_second %f what %f \n", 
                g_params.height_distance,    g_params.opening_height,    g_params.pillar_per_second, g_params.opening_height* 480.0f);
    }

    void Update()
    {
        for(int i=0; i< MAX_PILLARS; i++)
        {
            if (allocated_pillars[i])
            {
                Pillar_t &current_pillar = arr_pillars[i];
                current_pillar.Update();

                if (current_pillar.x < 0 - PILLAR_WIDTH) 
                {
                    // Mark as unallocated
                    allocated_pillars[i] = false;
                    current_pillar.Cleanup();
                }
            }
        }
    }

    void Render()
    {
        for(int i=0; i< MAX_PILLARS; i++)
        {
            if (allocated_pillars[i])
            {
                Pillar_t &current_pillar = arr_pillars[i];
                current_pillar.Render();
            }
        }
    }

    ~PillarFactory_t()
    {
        // delete all pillars
        for (int i = 0; i < MAX_PILLARS; i++)
        {
            if (allocated_pillars[i])
            {
                // Mark as unallocated
                allocated_pillars[i] = false;
            }
        }
    }
};

struct Text_t
{
    SDL_Texture *TextTexture;
    char m_TextString[100]; // actual text
    SDL_Rect dst = {0, 0, 200, 50};
    SDL_Color color = {255, 0, 0, 255};

    SDL_Texture *LoadTextToTexture(char *str)
    {
        SDL_Surface *textSurface = TTF_RenderText_Solid(g_font, str, color);

        if (!textSurface)
            printf("Unable to render text surface!");

        SDL_Texture *rtn_texture = SDL_CreateTextureFromSurface(g_Renderer, textSurface);

        dst.w = textSurface->w,
        dst.h = textSurface->h;
        SDL_FreeSurface(textSurface);
        return rtn_texture;
    }

    void UpdateText(char * format_str)
    {
        snprintf(m_TextString, sizeof(m_TextString), format_str, g_score);
        TextTexture = LoadTextToTexture(m_TextString);
    }

    void UpdateColor(SDL_Color clr)
    {
        color = clr;
    }

    void UpdateTextPos(SDL_Rect new_dst)
    {
        // only X and Y, w and h comes from the surface automagically
        dst.x = new_dst.x;
        dst.y = new_dst.y;
    }

    void Render()
    {
        SDL_RenderCopy(g_Renderer, TextTexture, NULL, &dst);
    }

};

void ClearScreen()
{
    SDL_SetRenderDrawColor(g_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_Renderer);
}

struct StartingScene : public GameScene
{
    Background_t background;
    Text_t BottomText;
    Text_t ScoreText;

    StartingScene() { Init(); };
    void Init(){
        snprintf(BottomText.m_TextString, sizeof(BottomText.m_TextString), "Press 2 to start!");
        BottomText.UpdateText(BottomText.m_TextString);
        BottomText.UpdateTextPos(SDL_Rect{WINDOW_HEIGHT/2,WINDOW_WIDTH/2-100});
        BottomText.UpdateColor(SDL_Color{255,255,255,255});

        if(g_score)
        {
            ScoreText.UpdateText("Last game Score: %d");
        }
    };

    void Update() { 
        background.UpdatePos(); 
    };
    void Render(){
        background.Render();
        BottomText.Render();
        ScoreText.Render();
    };
    void DeleteAllObjects()
    {
    };
    void HandleEvents(SDL_Event* e){

        if (e->key.type == SDL_KEYDOWN)
        {
            switch (e->key.keysym.sym)
            {
            case SDLK_2:
                DoGameStart();
                break;
            }
        }
    }

};

struct PlayingScene : public GameScene
{

    Eve_t eve = {WINDOW_WIDTH / 6, WINDOW_HEIGHT/2};
    PillarFactory_t pillarFactory;
    long long int frames_since_start = 0;

    Text_t score_text;
    Background_t background;

    PlayingScene(){};

    void Init()
    {
        pillarFactory.CreatePillar();
    }
    void Update()
    {
        frames_since_start++;
        // create new pillars
        if (frames_since_start%120==0)
            pillarFactory.CreatePillar();

        // game logic
        eve.UpdatePos();
        pillarFactory.Update();
        score_text.UpdateText("Points: %d");
        background.UpdatePos();

        // eve and pillar collision check
        for (int i = 0; i < MAX_PILLARS; i++)
        {

            if (pillarFactory.allocated_pillars[i])
            {
                Pillar_t* currPillar = &pillarFactory.arr_pillars[i];

                if (CheckAABBCollision(currPillar->top_rect, eve.rect))
                {
                    Mix_PlayChannel(-1, gHitSound, 0);
                    DoGameLost();
                }
                if (CheckAABBCollision(currPillar->bottom_rect, eve.rect))
                {
                    Mix_PlayChannel(-1, gHitSound, 0);
                    DoGameLost();
                }

                // check if pillar was scored when player passes through
                if (currPillar->bottom_rect.x < eve.rect.x && !pillarFactory.arr_pillars[i].was_scored)
                {
                    currPillar->SetScored();
                }
            }
        }
    }
    void Render()
    {
        background.Render();
        eve.Render();
        pillarFactory.Render();
        score_text.Render();
    }
    void DeleteAllObjects()
    {
        printf("GameScene virtual DeleteAllObjects was not defined!\n");
    }

    void HandleEvents(SDL_Event* e)
    {
        if (e->key.type == SDL_KEYDOWN)
        {
            switch (e->key.keysym.sym)
            {
            case SDLK_RIGHT:
                eve.StartOrStopMovingRight(true);
                break;
            case SDLK_LEFT:
                eve.StartOrStopMovingLeft(true);
                break;
            case SDLK_SPACE:
                eve.y_speed = -JUMP_SPEED;
                printf("space jump \n");
                break;
            }
        }
        else if (e->type == SDL_KEYUP)
        {
            switch (e->key.keysym.sym)
            {
            case SDLK_RIGHT:
                eve.StartOrStopMovingRight(false);
                break;
            case SDLK_LEFT:
                eve.StartOrStopMovingLeft(false);
                break;
            }
        }
    }
};

void Init()
{
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)<0)
        printf("SDL could not init! SDL error: %s\n", SDL_GetError());

    if (TTF_Init() == -1)
        printf("TTF init failed!\n");

    g_font = TTF_OpenFont("assets/VCR_OSD_MONO_1.001.ttf", 28);

    if (!g_font)
        printf("unable to open g_font!\n");

    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
    }

    // load sounds
    gHitSound = Mix_LoadWAV("assets/mixkit-game-ball-tap-2073.wav");
    if (gHitSound == NULL) {
        printf("Failed to load hit sound effect! SDL_mixer Error: %s\n", Mix_GetError());
    }

    SDL_Window *window = SDL_CreateWindow("Block Breaker",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH,
                                          WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN);

    g_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
}




int main(int argc, char* argv[]) {

    Init();

    GameScene* CurrentGameScene;
    CurrentGameScene = new StartingScene();

    CurrentGameScene->Init();

    // main loop
    SDL_Event e;
    bool quit = false;
    while (quit == false)
    {
        // time capture
        const uint64_t frameStartTime = SDL_GetTicks64();

        // game state handling
        if (g_game_state != game_state_current)
        {
            printf("g_game_state %d game_state_current %d changed\n", g_game_state, game_state_current);
            delete CurrentGameScene;
            game_state_current = g_game_state;
            switch (g_game_state)
            {
            case GAME_STATE_MENU:
                CurrentGameScene = new StartingScene();
                break;
            case GAME_STATE_PLAYING:
                CurrentGameScene = new PlayingScene(); 
                break;
            default:
                CurrentGameScene = new StartingScene(); 
                break;
            }

            CurrentGameScene->Init();
        }



        // event handling
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                quit = true;

            if (e.key.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                case SDLK_1:
                    DoGameLost();
                    break;
                case SDLK_2:
                    DoGameStart();
                    break;
                case SDLK_ESCAPE:
                    quit = true;
                    break;
                }
            }

            CurrentGameScene->HandleEvents(&e);
        }

        CurrentGameScene->Update();

        // Render handling
        ClearScreen();
        CurrentGameScene->Render();
        SDL_RenderPresent(g_Renderer);


        const uint64_t frameEndTime = SDL_GetTicks64();
        const uint64_t FrameElapsedTime = frameEndTime - frameStartTime;
        const int fps = 60; // frames per second
        if (FrameElapsedTime < 1000 / fps)
            SDL_Delay(static_cast<uint32_t>((1000 / fps) - FrameElapsedTime));

        g_frame_count++;
    }   // end main loop

    SDL_Quit();
    return 0;
}