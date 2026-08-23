#ifndef CAMERA_H
#define CAMERA_H

#define MOUSE_SENSITIVITY 0.01

#include "util.h"
#include "input.h"

struct Camera {
    double3 pos;
    f32 pitch, yaw;
    f32 fovy, near, far, aspect;
    simd_float4x4 view, proj;
};

void camera_init(struct Camera *cam, f32 fovy, f32 near, f32 far, f32 aspect);
void camera_update(struct Camera *cam, const struct Input *input);

#endif /* CAMERA_H */
