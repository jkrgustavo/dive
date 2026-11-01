#include "chunk.h"
#include "utils/mat_math.h"
#include <ba_buffer.h>
#include <simd/geometry.h>
#include <FastNoiseLite.h>


static const int3 face_vertex_mask[] = {
    { 0, 0, 1 },  // front
    { 0, 0, 0 },  // back
    { 1, 0, 0 },  // left
    { 0, 0, 0 },  // right
    { 0, 1, 0 },  // up
    { 0, 0, 0 },  // down
};

static u64 pack_quad(int3 p, u8 width, u8 height, u8 dir) {
    return  (u64)p.x
         | ((u64)p.y    << 12)
         | ((u64)p.z    << 24)
         | ((u64)width  << 36)
         | ((u64)height << 42)
         | ((u64)dir    << 48);
}

static void mesh_release(struct MeshBuffer *mesh) {
    sg_destroy_view(mesh->vbuf_view);
    sg_destroy_buffer(mesh->vbuf);

    sg_destroy_view(mesh->paddbuf_view);
    sg_destroy_buffer(mesh->paddbuf);
    mesh->vertex_count = 0;

    *mesh = (struct MeshBuffer){0};
}

// thread safe
void chunk_generate(u8 *data, int3 chunk_pos, i32 seed, bool *solid, bool *empty) {
    i32 base_y = chunk_to_voxel(chunk_pos).y;
    if (base_y >= 118) {
        *solid = false;
        *empty = true;
        memset(data, 0, sizeof(u8) * CHUNK_VOLUME);
        return;
    } else if (base_y + CHUNK_DIM_Y < -25) {
        *solid = true;
        *empty = false;
        memset(data, GREY, sizeof(u8) * CHUNK_VOLUME);
        return;
    }

    *solid = true;
    *empty = true;

    fnl_state cont_n = fnlCreateState(); 
    fnl_state mask_n = fnlCreateState(); 
    fnl_state ridge_n = fnlCreateState(); 
    fnl_state bump_n = fnlCreateState();

    cont_n.seed = seed;      
    cont_n.frequency = 0.0001f;

    mask_n.seed = seed + 1;  
    mask_n.frequency = 0.01f;

    ridge_n.seed = seed + 2; 
    ridge_n.frequency = 0.001f; 
    ridge_n.fractal_type = FNL_FRACTAL_RIDGED;

    bump_n.seed = seed + 3;  
    bump_n.frequency = 0.05f;   
    bump_n.fractal_type = FNL_FRACTAL_FBM;
    bump_n.octaves = 2;      
    bump_n.noise_type = FNL_NOISE_OPENSIMPLEX2S;

    i32 height_map[CHUNK_DIM_X * CHUNK_DIM_Z];

    for (u32 z = 0; z < CHUNK_DIM_Z; z++) {
        for (u32 x = 0; x < CHUNK_DIM_X; x++) {
            int3 pos_w = local_to_voxel(simd_make_int3(x, 0, z), chunk_pos);
                
            f32 cont = fnlGetNoise2D(&cont_n, pos_w.x, pos_w.z);
            f32 mask = (fnlGetNoise2D(&mask_n, pos_w.x, pos_w.z) + 1.0) / 2.0;
            f32 ridge = (fnlGetNoise2D(&ridge_n, pos_w.x, pos_w.z) + 1.0) / 2.0;
            f32 bumps = (fnlGetNoise2D(&bump_n, pos_w.x, pos_w.z) + 1.0) / 2.0;

             height_map[x + z * CHUNK_DIM_X] 
                 = (int)(cont * 30.0 + mask * mask * ridge * 80.0 + bumps * 3.0) + 5;
        }
    }

    for (u32 z = 0; z < CHUNK_DIM_Z; z++) {
        for (u32 y = 0; y < CHUNK_DIM_Y; y++) {
            for (u32 x = 0; x < CHUNK_DIM_X; x++) {
                int3 pos_w = local_to_voxel(simd_make_int3(x, y, z), chunk_pos);
                u32 idx = local_to_chunk_index(voxel_to_local(pos_w));

                i32 ground_height = height_map[x + z * CHUNK_DIM_X];
                i32 grass_height = ground_height - 1;

                if (pos_w.y < ground_height && pos_w.y >= grass_height) {
                    data[idx] = GREEN;
                    *empty = false;
                } else if (pos_w.y < ground_height) {
                    data[idx] = GREY;
                    *empty = false;
                } else {
                    data[idx] = AIR;
                    *solid = false;
                }
            }
        }
    }

}

// swap job's and chunk's buffers
void chunk_apply_terrain(struct Chunk *chunk, struct ChunkJob *cjob) {
    bs_assert(chunk && cjob);
    u8 *tmp = chunk->data;
    chunk->data = cjob->voxels;
    cjob->voxels = tmp;

    chunk->flags.empty = cjob->empty;
    chunk->flags.solid = cjob->solid;

    chunk->state = CHUNK_LOADED;
}

