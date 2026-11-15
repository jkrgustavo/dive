#ifndef VOXEL_H
#define VOXEL_H

#include "utils/util.h"

#define VOXEL_SIZE 0.1

typedef u8 voxel;

// convert continuous world coords to voxels
static inline int3 world_to_voxel(const double3 coord) { return simd_int_sat(floor(coord / VOXEL_SIZE)); }

// convert voxels to world coords
static inline double3 voxel_to_world(const int3 world_vox_pos) { return simd_double(world_vox_pos) * VOXEL_SIZE; }


enum Direction {
    FRONT = 0,  // +z
    BACK,       // -z
    LEFT,       // +x
    RIGHT,      // -x
    UP,         // +y
    DOWN        // -y
};

static const int3 dir_offset[] = {
    {  0,  0,  1 }, // front +z
    {  0,  0, -1 }, // back  -z
    {  1,  0,  0 }, // left  +x
    { -1,  0,  0 }, // right -x
    {  0,  1,  0 }, // up    +y
    {  0, -1,  0 }, // down  -y
};

enum VoxelColor {
    AIR = 0,
    RED,
    ORANGE,
    YELLOW,
    GREEN,
    BLUE,
    PURPLE,
    GREY,
    INVALID = 255
};

enum MaterialProps {
    MAT_OPAQUE   = 1,
    MAT_COLLIDES = 2,
    MAT_LIQUID   = 4
};

struct Material {
    u32 color;
    u8 flags;
};

extern const struct Material materials[256];

void palette_colors(u32 out[256]);

#endif  /* VOXEL_H */
