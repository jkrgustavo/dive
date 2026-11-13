#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dive_api.h"
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <cimgui.h>
#include <sokol_imgui.h>

static struct {
    // def'd in dive_api.h, struct of callbacks for the lib
    struct DiveAPI api;

    // state owned by host, used by lib
    struct DiveState *state;
#ifdef HOT

    // currently loaded copy
    void *lib;

    // path of currently loaded copy
    char lib_path[1024];

    // for unique copy file names
    unsigned int load_count;

    // mtime of current build
    struct timespec loaded_mtime;

    // mtime of new build
    struct timespec pending_mtime;

    // time new build has been unchanged
    double pending_time;
#endif
} host;

#ifdef HOT
#include <copyfile.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#define RELOAD_SETTLE_SEC 0.2

typedef struct DiveAPI (*dive_get_api_fn)();

static bool get_mtime(const char *path, struct timespec *out) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    *out = st.st_mtimespec;
    return true;
}

static bool same_time(struct timespec a, struct timespec b) {
    return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}

// load and swap in fresh copy. Fully load and check copy before
// closing the old one so no crashes on build errors
static bool load_dive() {
    const char *tmp = getenv("TMPDIR");
    char path[sizeof(host.lib_path)];
    snprintf(path, sizeof(path), "%s/dive_%d_%u.so", 
            tmp ? tmp : "/tmp", (int)getpid(), host.load_count++);

    // Load uniquely named copy, not DIVE_PATH itself
    if (copyfile(DIVE_PATH, path, NULL, COPYFILE_DATA) != 0) {
        perror("hot reload => copyfile\n");
        return false;
    }

    void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        fprintf(stderr, "hot reload => %s\n", dlerror());
        unlink(path);
        return false;
    }

    dive_get_api_fn get_api = (dive_get_api_fn)dlsym(lib, "dive_get_api");
    if (!get_api) {
        fprintf(stderr, "hot reload => dive_get_api not found in lib\n");
        goto fail;
    }

    struct DiveAPI api = get_api();
    if (api.version != DIVE_API_VERSION) {
        fprintf(stderr, "hot reload => lib has API version %d, host expects %d. Restart\n", 
                api.version, DIVE_API_VERSION);
        goto fail;
    }

    if (host.lib) {
        if (host.state) host.api.unload(host.state);
        dlclose(host.lib);
        unlink(host.lib_path);
    }

    host.lib = lib;
    host.api = api;
    snprintf(host.lib_path, sizeof(host.lib_path), "%s", path);
    return true;


fail:   // close and unlink
    dlclose(lib);
    unlink(path);
    return false;
}

static void check_reload() {
    struct timespec t;
    if (!get_mtime(DIVE_PATH, &t) || same_time(t, host.loaded_mtime)) return;

    // new build or being rewritten. Restart settle timer
    if (!same_time(t, host.pending_mtime)) {
        host.pending_mtime = t;
        host.pending_time = 0.0;
        return;
    }
    host.pending_time += sapp_frame_duration();
    if (host.pending_time < RELOAD_SETTLE_SEC) return;

    // Mark as handled whether it loads or not. This way failed builds only get
    // tried once.
    host.loaded_mtime = t;
    if (load_dive()) {
        host.api.reload(host.state);
        fprintf(stderr, "hot reload => reloaded\n");
    }
}

#else   /* DIVE_HOT_RELOAD */  // Static build

struct DiveAPI dive_get_api();
static void check_reload() {}
static bool load_dive() {
    host.api = dive_get_api();
    return true;
}

#endif  /* DIVE_HOT_RELOAD */

static void init() {
    sg_setup(&(sg_desc){ 
        .environment = sglue_environment(), 
        .logger.func = slog_func, 
        .buffer_pool_size = 2 * 2048 + 32,
        .view_pool_size = 2 * 2048 + 32,
    });
    simgui_setup(&(simgui_desc_t){ .logger.func = slog_func });

#ifdef HOT
    get_mtime(DIVE_PATH, &host.loaded_mtime);
#endif

    host.state = NULL;

    if (!load_dive()) {
        fprintf(stderr, "hot reload => Couldn't load the game lib\n");
        exit(1);
    }
    host.state = host.api.init();
}

static void frame() {
    // swap between frames
    check_reload();
    host.api.frame(host.state);
}

static void cleanup() {
    host.api.cleanup(host.state);
    simgui_shutdown();
    sg_shutdown();

#ifdef HOT
    dlclose(host.lib);
    unlink(host.lib_path);
#endif
}

static void event(const sapp_event *event) {
    host.api.event(host.state, event);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    sapp_run(&(sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 1700,
        .height = 1000,
        .window_title = "dive",
        .icon.sokol_default = true,
        .enable_clipboard = true,
        .logger.func = slog_func,
    });
}