// Takes in this chunk's world position in chunks
void chunk_init(struct Chunk *chunk, int3 chunk_pos) {
    bs_assert(chunk, "Don't init a loaded chunk");
    bs_assert(chunk->state == CHUNK_UNLOADED, "Chunk must be uninitialized before calling init");

    chunk->position = chunk_pos;
    chunk->state = CHUNK_UNLOADED;
    chunk->flags.empty = false;
    chunk->flags.solid = false;
    chunk->flags.dirty = false;
}

void chunk_set_block(struct Chunk *chunk, int3 local_pos, enum VoxelColor id) {
    bs_assert(chunk->state >= CHUNK_LOADED, "No setting blocks on an uninit chunk");
    u32 idx = local_to_chunk_index(local_pos);
    chunk->data[idx] = id;
    chunk->flags.dirty = true;
    chunk->version++;
}


static const i32 padded_stride[6] = {
     PADDED_CHUNK_DIM_X * PADDED_CHUNK_DIM_Y,   // FRONT +z
    -PADDED_CHUNK_DIM_X * PADDED_CHUNK_DIM_Y,   // BACK  -z
     1,                                         // LEFT  +x
    -1,                                         // RIGHT -x
     PADDED_CHUNK_DIM_X,                        // UP    +y
    -PADDED_CHUNK_DIM_X,                        // DOWN  -y
};

void _build_padded_data(u8 *padded, const struct Chunk *neighbors[27]) {
    // triple loop bc padded chunk dim isn't a power of two
    for (i32 z = -1; z < PADDED_CHUNK_DIM_Z-1; z++) {
        for (i32 y = -1; y < PADDED_CHUNK_DIM_Y-1; y++) {
            for (i32 x = -1; x < PADDED_CHUNK_DIM_X-1; x++) {
                int3 vox = simd_make_int3(x, y, z);
                int3 offset = voxel_to_chunk(vox) + 1;
                const struct Chunk *neighbor_chunk = neighbors[offset.x + offset.y*3 + offset.z*3*3];
                u8 vox_data;
                if (!neighbor_chunk) { 
                    vox_data = INVALID; 
                } else if (neighbor_chunk->flags.empty) {
                    vox_data = AIR;
                } else {
                    vox_data = neighbor_chunk->data[local_to_chunk_index(voxel_to_local(vox))];
                }
                u32 padded_idx = 
                    (x+1) + (y+1) * PADDED_CHUNK_DIM_X + (z+1) * (PADDED_CHUNK_DIM_X * PADDED_CHUNK_DIM_Y);

                padded[padded_idx] = vox_data;
            }
        }
    }
}

static inline const struct Chunk *neighbor_at(const struct Chunk *neighbors[27], int3 off) {
    return neighbors[(off.x + 1) + (off.y + 1) * 3 + (off.z + 1) * 9];
}

static inline u8 sample_data(const struct Chunk *chunk, int3 local_pos) {
    if (!chunk) return INVALID;

    if (chunk->flags.empty) return AIR;

    return chunk->data[local_to_chunk_index(local_pos)];
}

void build_padded_data(u8 *padded, const struct Chunk *neighbors[27]) {
    // double loop bc padded chunk dim isn't a power of two
    for (i32 z = -1; z < PADDED_CHUNK_DIM_Z-1; z++) {
        for (i32 y = -1; y < PADDED_CHUNK_DIM_Y-1; y++) {
                int3 vox    = simd_make_int3(0, y, z);
                int3 offset = voxel_to_chunk(vox);
                int3 local  = voxel_to_local(vox);

                u8 *row = padded 
                    + (y+1) * PADDED_CHUNK_DIM_X + (z+1) * PADDED_CHUNK_DIM_X * PADDED_CHUNK_DIM_Y;
                
                int3 off_right = offset + dir_offset[RIGHT];
                row[0] = sample_data(neighbor_at(neighbors, off_right), 
                            simd_make_int3(CHUNK_DIM.x - 1, local.y, local.z));

                int3 off_left = offset + dir_offset[LEFT];
                row[PADDED_CHUNK_DIM_X - 1] = sample_data(neighbor_at(neighbors, off_left),
                        simd_make_int3(0, local.y, local.z));

                const struct Chunk *chunk = neighbor_at(neighbors, offset);
                if (!chunk) {
                    memset(row + 1, INVALID, CHUNK_DIM.x);
                } else if (chunk->flags.empty) {
                    memset(row + 1, AIR, CHUNK_DIM.x);
                } else {
                    memcpy(row + 1, chunk->data + local_to_chunk_index(local), CHUNK_DIM.x);
                }

        }
    }
}

