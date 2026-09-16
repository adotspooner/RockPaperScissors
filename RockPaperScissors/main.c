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

#define SPEED 95.0f

// Burst mechanics. Entities cruise, then commit to a short explosive move:
// a hunter LUNGES at prey in a straight line, prey JUKES sideways out of it.
// A burst is committed - steering is ignored until it ends - so lunges can
// miss, and a miss leaves the hunter winded and briefly helpless.
#define BURST_SPEED 420.0f      // speed during a lunge or juke
#define BURST_TIME 0.20f        // how long a burst lasts
#define RECOVER_TIME 0.40f      // helpless window after a burst
#define RECOVER_MULT 0.45f      // speed while winded
// Rest before another burst is allowed. Without this both sides chain-burst
// forever at the same average speed and prey is literally uncatchable.
// Prey rests longer than hunters - that gap is what lets a hunt close.
#define JUKE_REST 0.85f
#define LUNGE_REST 0.15f
// A chaser at the same speed as a fleer never closes the gap - pursuit is
// only decidable if the hunter is faster. This is what ends a round.
#define CHASE_MULT 1.20f
#define LUNGE_RANGE 150.0f      // hunter commits inside this
#define JUKE_RANGE 120.0f       // prey bolts inside this

// Soft rectangular walls. A hard circular boundary used to pin fleeing
// entities at a fixed radius and slide them into an arc; this ramps an
// inward nudge over the last stretch instead, and uses the whole window.
#define WALL_MARGIN 280.0f      // how far in the push starts
#define WALL_FORCE 2.4f         // weight of the push against steering
#define EDGE (SPRITE_WIDTH / 2) // keep sprites fully on screen

// Capture effect: a puff of debris in the winning type's colour.
#define PARTICLE_COUNT 3072     // shared pool, oldest simply run out
#define BURST_PARTICLES 34      // spawned per capture
#define PARTICLE_LIFE 0.85f
#define PARTICLE_SPEED 440.0f
#define PARTICLE_DRAG 2.2f
#define PARTICLE_SIZE 11.0f

// Shockwave ring thrown off at the moment of capture.
#define SHOCK_COUNT 96
#define SHOCK_LIFE 0.28f
#define SHOCK_RADIUS 110.0f
#define SHOCK_THICKNESS 9.0f

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
    float burst;              // >0 while committed to burstDir
    float recover;            // >0 while winded
    float cooldown;           // >0 while another burst is disallowed
    struct Coord burstDir;
};

