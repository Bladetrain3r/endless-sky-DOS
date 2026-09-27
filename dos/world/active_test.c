/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "active.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail(const char *what) {fprintf(stderr,"FAIL %s\n",what);return 1;}
int main(int argc,char **argv) {
    WorldStore *s;ActiveSystem a={0},b={0},guard={0};uint64_t sol,other,id;
    uint32_t i;int round;
    if(argc<3) return fail("usage: active_test PACK SYSTEM [OTHER]");
    s=world_open(argv[1]);if(!s) {puts("OPEN_REJECTED");return 2;}
    if(world_store_bytes(s)>262144) return fail("store budget");
    if(!strcmp(argv[2],"--scan")) {
        char name[4096];size_t max=0;unsigned long scanned=0;
        while(fgets(name,sizeof name,stdin)) {
            size_t n=strlen(name);
            if(!n||name[n-1]!='\n') return fail("scan name too long");
            name[n-1]=0;
            if(!world_find_system(s,name,&id)||!world_load_system(s,id,&a)) return fail("scan load");
            if(a.allocation_bytes>max) max=a.allocation_bytes;
            world_release_system(&a);scanned++;
        }
        printf("SCAN\t%lu\t%lu\t%lu\n",scanned,(unsigned long)max,
            (unsigned long)world_store_bytes(s));
        world_close(s);return 0;
    }
    if(!world_find_system(s,argv[2],&sol)||!world_load_system(s,sol,&a)) return fail("load named system");
    if(a.allocation_bytes>65536||!a.allocation||strcmp(a.name,argv[2])) return fail("active budget/name");
    guard=a;
    if(world_load_system(s,sol,&a)||memcmp(&guard,&a,sizeof a)) return fail("failure clobbered active");
    if(world_load_system(s,UINT64_MAX,&b)||memcmp(&b,&(ActiveSystem){0},sizeof b)) return fail("missing ID clobbered output");
    other=sol;
    if(argc>3&&!world_find_system(s,argv[3],&other)) return fail("second name");
    if(!world_load_system(s,other,&b)) return fail("second load");
    if(strcmp(a.name,argv[2])||memcmp(&guard,&a,sizeof a)) return fail("first changed after second load");
    printf("S\t%s\t%s\t%lu\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%lu\t%lu\t%lu\n",
        a.name,a.display,(unsigned long)a.flags,a.x,a.y,a.jump_range,a.arrival_hyper,a.arrival_jump,
        a.departure_hyper,a.departure_jump,a.habitable,(unsigned long)a.object_count,(unsigned long)a.link_count,(unsigned long)a.trade_count);
    for(i=0;i<a.object_count;i++) {WorldObject *o=&a.objects[i];
        printf("O\t%ld\t%ld\t%016llx\t%.17g\t%.17g\t%.17g\t%s\n",(long)o->index,(long)o->parent,
            (unsigned long long)o->planet,o->distance,o->period,o->offset,o->sprite?o->sprite:"<NULL>");}
    for(i=0;i<a.link_count;i++) printf("L\t%016llx\n",(unsigned long long)a.links[i]);
    for(i=0;i<a.trade_count;i++) printf("T\t%016llx\t%ld\n",(unsigned long long)a.trade[i].commodity,(long)a.trade[i].price);
    world_release_system(&b);world_release_system(&a);
    for(round=0;round<100;round++) {
        if(!world_load_system(s,sol,&a)||a.allocation_bytes>65536) return fail("repeated load");
        world_release_system(&a);
        if(memcmp(&a,&(ActiveSystem){0},sizeof a)) return fail("release not cleared");
    }
    id=123;
    if(world_find_system(s,"__nonexistent_system__",&id)||id!=123) return fail("missing name clobbered ID");
    if(!world_load_system(s,sol,&b)) return fail("retained load");
    world_close(s);
    if(strcmp(b.name,argv[2])) return fail("name invalid after store close");
    for(i=0;i<b.object_count;i++)
        if(b.objects[i].sprite && strlen(b.objects[i].sprite)>4096) return fail("retained sprite");
    world_release_system(&b);
    return 0;
}
