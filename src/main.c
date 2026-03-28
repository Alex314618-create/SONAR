/**
 * main.c — SONAR game entry point and main loop
 *
 * Initializes all subsystems, runs the game loop with FPS camera controls,
 * and shuts down cleanly on exit.
 */

#include "core/window.h"
#include "core/input.h"
#include "core/timer.h"
#include "core/log.h"
#include "render/renderer.h"
#include "render/shader.h"
#include "render/camera.h"
#include "render/mesh.h"
#include "render/sonar_fx.h"
#include "render/hud.h"
#include "render/vfx.h"
#include "world/map.h"
#include "world/entity.h"
#include "world/trigger.h"
#include "world/stalker.h"
#include "world/physics.h"
#include "render/vfx_particles.h"
#include "sonar/raycast.h"
#include "sonar/sonar.h"
#include "sonar/energy.h"
#include "audio/audio.h"
#include "audio/sound.h"
#include "audio/spatial.h"

#include <SDL.h>
#include <glad/gl.h>
#include <cglm/cglm.h>
#include <math.h>

#define MOVE_SPEED   3.0f
#define SPRINT_MUL   1.8f

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* --- Init subsystems --- */
    if (window_init(960, 600, "SONAR") < 0) {
        return 1;
    }

    if (renderer_init() < 0) {
        window_shutdown();
        return 1;
    }

    input_init();
    timer_init();

    /* Capture mouse for FPS look */
    input_set_mouse_captured(1);

    /* Load map and physics */
    if (map_load("assets/maps/test_level.glb") < 0) {
        LOG_ERROR("Failed to load map");
        timer_shutdown();
        input_shutdown();
        renderer_shutdown();
        window_shutdown();
        return 1;
    }
    physics_init(map_get_collision_verts(), map_get_collision_tri_count());
    raycast_init(map_get_collision_verts(), map_get_collision_tri_count());
    entity_init(map_get_entities(), map_get_entity_count());
    trigger_init(map_get_entities(), map_get_entity_count());
    stalker_init(map_get_entities(), map_get_entity_count());

    /* Init sonar subsystems */
    energy_init();
    sonar_init();

    if (sonar_fx_init() < 0) {
        LOG_ERROR("Failed to init sonar FX");
        physics_shutdown();
        map_shutdown();
        timer_shutdown();
        input_shutdown();
        renderer_shutdown();
        window_shutdown();
        return 1;
    }

    if (hud_init() < 0) {
        LOG_ERROR("Failed to init HUD");
        sonar_fx_shutdown();
        physics_shutdown();
        map_shutdown();
        timer_shutdown();
        input_shutdown();
        renderer_shutdown();
        window_shutdown();
        return 1;
    }

    if (vfx_init() < 0) {
        LOG_ERROR("Failed to init VFX");
        hud_shutdown();
        sonar_fx_shutdown();
        physics_shutdown();
        map_shutdown();
        timer_shutdown();
        input_shutdown();
        renderer_shutdown();
        window_shutdown();
        return 1;
    }

    if (vfx_particles_init() < 0) {
        LOG_ERROR("Failed to init VFX particles");
        vfx_shutdown();
        hud_shutdown();
        sonar_fx_shutdown();
        physics_shutdown();
        map_shutdown();
        timer_shutdown();
        input_shutdown();
        renderer_shutdown();
        window_shutdown();
        return 1;
    }

    /* Init audio (non-fatal if fails — game works without sound) */
    int hasAudio = 0;
    Sound sndPing = {0};
    Sound sndEcho = {0};

    if (audio_init() == 0 && spatial_init() == 0) {
        hasAudio = 1;
        sndPing = sound_load_wav("assets/sounds/sonar_ping.wav");
        sndEcho = sound_load_wav("assets/sounds/sonar_echo.wav");
        if (!sndPing.buffer) LOG_ERROR("Failed to load sonar_ping.wav");
        if (!sndEcho.buffer) LOG_ERROR("Failed to load sonar_echo.wav");
    } else {
        LOG_ERROR("Audio init failed — running without sound");
    }

    /* Camera at spawn position */
    Camera cam;
    const float *spawn = map_get_player_spawn();
    camera_init(&cam, spawn[0], spawn[1], spawn[2], map_get_player_yaw(), 0.0f);

    /* Player collision bounds (relative to eye position) */
    AABB playerBounds = {
        .min = {-0.25f, -1.5f, -0.25f},
        .max = { 0.25f,  0.1f,  0.25f}
    };

    /* Head bob state */
    float bobPhase = 0.0f;
    int isSprinting = 0;

    /* Minimap state */
    int showMinimap = 0;
