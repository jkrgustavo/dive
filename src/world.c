#include "world.h"

void world_init(struct World *world, u32 chunk_count) {
    assert(world->chunks == NULL);
    world->chunk_count = chunk_count;

    world->chunks = calloc(world->chunk_count, sizeof(struct Chunk*));
    for (u32 c = 0; c < chunk_count; c++) {
        struct Chunk *cx = malloc(sizeof(struct Chunk));
        chunk_init(cx, simd_make_int3((c/2),  -1, (c%2)));
        assert(cx->flags.initialized);
        memset(cx->data, 1, sizeof(u8) * CHUNK_VOLUME/2);
        world->chunks[c] = cx;
    }
}

// TODO: some way of deriving the chunk position near the player
// TODO: only update chunks near the player and within the rendering distance
void world_update(struct World *world) {
    for (u32 c = 0; c < world->chunk_count; c++) {
        struct Chunk *cx = world->chunks[c];

        if (!cx->flags.initialized)
            chunk_init(cx, simd_make_int3((c/2),  -1, (c%2)));

        if (!cx->flags.meshed)
            chunk_mesh(cx);
    }
}

void world_render(struct World *world, double3 camera_pos) {

    for (u32 c = 0; c < world->chunk_count; c++) {
        chunk_render(world->chunks[c], camera_pos);
    }

}

void world_destory(struct World *world) {
    for (u32 c = 0; c < world->chunk_count; c++) {
        free(world->chunks[c]);
        world->chunks[c] = NULL;
    }
    world->chunk_count = 0;
}
