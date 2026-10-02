#ifndef CHUNK_H
#define CHUNK_H

#include "util.h"
#include "camera.h"

/* types of coordinates:
 *  | World - double3 - unbounded
 *  | Voxel - int3   - unbounded
 *  | Chunk - int3   - unbounded
 *  | Local - int3   - [0, CHUNK_DIM)
 *  | Index - size_t - [0, CHUNK_VOLUME)
 *
 * Minimum corner is the default. So voxel v occupies [v, v+1)
 * for each axis, and the same is true for chunks.
 *
 * Chunk dimensions must *always* be a power of 2
 * */

#define CHUNK_DIMENSION_X 32
#define CHUNK_DIMENSION_Y 32
#define CHUNK_DIMENSION_Z 32

#define CHUNK_MASK simd_make_int3(CHUNK_DIMENSION_X-1, CHUNK_DIMENSION_Y-1, CHUNK_DIMENSION_Z-1)
#define CHUNK_SHIFT simd_make_int3(__builtin_ctz(CHUNK_DIMENSION_X), __builtin_ctz(CHUNK_DIMENSION_Y), __builtin_ctz(CHUNK_DIMENSION_Z))
#define CHUNK_DIM simd_make_int3(CHUNK_DIMENSION_X, CHUNK_DIMENSION_Y, CHUNK_DIMENSION_Z)

#define CHUNK_VOLUME ((CHUNK_DIMENSION_X) * (CHUNK_DIMENSION_Y) * (CHUNK_DIMENSION_Z))

// absolute pos in chunks -> voxels
static inline int3 chunk_to_voxel(int3 offset) { return CHUNK_DIM * offset; }

// absolute pos in voxels -> chunks
static inline int3 voxel_to_chunk(int3 offset) { return offset >> CHUNK_SHIFT; }

// absolute pos in voxels -> pos within a chunk [0, CHUNK_MAX)
static inline int3 voxel_to_local(int3 offset) { return offset & CHUNK_MASK; }

// pos within a chunk -> absolute pos within the world (in voxels)
static inline int3 local_to_voxel(int3 voxel_offset, int3 chunk_offset) { return voxel_offset + chunk_to_voxel(chunk_offset); }

// pos within a chunk -> index into chunk data
static inline size_t local_to_index(int3 chunk_pos) {
    return (chunk_pos.x + (chunk_pos.y * CHUNK_DIMENSION_X) + (chunk_pos.z * (CHUNK_DIMENSION_X * CHUNK_DIMENSION_Y)));
}

// index into chunk data -> pos within a chunk
static inline int3 index_to_local(size_t idx) {
    return simd_make_int3(
        (int)(idx & (CHUNK_MASK.x)),
        (int)((idx >> CHUNK_SHIFT.x) & CHUNK_MASK.y),
        (int)(idx >> (CHUNK_SHIFT.x + CHUNK_SHIFT.y))
    );
}

// convert continuous world coords to voxels
static inline int3 world_to_voxel(double3 coord) { return simd_int_sat(floor(coord)); }

// convert voxels to world coords
static inline double3 voxel_to_world(int3 offset) { return simd_double(offset); }

enum VoxelType {
    AIR = 0,
    SOLID = 1
};

struct MeshBuffer {
    u32 vertex_count;

    u32 index_count;

    sg_buffer vbuf;
    sg_buffer ibuf;
};

struct Chunk {
    // world position in chunks
    int3 chunk_pos;

    // array of voxel data for this chunk
    u8 *data;

    struct MeshBuffer mesh;

    // whether the chunk is loaded/unloaded, etc.
    struct {
        bool allocated: 1;
        bool initialized: 1;
        bool meshing: 1;
        bool meshed: 1;
    } flags;

};


void chunk_alloc(struct Chunk *chunk);
void chunk_init(struct Chunk *chunk, int3 chunk_pos);
void chunk_mesh(struct Chunk *chunk);
void chunk_render(const struct Chunk *chunk, simd_float4x4 proj_view, double3 cam_pos);
void chunk_uninit(struct Chunk *chunk);
void chunk_destroy(struct Chunk *chunk);

#endif  /* CHUNK_H */
