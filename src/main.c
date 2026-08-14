#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <cimgui.h>
#include <sokol_imgui.h>
#include <assert.h>

#include <simd/simd.h>
#include <stdio.h>

void read_file(char* buff, size_t len, const char* path);

struct {
    sg_pass_action pass_action;
    sg_bindings bindings;
    sg_pipeline pipe;
} state;

static void init() {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    simgui_setup(&(simgui_desc_t){ 
        .logger.func = slog_func 
    });
    igGetIO()->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    state.pass_action = (sg_pass_action) {
        .colors[0] = {
            .clear_value = { 0.6, 0.5, 0.7, 1.0 },
            .load_action = SG_LOADACTION_CLEAR
        }
    };

    char vshader[1024];
    char fshader[1024];
    read_file(vshader, 1024, "src/shaders/vert.metal");
    read_file(fshader, 1024, "src/shaders/frag.metal");

    sg_shader shd = sg_make_shader(&(sg_shader_desc) {
        .label = "Plain",
        .vertex_func.source = vshader,
        .fragment_func.source = fshader,
    });

    state.pipe = sg_make_pipeline(&(sg_pipeline_desc) {
        .label = "Plain",
        .shader = shd,
        .layout.attrs = {
            [0] = { .format = SG_VERTEXFORMAT_FLOAT4 },
            [1] = { .format = SG_VERTEXFORMAT_FLOAT4 }
        }
    });

    // float4 bc its padded like this anyway
    simd_float4 vertices[] = {
            /* vertices */              /* color */
        { -0.5f, -0.5f, 0.0f, 1.0f },   { 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.0f,  0.5f, 0.0f, 1.0f },   { 0.0f, 1.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.0f, 1.0f },   { 0.0f, 0.0f, 1.0f, 1.0f }
    };
    
    state.bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){ .data = SG_RANGE(vertices) });
}

static void frame() {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });

    igShowDemoWindow(0);

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pipe);
    sg_apply_bindings(&state.bindings);
    sg_draw(0, 3, 1);
    simgui_render();
    sg_end_pass();
    sg_commit();
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
        .depth_format = SAPP_PIXELFORMAT_NONE,
        .window_title = "dive",
        .icon.sokol_default = true,
        .enable_clipboard = true,
        .logger.func = slog_func,
    };
}

void read_file(char* buff, size_t len, const char* path) {
    FILE *fptr;
    fptr = fopen(path, "r");
    assert(fptr);

    int status = fseek(fptr, 0, SEEK_END);
    assert(status == 0);

    long size = ftell(fptr);
    assert(size > 0 && size < (long)len);
    rewind(fptr);

    size_t n = fread(buff, 1, (size_t)size, fptr);
    buff[n] = '\0';
}
