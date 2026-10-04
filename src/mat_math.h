#ifndef MATH_H
#define MATH_H

#include "util.h"

#define PI (3.14159)

inline static f32 radians(f32 deg) {
    return deg * (PI/180.0);
}

static inline float3 derive_forward(f32 pitch, f32 yaw) {
    f32 cosp = cos(pitch), sinp = sin(pitch);
    f32 cosy = cos(yaw), siny = sin(yaw);
    return simd_make_float3(siny * cosp, sinp, -cosy * cosp);
}

inline static simd_float4x4 mat_translation(float3 t) {
    return (simd_float4x4) {{
        {1.0, 0.0, 0.0, 0.0}, 
        {0.0, 1.0, 0.0, 0.0}, 
        {0.0, 0.0, 1.0, 0.0}, 
        {t.x, t.y, t.z, 1.0}
    }};
}

inline static simd_float4x4 mat_scale(float3 s) {
    return (simd_float4x4) {{
        {s.x, 0.0, 0.0, 0.0}, 
        {0.0, s.y, 0.0, 0.0}, 
        {0.0, 0.0, s.z, 0.0}, 
        {0.0, 0.0, 0.0, 1.0}
    }};
}

inline static simd_float4x4 mat_rotation(f32 radians, float3 axis) {
    axis = simd_normalize(axis);
    f32 c = cos(radians), s = sin(radians), t = 1.0 - c;
    f32 x = axis.x, y = axis.y, z = axis.z;
    return (simd_float4x4) {{
        {t*x*x + c,   t*x*y + s*z, t*x*z - s*y, 0.0},
        {t*x*y - s*z, t*y*y + c,   t*y*z + s*x, 0.0},
        {t*x*z + s*y, t*y*z - s*x, t*z*z + c,   0.0},
        {0.0,         0.0,         0.0,         1.0},
    }};
}

inline static simd_float4x4 mat_view_dir(float3 dir, float3 up) {
    float3 f = simd_normalize(-dir);
    float3 r = simd_normalize(simd_cross(up, f));
    float3 u = simd_cross(f, r);
    return (simd_float4x4){{
        {r.x, u.x, f.x, 0},
        {r.y, u.y, f.y, 0},
        {r.z, u.z, f.z, 0},
        {0,   0,   0,   1},
    }};
}

inline static simd_float4x4 mat_proj(float fovy, float aspect, float near) {
    float ys = 1.0 / tanf(fovy * 0.5);
    float xs = ys / aspect;
    return (simd_float4x4){{
        {xs,  0.0, 0.0,  0.0},
        {0.0, ys,  0.0,  0.0},
        {0.0, 0.0, 0.0, -1.0},
        {0.0, 0.0, near, 0.0},
    }};
}

#endif /* MATH_H */
