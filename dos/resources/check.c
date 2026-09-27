/* Compare portable resources to native Ship resource traces.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "resources.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same(double a, double b, double *max_error)
{
    double error = fabs(a - b);
    if(error > *max_error) *max_error = error;
    return error <= 1e-9;
}

static int reject_bad_profiles(const char *valid_path)
{
    const char *bad[] = {
        "ESRESOURCE1\n1 2 nan .1 3 4 5\n",
        "ESRESOURCE1\n1 2 3 .1 4 5\n",
        "ESRESOURCE1\n10000000000 2 3 .1 4 5 6\n",
        "ESRESOURCE1\n1 2 3 .1 4 5 6 trailing\n"
    };
    const char *slash = strrchr(valid_path, '/');
    char path[512];
    ResourceProfile profile, sentinel = {11., 12., 13., .14, 15., 16., 17.};
    size_t i, prefix = slash ? (size_t)(slash - valid_path + 1) : 0;
    FILE *file;
    if(prefix + sizeof("BADRES.TMP") > sizeof(path)) return 0;
    memcpy(path, valid_path, prefix);
    strcpy(path + prefix, "BADRES.TMP");
    for(i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        file = fopen(path, "w");
        if(!file || fputs(bad[i], file) == EOF || fclose(file)) return 0;
        profile = sentinel;
        if(resource_load(&profile, path) || memcmp(&profile, &sentinel, sizeof(profile))) {
            remove(path); return 0;
        }
    }
    return remove(path) == 0;
}

int main(int argc, char **argv)
{
    FILE *file;
    char line[512], name[80], previous[80] = "";
    ResourceProfile stock, profile;
    ResourceState state;
    int rows = 0, cases = 0, tick, initial_overheated, request, expected_can, expected_fired;
    int expected_overheated, can, fired, witnessed_starve = 0, witnessed_hot = 0;
    int witnessed_recovery = 0, witnessed_overflow = 0, witnessed_exact = 0;
    double initial_energy, initial_heat, expected_energy, expected_heat, max_error = 0.;
    if(argc != 3 || !resource_load(&stock, argv[2]) || !reject_bad_profiles(argv[2])) return 2;
    if(fabs(stock.capacity - 4000.) > 1e-9 || fabs(stock.shot_energy - 5.8) > 1e-9)
        return 2;
    file = fopen(argv[1], "r");
    if(!file || !fgets(line, sizeof(line), file)) return 2;
    while(fgets(line, sizeof(line), file)) {
        if(sscanf(line, "%79[^,],%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d,%d,%d,%lf,%lf,%d",
            name, &tick, &profile.capacity, &profile.generation,
            &profile.heat_generation, &profile.dissipation, &profile.max_heat,
            &profile.shot_energy, &profile.shot_heat, &initial_energy,
            &initial_heat, &initial_overheated, &request, &expected_can,
            &expected_fired, &expected_energy, &expected_heat, &expected_overheated) != 18) {
            fprintf(stderr, "Malformed native row %d\n", rows + 1); return 1;
        }
        if(strcmp(name, previous)) {
            if(tick != 0) return 1;
            strcpy(previous, name);
            state.energy = initial_energy;
            state.heat = initial_heat;
            state.overheated = initial_overheated;
            ++cases;
        }
        resource_tick(&state, &profile);
        can = resource_can_fire(&state, &profile);
        fired = request ? resource_fire(&state, &profile) : 0;
        if(can != expected_can || fired != expected_fired ||
            state.overheated != expected_overheated ||
            !same(state.energy, expected_energy, &max_error) ||
            !same(state.heat, expected_heat, &max_error)) {
            fprintf(stderr, "Mismatch %s tick %d: E %.17g/%.17g H %.17g/%.17g hot %d/%d can %d/%d fire %d/%d\n",
                name, tick, state.energy, expected_energy, state.heat, expected_heat,
                state.overheated, expected_overheated, can, expected_can, fired, expected_fired);
            return 1;
        }
        if(!strcmp(name, "energy_starve") && request && !can) witnessed_starve = 1;
        if(!strcmp(name, "heat_overheat") && state.overheated && !can) witnessed_hot = 1;
        if(!strcmp(name, "heat_recovery") && tick && !state.overheated) witnessed_recovery = 1;
        if(!strcmp(name, "energy_overflow") && expected_energy > profile.capacity) witnessed_overflow = 1;
        if(!strcmp(name, "heat_exact_max") && state.heat == profile.max_heat && !state.overheated)
            witnessed_exact |= 1;
        if(!strcmp(name, "heat_exact_recover") && state.heat == .9 * profile.max_heat && state.overheated)
            witnessed_exact |= 2;
        ++rows;
    }
    fclose(file);
    if(cases != 9 || rows != 927 || !witnessed_starve || !witnessed_hot ||
       !witnessed_recovery || !witnessed_overflow || witnessed_exact != 3) return 1;
    state.energy = 0.; state.heat = stock.max_heat * 2.; state.overheated = 1;
    resource_reset(&state, &stock);
    if(state.energy != stock.capacity || state.heat != 0. || state.overheated ||
       !resource_can_fire(&state, &stock)) return 1;
    state.energy = 0.; state.heat = stock.max_heat + 1.; state.overheated = 1;
    if(resource_fire(&state, &stock) || state.energy != 0. ||
       state.heat != stock.max_heat + 1.) return 1;
    printf("status=pass\ncases=%d\nrows=%d\nmax_error=%.17g\n", cases, rows, max_error);
    return 0;
}
