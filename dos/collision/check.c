/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mask.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef __DJGPP__
#include <float.h>
#endif
static CollisionMask masks[3];
int main(int argc,char **argv)
{
    FILE *f;
    unsigned declared;
    unsigned count=0,failures=0,inside_failures=0,hit_failures=0,id,angle;
    int inside;
    double fraction,max_error=0.;
    MaskPoint p,v;
#ifdef __DJGPP__
    if(argc==4) _control87(PC_53, MCW_PC);
    printf("fpu_precision_bits=%u\n",(unsigned)(_control87(0,0)&MCW_PC));
#endif
    if((argc!=3 && argc!=4) || !masks_load(argv[1],masks,3)) return 1;
    f=fopen(argv[2],"rb");
    if(!f || fread(&declared,4,1,f)!=1) return 2;
    for(;;) {
        double actual,error;
        int got;
        if(count==declared) break;
        got=fread(&id,4,1,f); got+=fread(&angle,4,1,f);
        got+=fread(&p.x,8,1,f); got+=fread(&p.y,8,1,f);
        got+=fread(&v.x,8,1,f); got+=fread(&v.y,8,1,f);
        got+=fread(&inside,4,1,f); got+=fread(&fraction,8,1,f);
        int ainside,bad_inside,bad_hit;
        if(got!=8 || id>=3 || angle>=65536) return 3;
        actual=mask_collide(&masks[id],p,v,(uint16_t)angle);
        ainside=mask_contains(&masks[id],p,(uint16_t)angle);
        error=fabs(actual-fraction);
        bad_inside=ainside!=inside;
        bad_hit=(actual<1.)!=(fraction<1.);
        inside_failures+=bad_inside; hit_failures+=bad_hit;
        if(error>max_error) max_error=error;
        if(bad_inside || bad_hit || !isfinite(actual) || error>1e-10) {
            if(failures<8) printf("mismatch_%u=case%u mask%u angle%u contains%d/%d hit%.17g/%.17g\n",
                failures,count,id,angle,ainside,inside,actual,fraction);
            ++failures;
        }
        ++count;
    }
    if(fgetc(f)!=EOF || ferror(f) || !count) return 3;
    fclose(f);
    printf("cases=%u\nfailures=%u\ncontains_failures=%u\nhit_failures=%u\nmax_fraction_error=%.17g\n",
           count,failures,inside_failures,hit_failures,max_error);
    printf("mask_storage_bytes=%lu\nstatus=%s\n",(unsigned long)sizeof(masks),failures?"failed":"pass");
    return failures?4:0;
}
