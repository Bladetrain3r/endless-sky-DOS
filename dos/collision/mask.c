/* SPDX-License-Identifier: GPL-3.0-or-later
 * Query arithmetic adapted from Endless Sky image/Mask.cpp.
 * Original Copyright (c) 2014 Michael Zahniser and contributors.
 */
#include "mask.h"
#include "../motion/motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static MaskPoint sub(MaskPoint a, MaskPoint b) { return (MaskPoint){a.x-b.x,a.y-b.y}; }
static double dot(MaskPoint a, MaskPoint b) { return a.x*b.x+a.y*b.y; }
static double cross(MaskPoint a, MaskPoint b) { return a.x*b.y-a.y*b.x; }
static double length(MaskPoint a) { return sqrt(dot(a,a)); }
static MaskPoint rotate(MaskPoint p, uint16_t facing)
{
    double x,y;
    motion_unit((uint16_t)(-facing),&x,&y);
    return (MaskPoint){-y*p.x-x*p.y,-y*p.y+x*p.x};
}

static int contains(const CollisionMask *m, MaskPoint p)
{
    unsigned outline,first=0,intersections=0;
    for(outline=0;outline<m->outlines;++outline) {
        unsigned i,end=m->ends[outline];
        MaskPoint prev=m->vertices[end-1];
        for(i=first;i<end;++i) {
            MaskPoint next=m->vertices[i];
            if(prev.x!=next.x && ((prev.x<=p.x)==(p.x<next.x))) {
                double y=prev.y+(next.y-prev.y)*(p.x-prev.x)/(next.x-prev.x);
                intersections+=(y>=p.y);
            }
            prev=next;
        }
        first=end;
    }
    return intersections&1;
}

int mask_contains(const CollisionMask *m, MaskPoint p, uint16_t facing)
{
    if(!m->outlines || length(p)>m->radius) return 0;
    return contains(m,rotate(p,facing));
}

double mask_collide(const CollisionMask *m, MaskPoint p, MaskPoint v, uint16_t facing)
{
    double distance=length(p),closest=1.;
    unsigned outline,first=0;
    MaskPoint near={-p.x,-p.y};
    /* Keep upstream's sequence, including p+v then subtraction, rather than
     * replacing it with algebraically equal arithmetic at edge boundaries. */
    MaskPoint end={p.x+v.x,p.y+v.y};
    MaskPoint segment=sub(end,p);
    double square=dot(segment,segment);
    if(!m->outlines || distance>m->radius+length(v)) return 1.;
    if(square) {
        double u=dot(segment,near)/square;
        if(u<0.) u=0.;
        if(u>1.) u=1.;
        near.x-=u*segment.x; near.y-=u*segment.y;
    }
    if(dot(near,near)>m->radius*m->radius) return 1.;
    p=rotate(p,facing); v=rotate(v,facing);
    if(distance<=m->radius && contains(m,p)) return 0.;
    for(outline=0;outline<m->outlines;++outline) {
        unsigned i,limit=m->ends[outline];
        MaskPoint prev=m->vertices[limit-1];
        for(i=first;i<limit;++i) {
            MaskPoint next=m->vertices[i],edge=sub(next,prev);
            double determinant=cross(edge,v);
            if(determinant>0.) {
                MaskPoint start=sub(prev,p);
                double ub=cross(v,start),ua=cross(edge,start);
                if(ub>=0. && ub<determinant && ua>=0.) {
                    double fraction=ua/determinant;
                    if(fraction<closest) closest=fraction;
                }
            }
            prev=next;
        }
        first=limit;
    }
    return closest;
}

int masks_load(const char *path, CollisionMask *masks, unsigned count)
{
    FILE *f=fopen(path,"rb");
    char magic[8];
    unsigned declared,index;
    int ok=0;
    if(!f) return 0;
    memset(masks,0,count*sizeof(*masks));
    if(fread(magic,1,8,f)!=8 || memcmp(magic,"ESMASK1",8) ||
       fread(&declared,4,1,f)!=1 || declared!=count) goto done;
    for(index=0;index<count;++index) {
        CollisionMask *m=&masks[index];
        unsigned id,outline,total=0;
        if(fread(&id,4,1,f)!=1 || fread(&m->outlines,4,1,f)!=1 ||
           fread(&m->points,4,1,f)!=1 || fread(&m->radius,8,1,f)!=1 ||
           id!=index || !m->outlines || m->outlines>MASK_MAX_OUTLINES ||
           m->points>MASK_MAX_POINTS || !isfinite(m->radius) || m->radius<=0.) goto done;
        for(outline=0;outline<m->outlines;++outline) {
            unsigned n,i;
            if(fread(&n,4,1,f)!=1 || n<3 || n>m->points-total) goto done;
            for(i=0;i<n;++i) {
                MaskPoint *p=&m->vertices[total++];
                if(fread(&p->x,8,1,f)!=1 || fread(&p->y,8,1,f)!=1 || !isfinite(p->x) || !isfinite(p->y) ||
                   length(*p)>m->radius+1e-10) goto done;
            }
            m->ends[outline]=total;
        }
        if(total!=m->points) goto done;
    }
    if(fgetc(f)!=EOF || ferror(f)) goto done;
    ok=1;
done:
    fclose(f);
    if(!ok) memset(masks,0,count*sizeof(*masks));
    return ok;
}
