#include "voxel.h"

#define MAT(abgr, flag) { (abgr), (u8)((flag) | (((abgr) >> 24) == 0xFF ? MAT_OPAQUE : 0))}

const struct Material materials[256] = {
    [AIR]       = {0},
    [RED]       = MAT(0xFF0000FF, MAT_COLLIDES),
    [ORANGE]    = MAT(0xFF0099FF, MAT_COLLIDES),
    [YELLOW]    = MAT(0xFF00FFFF, MAT_COLLIDES),
    [GREEN]     = MAT(0xFF33DD33, MAT_COLLIDES),
    [BLUE]      = MAT(0xFFFF0000, MAT_COLLIDES),
    [PURPLE]    = MAT(0xFFFF00FF, MAT_COLLIDES),
    [GREY]      = MAT(0xFFAAAAAA, MAT_COLLIDES),
    [WATER]     = MAT(0xAAFF6644, MAT_LIQUID),
    [INVALID]   = MAT(0xFFFF00FF, MAT_COLLIDES),
};

void palette_colors(u32 out[256]) {
    for (u32 i = 0; i < 256; i++) {
        struct Material mat = materials[i];
        out[i] = mat.color;
    }
}
