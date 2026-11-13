#ifndef DIVE_API_H
#define DIVE_API_H

#define DIVE_API_VERSION 2

#include <sokol_app.h>

struct DiveState;

struct DiveAPI {
    int version;
    struct DiveState *(*init)(void);
    void (*reload)(struct DiveState *);
    void (*unload)(struct DiveState *);
    void (*frame)(struct DiveState *);
    void (*event)(struct DiveState *, const sapp_event *);
    void (*cleanup)(struct DiveState *);
};

#endif
