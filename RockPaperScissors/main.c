#include "raylib.h"
#include "math.h"
#include "stdio.h"
#include "time.h"
#include "stdlib.h"

#define ENTITY_COUNT 360
#define WINDOW_WIDTH 2560
#define WINDOW_HEIGHT 1440
#define SPRITE_ORIGINAL_WIDTH 32
#define SPRITE_ORIGINAL_HEIGHT 32
#define SPRITE_WIDTH 64
#define SPRITE_HEIGHT 64

#define SPEED 64.0f

enum Type {
    ROCK,
    PAPER,
    SCISSORS
};

struct Coord {
    float x;
    float y;
};

struct Entity {
    enum Type type;
    struct Coord position;
};

float VectorLength(struct Coord a, struct Coord b) {
    return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

enum Type GetVictimType(enum Type hunterType) {
    switch (hunterType) {
    case ROCK:
        return SCISSORS;
    case PAPER:
        return ROCK;
    case SCISSORS:
        return PAPER;
    }
}

enum Type GetHunterType(enum Type victimType) {
    switch (victimType) {
    case ROCK:
        return PAPER;
    case PAPER:
        return SCISSORS;
    case SCISSORS:
        return ROCK;
    }
}

struct Entity entities[ENTITY_COUNT];

void initEntities() {
    for (int i = 0; i < ENTITY_COUNT; i += 1) {
        int x = rand() % (WINDOW_WIDTH - 50);
        int y = rand() % (WINDOW_HEIGHT - 50);

        enum Type t;
        switch (i % 3) {
        case 0:
            t = ROCK;
            break;
        case 1:
            t = PAPER;
            break;
        case 2:
            t = SCISSORS;
            break;
        }

        entities[i].type = t;
        entities[i].position.x = (float)x;
        entities[i].position.y = (float)y;
    }
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{

    srand(time(NULL));

    initEntities();


    //--------------------------------------------------------------------------------------
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = WINDOW_WIDTH;
    const int screenHeight = WINDOW_HEIGHT;

    InitWindow(screenWidth, screenHeight, "ROCK PAPER SCISSORS");


    SetTargetFPS(30);
    //--------------------------------------------------------------------------------------


    Image rockImage = LoadImage("assets/rock.png");
    Texture2D rock = LoadTextureFromImage(rockImage);
    UnloadImage(rockImage);

    Image paperImage = LoadImage("assets/paper.png");
    Texture2D paper = LoadTextureFromImage(paperImage);
    UnloadImage(paperImage);

    Image scissorsImage = LoadImage("assets/scissors.png");
    Texture2D scissors = LoadTextureFromImage(scissorsImage);
    UnloadImage(scissorsImage);

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        for (int i = 0; i < ENTITY_COUNT; i += 1) {

            struct Coord center;
            center.x = WINDOW_WIDTH / 2;
            center.y = WINDOW_HEIGHT / 2;

            enum Type t = entities[i].type;
            enum Type victimType = GetVictimType(t);
            enum Type hunterType = GetHunterType(t);
            float hx = entities[i].position.x;
            float hy = entities[i].position.y;

            float centerLength = VectorLength(center, entities[i].position);

            int closestVictim = -1;
            float minDistance = 1000000;

            int closestHunter = -1;
            float minHunterDistance = 1000000;

            for (int k = 0; k < ENTITY_COUNT; k += 1) {

                if (entities[k].type == victimType) {
                    float cx = entities[k].position.x;
                    float cy = entities[k].position.y;

                    float length = VectorLength(entities[k].position, entities[i].position);

                    if (length <= minDistance) {
                        closestVictim = k;
                        minDistance = length;
                    }
                }
                else if (entities[k].type == hunterType) {
                    float cx = entities[k].position.x;
                    float cy = entities[k].position.y;

                    float length = VectorLength(entities[k].position, entities[i].position);

                    if (length <= minHunterDistance) {
                        closestHunter = k;
                        minHunterDistance = length;
                    }
                }

            }

            struct Coord direction;
            if ((closestVictim == -1 && closestHunter == -1) || centerLength > (WINDOW_HEIGHT / 2)) {
                direction.x = (center.x - hx) / centerLength;
                direction.y = (center.y - hy) / centerLength;
            }
            else if (closestVictim != -1 && minHunterDistance < minDistance) {
                float vx = entities[closestHunter].position.x;
                float vy = entities[closestHunter].position.y;
                direction.x = (vx - hx) / minDistance;
                direction.y = (vy - hy) / minDistance;
                direction.x *= -1;
                direction.y *= -1;
            }
            else {
                float vx = entities[closestVictim].position.x;
                float vy = entities[closestVictim].position.y;
                direction.x = (vx - hx) / minDistance;
                direction.y = (vy - hy) / minDistance;
            }

            entities[i].position.x += direction.x * GetFrameTime() * SPEED;
            entities[i].position.y += direction.y * GetFrameTime() * SPEED;

            if (minDistance < (float)SPRITE_WIDTH * 0.9f) {
                entities[closestVictim].type = t;
            }

        }

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(LIGHTGRAY);

        char str[100];

        unsigned int rC = 0;
        unsigned int pC = 0;
        unsigned int sC = 0;

        for (int i = 0; i < ENTITY_COUNT; i += 1) {
            enum Type t = entities[i].type;
            float x = entities[i].position.x;
            float y = entities[i].position.y;

            float sx = x - ((float)SPRITE_WIDTH / 2.0);
            float sy = y - ((float)SPRITE_HEIGHT / 2.0);

            struct Vector2 pos;
            pos.x = sx;
            pos.y = sy;

            switch (t) {
            case ROCK:

                DrawTextureEx(rock, pos, 0.0f, 2.0f, WHITE);
                rC += 1;
                break;
            case PAPER:
                DrawTextureEx(paper, pos, 0.0f, 2.0f, WHITE);
                pC += 1;
                break;
            case SCISSORS:
                DrawTextureEx(scissors, pos, 0.0f, 2.0f, WHITE);
                sC += 1;
                break;
            }
            //DrawLine(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, x, y, BLACK);
        }

        unsigned long cstd = __STDC_VERSION__;

        sprintf_s(str, sizeof(str), "R: %d", rC);
        DrawText(str, 20, 50, 24, BLACK);
        sprintf_s(str, sizeof(str), "P: %d", pC);
        DrawText(str, 120, 50, 24, BLACK);
        sprintf_s(str, sizeof(str), "S: %d", sC);
        DrawText(str, 220, 50, 24, BLACK);

        sprintf_s(str, sizeof(str), "CSTD: %d", cstd);
        DrawText(str, WINDOW_WIDTH - 350, 50, 24, BLACK);
        int fps = GetFPS();
        sprintf_s(str, sizeof(str), "FPS: %d", fps);
        DrawText(str, WINDOW_WIDTH - 150, 50, 24, BLACK);

        EndDrawing();

        if (rC == ENTITY_COUNT || pC == ENTITY_COUNT || sC == ENTITY_COUNT) {
            WaitTime(2.5f);
            initEntities();
        }
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(rock);
    UnloadTexture(paper);
    UnloadTexture(scissors);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
