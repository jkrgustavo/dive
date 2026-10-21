#ifndef PLAYER_H
#define PLAYER_H

#include "util.h"
#include "input.h"

struct AABB { double3 min, max; };

struct Player {
    // Player's position in the world
    double3 position, velocity;

    // movement in meters
    double3 movespeed, sprintspeed, jumpheight;

    f32 mouse_sensitivity;

    // Player size in meters
    double3 size;

    struct {
        bool sprint: 1;
        bool gravity: 1;
        bool collisions: 1;
        bool grounded: 1;
    } flags;
};

#include "camera.h"
#include "world.h"

void player_init(struct Player *player, double3 pos);
void player_update(struct Player *player, const struct World *world, const struct Camera *cam, const struct Input *input);

#endif /* PLAYER_H */
