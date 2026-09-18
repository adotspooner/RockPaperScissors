#ifndef PARTICLES_H
#define PARTICLES_H

#include "raylib.h"

void psReset(unsigned long long seed);
void psSpawnCaptureBurst(float x, float y, Color color);
void psUpdate(float dt);
void psDraw(void);

#endif // PARTICLES_H
