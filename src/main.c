#include "util.h"
#include "render.h"
#include "camera.h"
#include "mat_math.h"
#include "input.h"
#include "world.h"

#define CHUNK_COUNT 1

void movement_window();

struct {
    struct Renderer renderer;
    struct Camera camera;
    struct Input input;
    struct World world;
} state;

static void init() {
    sg_setup(&(sg_desc){ .environment = sglue_environment(), .logger.func = slog_func });
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });
    igGetIO()->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    sapp_lock_mouse(true);
    
    input_init(&state.input);
    renderer_init(&state.renderer);
    camera_init(&state.camera, radians(60.0), 0.1, 1000.0);
    world_init(&state.world, CHUNK_COUNT);

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
    world_update(&state.world);

    movement_window();

    renderer_begin_pass(&state.renderer, &state.camera);
    world_render(&state.world, state.camera.pos);
    renderer_end_pass();

    input_end_frame(&state.input);
}

static void cleanup() {
    world_destory(&state.world);
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
    double3 pos = state.camera.pos;
    igBegin("Mouse direction", 0, 0);
    igText("Mouse forward: (%f, %f, %f)", forward.x, forward.y, forward.z);
    igSpacing();
    igText("Camera pos: (%f, %f, %f)", pos.x, pos.y, pos.z);
    igEnd();
}
