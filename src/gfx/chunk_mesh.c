#include "chunk_mesh.h"

struct MeshBuffer renderer_mesh_create(
        const u64 *quads, u32 opaque_count, u32 trans_count, 
        const voxel *padded,     size_t padded_count
) {
    struct MeshBuffer out = { .trans_count = trans_count, .opaque_count = opaque_count };

    out.quads = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .data = { .ptr = quads, .size = (out.opaque_count + out.trans_count) * sizeof(u64) }
    });

    out.quad_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = out.quads,
    });

    out.padded = sg_make_buffer(&(sg_buffer_desc) {
        .usage.storage_buffer = true,
        .data = { .ptr = padded, .size = padded_count * sizeof(voxel) }
    });
    out.padded_view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = out.padded
    });

    return out;
}

void renderer_mesh_destroy(struct MeshBuffer *mesh) {
    sg_destroy_view(mesh->quad_view);
    sg_destroy_buffer(mesh->quads);

    sg_destroy_view(mesh->padded_view);
    sg_destroy_buffer(mesh->padded);
    mesh->trans_count = 0;
    mesh->opaque_count = 0;

    *mesh = (struct MeshBuffer){0};
}
