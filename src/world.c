#include "world.h"
#include <stdlib.h>

static void generate_terrain(struct Chunk *chunk) {
    if (chunk->position.y > 1) {
        memset(chunk->data, 0, sizeof(u8) * CHUNK_VOLUME/2);
        return;
    }

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


int cmp(const void *a, const void *b) {
    double3 pos_a = simd_double(*(int3*)a);
    double3 pos_b = simd_double(*(int3*)b);

    double d_a = simd_distance_squared(pos_a, simd_make_double3(0.0));
    double d_b = simd_distance_squared(pos_b, simd_make_double3(0.0));

    return d_a - d_b;
}

static void build_offsets(struct World *world) {
    int3 offsets[WORLD_VOLUME] = {0};
    u32 n = 0;
    for (u32 c = 0; c < WORLD_VOLUME; c++) {
        int3 off = world_index_to_chunk(c, world->position) - world->center;
        if (off.x * off.x + off.z * off.z <= world->view_radius.x * (world->view_radius.z + 1)
                && abs(off.y) <= world->view_radius.y
        ) {
            offsets[n++] = off;
        }
    }

    assert(n > 0);
    qsort(offsets, n, sizeof(int3), cmp);
    world->offset_count = n;
    world->offsets = malloc(sizeof(int3) * n);
    memcpy(world->offsets, offsets, sizeof(int3) * n);
}

static void load_chunk(struct World *world, struct Chunk *cx, int3 chunk_pos) {
    chunk_init(cx, chunk_pos);
    generate_terrain(cx);

    for (u32 dir = 0; dir < 6; dir++) {
        struct Chunk *nc = world_get_chunk(world, cx->position + dir_offset[dir]);
        if (nc != NULL) nc->flags.dirty = true;
    }

}

static void world_recenter(struct World *world) {
    for (u32 i = 0; i < world->offset_count; i++) {
        int3 chunk_pos = world->center + world->offsets[i];
        u32 cidx = chunk_to_world_index(chunk_pos);
        if (world->chunks[cidx] == NULL) { 
            world->chunks[cidx] = calloc(1, sizeof(struct Chunk));
        }

        struct Chunk *cx = world->chunks[cidx];
        if (cx->flags.initialized && simd_all(cx->position == chunk_pos)) 
            continue;

        if (world->load_limits.count >= world->load_limits.max) 
            break;

        if (cx->flags.initialized)
            chunk_reset(cx);
        
        load_chunk(world, cx, chunk_pos);
        world->load_limits.count++;
    }
}

void world_init(struct World *world) {
    assert(world->chunks == NULL);
    world->chunk_count = WORLD_VOLUME;

    world->position = -WORLD_DIM + (WORLD_DIM/2);
    world->center = world->position + (WORLD_DIM/2);

    world->view_radius = WORLD_DIM/2 - 1;

    world->chunks = calloc(world->chunk_count, sizeof(struct Chunk*));

    world->load_limits.max = 5;
    world->load_limits.count = 0;
    world->mesh_limits.max = 1;
    world->mesh_limits.count = 0;

    build_offsets(world);

    world_recenter(world);
}

static void world_mesh(struct World *world) {
    for (u32 i = 0; i < world->offset_count; i++) {
        struct Chunk *cx = world_get_chunk(world, world->center + world->offsets[i]);
        if (cx == NULL) continue;


        if ((!cx->flags.meshed || cx->flags.dirty) 
            && (world->mesh_limits.count < world->mesh_limits.max)
        ) {
            const struct Chunk *neighbors[6];
            for (u32 dir = 0; dir < 6; dir++) {
                neighbors[dir] = world_get_chunk(world, cx->position + dir_offset[dir]);
            }
            chunk_mesh(cx, neighbors);
            world->mesh_limits.count++;
        }

    }
}

void world_update(struct World *world, struct Player *player) {
    world->center = voxel_to_chunk(world_to_voxel(player->position));
    world->position = world->center - WORLD_DIM/2;
    world->load_limits.count = 0;
    world->mesh_limits.count = 0;

    world_recenter(world);
    world_mesh(world);
}

void world_render(struct World *world, double3 camera_pos) {
    for (u32 i = 0; i < world->offset_count; i++) {
        struct Chunk *cx = world_get_chunk(world, world->center + world->offsets[i]);
        if (cx == NULL) { continue; }

        if (cx->flags.initialized && cx->flags.meshed)
            chunk_render(cx, camera_pos);
    }
}

void world_destroy(struct World *world) {
    for (u32 c = 0; c < world->chunk_count; c++) {
        if (world->chunks[c] == NULL) continue;
        chunk_destroy(world->chunks[c]);
        free(world->chunks[c]);
        world->chunks[c] = NULL;
    }
    free(world->chunks);
    free(world->offsets);
    world->chunk_count = 0;
    world->offset_count = 0;
}
