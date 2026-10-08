#ifndef PLAYER_H
#define PLAYER_H

#include "util.h"
#include "input.h"

struct Player {
    // Player's position in the world
    double3 position;

    // Speed in voxels
    double3 movespeed;

    f32 mouse_sensitivity;

    // Height in voxels, camera at the top
    u8 height;

    struct {
        bool sprint: 1;
    } flags;
};

#include "camera.h"

void player_init(struct Player *player, double3 pos);
void player_update(struct Player *player, const struct Camera *cam, const struct Input *input);

#endif /* PLAYER_H */
