#ifndef CHUNK_H
#define CHUNK_H

#include "utils/util.h"

/* Types of coordinates:
 *  | World - double3 - unbounded
 *  | Voxel - int3   - unbounded
 *  | Chunk - int3   - unbounded
 *  | Local - int3   - [0, CHUNK_DIM)
 *  | Index - size_t - [0, CHUNK_VOLUME)
 *
 * Minimum corner is the default. So voxel v occupies [v, v+1)
 * for each axis, and the same is true for chunks.
 * Chunk dimensions must *always* be a power of 2
 *
 * Quad vert packing scheme:
 *  | x pos  - 6
 *  | y pos  - 6
 *  | z pos  - 6
 *  | width  - 6
 *  | height - 6
 *  | dir    - 3
 *  | reserved - 31
 *
 * Pos refers to the local coords within a chunk.
 * Fits inside a single u64, unpack inside vertex shader, and
 * lives in the MeshBuffer.
 */

#define VOXEL_SIZE 0.1

#define CHUNK_DIM_X 32
#define CHUNK_DIM_Y 32
#define CHUNK_DIM_Z 32

#define CHUNK_MASK simd_make_int3(CHUNK_DIM_X-1, CHUNK_DIM_Y-1, CHUNK_DIM_Z-1)
#define CHUNK_SHIFT simd_make_int3(__builtin_ctz(CHUNK_DIM_X), __builtin_ctz(CHUNK_DIM_Y), __builtin_ctz(CHUNK_DIM_Z))
#define CHUNK_DIM simd_make_int3(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z)

#define CHUNK_VOLUME ((CHUNK_DIM_X) * (CHUNK_DIM_Y) * (CHUNK_DIM_Z))

// absolute pos in chunks -> voxels
static inline int3 chunk_to_voxel(const int3 chunk_pos) { return CHUNK_DIM * chunk_pos; }

// absolute pos in voxels -> chunks
static inline int3 voxel_to_chunk(const int3 vox_pos) { return vox_pos >> CHUNK_SHIFT; }

// absolute pos in voxels -> pos within a chunk [0, CHUNK_MAX)
static inline int3 voxel_to_local(const int3 vox_pos) { return vox_pos & CHUNK_MASK; }

// pos within a chunk -> absolute pos within the world (in voxels)
static inline int3 local_to_voxel(const int3 local_pos, const int3 chunk_pos) { return local_pos + chunk_to_voxel(chunk_pos); }

// pos within a chunk -> index into chunk data
static inline u32 local_to_chunk_index(const int3 local_pos) {
    return (local_pos.x + (local_pos.y * CHUNK_DIM_X) + (local_pos.z * (CHUNK_DIM_X * CHUNK_DIM_Y)));
}

// index into chunk data -> pos within a chunk
static inline int3 chunk_index_to_local(const u32 idx) {
    return simd_make_int3(
        (i32)(idx & (CHUNK_MASK.x)),
        (i32)((idx >> CHUNK_SHIFT.x) & CHUNK_MASK.y),
        (i32)(idx >> (CHUNK_SHIFT.x + CHUNK_SHIFT.y))
    );
}

// convert continuous world coords to voxels
static inline int3 world_to_voxel(const double3 coord) { return simd_int_sat(floor(coord / VOXEL_SIZE)); }

// convert voxels to world coords
static inline double3 voxel_to_world(const int3 world_vox_pos) { return simd_double(world_vox_pos) * VOXEL_SIZE; }


// takes a local voxel pos and checks 
static inline bool voxel_in_chunk(const int3 local_pos) {
    return simd_all(local_pos >= 0) && simd_all(local_pos < CHUNK_DIM);
}

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

struct MeshScratch {
    // scratch bs_buffer for quad generation
    struct QuadBuf {
        u64 *items;
        size_t count, capacity;
    } quads;

    // padded chunk data to be sent to the GPU
    u8 padded_data[(CHUNK_DIM_X + 2) * (CHUNK_DIM_Y + 2) * (CHUNK_DIM_Z + 2)];
};

struct MeshBuffer {
    u32 vertex_count;

    sg_view vbuf_view;
    sg_buffer vbuf;

    sg_view paddbuf_view;
    sg_buffer paddbuf;
};

struct Chunk {
    // world position in chunks
    int3 position;

    // array of voxel data for this chunk.
    // data is an index into a color palette [0, 255]
    u8 *data;

    struct MeshBuffer mesh;

    // whether the chunk is loaded/unloaded, etc.
    struct {
        bool initialized: 1;
        bool meshing: 1;
        bool meshed: 1;
        bool empty: 1;
        bool dirty: 1;
    } flags;

};


void chunk_init(struct Chunk *chunk, int3 chunk_pos);
void chunk_set_block(struct Chunk *chunk, int3 local_pos, enum VoxelColor id);
void chunk_mesh(struct Chunk *chunk, const struct Chunk *neighbors[27], struct MeshScratch *scratch);
void chunk_render(const struct Chunk *chunk, sg_buffer ibuf, sg_view palette, double3 cam_pos);
void chunk_reset(struct Chunk *chunk);
void chunk_destroy(struct Chunk *chunk);

#endif  /* CHUNK_H */
