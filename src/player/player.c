#include "player.h"
#include "utils/mat_math.h"
#include "gfx/camera.h"

#define GRAVITY (5.0)
#define COLLISION_EPS (1e-5)

enum Axis {
    AXIS_X = 0,
    AXIS_Y,
    AXIS_Z
};

static f64 get_axis(double3 val, u32 axis) {
    return axis == AXIS_X ? val.x : axis == AXIS_Y ? val.y : val.z;
}

static void offset_aabb(struct AABB *box, u32 axis, f64 offset) {
    if (axis == AXIS_X) {
        box->min.x += offset;
        box->max.x += offset;
    } else if (axis == AXIS_Y) {
        box->min.y += offset;
        box->max.y += offset;
    } else {
        box->min.z += offset;
        box->max.z += offset;
    }
}

static bool aabb_intersects(const struct AABB a, const struct AABB b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}

static f64 clip_axis(const struct AABB pbox, const struct AABB vbox, u32 axis, f64 intent) {
    // check for overlap in other axis
    for (u32 i = 1; i <= 2; i++) {
        u32 other_axis = (axis + i) % 3;
        if (get_axis(pbox.max, other_axis) <= get_axis(vbox.min, other_axis) + COLLISION_EPS
                || get_axis(pbox.min, other_axis) >= get_axis(vbox.max, other_axis) - COLLISION_EPS
           ) {
            return intent;
        }
    }

    f64 pmin = get_axis(pbox.min, axis), pmax = get_axis(pbox.max, axis);
    f64 vmin = get_axis(vbox.min, axis), vmax = get_axis(vbox.max, axis);
    if (intent > 0 && pmax <= vmin + COLLISION_EPS) {
        intent = simd_min(intent, vmin - pmax);
    } else if (intent < 0 && pmin >= vmax - COLLISION_EPS) {
        intent = simd_max(intent, vmax - pmin);
    }

    return intent;
}

static f64 sweep_axis(const struct World *world, struct AABB pbox, struct AABB region, u32 axis, f64 intent) {
    int3 low = world_to_voxel(region.min);
    int3 high = world_to_voxel(region.max);
    for (i32 x = low.x; x <= high.x; x++) {
        for (i32 y = low.y; y <= high.y; y++) {
            for (i32 z = low.z; z <= high.z; z++) {
                int3 vox_pos = simd_make_int3(x, y, z);
                if (!world_solid_at(world, vox_pos)) continue;
                struct AABB vbox = { .min = voxel_to_world(vox_pos), .max = voxel_to_world(vox_pos) + VOXEL_SIZE };
                intent = clip_axis(pbox, vbox, axis, intent);
            }
        }
    }

    return intent;
}

static double3 collide(struct Player *player, const struct World *world, double3 intent) {
    double3 psize = player->size;
    struct AABB pbox = { 
        .min = { player->position.x-(psize.x/2.0), player->position.y, player->position.z-(psize.z/2.0) },
        .max = { player->position.x+(psize.x/2.0), player->position.y+psize.y, player->position.z+(psize.z/2.0) }
    };

    // stretch region along intended movement
    struct AABB region = pbox;
    if (intent.x < 0) { region.min.x += intent.x; } else { region.max.x += intent.x; }
    if (intent.y < 0) { region.min.y += intent.y; } else { region.max.y += intent.y; }
    if (intent.z < 0) { region.min.z += intent.z; } else { region.max.z += intent.z; }

    // clip each axis against every solid voxel in the region
    f64 clipped[3] = {0};
    u32 order[3] = { AXIS_Y, AXIS_X, AXIS_Z };
    for (u32 i = 0; i < 3; i++) {
        u32 axis = order[i];
        clipped[axis] = sweep_axis(world, pbox, region, axis, get_axis(intent, axis));
        offset_aabb(&pbox, axis, clipped[axis]);
    }

    double3 clipped_vec = simd_make_double3(clipped[AXIS_X], clipped[AXIS_Y], clipped[AXIS_Z]);


    player->flags.grounded = intent.y < 0 && clipped_vec.y != intent.y;
    if (clipped_vec.x != intent.x) player->velocity.x = 0;
    if (clipped_vec.y != intent.y) player->velocity.y = 0;
    if (clipped_vec.z != intent.z) player->velocity.z = 0;

    return clipped_vec;
}

