#ifndef CAMERA_H
#define CAMERA_H


#include "util.h"
#include "input.h"

struct Camera {
    double3 pos;
    f32 pitch, yaw;
    f32 fovy, near, far, aspect;
    simd_float4x4 view, proj;
};

#include "player.h"

void camera_init(struct Camera *cam, const struct Player *player, f32 fovy, f32 near, f32 far);
void camera_update(struct Camera *cam, const struct Player *player, const struct Input *input);

#endif /* CAMERA_H */
