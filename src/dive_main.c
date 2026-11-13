#include "ba_buffer.h"
#include "utils/util.h"
#include "gfx/render.h"
#include "gfx/camera.h"
#include "utils/mat_math.h"
#include "player/input.h"
#include "world/world.h"
#include "player/player.h"
#include "debug.h"
#include "utils/jobs.h"
#include "dive_api.h"

struct DiveState {
    struct Renderer renderer;
    struct Camera camera;
    struct Input input;
    struct World world;
    struct Player player;
    struct JobSystem jobs;
    struct DebugUI debug;
};

static struct DiveState *init_cb() {

    struct DiveState *state = calloc(1, sizeof(struct DiveState));

    igGetIO()->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    input_init(&state->input);
    player_init(&state->player, (double3){ 0.0, 5.0, 0.0 });
    bs_assert(renderer_init(&state->renderer), "Error initializing pipeline!");
    camera_init(&state->camera, &state->player, radians(60.0), 0.01, 1000.0);
    jobs_init(&state->jobs, 0);
    world_init(&state->world, &state->jobs);

    debug_init(&state->debug);

    return state;
}

static void frame_cb(struct DiveState *state) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });
    
    input_update(&state->input);
    player_update(&state->player, &state->world, &state->input, state->camera.pitch, state->camera.yaw);
    camera_update(&state->camera, &state->player, &state->input);
    world_update(&state->world, state->player.position);
    debug_update(&state->debug, &state->input, &state->player, &state->world);

    renderer_begin_pass(&state->renderer, state->camera.proj, state->camera.view);
    world_render(&state->world, state->camera.pos);
    debug_draw(&state->debug, &state->camera, &state->input, &state->world, &state->player);
    renderer_end_pass();

    input_end_frame(&state->input);
}

static void cleanup_cb(struct DiveState *state) {
    jobs_destroy(&state->jobs);
    world_destroy(&state->world);
    renderer_destroy(&state->renderer);
}

static void event_cb(struct DiveState *state, const sapp_event* event) {
    simgui_handle_event(event);
    if (event->type == SAPP_EVENTTYPE_KEY_DOWN && igGetIO()->WantCaptureKeyboard) 
        return;
    input_handle(&state->input, event);
}

// TODO: Add a size check
// TODO: Destroy and rebuild pipelines
static void reload_cb(struct DiveState *state) {
    jobs_start(&state->jobs, 0);
    renderer_reload(&state->renderer);
}

static void unload_cb(struct DiveState *state) {
    jobs_stop(&state->jobs);
}

struct DiveAPI dive_get_api() {
    return (struct DiveAPI) {
        .version = DIVE_API_VERSION,
        .init = init_cb,
        .frame = frame_cb,
        .event = event_cb,
        .cleanup = cleanup_cb,
        .reload = reload_cb,
        .unload = unload_cb
    };
}
