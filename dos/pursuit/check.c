/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pursuit.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { printf("check failed line %d: %s\n", __LINE__, #x); exit(1); } } while(0)
static double maximum(double a, double b) { return a > b ? a : b; }
static void invalid_profile(const char *path, const unsigned char *bytes, size_t count)
{
    MotionParameters unchanged={3.,4.,5.,6.,7.};
    MotionParameters copy=unchanged;
    FILE *f=fopen(path,"wb"); CHECK(f);
    CHECK(fwrite(bytes,1,count,f)==count && !fclose(f));
    CHECK(!pursuit_load(&copy,path));
    CHECK(copy.acceleration==unchanged.acceleration && copy.reverse_acceleration==unchanged.reverse_acceleration
          && copy.turn_rate==unchanged.turn_rate && copy.drag==unchanged.drag
          && copy.acceleration_multiplier==unchanged.acceleration_multiplier);
}
int main(int argc, char **argv)
{
    FILE *file;
    unsigned char magic[8];
    uint32_t command_count, intercept_count, i;
    double turn_error = 0., time_error = 0.;
    MotionParameters p;
    CHECK(argc == 4 && pursuit_load(&p, argv[2]));
    CHECK(p.acceleration > 0. && p.reverse_acceleration == 0. && p.turn_rate > 0.);
    {
        unsigned char bytes[49]; uint64_t bits;
        FILE *profile=fopen(argv[2],"rb"); CHECK(profile);
        CHECK(fread(bytes,1,48,profile)==48 && !fclose(profile));
        invalid_profile(argv[3],bytes,47);
        bytes[48]=0; invalid_profile(argv[3],bytes,49);
        bytes[0]='X'; invalid_profile(argv[3],bytes,48); bytes[0]='E';
        bits=UINT64_C(0x7ff8000000000000); memcpy(bytes+8,&bits,8);
        invalid_profile(argv[3],bytes,48);
        bits=UINT64_C(0x4026000000000000); memcpy(bytes+8,&bits,8);
        invalid_profile(argv[3],bytes,48);
    }
    file = fopen(argv[1], "rb"); CHECK(file);
    CHECK(fread(magic, 1, 8, file) == 8 && !memcmp(magic, "ESPTRACE", 8));
    CHECK(fread(&command_count, 4, 1, file) == 1);
    CHECK(fread(&intercept_count, 4, 1, file) == 1);
    CHECK(command_count >= 16 && intercept_count >= 10);
    for(i = 0; i < command_count; ++i) {
        char name[32]; double d[13]; MotionState s; MotionCommand c;
        CHECK(fread(name, 1, sizeof name, file) == sizeof name);
        CHECK(fread(d, sizeof(double), 13, file) == 13);
        s.x=d[0]; s.y=d[1]; s.vx=d[2]; s.vy=d[3]; s.angle=motion_angle(d[4]);
        c=pursuit_command(&s,&p,d[5],d[6]);
        turn_error=maximum(turn_error,fabs(c.turn-d[9]));
        if(!(fabs(c.turn-d[9]) <= 1e-12 && c.forward == (int)d[10]
             && c.back == (int)d[11] && d[12] == 0. && c.stop == 0))
            fprintf(stderr,"case=%s turn=%.17g/%.17g forward=%d/%g\n",name,c.turn,d[9],c.forward,d[10]);
        CHECK(fabs(c.turn-d[9]) <= 1e-12 && c.forward == (int)d[10]
              && c.back == (int)d[11] && d[12] == 0. && c.stop == 0);
    }
    for(i = 0; i < intercept_count; ++i) {
        char name[32]; double d[6], actual;
        CHECK(fread(name, 1, sizeof name, file) == sizeof name);
        CHECK(fread(d,sizeof(double),6,file) == 6);
        actual=pursuit_intercept(d[0],d[1],d[2],d[3],d[4]);
        if(isnan(d[5])) {
            if(!isnan(actual)) {
                uint64_t actual_bits, expected_bits;
                memcpy(&actual_bits,&actual,8); memcpy(&expected_bits,&d[5],8);
                printf("intercept=%s expected_bits=%08lx%08lx actual_bits=%08lx%08lx self_equal=%d\n",name,
                    (unsigned long)(expected_bits>>32),(unsigned long)(expected_bits&0xffffffffu),
                    (unsigned long)(actual_bits>>32),(unsigned long)(actual_bits&0xffffffffu),actual==actual);
            }
            CHECK(isnan(actual));
        }
        else if(isinf(d[5])) {
            if(!(isinf(actual) && signbit(actual)==signbit(d[5])))
                printf("intercept=%s expected=%.17g actual=%.17g\n",name,d[5],actual);
            CHECK(isinf(actual) && signbit(actual)==signbit(d[5]));
        }
        else {
            double error=fabs(actual-d[5]);
            time_error=maximum(time_error,error);
            if(error > 1e-8) fprintf(stderr,"intercept=%s time=%.17g/%.17g\n",name,actual,d[5]);
            CHECK(error <= 1e-8);
        }
    }
    CHECK(fgetc(file) == EOF && !ferror(file)); fclose(file);
    printf("commands=%u\nintercepts=%u\nturn_max_error=%.17g\ntime_max_error=%.17g\nstatus=pass\n",
           (unsigned)command_count,(unsigned)intercept_count,turn_error,time_error);
    return 0;
}