float VectorLength(struct Coord a, struct Coord b) {
    return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// Normalise a vector, with a stable fallback when it has no length. Two
// entities can land on the exact same position, and dividing by that zero
// distance yields 0/0 = NaN - which freezes the entity forever, since every
// comparison against NaN is false so it is never caught and never moves.
struct Coord SafeDirection(float dx, float dy) {
    struct Coord d;
    float len = sqrtf(dx * dx + dy * dy);

    if (len > 0.0001f) {
        d.x = dx / len;
        d.y = dy / len;
    }
    else {
        d.x = 1.0f;
        d.y = 0.0f;
    }

    return d;
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

struct Particle {
    struct Coord position;
    struct Coord velocity;
    float life;               // seconds remaining, <=0 means the slot is free
    Color color;
};

struct Particle particles[PARTICLE_COUNT];

struct Shockwave {
    struct Coord position;
    float life;
    Color color;
};

struct Shockwave shockwaves[SHOCK_COUNT];

Color GetTypeColor(enum Type t) {
    switch (t) {
    case ROCK:
        return BROWN;
    case PAPER:
        return SKYBLUE;
    case SCISSORS:
        return RED;
    }
    return WHITE;
}

float RandUnit() {
    return (float)rand() / (float)RAND_MAX;
}

// Scatter debris from a capture. Silently does nothing if the pool is full,
// which only happens during a pile-up and is not worth handling.
void spawnCaptureBurst(float x, float y, Color c) {
    int spawned = 0;

    for (int p = 0; p < PARTICLE_COUNT && spawned < BURST_PARTICLES; p += 1) {
        if (particles[p].life > 0.0f) {
            continue;
        }

        float angle = RandUnit() * 2.0f * PI;
        float speed = PARTICLE_SPEED * (0.35f + 0.65f * RandUnit());

        particles[p].position.x = x;
        particles[p].position.y = y;
        particles[p].velocity.x = cosf(angle) * speed;
        particles[p].velocity.y = sinf(angle) * speed;
        particles[p].life = PARTICLE_LIFE * (0.7f + 0.3f * RandUnit());
        particles[p].color = c;
        spawned += 1;
    }

    for (int s = 0; s < SHOCK_COUNT; s += 1) {
        if (shockwaves[s].life > 0.0f) {
            continue;
        }

        shockwaves[s].position.x = x;
        shockwaves[s].position.y = y;
        shockwaves[s].life = SHOCK_LIFE;
        shockwaves[s].color = c;
        break;
    }
}

void initEntities() {
    for (int p = 0; p < PARTICLE_COUNT; p += 1) {
        particles[p].life = 0.0f;
    }
    for (int s = 0; s < SHOCK_COUNT; s += 1) {
        shockwaves[s].life = 0.0f;
    }

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
        entities[i].burst = 0.0f;
        entities[i].recover = 0.0f;
        entities[i].cooldown = 0.0f;
        entities[i].burstDir.x = 0.0f;
        entities[i].burstDir.y = 0.0f;
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

        float dt = GetFrameTime();

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
            int flee = (closestHunter != -1) &&
                       (closestVictim == -1 || minHunterDistance < minDistance);

            if (closestVictim == -1 && closestHunter == -1) {
                direction = SafeDirection(center.x - hx, center.y - hy);
            }
            else if (flee) {
                float vx = entities[closestHunter].position.x;
                float vy = entities[closestHunter].position.y;
                direction = SafeDirection(hx - vx, hy - vy);
            }
            else {
                float vx = entities[closestVictim].position.x;
                float vy = entities[closestVictim].position.y;
                direction = SafeDirection(vx - hx, vy - hy);
            }

            // Ramp an inward push over the last WALL_MARGIN pixels of each edge
            // and blend it into the steering, so entities curve away from walls
            // instead of snapping to a new target.
            float wx = 0.0f;
            float wy = 0.0f;

            if (hx < WALL_MARGIN) {
                wx += (WALL_MARGIN - hx) / WALL_MARGIN;
            }
            else if (hx > WINDOW_WIDTH - WALL_MARGIN) {
                wx -= (hx - (WINDOW_WIDTH - WALL_MARGIN)) / WALL_MARGIN;
            }

            if (hy < WALL_MARGIN) {
                wy += (WALL_MARGIN - hy) / WALL_MARGIN;
            }
            else if (hy > WINDOW_HEIGHT - WALL_MARGIN) {
                wy -= (hy - (WINDOW_HEIGHT - WALL_MARGIN)) / WALL_MARGIN;
            }

            if (wx != 0.0f || wy != 0.0f) {
                direction.x += wx * WALL_FORCE;
                direction.y += wy * WALL_FORCE;

                float dlen = sqrtf(direction.x * direction.x + direction.y * direction.y);
                if (dlen > 0.0001f) {
                    direction.x /= dlen;
                    direction.y /= dlen;
                }
            }

            // Hunting is faster than running; that margin is what closes a hunt.
            float speed = flee ? SPEED : SPEED * CHASE_MULT;

            if (entities[i].cooldown > 0.0f) {
                entities[i].cooldown -= dt;
            }

            if (entities[i].burst > 0.0f) {
                // Committed. Steering is ignored, which is what makes a miss possible.
                direction = entities[i].burstDir;
                speed = BURST_SPEED;
                entities[i].burst -= dt;
                if (entities[i].burst <= 0.0f) {
                    entities[i].recover = RECOVER_TIME;
                }
            }
            else if (entities[i].recover > 0.0f) {
                speed = SPEED * RECOVER_MULT;
                entities[i].recover -= dt;
            }
            else if (entities[i].cooldown <= 0.0f
                     && closestHunter != -1 && minHunterDistance < JUKE_RANGE) {
                // Bolt at 60 degrees off straight-away: keeps ground gained while
                // cutting hard across the hunter's committed line.
                struct Coord away = SafeDirection(hx - entities[closestHunter].position.x,
                                                  hy - entities[closestHunter].position.y);
                float ax = away.x;
                float ay = away.y;
                float s = (rand() & 1) ? 1.0f : -1.0f;
                entities[i].burstDir.x = ax * 0.5f - ay * 0.866f * s;
                entities[i].burstDir.y = ay * 0.5f + ax * 0.866f * s;
                entities[i].burst = BURST_TIME;
                entities[i].cooldown = BURST_TIME + RECOVER_TIME + JUKE_REST;
                direction = entities[i].burstDir;
                speed = BURST_SPEED;
            }
            else if (entities[i].cooldown <= 0.0f
                     && closestVictim != -1 && minDistance < LUNGE_RANGE) {
                // Pick a line and go. No course correction from here.
                entities[i].burstDir = direction;
                entities[i].burst = BURST_TIME;
                entities[i].cooldown = BURST_TIME + RECOVER_TIME + LUNGE_REST;
                speed = BURST_SPEED;
            }

            entities[i].position.x += direction.x * dt * speed;
            entities[i].position.y += direction.y * dt * speed;

            // A burst ignores steering entirely, so it can still reach the edge.
            // Reflect it rather than clamping, so a juke rebounds off the wall.
            if (entities[i].position.x < EDGE) {
                entities[i].position.x = EDGE;
                entities[i].burstDir.x = -entities[i].burstDir.x;
            }
            else if (entities[i].position.x > WINDOW_WIDTH - EDGE) {
                entities[i].position.x = WINDOW_WIDTH - EDGE;
                entities[i].burstDir.x = -entities[i].burstDir.x;
            }

            if (entities[i].position.y < EDGE) {
                entities[i].position.y = EDGE;
                entities[i].burstDir.y = -entities[i].burstDir.y;
            }
            else if (entities[i].position.y > WINDOW_HEIGHT - EDGE) {
                entities[i].position.y = WINDOW_HEIGHT - EDGE;
                entities[i].burstDir.y = -entities[i].burstDir.y;
            }

            if (minDistance < (float)SPRITE_WIDTH * 0.9f) {
                spawnCaptureBurst(entities[closestVictim].position.x,
                                  entities[closestVictim].position.y,
                                  GetTypeColor(t));
                entities[closestVictim].type = t;
            }

        }

        for (int p = 0; p < PARTICLE_COUNT; p += 1) {
            if (particles[p].life <= 0.0f) {
                continue;
            }

            particles[p].life -= dt;
            particles[p].position.x += particles[p].velocity.x * dt;
            particles[p].position.y += particles[p].velocity.y * dt;

            float drag = 1.0f - PARTICLE_DRAG * dt;
            if (drag < 0.0f) {
                drag = 0.0f;
            }
            particles[p].velocity.x *= drag;
            particles[p].velocity.y *= drag;
        }

        for (int s = 0; s < SHOCK_COUNT; s += 1) {
            if (shockwaves[s].life > 0.0f) {
                shockwaves[s].life -= dt;
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

        for (int s = 0; s < SHOCK_COUNT; s += 1) {
            if (shockwaves[s].life <= 0.0f) {
                continue;
            }

            // Expands outward as it dies, thinning and fading as it goes.
            float s01 = shockwaves[s].life / SHOCK_LIFE;
            float grow = 1.0f - s01;

            struct Vector2 spos;
            spos.x = shockwaves[s].position.x;
            spos.y = shockwaves[s].position.y;

            float outer = SHOCK_RADIUS * grow;
            float inner = outer - SHOCK_THICKNESS * s01;
            if (inner < 0.0f) {
                inner = 0.0f;
            }

            Color sc = shockwaves[s].color;
            sc.a = (unsigned char)(230.0f * s01);

            DrawRing(spos, inner, outer, 0.0f, 360.0f, 32, sc);
        }

        for (int p = 0; p < PARTICLE_COUNT; p += 1) {
            if (particles[p].life <= 0.0f) {
                continue;
            }

            // Shrink and fade together over the particle's remaining life.
            float t01 = particles[p].life / PARTICLE_LIFE;
            if (t01 > 1.0f) {
                t01 = 1.0f;
            }

            struct Vector2 ppos;
            ppos.x = particles[p].position.x;
            ppos.y = particles[p].position.y;

            // Flash white at the instant of impact, cooling to the type colour.
            float heat = t01 * t01 * t01;

            Color base = particles[p].color;
            Color pc;
            pc.r = (unsigned char)(base.r + (255.0f - base.r) * heat);
            pc.g = (unsigned char)(base.g + (255.0f - base.g) * heat);
            pc.b = (unsigned char)(base.b + (255.0f - base.b) * heat);
            pc.a = (unsigned char)(255.0f * t01);

            DrawCircleV(ppos, PARTICLE_SIZE * (0.30f + 0.70f * t01), pc);
        }

        unsigned long cstd = __STDC_VERSION__;

        snprintf(str, sizeof(str), "R: %d", rC);
        DrawText(str, 20, 50, 24, BLACK);
        snprintf(str, sizeof(str), "P: %d", pC);
        DrawText(str, 120, 50, 24, BLACK);
        snprintf(str, sizeof(str), "S: %d", sC);
        DrawText(str, 220, 50, 24, BLACK);

        snprintf(str, sizeof(str), "CSTD: %d", cstd);
        DrawText(str, WINDOW_WIDTH - 350, 50, 24, BLACK);
        int fps = GetFPS();
        snprintf(str, sizeof(str), "FPS: %d", fps);
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