#define MINIMAP_DRAIN 20.0f

    /* --- Main loop --- */
    while (!window_should_close()) {
        input_update();
        timer_tick();

        float dt = timer_dt();

        /* ESC to quit */
        if (input_key_pressed(SDL_SCANCODE_ESCAPE)) {
            window_set_should_close(1);
        }

        /* Mouse look */
        int mdx, mdy;
        input_mouse_delta(&mdx, &mdy);
        if (mdx || mdy) {
            camera_update(&cam, (float)mdx, (float)mdy);
        }

        /* WASD movement (horizontal plane only) */
        float speed = MOVE_SPEED * dt;
        isSprinting = input_key_down(SDL_SCANCODE_LSHIFT);
        if (isSprinting) {
            speed *= SPRINT_MUL;
        }

        vec3 moveDir = {0.0f, 0.0f, 0.0f};

        if (input_key_down(SDL_SCANCODE_W)) {
            vec3 fwd = {cam.front[0], 0.0f, cam.front[2]};
            glm_vec3_normalize(fwd);
            glm_vec3_muladds(fwd, speed, moveDir);
        }
        if (input_key_down(SDL_SCANCODE_S)) {
            vec3 fwd = {cam.front[0], 0.0f, cam.front[2]};
            glm_vec3_normalize(fwd);
            glm_vec3_muladds(fwd, -speed, moveDir);
        }
        if (input_key_down(SDL_SCANCODE_D)) {
            glm_vec3_muladds(cam.right, speed, moveDir);
        }
        if (input_key_down(SDL_SCANCODE_A)) {
            glm_vec3_muladds(cam.right, -speed, moveDir);
        }

        /* Physics-based movement with collision */
        vec3 newPos;
        physics_move(cam.position, moveDir, &playerBounds, newPos);
        glm_vec3_copy(newPos, cam.position);

        /* Head bob */
        int isMoving = (moveDir[0] != 0.0f || moveDir[1] != 0.0f || moveDir[2] != 0.0f);
        if (isMoving) {
            bobPhase += dt * 7.5f;
        } else {
            bobPhase *= 0.85f;
        }

        /* Update audio listener to match camera */
        if (hasAudio) {
            audio_set_listener(cam.position, cam.front, cam.up);
            spatial_update();
        }

        /* F key: interact with nearest entity */
        if (input_key_pressed(SDL_SCANCODE_F)) {
            int idx = entity_find_nearest_interactable(
                map_get_entities(), map_get_entity_count(),
                cam.position, 2.5f);
            if (idx >= 0)
                entity_activate(&map_get_entities()[idx]);
        }

        /* M key: toggle minimap */
        if (input_key_pressed(SDL_SCANCODE_M)) {
            showMinimap = !showMinimap;
        }

        /* Minimap energy drain */
        if (showMinimap) {
            if (!energy_spend(MINIMAP_DRAIN * dt)) {
                showMinimap = 0;
            }
        }

        /* --- Sonar input --- */
        sonar_frame_begin();

        int sonarFired = 0;

        /* RMB: toggle sonar mode */
        if (input_mouse_pressed(SDL_BUTTON_RIGHT)) {
            sonar_toggle_mode();
        }

        /* LMB hold: continuous fire */
        if (input_mouse_down(SDL_BUTTON_LEFT)) {
            sonar_fire_continuous(cam.position, cam.front, cam.up, cam.right, dt);
            sonarFired = 1;
        }

        /* Recharge energy when not firing and minimap not active */
        if (!sonarFired && !showMinimap) {
            energy_recharge(dt);
        }

        sonar_set_player(cam.position, cam.front, cam.up, cam.right);
        sonar_update(dt);
        entity_update(dt, map_get_entities(), map_get_entity_count());
        trigger_update(dt, cam.position);
        stalker_update(dt, cam.position, cam.front);
        vfx_update(dt);
        vfx_particles_update(dt);

        /* --- Render --- */
        renderer_begin_frame();

        /* Set camera matrices */
        int winW, winH;
        window_get_size(&winW, &winH);
        float aspect = (winH > 0) ? (float)winW / (float)winH : 1.0f;

        mat4 view, proj, model;

        /* Apply head bob (render only — doesn't affect collision pos) */
        float bobAmp = isSprinting ? 0.042f : 0.028f;
        float bobAmpX = isSprinting ? 0.0135f : 0.009f;
        float bobOffsetY = sinf(bobPhase) * bobAmp;
        float bobOffsetX = sinf(bobPhase * 0.5f) * bobAmpX;
        float savedX = cam.position[0];
        float savedY = cam.position[1];
        cam.position[0] += bobOffsetX;
        cam.position[1] += bobOffsetY;
        camera_view_matrix(&cam, view);
        cam.position[0] = savedX;
        cam.position[1] = savedY;

        camera_proj_matrix(&cam, aspect, proj);
        glm_mat4_identity(model);

        /* 1. Depth-only world pass */
        {
            uint32_t shader = renderer_get_basic_shader();
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            shader_use(shader);
            shader_set_mat4(shader, "u_view", (float *)view);
            shader_set_mat4(shader, "u_proj", (float *)proj);
            shader_set_mat4(shader, "u_model", (float *)model);
            model_draw(map_get_render_model(), shader);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }

        /* 2. Sonar points (all persistent) */
        sonar_fx_render(sonar_get_points(), sonar_get_point_count(),
                        (float *)view, (float *)proj, cam.position);

        /* 2b. VFX particles (shockwave, collapse) */
        vfx_particles_render((float *)view, (float *)proj, cam.position);

        /* 3. Laser lines — only when LMB held */
        if (input_mouse_down(SDL_BUTTON_LEFT)) {
            int gunX, gunY;
            vfx_get_gun_muzzle(winW, winH, &gunX, &gunY);
            vfx_render_laser_lines(
                sonar_get_points(),
                sonar_get_frame_start(),
                sonar_get_write_head(),
                MAX_SONAR_POINTS,
                (float *)view, (float *)proj,
                winW, winH, gunX, gunY
            );
        }

        /* 4. Fullscreen VFX */
        vfx_render_scanlines(winW, winH);
        vfx_render_pulse_ripple(winW, winH);

        /* 5. Gun sprite */
        vfx_render_gun(winW, winH, input_mouse_down(SDL_BUTTON_LEFT));

        /* 6. HUD (energy, minimap, etc.) */
        float playerYaw = atan2f(cam.front[0], cam.front[2]);
        hud_render(winW, winH, energy_get_fraction() * 100.0f,
                   (int)sonar_get_mode(), timer_fps(), cam.position,
                   showMinimap, playerYaw);

        renderer_end_frame();
    }

    /* --- Shutdown (reverse order) --- */
    if (hasAudio) {
        sound_destroy(&sndPing);
        sound_destroy(&sndEcho);
        spatial_shutdown();
        audio_shutdown();
    }
    hud_shutdown();
    vfx_particles_shutdown();
    vfx_shutdown();
    sonar_fx_shutdown();
    sonar_shutdown();
    energy_shutdown();
    raycast_shutdown();
    physics_shutdown();
    stalker_shutdown();
    trigger_shutdown();
    entity_shutdown();
    map_shutdown();
    timer_shutdown();
    input_shutdown();
    renderer_shutdown();
    window_shutdown();

    LOG_INFO("Clean shutdown");
    return 0;
}
