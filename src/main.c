#include "util.h"
#include "render.h"
#include "camera.h"
#include "mat_math.h"
#include "input.h"
#include "world.h"
#include "player.h"
// #include "debug.h"

struct {
    struct Renderer renderer;
    struct Camera camera;
    struct Input input;
    struct World world;
    struct Player player;
    // struct DebugUI debug;
} state;

static void init() {
    sg_setup(&(sg_desc){ 
        .environment = sglue_environment(), 
        .logger.func = slog_func, 
        .buffer_pool_size = 512,
        .view_pool_size = 256,
    });
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });
    igGetIO()->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    input_init(&state.input);
    player_init(&state.player, (double3){ 0.0, 1.0, 0.0 });
    renderer_init(&state.renderer);
    camera_init(&state.camera, &state.player, radians(60.0), 0.1, 1000.0);
    world_init(&state.world);

    // debug_init(&state.debug);
}

static void frame() {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });
    
    input_update(&state.input);
    player_update(&state.player, &state.camera, &state.input);
    camera_update(&state.camera, &state.player, &state.input);
    world_update(&state.world, &state.player);
    // debug_update(&state.debug, &state.input);

    renderer_begin_pass(&state.renderer, &state.camera);
    world_render(&state.world, state.camera.pos);
    // debug_draw(&state.debug, &state.camera, &state.input, &state.world, &state.player);
    renderer_end_pass();

    input_end_frame(&state.input);
}

static void cleanup() {
    world_destroy(&state.world);
    simgui_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* event) {
    simgui_handle_event(event);
    if (event->type == SAPP_EVENTTYPE_KEY_DOWN && igGetIO()->WantCaptureKeyboard) 
        return;
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
/*
 * TODO:
 *  - Add physics
 *  - textures
 *  - chunk streaming
 * */
