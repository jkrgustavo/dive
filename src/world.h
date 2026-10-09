#ifndef WORLD_H
#define WORLD_H

#include "util.h"
#include "chunk.h"
#include "player.h"

/* Types of coordinates:
 *  | World  - double3 - unbounded
 *  | Voxel  - int3    - unbounded
 *  | Chunk  - int3    - unbounded
 *  | Local  - int3    - [0, CHUNK_DIM)
 *  | Index  - u32     - [0, CHUNK_VOLUME)
 *
 * Offsets:
 *  | Offset - int3   - (-(dimension/2), (dimension/2))
 *
 * Minimum corner is the default for positions. So 
 * voxel v occupies [v, v+1) for each axis, and the same 
 * is true for chunks.
 *
 * Each world dimension *must* be a power of 2!
 *
 * Offsets are based off the world center.
 */

#define WORLD_SIZE_X 8
#define WORLD_SIZE_Y 4
#define WORLD_SIZE_Z 8

#define WORLD_MASK simd_make_int3(WORLD_SIZE_X-1, WORLD_SIZE_Y-1, WORLD_SIZE_Z-1)
#define WORLD_SHIFT simd_make_int3(__builtin_ctz(WORLD_SIZE_X), __builtin_ctz(WORLD_SIZE_Y), __builtin_ctz(WORLD_SIZE_Z))
#define WORLD_DIM simd_make_int3(WORLD_SIZE_X, WORLD_SIZE_Y, WORLD_SIZE_Z)

#define WORLD_VOLUME (WORLD_SIZE_X * WORLD_SIZE_Y * WORLD_SIZE_Z)

// abs pos in chunks -> index into world's chunk array
static inline u32 chunk_to_world_index(int3 chunk_pos) {
    int3 s = chunk_pos & WORLD_MASK;
    return s.x + (s.y * WORLD_SIZE_X) + (s.z * (WORLD_SIZE_X * WORLD_SIZE_Y));
}

// index into world's chunk array -> abs position in chunks
static inline int3 world_index_to_chunk(u32 idx, int3 world_pos) {
    int3 s = simd_make_int3(
        (i32)(idx & (WORLD_MASK.x)),
        (i32)((idx >> WORLD_SHIFT.x) & WORLD_MASK.y),
        (i32)(idx >> (WORLD_SHIFT.x + WORLD_SHIFT.y))
    ); 
    return world_pos + ((s - world_pos) & WORLD_MASK);
}

struct World {
    // Total number of chunks in the chunk array
    u32 chunk_count;

    // World position in chunks
    int3 position; 

    // Position of world's center in chunks
    int3 center;

    // Chunk offsets from world center. Sorted by distance from world center
    int3 *offsets;
    u32 offset_count;

    // View radius, x == z, y is the height of the cylinder
    int3 view_radius;

    // Shared index buffer across all chunks
    sg_buffer index_buffer;

    // Tracks and limits how many chunks are processed in a frame
    struct {
        u32 count, max;
    } load_limits, mesh_limits;

    // array of chunks that are currently loaded
    struct Chunk **chunks;
};

// takes a chunk position and checks if it's within the loaded window
static inline bool chunk_in_bounds(struct World *world, int3 chunk_pos) {
    return simd_all(chunk_pos >= world->position) && simd_all(chunk_pos < (world->position + WORLD_DIM));
}

static inline struct Chunk *world_get_chunk(struct World *world, int3 chunk_pos) {
    struct Chunk *c = world->chunks[chunk_to_world_index(chunk_pos)];
    if (c == NULL || !c->flags.initialized || !simd_all(c->position == chunk_pos)) {
        return NULL;
    } else {
        return c;
    }     
}

void world_init(struct World *world);
void world_update(struct World *world, struct Player *player);
void world_render(struct World *world, double3 camera_pos);
void world_destroy(struct World *world);

#endif /* WORLD_H */