bool chunk_prepare_mesh(struct Chunk *chunk, const struct Chunk *neighbors[27], struct MeshScratch *scratch) {
    bs_assert(chunk && chunk->state >= CHUNK_LOADED, "Invalid chunk");
    if ((chunk->state == CHUNK_READY && !chunk->flags.dirty) 
            || chunk->flags.empty 
            || chunk->flags.solid
            || chunk->state == CHUNK_MESHING)
        return false;

    chunk->state = CHUNK_MESHING;
    build_padded_data(scratch->padded_data, neighbors);
    bs_buffer_clear(&scratch->quads);
    chunk->flags.dirty = false;

    return true;
}

// Thread safe
void chunk_mesh(struct MeshScratch *scratch) {
    scratch->solid = true;
    for (size_t vox = 0; vox < CHUNK_VOLUME; vox++) {
        int3 local_pos = chunk_index_to_local(vox);
        i32 vox_padded_idx = 
            (local_pos.x+1) + (local_pos.y+1) * PADDED_CHUNK_DIM_X + (local_pos.z+1) * PADDED_CHUNK_DIM_X * PADDED_CHUNK_DIM_Y;

        if (scratch->padded_data[vox_padded_idx] == AIR) {
            scratch->solid = false;
            continue;
        }

        for (u32 dir = 0; dir < 6; dir++) {
            if (scratch->padded_data[vox_padded_idx + padded_stride[dir]] != AIR)
                continue;

            u64 quad = pack_quad(local_pos + face_vertex_mask[dir], 1, 1, dir);
            bs_buffer_append(&scratch->quads, quad);
        }
    }
}

// updates in-flight flags
void chunk_apply_mesh(struct Chunk *chunk, struct ChunkJob *cjob, bool fresh) {
    mesh_release(&chunk->mesh);

    struct MeshScratch *scratch = &cjob->scratch;

    chunk->mesh.vertex_count = scratch->quads.count;

    if (scratch->quads.count == 0 && fresh) {
        chunk->flags.solid = scratch->solid;
        chunk->flags.empty = !scratch->solid;
        if (chunk->flags.empty) {
            free(chunk->data);
            chunk->data = NULL;
        }
        chunk->state = CHUNK_READY;
        return;
    }

    chunk->mesh.vbuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .data = { .ptr = scratch->quads.items, .size = scratch->quads.count * sizeof(u64) }
    });
    chunk->mesh.vbuf_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = chunk->mesh.vbuf,
    });

    chunk->mesh.paddbuf = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .data = { .ptr = scratch->padded_data, .size = PADDED_CHUNK_VOLUME * sizeof(u8) }
    });
    chunk->mesh.paddbuf_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = chunk->mesh.paddbuf
    });

    chunk->state = CHUNK_READY;
}

void chunk_set_buried(struct Chunk *chunk) {
    mesh_release(&chunk->mesh);
    chunk->state = CHUNK_READY;
    chunk->flags.dirty = false;
}

void chunk_render(const struct Chunk *chunk, sg_buffer ibuf, sg_view palette, double3 cam_pos) {
    bs_assert(chunk);
    bs_assert(chunk->state == CHUNK_READY || chunk->state == CHUNK_MESHING, "Attempt to render an invalid chunk");
    if (chunk->mesh.vertex_count == 0) return;

    double3 chunk_pos = voxel_to_world(chunk_to_voxel(chunk->position));
    double3 rel_pos_d = chunk_pos - cam_pos;
    float3 rel_pos = simd_float(rel_pos_d);
    float3 scale = simd_make_float3(VOXEL_SIZE, VOXEL_SIZE, VOXEL_SIZE);

    simd_float4x4 model_mat = simd_mul(mat_translation(rel_pos), mat_scale(scale));

    sg_apply_bindings(&(sg_bindings){
        .index_buffer = ibuf,
        .views = { 
            [0] = chunk->mesh.vbuf_view, 
            [1] = chunk->mesh.paddbuf_view,
            [2] = palette
        }
    });
    // view and proj mats are applied in the begin_pass function
    sg_apply_uniforms(1, &SG_RANGE(model_mat));
    sg_draw(0, chunk->mesh.vertex_count * 6, 1);
}

void chunk_reset(struct Chunk *chunk) {
    assert(chunk);
    if (!chunk->flags.empty) {
        mesh_release(&chunk->mesh);
    }

    chunk->state = CHUNK_UNLOADED;
    chunk->flags.dirty = false;
    chunk->flags.empty = false;
    chunk->flags.solid = false;
    chunk->position = simd_make_int3(0, 0, 0);

    chunk->version++;
}

void chunk_destroy(struct Chunk *chunk) {
    assert(chunk);
    if (!chunk->flags.empty) {
        mesh_release(&chunk->mesh);
        free(chunk->data);
    }

    memset(chunk, 0, sizeof(struct Chunk));
}
