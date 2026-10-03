#ifndef WORLD_H
#define WORLD_H

#include "util.h"
#include "chunk.h"

/* Types of coordinates:
 *  | World - double3 - unbounded
 *  | Voxel - int3   - unbounded
 *  | Chunk - int3   - unbounded
 *  | Local - int3   - [0, CHUNK_DIM)
 *  | Index - size_t - [0, CHUNK_VOLUME)
 *
 * Minimum corner is the default. So voxel v occupies [v, v+1)
 * for each axis, and the same is true for chunks.
 */

struct World {
    u32 chunk_count;

    struct Chunk **chunks;
};


void world_init(struct World *world, u32 chunk_count);
void world_update(struct World *world);
void world_render(struct World *world, double3 camera_pos);
void world_destory(struct World *world);

#endif /* WORLD_H */
