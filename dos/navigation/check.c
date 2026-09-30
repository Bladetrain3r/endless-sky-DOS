/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "approach.h"
#include "../pursuit/pursuit.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"check failed line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
int main(int argc,char **argv)
{
    FILE *f; char magic[8]; uint32_t count,i; MotionParameters p; double max_error=0.;
    CHECK(argc==3 && pursuit_load(&p,argv[2]));
    f=fopen(argv[1],"rb"); CHECK(f);
    CHECK(fread(magic,1,8,f)==8 && !memcmp(magic,"ESAPTRC1",8));
    CHECK(fread(&count,4,1,f)==1 && count>=20 && count<=100);
    for(i=0;i<count;++i) {
        char name[32]; double d[13],error; MotionState s; MotionCommand c;
        CHECK(fread(name,1,32,f)==32 && fread(d,sizeof(double),13,f)==13);
        name[31]=0;
        s.x=d[0]; s.y=d[1]; s.vx=d[2]; s.vy=d[3]; s.angle=motion_angle(d[4]);
        c=navigation_approach(&s,&p,d[5],d[6],d[7]);
        error=fabs(c.turn-d[8]); if(error>max_error) max_error=error;
        if(!(error<=1e-12 && c.forward==(int)d[9] && c.back==(int)d[10]
             && c.stop==(int)d[11]))
            fprintf(stderr,"case=%s turn=%.17g/%.17g forward=%d/%.0f back=%d/%.0f\n",
                    name,c.turn,d[8],c.forward,d[9],c.back,d[10]);
        CHECK(error<=1e-12 && c.forward==(int)d[9] && c.back==(int)d[10]
              && c.stop==(int)d[11]);
        if(d[12]) CHECK(!c.forward && !c.back && !c.stop && !c.turn);
    }
    CHECK(fgetc(f)==EOF && !ferror(f)); fclose(f);
    printf("cases=%u\nturn_max_error=%.17g\nstatus=pass\n",(unsigned)count,max_error);
    return 0;
}
