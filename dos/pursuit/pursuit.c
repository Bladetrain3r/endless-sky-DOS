/* SPDX-License-Identifier: GPL-3.0-or-later
 * Adapted from Endless Sky AI.cpp, Copyright (c) 2014 Michael Zahniser.
 * Bounded stock Star Barge attack steering, not full AI.
 */
#include "pursuit.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TO_DEG (180. / 3.14159265358979323846)
#define PURSUIT_PI 3.14159265358979323846
/* Match std::min/max first-argument behavior, including a NaN second value. */
static double lesser(double a, double b) { return b < a ? b : a; }
static double greater(double a, double b) { return a < b ? b : a; }
static double quiet_nan(void)
{
    const uint64_t bits = UINT64_C(0x7ff8000000000000);
    double value;
    memcpy(&value, &bits, 8);
    return value;
}
static double positive_infinity(void)
{
    const uint64_t bits = UINT64_C(0x7ff0000000000000);
    double value;
    memcpy(&value, &bits, 8);
    return value;
}

int pursuit_load(MotionParameters *params, const char *path)
{
    static const unsigned char magic[8] = {'E','S','P','U','R','S','1',0};
    unsigned char bytes[48];
    MotionParameters value;
    double fields[5];
    FILE *file;
    int i, j;
    if(!params || !path) return 0;
    file = fopen(path, "rb");
    if(!file) return 0;
    if(fread(bytes, 1, sizeof bytes, file) != sizeof bytes || fgetc(file) != EOF
       || ferror(file) || memcmp(bytes, magic, 8)) { fclose(file); return 0; }
    fclose(file);
    if(sizeof(double) != 8 || sizeof(uint64_t) != 8) return 0;
    for(i = 0; i < 5; ++i) {
        uint64_t bits = 0;
        for(j = 0; j < 8; ++j) bits |= (uint64_t)bytes[8 + i * 8 + j] << (8 * j);
        memcpy(&fields[i], &bits, 8);
        if(!isfinite(fields[i])) return 0;
    }
    value.acceleration = fields[0]; value.reverse_acceleration = fields[1];
    value.turn_rate = fields[2]; value.drag = fields[3];
    value.acceleration_multiplier = fields[4];
    if(value.acceleration <= 0. || value.acceleration > 10.
       || value.reverse_acceleration != 0.
       || value.turn_rate <= 0. || value.turn_rate > 20.
       || value.drag <= 0. || value.drag > 1.
       || value.acceleration_multiplier != 1.)
        return 0;
    *params = value;
    return 1;
}

double pursuit_turn_toward(const MotionState *s, const MotionParameters *p,
                           double dx, double dy)
{
    double fx, fy, cross, dot;
    motion_unit(s->angle, &fx, &fy);
    cross = dx * fy - dy * fx;
    dot = dx * fx + dy * fy;
    if(dot > 0.) {
        const double length_squared = dx * dx + dy * dy;
        const int close = dot * dot >= .9999 * length_squared;
        const double length = sqrt(length_squared);
        const double sine = lesser(1., greater(-1., cross / length));
        const double angle = asin(sine) * TO_DEG;
        if(fabs(angle) < p->turn_rate) {
            if(close) return 0.;
            return -angle / p->turn_rate;
        }
    }
    return cross < 0. ? 1. : -1.;
}

MotionCommand pursuit_command(const MotionState *s, const MotionParameters *p,
                              double target_x, double target_y)
{
    const double dx = target_x - s->x;
    const double dy = target_y - s->y;
    const double distance = sqrt(dx * dx + dy * dy);
    const double speed = sqrt(s->vx * s->vx + s->vy * s->vy);
    const double diameter = greater(200., (360. / p->turn_rate) * speed / PURSUIT_PI);
    double fx, fy, facing, ux, uy;
    MotionCommand command = {0, 0, 0, 0.};
    command.turn = pursuit_turn_toward(s, p, dx, dy);
    motion_unit(s->angle, &fx, &fy);
    /* Native Point::Unit returns (1,0) for zero displacement. */
    ux = distance ? dx * (1. / distance) : 1.;
    uy = distance ? dy * (1. / distance) : 0.;
    facing = fx * ux + fy * uy;
    if((facing >= 0. && distance > diameter)
       || ((s->vx * dx + s->vy * dy) < 0. && facing >= .9))
        command.forward = 1;
    return command;
}

double pursuit_intercept(double dx, double dy, double vx, double vy, double speed)
{
    const double a = vx * vx + vy * vy - speed * speed;
    const double b = 2. * (dx * vx + dy * vy);
    const double c = dx * dx + dy * dy;
    double discriminant = b * b - 4. * a * c;
    double r1, r2;
    if(discriminant < 0.) return quiet_nan();
    discriminant = sqrt(discriminant);
    /* On DJGPP x87, unordered comparisons after 0/0 can select -infinity.
     * Resolve the degenerate quadratic explicitly with native classifications. */
    if(a == 0.) return b < 0. ? positive_infinity() : quiet_nan();
    r1 = (-b + discriminant) / (2. * a);
    r2 = (-b - discriminant) / (2. * a);
    if(r1 >= 0. && r2 >= 0.) return lesser(r1, r2);
    if(r1 >= 0. || r2 >= 0.) return greater(r1, r2);
    return quiet_nan();
}
