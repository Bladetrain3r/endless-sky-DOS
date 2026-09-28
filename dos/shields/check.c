/* Compare portable shield generation with unchanged Ship::DoGeneration traces.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "shields.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same(double a, double b, double *maximum)
{
    double error = fabs(a - b);
    if(error > *maximum) *maximum = error;
    return error <= 1e-9;
}

static int reject_bad_profiles(const char *valid)
{
    char path[512];
    const char *slash = strrchr(valid, '/');
    size_t prefix = slash ? (size_t)(slash - valid + 1) : 0;
    ShieldProfile parsed, sentinel = {11.,12.,13.,14.,15.,16.,17.,18,19};
    unsigned char bytes[72];
    int i;
    FILE *file;
    if(prefix + sizeof("BADSHLD.TMP") > sizeof(path)) return 0;
    memcpy(path, valid, prefix); strcpy(path + prefix, "BADSHLD.TMP");
    file = fopen(valid, "rb");
    if(!file || fread(bytes,1,72,file)!=72) return 0;
    fclose(file);
    for(i=0;i<5;++i) {
        file=fopen(path,"wb"); if(!file) return 0;
        if(i==0) bytes[0]='X';
        if(i==2) { unsigned j; for(j=8;j<16;++j) bytes[j]=255; }
        if(i==3) { bytes[64]=255; bytes[65]=255; bytes[66]=255; bytes[67]=127; }
        if(fwrite(bytes,1,i==1?71:72,file)!=(size_t)(i==1?71:72)) return 0;
        if(i==4 && fputc('X',file)==EOF) return 0;
        if(fclose(file)) return 0;
        parsed=sentinel;
        if(shield_load(&parsed,path) || memcmp(&parsed,&sentinel,sizeof(parsed))) return 0;
        file=fopen(valid,"rb"); if(!file || fread(bytes,1,72,file)!=72) return 0;
        fclose(file);
    }
    return remove(path)==0;
}

int main(int argc, char **argv)
{
    FILE *file;
    char magic[8], name[80], previous[80]="";
    unsigned count, row;
    ShieldProfile stock, profile;
    ShieldState shield;
    ResourceProfile resource_profile;
    ResourceState resources;
    int cases=0, rows=0, witnessed_partial=0, witnessed_starve=0;
    int witnessed_hot=0, witnessed_recovery=0, witnessed_delay=0, witnessed_damage=0;
    double max_error=0.;
    if(argc!=4 || !shield_load(&stock,argv[2]) ||
       !resource_load(&resource_profile,argv[3]) || !reject_bad_profiles(argv[2])) return 2;
    if(stock.capacity!=1400. || stock.rate<=0. || stock.energy_cost<=0.) return 2;
    file=fopen(argv[1],"rb");
    if(!file || fread(magic,1,8,file)!=8 || memcmp(magic,"ESSHTR1\0",8) ||
       fread(&count,4,1,file)!=1 || count!=423) return 2;
    for(row=0;row<count;++row) {
        double v[22];
        int tick, damage, prior_disabled, expected_overheated, expected_delay;
        if(fread(name,1,80,file)!=80 || name[79] || fread(v,8,22,file)!=22) return 1;
        tick=(int)v[0];
        profile=(ShieldProfile){v[1],v[2],v[3],v[4],v[5],v[6],v[7],(int)v[8],(int)v[9]};
        damage=(int)v[15]; prior_disabled=(int)v[16];
        expected_overheated=(int)v[20]; expected_delay=(int)v[21];
        if(strcmp(name,previous)) {
            if(tick!=0) return 1;
            strcpy(previous,name);
            shield.shields=v[10]; resources.energy=v[11]; resources.heat=v[12];
            resources.overheated=(int)v[13]; shield.delay=(int)v[14];
            ++cases;
        }
        if(resources.overheated!=prior_disabled) return 1;
        if(damage) shield_damage(&shield,&profile,v[15],prior_disabled);
        shield_tick(&shield,&profile,&resources,prior_disabled);
        resource_tick(&resources,&resource_profile);
        if(!same(shield.shields,v[17],&max_error) ||
           !same(resources.energy,v[18],&max_error) ||
           !same(resources.heat,v[19],&max_error) ||
           resources.overheated!=expected_overheated || shield.delay!=expected_delay) {
            fprintf(stderr,"Mismatch %s tick %d: S %.17g/%.17g E %.17g/%.17g H %.17g/%.17g hot %d/%d delay %d/%d\n",
                name,tick,shield.shields,v[17],resources.energy,v[18],resources.heat,v[19],
                resources.overheated,expected_overheated,shield.delay,expected_delay);
            return 1;
        }
        if(!strcmp(name,"stock_partial") && shield.shields>700.) witnessed_partial=1;
        if(!strcmp(name,"energy_limited") && shield.shields<701.) witnessed_starve=1;
        if(!strcmp(name,"hot_prior_disabled") && tick==0 && prior_disabled &&
           shield.shields==700. && shield.delay==4 && !resources.overheated) witnessed_hot=1;
        if(!strcmp(name,"hot_prior_disabled") && tick==1 && !prior_disabled &&
           shield.shields>700. && shield.delay==3) witnessed_recovery=1;
        if(!strcmp(name,"synthetic_delay") && tick==0 && expected_delay==3 &&
           same(shield.shields,700.05,&max_error)) witnessed_delay=1;
        if(!strcmp(name,"synthetic_damage") && damage) witnessed_damage=1;
        ++rows;
    }
    if(fgetc(file)!=EOF || ferror(file)) return 1;
    fclose(file);
    if(cases!=10 || rows!=423 || !witnessed_partial || !witnessed_starve ||
       !witnessed_hot || !witnessed_recovery || !witnessed_delay || !witnessed_damage) return 1;
    shield_reset(&shield,&stock);
    if(shield.shields!=stock.capacity || shield.delay) return 1;
    printf("status=pass\ncases=%d\nrows=%d\nmax_error=%.17g\n",cases,rows,max_error);
    return 0;
}
