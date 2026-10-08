#include "world.h"

void generate_terrain(struct Chunk *chunk) {
    i32 ground_height = 0;
    for (u32 v = 0; v < CHUNK_VOLUME; v++) {
        int3 pos_w = local_to_voxel(chunk_index_to_local(v), chunk->position);
        ground_height = sin(pos_w.x / 2.5) * 3;
        if (pos_w.y < ground_height) {
            chunk->data[v] = SOLID;
        } else {
            chunk->data[v] = AIR;
        }

    }
}

void world_init(struct World *world) {
    assert(world->chunks == NULL);
    world->chunk_count = WORLD_VOLUME;
    world->position = -WORLD_DIM + (WORLD_DIM/2);
    world->center = world->position + (WORLD_DIM/2);

    world->chunks = calloc(world->chunk_count, sizeof(struct Chunk*));
    for (u32 c = 0; c < world->chunk_count; c++) {
        struct Chunk *cx = calloc(1, sizeof(struct Chunk));
        int3 chunk_pos = world_index_to_chunk(c, world->position);

        chunk_init(cx, chunk_pos);
        assert(cx->flags.initialized);

        generate_terrain(cx);
        world->chunks[c] = cx;
    }
}

void world_update(struct World *world) {
    for (u32 c = 0; c < world->chunk_count; c++) {
        struct Chunk *cx = world->chunks[c];

        if (!cx->flags.initialized)
            chunk_init(cx, world_index_to_chunk(c, world->position));

        if (!cx->flags.meshed) {
            const struct Chunk *neighbors[6];
            for (u32 dir = 0; dir < 6; dir++) {
                neighbors[dir] = world_get_chunk(world, cx->position + dir_offset[dir]);
            }
            chunk_mesh(cx, neighbors);
        }
    }
}

void world_render(struct World *world, double3 camera_pos) {
    for (u32 c = 0; c < world->chunk_count; c++) {
        chunk_render(world->chunks[c], camera_pos);
    }
}

void world_destroy(struct World *world) {
    for (u32 c = 0; c < world->chunk_count; c++) {
        chunk_destroy(world->chunks[c]);
        free(world->chunks[c]);
        world->chunks[c] = NULL;
    }
    free(world->chunks);
    world->chunk_count = 0;
}
