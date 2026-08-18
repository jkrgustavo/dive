#ifndef CAMERA_H
#define CAMERA_H

#include "util.h"

struct Camera {
    double3 pos;
    float3 forward, up;
    float fovy, near, far, aspect;
    simd_float4x4 view, proj;
};

void camera_init(struct Camera *cam, float fovy, float near, float far, float aspect);
void camera_update(struct Camera *cam, float3 forward);

#endif /* CAMERA_H */
