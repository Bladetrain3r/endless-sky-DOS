/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(c) do { if(!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while(0)

static double maximum(double a, double b) { return a > b ? a : b; }

static void boundaries(void)
{
    MotionParameters p = { .1, 0., 1., .1, 1. };
    MotionState s = { 0., 0., 2., 3., 0 };
    MotionCommand c = { 0, 0, 0, 0. };
    motion_step(&s, &p, &c);
    CHECK(s.vx == 2. && s.vy == 3. && s.x == 2. && s.y == 3.);
    c.back = 1;
    motion_step(&s, &p, &c);
    CHECK(s.vx == 2. && s.vy == 3.); /* No reverse engine: no braking. */
    c.forward = 1;
    motion_step(&s, &p, &c);
    CHECK(s.vx == 2. && s.vy == 3.); /* Conflicting commands cancel. */
    s = (MotionState){ 0., 0., .003, .001, 0 };
    c = (MotionCommand){ 1, 0, 1, 0. };
    motion_step(&s, &p, &c);
    CHECK(s.vx == .003 && s.vy == 0.); /* STOP keeps lateral drift. */
    CHECK(motion_angle(360.) == 0 && motion_angle(-90.) == 49152);
    CHECK(motion_angle(180. / 65536.) == 1);
    CHECK(motion_angle(-180. / 65536.) == 65535);
}

static void angles(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned i;
    double max_error = 0.;
    CHECK(f);
    for(i = 0; i < 65536; ++i) {
        double expected[2], x, y, error;
        CHECK(fread(expected, sizeof(double), 2, f) == 2);
        motion_unit((uint16_t)i, &x, &y);
        CHECK(isfinite(x) && isfinite(y) && isfinite(expected[0]) && isfinite(expected[1]));
        error = maximum(fabs(x - expected[0]), fabs(y - expected[1]));
        CHECK(isfinite(error) && error < 1e-14);
        if(error > max_error) max_error = error;
    }
    CHECK(fgetc(f) == EOF);
    fclose(f);
    printf("angle_cases=65536\nangle_max_error=%.17g\n", max_error);
}

static void trajectories(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[1024], extra;
    char previous_case[80] = "";
    int previous_tick = -1, cases = 0, steps = 0;
    double max_position = 0., max_velocity = 0.;
    MotionState s = {0};
    CHECK(f && fgets(line, sizeof(line), f));
    CHECK(!strcmp(line, "case,tick,cmd_forward,cmd_back,cmd_stop,cmd_turn,accel,reverse_accel,turn_rate,drag,mult,x,y,vx,vy,angle_steps\n"));
    while(fgets(line, sizeof(line), f)) {
        MotionParameters p;
        MotionCommand c;
        MotionState expected;
        unsigned angle;
        char id[80];
        int tick;
        double ep, ev;
        int fields = sscanf(line, "%79[^,],%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%u %c",
            id, &tick, &c.forward, &c.back, &c.stop, &c.turn,
            &p.acceleration, &p.reverse_acceleration, &p.turn_rate, &p.drag,
            &p.acceleration_multiplier, &expected.x, &expected.y,
            &expected.vx, &expected.vy, &angle, &extra);
        CHECK(fields == 16 && angle < 65536);
        expected.angle = (uint16_t)angle;
        if(strcmp(id, previous_case)) {
            CHECK(tick == 0);
            s = expected;
            ++cases;
        } else {
            CHECK(tick == previous_tick + 1);
            motion_step(&s, &p, &c);
            ++steps;
        }
        CHECK(isfinite(s.x) && isfinite(s.y) && isfinite(s.vx) && isfinite(s.vy));
        CHECK(isfinite(expected.x) && isfinite(expected.y)
              && isfinite(expected.vx) && isfinite(expected.vy));
        ep = maximum(fabs(s.x - expected.x), fabs(s.y - expected.y));
        ev = maximum(fabs(s.vx - expected.vx), fabs(s.vy - expected.vy));
        if(!(isfinite(ep) && isfinite(ev) && ep < 1e-7 && ev < 1e-9 && s.angle == angle))
            fprintf(stderr, "case=%s tick=%d position=%.17g velocity=%.17g angle=%u/%u\n",
                    id, tick, ep, ev, (unsigned)s.angle, angle);
        CHECK(isfinite(ep) && isfinite(ev) && ep < 1e-7 && ev < 1e-9 && s.angle == angle);
        if(ep > max_position) max_position = ep;
        if(ev > max_velocity) max_velocity = ev;
        strcpy(previous_case, id);
        previous_tick = tick;
    }
    CHECK(!ferror(f) && cases >= 10 && steps >= 9000);
    fclose(f);
    printf("cases=%d\nsteps=%d\nposition_max_error=%.17g\nvelocity_max_error=%.17g\n",
           cases, steps, max_position, max_velocity);
}

static double milliseconds(void)
{
#ifdef __DJGPP__
    return (double)uclock() * 1000. / UCLOCKS_PER_SEC;
#else
    return (double)clock() * 1000. / CLOCKS_PER_SEC;
#endif
}

static void benchmark(void)
{
    MotionState ships[64] = {{0}};
    MotionParameters p = { .08, .02, 2., .02, 1. };
    MotionCommand c = { 1, 0, 0, .5 };
    int tick, ship;
    double start, elapsed, sum = 0.;
    for(ship = 0; ship < 64; ++ship) ships[ship].angle = (uint16_t)(ship * 1024);
    start = milliseconds();
    for(tick = 0; tick < 600; ++tick)
        for(ship = 0; ship < 64; ++ship) motion_step(&ships[ship], &p, &c);
    elapsed = milliseconds() - start;
    for(ship = 0; ship < 64; ++ship) sum += fabs(ships[ship].x) + fabs(ships[ship].y);
    CHECK(isfinite(sum) && sum > 1.);
    printf("benchmark_steps=38400\nbenchmark_ms=%.6f\nms_per_64_ship_tick=%.6f\nstate_bytes=%lu\nchecksum=%.17g\n",
           elapsed, elapsed / 600., (unsigned long)sizeof(ships), sum);
}

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    boundaries();
    angles(argv[2]);
    trajectories(argv[1]);
    benchmark();
    puts("status=pass");
    return 0;
}
