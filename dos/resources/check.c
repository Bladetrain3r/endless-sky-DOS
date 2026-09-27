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
    const char *slash = strrchr(valid_path, '/');
    char path[512];
    ResourceProfile profile, sentinel = {11., 12., 13., .14, 15., 16., 17.};
    size_t i, prefix = slash ? (size_t)(slash - valid_path + 1) : 0;
    FILE *file;
    if(prefix + sizeof("BADRES.TMP") > sizeof(path)) return 0;
    memcpy(path, valid_path, prefix);
    strcpy(path + prefix, "BADRES.TMP");
    for(i=0;i<4;++i) {
        double v[7]={4000.,1.9,2.9,.00077,18600.,5.8,18.};
        if(i==0) v[2]=NAN;
        if(i==2) v[0]=1e10;
        file=fopen(path,"wb");
        if(!file) return 0;
        if(fwrite("ESRES2\0\0",1,8,file)!=8 || fwrite(v,8,i==1?6:7,file)!=(i==1?6:7)) return 0;
        if(i==3 && fputc('X',file)==EOF) return 0;
        if(fclose(file)) return 0;
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
    char header[8],name[80],previous[80]="";
    unsigned row_count,row;
    ResourceProfile stock, profile;
    ResourceState state;
    int rows = 0, cases = 0, tick, initial_overheated, request, expected_can, expected_fired;
    int expected_overheated, can, fired, witnessed_starve = 0, witnessed_hot = 0;
    int witnessed_recovery = 0, witnessed_overflow = 0, witnessed_exact = 0;
    double initial_energy, initial_heat, expected_energy, expected_heat, max_error = 0.;
    if(argc != 3 || !resource_load(&stock, argv[2]) || !reject_bad_profiles(argv[2])) return 2;
    if(fabs(stock.capacity - 4000.) > 1e-9 || fabs(stock.shot_energy - 5.8) > 1e-9)
        return 2;
    file=fopen(argv[1],"rb");
    if(!file || fread(header,1,8,file)!=8 || memcmp(header,"ESRTRC1",8) ||
       fread(&row_count,4,1,file)!=1 || row_count!=927) return 2;
    for(row=0;row<row_count;++row) {
        double v[17];
        if(fread(name,1,80,file)!=80 || name[79] || fread(v,8,17,file)!=17) return 1;
        tick=(int)v[0];
        profile=(ResourceProfile){v[1],v[2],v[3],v[4],v[5],v[6],v[7]};
        initial_energy=v[8];initial_heat=v[9];initial_overheated=(int)v[10];
        request=(int)v[11];expected_can=(int)v[12];expected_fired=(int)v[13];
        expected_energy=v[14];expected_heat=v[15];expected_overheated=(int)v[16];
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
    if(fgetc(file)!=EOF || ferror(file)) return 1;
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
