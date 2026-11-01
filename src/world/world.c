#include <ba_buffer.h>
#include <tgmath.h>

#include "world.h"
#include "jobs.h"

static void build_ibuf(struct World *world) {
    const u32 face_indices[] = { 0, 1, 2,  0, 2, 3 };
    u32 quad_count = CHUNK_VOLUME * 3;

    u32 *temp_index_buffer = malloc(sizeof(u32) * quad_count * 6);

    for (u32 q = 0; q < quad_count; q++) {
        u32 base = q * 4;
        for (u32 i = 0; i < 6; i++) {
            temp_index_buffer[q * 6 + i] = base + face_indices[i];
        }
    }

    world->index_buffer = sg_make_buffer(&(sg_buffer_desc) {
        .usage.index_buffer = true,
        .data = {
            .ptr = temp_index_buffer,
            .size = sizeof(u32) * quad_count * 6
        }
    });
    free(temp_index_buffer);
}

static struct ChunkJob *chunk_job_get(struct World *world, enum ChunkJobType type) {
    // chunks are generated first, this prevents generation from hogging the pool
    if (type == JOB_GENERATE && world->gen_limit.in_flight >= world->gen_limit.max)
        return NULL;

    for (u32 i = 0; i < JOB_POOL_SIZE; i++) {
        if (!world->job_pool[i].in_use) {
            world->job_pool[i].in_use = true;
            world->job_pool[i].type = type;
            world->gen_limit.in_flight += (type == JOB_GENERATE);
            return &world->job_pool[i];
        }
    }
    // pool exausted
    return NULL;
}

static void chunk_job_release(struct World *world, struct ChunkJob *job) {
    world->gen_limit.in_flight -= (job->type == JOB_GENERATE);
    job->in_use = false;
}

static void build_palette(struct World *world) {
    u32 palette[256] = {0};
    palette[RED] = 0xFF0000FF;
    palette[ORANGE] = 0xFF0099FF;
    palette[YELLOW] = 0xFF00FFFF;
    palette[GREEN] = 0xFF33DD33;
    palette[BLUE] = 0xFFFF0000;
    palette[PURPLE] = 0xFFFF00FF;
    palette[GREY] = 0xFFAAAAAA;
    palette[INVALID] = palette[PURPLE];

    world->palette.palette_buf = sg_make_buffer(&(sg_buffer_desc) {
        .data = SG_RANGE(palette),
        .usage.storage_buffer = true
    });
    world->palette.palette_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = world->palette.palette_buf
    });
}


static void chunk_job_run(struct Job *job) {
    struct ChunkJob *cjob = (struct ChunkJob *)job;
    if (cjob->type == JOB_GENERATE) {
        chunk_generate(cjob->voxels, cjob->chunk_pos, cjob->seed, &cjob->solid, &cjob->empty);
    } else {
        chunk_mesh(&cjob->scratch);
    }
}

static int cmp(const void *a, const void *b) {
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
        if (offset_in_view(off)) {
            offsets[n++] = off;
        }
    }

    bs_assert(n > 0, "somehow no offsets");
    qsort(offsets, n, sizeof(int3), cmp);
    world->offset_count = n;
    world->offsets = malloc(sizeof(int3) * n);
    memcpy(world->offsets, offsets, sizeof(int3) * n);
}

// sets the chunk-job's fields
static void load_chunk(struct World *world, struct Chunk *cx, struct ChunkJob *cjob, int3 chunk_pos) {
    chunk_init(cx, chunk_pos);
    cjob->job.run = chunk_job_run;
    cjob->type = JOB_GENERATE;
    cjob->chunk_pos = chunk_pos;
    cjob->version = cx->version;
    if (!cjob->voxels) cjob->voxels = malloc(sizeof(u8) * CHUNK_VOLUME);
    cjob->seed = world->seed;
    cx->state = CHUNK_LOADING;
    jobs_submit(world->job_system, &cjob->job);
}

static void world_recenter(struct World *world) {
    for (u32 i = 0; i < world->offset_count; i++) {

        int3 chunk_pos = world->center + world->offsets[i];
        u32 cidx = chunk_to_world_index(chunk_pos);
        if (world->chunks[cidx] == NULL) { 
            world->chunks[cidx] = calloc(1, sizeof(struct Chunk));
            world->chunks[cidx]->version = 0;
        }

        struct Chunk *cx = world->chunks[cidx];

        if (cx->state != CHUNK_UNLOADED && simd_all(cx->position == chunk_pos)) continue;

        struct ChunkJob *cjob = chunk_job_get(world, JOB_GENERATE);
        if (!cjob) break;

        if (cx->state != CHUNK_UNLOADED) chunk_reset(cx);
        
        load_chunk(world, cx, cjob, chunk_pos);
    }
}

void world_init(struct World *world, struct JobSystem *job_sys) {
    bs_assert(world->chunks == NULL, "Multiple calls to init");
    world->seed = 100;

    world->chunk_count = WORLD_VOLUME;

    world->position = -WORLD_DIM + (WORLD_DIM/2);
    world->center = world->position + (WORLD_DIM/2);

    world->view_radius = WORLD_VIEW_RAD;

    world->chunks = calloc(world->chunk_count, sizeof(struct Chunk*));

    world->job_pool = calloc(JOB_POOL_SIZE, sizeof(struct ChunkJob));
    world->job_system = job_sys;

    world->gen_limit.max = floor(JOB_POOL_SIZE / 2.0) + 2;
    world->gen_limit.in_flight = 0;

    build_ibuf(world);
    build_palette(world);
    build_offsets(world);
    world_recenter(world);
}

