/* DOS trace comparison for stock Star Barge generic resource and shield kernels.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../resources/resources.h"
#include "../shields/shields.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int read32(FILE *file, uint32_t *value)
{
    unsigned char b[4];
    if(fread(b, 1, 4, file) != 4) return 0;
    *value = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    return 1;
}
static int read64(FILE *file, double *value)
{
    unsigned char b[8]; uint64_t bits = 0; unsigned i;
    if(fread(b, 1, 8, file) != 8) return 0;
    for(i = 0; i < 8; ++i) bits |= (uint64_t)b[i] << (8 * i);
    memcpy(value, &bits, 8);
    return 1;
}
int main(int argc, char **argv)
{
    FILE *file, *hullFile; ResourceProfile rp; ShieldProfile sp; ResourceState rs; ShieldState ss;
    uint32_t count, id, tick, prior, overheat, delay, disabled, i, previous = 99;
    double expectedShield, expectedEnergy, expectedHeat, hull, maxHull, minimumHull, error = 0., d;
    char header[9];
    if(argc != 5 || !resource_load(&rp, argv[2]) || !shield_load(&sp, argv[3])
        || !(file = fopen(argv[1], "rb"))) return 2;
    hullFile = fopen(argv[4], "rb");
    if(!hullFile || fread(header, 1, 9, hullFile) != 9 || memcmp(header, "ESPLAYER1", 9)
        || !read64(hullFile, &maxHull) || !read64(hullFile, &minimumHull)
        || fgetc(hullFile) != EOF || !isfinite(maxHull) || !isfinite(minimumHull)
        || maxHull <= 0. || minimumHull <= 0. || minimumHull >= maxHull) return 9;
    fclose(hullFile);
    if(!read32(file, &count) || count != 430) return 3;
    for(i = 0; i < count; ++i) {
        if(!read32(file, &id) || !read32(file, &tick) || !read32(file, &prior)
            || !read64(file, &expectedShield) || !read64(file, &expectedEnergy)
            || !read64(file, &expectedHeat) || !read32(file, &overheat)
            || !read32(file, &delay) || !read64(file, &hull) || !read32(file, &disabled)
            || id > 4) return 4;
        if(id != previous) {
            ss.shields = (id == 1 || id == 2 || id == 4) ? 0. : sp.capacity;
            ss.delay = 0;
            rs.energy = id == 2 ? 0. : id == 4 ? 100. : rp.capacity;
            rs.heat = id == 3 ? rp.max_heat + 1. : 0.;
            rs.overheated = id == 3;
            previous = id;
            if(tick != 0) return 5;
        }
        if((unsigned)(hull < minimumHull || rs.overheated) != prior) return 6;
        shield_tick(&ss, &sp, &rs, (int)prior);
        resource_tick_disabled(&rs, &rp, hull < minimumHull);
        d = fabs(ss.shields - expectedShield); if(d > error) error = d;
        d = fabs(rs.energy - expectedEnergy); if(d > error) error = d;
        d = fabs(rs.heat - expectedHeat); if(d > error) error = d;
        if(error > 1e-8 || (unsigned)rs.overheated != overheat || (unsigned)ss.delay != delay
            || (unsigned)(hull < minimumHull || rs.overheated) != disabled) {
            printf("status=fail row=%lu case=%lu tick=%lu error=%.17g\n",
                (unsigned long)i, (unsigned long)id, (unsigned long)tick, error);
            return 7;
        }
    }
    if(fgetc(file) != EOF || ferror(file)) return 8;
    fclose(file);
    printf("status=pass rows=%lu max_error=%.17g\n", (unsigned long)count, error);
    return 0;
}
