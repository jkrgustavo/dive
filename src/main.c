#include "util.h"
#include "chunk.h"
#include "render.h"
#include "camera.h"
#include "math.h"

#define CHUNK_COUNT 4

void movement_window();

struct {
    struct Renderer renderer;
    struct Camera camera;
    struct Chunk chunks[CHUNK_COUNT];
} state;

static void init() {
    sg_setup(&(sg_desc){ .environment = sglue_environment(), .logger.func = slog_func });
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });
    igGetIO()->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    f32 aspect = (float)sapp_width() / sapp_height();
    
    renderer_init(&state.renderer);
    camera_init(&state.camera, (60.0 * (PI / 180.0)), 0.01, 1000.0, aspect);
    for (int c = 0; c < CHUNK_COUNT; c++) {
        struct Chunk *cx = &state.chunks[c];
        chunk_alloc(cx);
        chunk_init(cx, simd_make_int3((c/2),  -1, (c%2)));
        memset(cx->data, 1, sizeof(u8) * CHUNK_VOLUME);
        chunk_mesh(cx);
    }


}

static void frame() {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });
    
    movement_window();

    renderer_begin_pass(&state.renderer);
    for (int i = 0; i < CHUNK_COUNT; i++) {
        chunk_render(&state.chunks[i], &state.camera);
    }
    sg_draw(0, 3, 1);
    renderer_end_pass();
}

static void cleanup() {
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* event) {
    simgui_handle_event(event);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 1024,
        .height = 768,
        .window_title = "dive",
        .icon.sokol_default = true,
        .enable_clipboard = true,
        .logger.func = slog_func,
    };
}

void movement_window() {
    f32 tmpf[3] = { state.camera.forward.x, state.camera.forward.y, state.camera.forward.z };
    f32 tmpp[3] = { state.camera.pos.x, state.camera.pos.y, state.camera.pos.z, };
    igSetNextWindowSize((ImVec2){400, 100}, ImGuiCond_Once);
    igBegin("move the mouse :)", NULL, 0);
    igSliderFloat3("forward", tmpf, -1.0, 1.0);
    igSliderFloat3("pos", tmpp, -25.0, 25.0);
    igEnd();
    float3 forward = simd_make_float3(tmpf[0], tmpf[1], tmpf[2]);
    double3 pos = simd_make_double3(tmpp[0], tmpp[1], tmpp[2]);
    state.camera.pos = pos;
    camera_update(&state.camera, forward);
}