static void world_mesh(struct World *world) {
    for (u32 i = 0; i < world->offset_count; i++) {

        struct Chunk *cx = world_get_chunk(world, world->center + world->offsets[i]);
        if (!cx || cx->flags.empty 
                || cx->state == CHUNK_LOADING 
                || cx->state == CHUNK_MESHING 
                || (cx->state == CHUNK_READY && !cx->flags.dirty)) 
            continue; 

        bool neighbor_loading = false;
        bool buried = cx->flags.solid;
        const struct Chunk *neighbors[27];
        for (u32 n = 0; n < 27; n++) {
            // 3x3x3 block around the chunk. idx 13 is the current chunk
            int3 neighbor_offset = simd_make_int3(n%3-1, (n/3)%3-1, n/9-1);
            int3 neighbor_pos = cx->position + neighbor_offset;
            neighbors[n] = world_get_chunk(world, neighbor_pos);

            bool face_neighbor = simd_reduce_add(simd_abs(neighbor_offset)) == 1;
            if (neighbors[n] && face_neighbor && (!cx->flags.solid || !neighbors[n]->flags.solid)) 
                buried = false;

            if (!neighbors[n] && offset_in_view(neighbor_pos - world->center)) 
                neighbor_loading = true;
        }

        if (neighbor_loading) continue;
        if (buried) {
            chunk_set_buried(cx);
            continue;
        }

        struct ChunkJob *cjob = chunk_job_get(world, JOB_MESH);
        if (!cjob) break;

        if (!chunk_prepare_mesh(cx, neighbors, &cjob->scratch)) {
            chunk_job_release(world, cjob);
            continue;
        }
        cjob->type = JOB_MESH;
        cjob->version = cx->version;
        cjob->job.run = chunk_job_run;
        cjob->chunk_pos = cx->position;
        jobs_submit(world->job_system, &cjob->job);
    }
}

static void world_apply_finished_jobs(struct World *world) {
    struct Job *job;
    while ((job = jobs_poll(world->job_system)) != NULL) {
        struct ChunkJob *cjob = (struct ChunkJob *)job;
        struct Chunk *chunk = world->chunks[chunk_to_world_index(cjob->chunk_pos)];

        bool same = simd_all(chunk->position == cjob->chunk_pos);
        bool fresh = same && chunk->version == cjob->version;

        if (cjob->type == JOB_MESH && same && chunk->state == CHUNK_MESHING) {
            chunk_apply_mesh(chunk, cjob, fresh);
            if (!fresh) chunk->flags.dirty = true;
        }
        if (fresh && cjob->type == JOB_GENERATE && chunk->state == CHUNK_LOADING) {
            chunk_apply_terrain(chunk, cjob);
            // mark any neighbor chunks in a 3x3x3 grid that are meshed as dirty
            for (u32 i = 0; i < 27; i++) {
                if (i == 13) continue;
                int3 neighbor_pos = chunk->position + simd_make_int3(i%3-1, (i/3)%3-1, i/9-1);
                struct Chunk *nc = world_get_chunk(world, neighbor_pos);
                if (nc && nc->state == CHUNK_READY && !nc->flags.empty) {
                    nc->flags.dirty = true;
                    nc->version++;
                } else if (nc && nc->state == CHUNK_MESHING) {
                    nc->flags.dirty = true;
                }
            }
        }
        chunk_job_release(world, cjob);
    }
}

void world_update(struct World *world, double3 player_pos) {
    world->center = voxel_to_chunk(world_to_voxel(player_pos));
    world->position = world->center - WORLD_DIM/2;

    world_apply_finished_jobs(world);
    world_recenter(world);
    world_mesh(world);
}

void world_render(struct World *world, double3 camera_pos) {
    for (u32 i = 0; i < world->offset_count; i++) {
        struct Chunk *cx = world_get_chunk(world, world->center + world->offsets[i]);
        if (cx == NULL) { continue; }

        if (cx->state == CHUNK_READY || cx->state == CHUNK_MESHING)
            chunk_render(cx, world->index_buffer, world->palette.palette_view, camera_pos);
    }
}

void world_destroy(struct World *world) {
    for (u32 c = 0; c < world->chunk_count; c++) {
        if (world->chunks[c] == NULL) continue;
        chunk_destroy(world->chunks[c]);
        free(world->chunks[c]);
        world->chunks[c] = NULL;
    }
    sg_destroy_buffer(world->index_buffer);
    sg_destroy_view(world->palette.palette_view);
    sg_destroy_buffer(world->palette.palette_buf);
    free(world->chunks);
    free(world->offsets);
    for (u32 i = 0; i < JOB_POOL_SIZE; i++) {
        struct ChunkJob *cjob = &world->job_pool[i];
        bs_buffer_free(&cjob->scratch.quads);
        if (cjob->voxels)
            free(cjob->voxels);
    }
    free(world->job_pool);
    world->chunk_count = 0;
    world->offset_count = 0;
}