void player_init(struct Player *player, double3 pos) {
    player->position = pos;
    player->velocity = simd_make_double3(0.0, 0.0, 0.0);

    player->movespeed = simd_make_double3(1.0, 1.0, 1.0);
    player->sprintspeed = player->movespeed * 1.2;
    player->jumpheight = simd_make_double3(0.0, 2.0, 0.0);
    player->mouse_sensitivity = 0.005;

    player->size = simd_make_double3(0.15, 0.4, 0.15);

    // TODO: collisions flag
    player->flags.sprint = false;
    player->flags.collisions = true;
    player->flags.gravity = true;
    player->flags.grounded = false;
}

static void update_mouse_lock(const struct Input *input) {
    if (input_key_pressed(input, SAPP_KEYCODE_Q)) {
        if (sapp_mouse_locked()) {
            sapp_lock_mouse(false);
            igGetIO()->ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            sapp_lock_mouse(true);
            igGetIO()->ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }
    }
}

// returns player's intended movement
static double3 derive_move_intent(struct Player *player, const struct Input *input, f32 pitch, f32 yaw) {
    double3 forward = simd_double(derive_forward(pitch, yaw));
    double3 right = simd_make_double3(cos(yaw), 0.0, sin(yaw));
    double3 up = simd_make_double3(0.0, 1.0, 0.0);

    double3 move_dir = simd_make_double3(0.0, 0.0, 0.0);
    double3 intent = simd_make_double3(0.0, 0.0, 0.0);

    forward.y = 0.0;

    if (input_key_pressed(input, SAPP_KEYCODE_F)) player->flags.sprint = !player->flags.sprint;

    if (input_key_down(input, SAPP_KEYCODE_W)) move_dir += forward;
    if (input_key_down(input, SAPP_KEYCODE_S)) move_dir -= forward;
    if (input_key_down(input, SAPP_KEYCODE_D)) move_dir += right;
    if (input_key_down(input, SAPP_KEYCODE_A)) move_dir -= right;

    if (player->flags.gravity) {
        if (input_key_pressed(input, SAPP_KEYCODE_SPACE) && player->flags.grounded) move_dir += up;
    } else {
        if (input_key_down(input, SAPP_KEYCODE_SPACE)) move_dir += up;
        if (input_key_down(input, SAPP_KEYCODE_LEFT_SHIFT)) move_dir -= up;
    }

    // only normalize the x and z components if gravity is enabled
    if (player->flags.gravity) {
        if (simd_length_squared(move_dir.xz) != 0)
            intent.xz = simd_normalize(move_dir.xz) * (player->flags.sprint ? player->sprintspeed.xz : player->movespeed.xz);

        intent.y = move_dir.y * player->jumpheight.y;
    } else if(simd_length_squared(move_dir) != 0) {
        intent = simd_normalize(move_dir) * (player->flags.sprint ? player->sprintspeed : player->movespeed);
    }


    return intent;
}

static void move(struct Player *player, const struct World *world, const struct Input *input, f32 pitch, f32 yaw) {
    double3 intent = derive_move_intent(player, input, pitch, yaw);
    f64 dt = sapp_frame_duration();

    if (player->flags.gravity) {
        player->velocity.xz = 0.0;

        player->velocity += intent;
        player->velocity.y -= GRAVITY * dt;

        player->velocity.y = simd_clamp(player->velocity.y, -10.0, 10.0);
    } else {
        player->velocity = intent;
    }

    if (simd_length_squared(player->velocity) > 0.0) {

        double3 collision = collide(player, world, player->velocity * dt);

        player->position += collision;
    }
}

void player_update(struct Player *player, const struct World *world, const struct Camera *cam, const struct Input *input) {
    if (input_key_pressed(input, SAPP_KEYCODE_ESCAPE))
        sapp_request_quit();

    update_mouse_lock(input);
    move(player, world, input, cam->pitch, cam->yaw);

}
