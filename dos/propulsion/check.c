/* Differential replay of native Ship::Move and optional post-move shot.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "propulsion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while(0)
enum { TICK, FORWARD, BACK, STOP, TURN, SHOT, ACCEL, REVERSE, TURN_RATE, DRAG, MULT,
    CAPACITY, GENERATION, HEAT_GENERATION, DISSIPATION, MAX_HEAT, SHOT_ENERGY, SHOT_HEAT,
    START_ENERGY, START_HEAT, START_OVERHEATED, X, Y, VX, VY, ANGLE, ENERGY, HEAT,
    OVERHEATED, FIRED, FIELDS };

static double max2(double a, double b) { return a > b ? a : b; }

static void malformed_profiles(const PropulsionProfile *valid)
{
    int mode;
    for(mode = 0; mode < 4; ++mode) {
        FILE *out = fopen("BAD.DAT", "wb");
        PropulsionProfile candidate = *valid, body = *valid;
        CHECK(out);
        if(mode == 0) fwrite("BADMAGIC", 1, 8, out);
        else fwrite("ESPROP2\0", 1, 8, out);
        if(mode == 3) body.turn_heat = NAN;
        CHECK(fwrite(&body, 1, mode == 1 ? 8 : sizeof(body), out)
              == (unsigned)(mode == 1 ? 8 : sizeof(body)));
        if(mode == 2) CHECK(fputc('X', out) == 'X');
        CHECK(fclose(out) == 0);
        CHECK(!propulsion_load(&candidate, "BAD.DAT"));
        CHECK(memcmp(&candidate, valid, sizeof(candidate)) == 0);
        CHECK(remove("BAD.DAT") == 0);
    }
}

int main(int argc, char **argv)
{
    FILE *file;
    char name[80], previous[80] = "";
    double v[FIELDS], max_position = 0., max_velocity = 0., max_energy = 0., max_heat = 0.;
    int cases = 0, steps = 0, partial = 0, shared = 0, zero_capacity = 0;
    int recovery = 0, shot_denials = 0, previous_tick = -1;
    PropulsionProfile propulsion;
    MotionState motion = {0};
    ResourceState resources = {0};
    ResourceProfile profile = {0};
    CHECK(argc == 4);
    CHECK(resource_load(&profile, argv[2]));
    CHECK(propulsion_load(&propulsion, argv[3]));
    malformed_profiles(&propulsion);
    CHECK(fabs(propulsion.thrust_energy - 1.) < 1e-14);
    CHECK(fabs(propulsion.turn_energy - .5) < 1e-14);
    file = fopen(argv[1], "rb"); CHECK(file);
    while(fread(name, 1, 80, file) == 80) {
        double ep, ev, ee, eh;
        int field;
        CHECK(name[79] == '\0' && fread(v, sizeof(double), FIELDS, file) == FIELDS);
        for(field = 0; field < FIELDS; ++field) CHECK(isfinite(v[field]));
        if(strcmp(name, previous)) {
            CHECK(v[TICK] == 0. && cases < 100);
            profile.capacity = v[CAPACITY]; profile.generation = v[GENERATION];
            profile.heat_generation = v[HEAT_GENERATION]; profile.dissipation = v[DISSIPATION];
            profile.max_heat = v[MAX_HEAT]; profile.shot_energy = v[SHOT_ENERGY];
            profile.shot_heat = v[SHOT_HEAT];
            motion = (MotionState){v[X], v[Y], v[VX], v[VY], (unsigned short)v[ANGLE]};
            resources = (ResourceState){v[ENERGY], v[HEAT], (int)v[OVERHEATED]};
            ++cases;
        } else {
            MotionParameters parameters = {v[ACCEL], v[REVERSE], v[TURN_RATE], v[DRAG], v[MULT]};
            MotionCommand command = {(int)v[FORWARD], (int)v[BACK], (int)v[STOP], v[TURN]};
            int fired = 0;
            CHECK(v[TICK] == previous_tick + 1);
            resource_tick(&resources, &profile);
            propulsion_step(&motion, &parameters, &command, &resources, &profile, &propulsion);
            if(v[SHOT]) fired = resource_fire(&resources, &profile);
            CHECK(fired == (int)v[FIRED]);
            if(v[SHOT] && !fired) ++shot_denials;
            ++steps;
            if(!strcmp(name, "tiny_forward") && v[TICK] == 1.) {
                CHECK(motion.vy < -.5 && motion.vy > -.62);
                ++partial;
            }
            if(!strcmp(name, "negative_shared") && v[TICK] == 1.) {
                CHECK(motion.angle != 0 && resources.energy == 0.);
                ++shared;
            }
            if(!strcmp(name, "zero_capacity") && v[TICK] == 1.) {
                CHECK(motion.vx == 2. && motion.vy == -1.);
                ++zero_capacity;
            }
            if(!strcmp(name, "heat_recovery") && v[OVERHEATED] == 0. && previous_tick > 0)
                ++recovery;
        }
        ep = max2(fabs(motion.x - v[X]), fabs(motion.y - v[Y]));
        ev = max2(fabs(motion.vx - v[VX]), fabs(motion.vy - v[VY]));
        ee = fabs(resources.energy - v[ENERGY]); eh = fabs(resources.heat - v[HEAT]);
        if(!(ep <= 1e-8 && ev <= 1e-10 && ee <= 1e-9 && eh <= 1e-9
            && motion.angle == (unsigned)v[ANGLE] && resources.overheated == (int)v[OVERHEATED]))
            printf("%s tick %.0f ep %.17g ev %.17g ee %.17g eh %.17g angle %u/%.0f hot %d/%.0f\n",
                name, v[TICK], ep, ev, ee, eh, (unsigned)motion.angle, v[ANGLE],
                resources.overheated, v[OVERHEATED]);
        CHECK(ep <= 1e-8 && ev <= 1e-10 && ee <= 1e-9 && eh <= 1e-9);
        CHECK(motion.angle == (unsigned)v[ANGLE] && resources.overheated == (int)v[OVERHEATED]);
        max_position = max2(max_position, ep); max_velocity = max2(max_velocity, ev);
        max_energy = max2(max_energy, ee); max_heat = max2(max_heat, eh);
        strcpy(previous, name); previous_tick = (int)v[TICK];
    }
    CHECK(!ferror(file) && cases == 17 && steps == 667 && partial == 1
          && shared == 1 && zero_capacity == 1 && recovery > 0 && shot_denials > 0);
    fclose(file);
    printf("cases=%d\nsteps=%d\nmalformed_profiles=4\nfirst_starvation_partial=%d\nnegative_shared=%d\nzero_capacity=%d\nrecovery_ticks=%d\nshot_denials=%d\n", cases, steps, partial, shared, zero_capacity, recovery, shot_denials);
    printf("position_max_error=%.17g\nvelocity_max_error=%.17g\nenergy_max_error=%.17g\nheat_max_error=%.17g\nstatus=pass\n",
        max_position, max_velocity, max_energy, max_heat);
    return 0;
}
