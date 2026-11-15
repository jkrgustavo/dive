#include "voxel.h"

struct Palette palette_build() {
    u32 palette[256] = {0};
    palette[RED] = 0xFF0000FF;
    palette[ORANGE] = 0xFF0099FF;
    palette[YELLOW] = 0xFF00FFFF;
    palette[GREEN] = 0xFF33DD33;
    palette[BLUE] = 0xFFFF0000;
    palette[PURPLE] = 0xFFFF00FF;
    palette[GREY] = 0xFFAAAAAA;
    palette[INVALID] = palette[PURPLE];

    struct Palette p = {0};

    p.buf = sg_make_buffer(&(sg_buffer_desc) {
        .data = SG_RANGE(palette),
        .usage.storage_buffer = true
    });
    p.view = sg_make_view(&(sg_view_desc) {
        .storage_buffer.buffer = p.buf
    });

    return p;
}

void palette_destroy(struct Palette *palette) {
    sg_destroy_view(palette->view);
    sg_destroy_buffer(palette->buf);
}
