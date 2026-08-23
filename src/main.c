#include "util.h"
#include "chunk.h"
#include "render.h"
#include "camera.h"
#include "math.h"
#include "input.h"

#define CHUNK_COUNT 4

void movement_window();
void player_update_game_state(const struct Input *input);

struct {
    struct Renderer renderer;
    struct Camera camera;
    struct Input input;
    struct Chunk chunks[CHUNK_COUNT];
} state;

static void init() {
    sg_setup(&(sg_desc){ .environment = sglue_environment(), .logger.func = slog_func });
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });
    igGetIO()->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    f32 aspect = (float)sapp_width() / sapp_height();

    sapp_lock_mouse(true);
    
    input_init(&state.input);
    renderer_init(&state.renderer);
    camera_init(&state.camera, radians(60.0), 0.01, 1000.0, aspect);
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
    
    input_update(&state.input);
    player_update_game_state(&state.input);
    camera_update(&state.camera, &state.input);

    movement_window();

    renderer_begin_pass(&state.renderer);
    for (int i = 0; i < CHUNK_COUNT; i++) {
        chunk_render(&state.chunks[i], &state.camera);
    }
    renderer_end_pass();

    input_end_frame(&state.input);
}

static void cleanup() {
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* event) {
    simgui_handle_event(event);
    input_handle(&state.input, event);
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
    float3 forward = derive_forward(state.camera.pitch, state.camera.yaw);
    igBegin("Mouse direction", 0, 0);
    igText("Mouse forward: (%f, %f, %f)", forward.x, forward.y, forward.z);
    igEnd();
}

void player_update_game_state(const struct Input *input) {
    if (input_key_pressed(input, SAPP_KEYCODE_ESCAPE))
        sapp_request_quit();

    if (input_key_pressed(input, SAPP_KEYCODE_Q)) {
        if (sapp_mouse_locked()) {
            sapp_lock_mouse(false);
        } else {
            sapp_lock_mouse(true);
        }
    }
}
