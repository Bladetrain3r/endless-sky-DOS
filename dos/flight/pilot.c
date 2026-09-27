/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pilot.h"
#include "input.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int pilot_load(Pilot *pilot, const char *path)
{
    FILE *f = fopen(path, "r");
    char version[16], name[32], extra;
    MotionParameters p;
    int count;
    if(!f) return 0;
    count = fscanf(f, "%15s %31s %lf %lf %lf %lf %lf %c", version, name,
        &p.acceleration, &p.reverse_acceleration, &p.turn_rate, &p.drag,
        &p.acceleration_multiplier, &extra);
    fclose(f);
    if(count != 7 || strcmp(version, "ESPILOT1") || strcmp(name, "Sparrow") ||
       !isfinite(p.acceleration) || !isfinite(p.reverse_acceleration) ||
       !isfinite(p.turn_rate) || !isfinite(p.drag) || !isfinite(p.acceleration_multiplier) ||
       p.acceleration <= 0. || p.acceleration > 10. || p.reverse_acceleration != 0. ||
       p.turn_rate <= 0. || p.turn_rate > 20. || p.drag <= 0. || p.drag > 1. ||
       p.acceleration_multiplier != 1.) return 0;
    memset(pilot, 0, sizeof(*pilot));
    pilot->parameters = p;
    return 1;
}

void pilot_reset(Pilot *pilot, Scene *scene)
{
    pilot->state = (MotionState){400., 300., 0., 0., 0};
    pilot->command = (MotionCommand){0};
    pilot->follow = 1;
    scene->camera_enabled = 0;
    scene->camera_x = scene->camera_y = 0;
}

void pilot_keys(Pilot *pilot, Scene *scene, unsigned keys)
{
    unsigned pressed = keys & ~pilot->previous_keys;
    if(pressed & INPUT_RESET) pilot_reset(pilot, scene);
    if(pressed & INPUT_CAMERA) pilot->follow = !pilot->follow;
    pilot->command.forward = !!(keys & INPUT_FORWARD);
    pilot->command.back = !!(keys & INPUT_BACK);
    pilot->command.turn = !!(keys & INPUT_RIGHT) - !!(keys & INPUT_LEFT);
    pilot->previous_keys = keys;
}

void pilot_step(Pilot *pilot, Scene *scene)
{
    motion_step(&pilot->state, &pilot->parameters, &pilot->command);
    ++pilot->ticks;
    if(pilot->follow) {
        scene->camera_x = (int)lround(pilot->state.x) - 400;
        scene->camera_y = (int)lround(pilot->state.y) - 300;
    }
}

void pilot_draw(unsigned char *frame, const Pilot *pilot, const Scene *scene,
                const Sprite *sprite, const unsigned char *blend, const unsigned char *add)
{
    unsigned heading = ((unsigned)pilot->state.angle * sprite->frames + 32768u) / 65536u;
    sprite_draw(frame, sprite, heading, (int)lround(pilot->state.x) - scene->camera_x,
                (int)lround(pilot->state.y) - scene->camera_y, blend, add);
}

/* Deterministic integration route; feeds the same held-key mapping as a human.
 * Deliberately no reset, so checkpoints can compare cumulative native motion. */
unsigned pilot_replay_keys(unsigned tick)
{
    unsigned phase = tick % 900;
    if(phase < 120) return INPUT_FORWARD;
    if(phase < 240) return 0;
    if(phase < 360) return INPUT_FORWARD | INPUT_LEFT;
    if(phase < 480) return INPUT_BACK;
    if(phase < 600) return INPUT_FORWARD | INPUT_RIGHT;
    if(phase < 720) return INPUT_FORWARD | INPUT_BACK;
    return 0;
}
