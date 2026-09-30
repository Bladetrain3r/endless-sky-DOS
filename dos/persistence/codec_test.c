/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void wr32(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint32_t crcstep(uint32_t c,const uint8_t*p,size_t n){size_t i;unsigned j;for(i=0;i<n;i++){c^=p[i];for(j=0;j<8;j++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return c;}
static uint32_t crc(const uint8_t*p,size_t n){return crcstep(crcstep(0xffffffffu,p,60),p+64,n-64)^0xffffffffu;}
static void seal(uint8_t*p,size_t n){wr32(p+60,crc(p,n));}
static size_t find_bytes(const uint8_t*p,size_t n,const char*s){size_t z=strlen(s),i;for(i=0;i+z<=n;i++)if(!memcmp(p+i,s,z))return i;return n;}

static void fixture(PSState*s,PSContext*c)
{
 unsigned i; memset(s,0,sizeof(*s));memset(c,0,sizeof(*c));
 for(i=0;i<32;i++)c->content[i]=(uint8_t)(0xa0+i);
 c->cargo_capacity=20;c->max_crew=1;c->max_fuel=400;c->max_shields=400;c->max_hull=200;
 for(i=0;i<16;i++){s->pilot_id[i]=(uint8_t)(i+1);s->ship_id[i]=(uint8_t)(0xf0-i);s->flagship_id[i]=s->ship_id[i];}
 strcpy(s->name,"Zo\303\253");strcpy(s->system,"system:Sol");strcpy(s->planet,"planet:Earth");
 s->day=29;s->month=2;s->year=2400;strcpy(s->model,"ship:Sparrow");strcpy(s->ship_name,"Bit Finch");
 s->crew=1;s->fuel=0.5;s->shields=400.;s->hull=134.25;s->credits=INT64_MIN;
 s->cargo_count=2;strcpy(s->cargo[0].key,"commodity:Clothing");s->cargo[0].tons=1;s->cargo[0].basis=INT64_MIN;
 strcpy(s->cargo[1].key,"commodity:Food");s->cargo[1].tons=19;s->cargo[1].basis=INT64_MAX;
 s->sale_count=2;strcpy(s->sales[0].system,"system:Sol");strcpy(s->sales[0].commodity,"commodity:Clothing");s->sales[0].delta=INT32_MIN;
 strcpy(s->sales[1].system,"system:Sol");strcpy(s->sales[1].commodity,"commodity:Food");s->sales[1].delta=-1;
}

/* Independent append-only reference encoder for a stable byte-layout vector. */
typedef struct { uint8_t b[2048];size_t n; } Vec;
static void vb(Vec*v,const void*p,size_t n){memcpy(v->b+v->n,p,n);v->n+=n;}
static void v16(Vec*v,uint16_t x){uint8_t b[2]={(uint8_t)x,(uint8_t)(x>>8)};vb(v,b,2);}
static void v32(Vec*v,uint32_t x){uint8_t b[4];wr32(b,x);vb(v,b,4);}
static void v64(Vec*v,uint64_t x){v32(v,(uint32_t)x);v32(v,(uint32_t)(x>>32));}
static void vs(Vec*v,const char*s){size_t n=strlen(s);v16(v,(uint16_t)n);vb(v,s,n);}
static void vh(Vec*v,uint32_t t,uint32_t n){v32(v,t);v32(v,1);v32(v,n);}
static void expected(Vec*v,const PSState*s,const PSContext*c,uint64_t gen)
{
 Vec a={{0},0},b={{0},0},d={{0},0},e={{0},0};uint64_t bits;unsigned i;memset(v,0,sizeof(*v));v->n=64;
 vb(&a,s->pilot_id,16);vs(&a,s->name);v32(&a,(uint32_t)s->day);v32(&a,(uint32_t)s->month);v32(&a,(uint32_t)s->year);vs(&a,s->system);vs(&a,s->planet);vb(&a,s->flagship_id,16);
 vb(&b,s->ship_id,16);vs(&b,s->model);vs(&b,s->variant);vs(&b,s->ship_name);v32(&b,(uint32_t)s->crew);memcpy(&bits,&s->fuel,8);v64(&b,bits);memcpy(&bits,&s->shields,8);v64(&b,bits);memcpy(&bits,&s->hull,8);v64(&b,bits);
 v64(&d,(uint64_t)s->credits);v32(&d,s->cargo_count);for(i=0;i<s->cargo_count;i++){vs(&d,s->cargo[i].key);v32(&d,(uint32_t)s->cargo[i].tons);v64(&d,(uint64_t)s->cargo[i].basis);}
 v32(&e,s->scope_flags);v32(&e,s->sale_count);for(i=0;i<s->sale_count;i++){vs(&e,s->sales[i].system);vs(&e,s->sales[i].commodity);v32(&e,(uint32_t)s->sales[i].delta);}
 vh(v,1,(uint32_t)a.n);vb(v,a.b,a.n);vh(v,2,(uint32_t)b.n);vb(v,b.b,b.n);vh(v,3,(uint32_t)d.n);vb(v,d.b,d.n);vh(v,4,(uint32_t)e.n);vb(v,e.b,e.n);
 memcpy(v->b,"ESDSAVE1",8);wr32(v->b+8,1);wr32(v->b+12,64);v->b[16]=(uint8_t)gen;v->b[17]=(uint8_t)(gen>>8);v->b[18]=(uint8_t)(gen>>16);v->b[19]=(uint8_t)(gen>>24);v->b[20]=(uint8_t)(gen>>32);v->b[21]=(uint8_t)(gen>>40);v->b[22]=(uint8_t)(gen>>48);v->b[23]=(uint8_t)(gen>>56);wr32(v->b+24,(uint32_t)(v->n-64));memcpy(v->b+28,c->content,32);seal(v->b,v->n);
}

static int same_state(const PSState*a,const PSState*b)
{
 uint64_t x,y;
 if(memcmp(a,b,sizeof(*a))) return 0;
 memcpy(&x,&a->fuel,8);memcpy(&y,&b->fuel,8);if(x!=y)return 0;
 memcpy(&x,&a->shields,8);memcpy(&y,&b->shields,8);if(x!=y)return 0;
 memcpy(&x,&a->hull,8);memcpy(&y,&b->hull,8);return x==y;
}

static int failure_keeps(const uint8_t*p,size_t n,const PSContext*c,PSResult want)
{
 PSState before,out;uint64_t gb=UINT64_C(0xfeedfacecafebeef),g=gb;memset(&before,0x5a,sizeof(before));out=before;
 if(ps_decode(p,n,c,&out,&g)!=want)return 0;
 return !memcmp(&out,&before,sizeof(out))&&g==gb;
}

int main(void)
{
 PSState s,out,bad;PSContext c,badc;uint8_t bytes[PS_MAX_FILE],copy[PS_MAX_FILE];size_t n=0,i,at;uint64_t gen=0;Vec exp;
 fixture(&s,&c);memset(bytes,0xcc,sizeof(bytes));CHECK(ps_encode(&s,&c,UINT64_C(0x8877665544332211),bytes,sizeof(bytes),&n)==PS_OK);
 expected(&exp,&s,&c,UINT64_C(0x8877665544332211));CHECK(n==exp.n);CHECK(!memcmp(bytes,exp.b,n));
 CHECK(rd32(bytes+60)==UINT32_C(0xa5dc554b)); /* fixed known-vector CRC */
 memset(&out,0,sizeof(out));CHECK(ps_decode(bytes,n,&c,&out,&gen)==PS_OK);CHECK(gen==UINT64_C(0x8877665544332211));CHECK(same_state(&s,&out));

 for(i=0;i<n;i++)CHECK(failure_keeps(bytes,i,&c,PS_CORRUPT));
 for(i=0;i<n;i++){PSResult want=(i>=8&&i<16)?PS_UNSUPPORTED:PS_CORRUPT;memcpy(copy,bytes,n);copy[i]^=1;CHECK(failure_keeps(copy,n,&c,want));}
 memcpy(copy,bytes,n);copy[n]=7;CHECK(failure_keeps(copy,n+1,&c,PS_CORRUPT));

 memcpy(copy,bytes,n);wr32(copy+8,2);seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_UNSUPPORTED));
 memcpy(copy,bytes,n);wr32(copy+12,80);CHECK(failure_keeps(copy,n,&c,PS_UNSUPPORTED));
 badc=c;badc.content[0]^=1;CHECK(failure_keeps(bytes,n,&badc,PS_UNSUPPORTED));
 memcpy(copy,bytes,n);wr32(copy+64,99);seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_UNSUPPORTED));
 memcpy(copy,bytes,n);wr32(copy+68,2);seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_UNSUPPORTED));
 memcpy(copy,bytes,n);wr32(copy+16,0);wr32(copy+20,0);seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_INVALID));

 bad=s;bad.day=30;CHECK(ps_encode(&bad,&c,1,copy,sizeof(copy),&i)==PS_INVALID);
 bad=s;bad.year=2100;CHECK(ps_validate(&bad,&c)==PS_INVALID);bad.day=28;CHECK(ps_validate(&bad,&c)==PS_OK);
 bad=s;bad.name[0]=(char)0xc0;bad.name[1]=(char)0x80;bad.name[2]=0;CHECK(ps_validate(&bad,&c)==PS_INVALID);
 bad=s;memset(bad.pilot_id,0,16);CHECK(ps_validate(&bad,&c)==PS_INVALID);
 bad=s;memcpy(bad.pilot_id,bad.ship_id,16);CHECK(ps_validate(&bad,&c)==PS_INVALID);
 bad=s;bad.cargo_count=PS_COMMODITIES+1;CHECK(ps_validate(&bad,&c)==PS_INVALID);
 bad=s;strcpy(bad.cargo[1].key,"commodity:Clothing");CHECK(ps_validate(&bad,&c)==PS_INVALID);
 bad=s;bad.sales[1]=bad.sales[0];CHECK(ps_validate(&bad,&c)==PS_INVALID);
 bad=s;bad.scope_flags=1;CHECK(ps_validate(&bad,&c)==PS_UNSUPPORTED);
 bad=s;strcpy(bad.variant,"custom");CHECK(ps_validate(&bad,&c)==PS_UNSUPPORTED);
 bad=s;strcpy(bad.system,"system:Alpha Centauri");CHECK(ps_validate(&bad,&c)==PS_UNSUPPORTED);
 badc=c;badc.cargo_capacity=19;CHECK(ps_validate(&s,&badc)==PS_INVALID);
 badc=c;badc.max_fuel=0.25;CHECK(ps_validate(&s,&badc)==PS_INVALID);
 bad=s;{uint64_t nan=UINT64_C(0x7ff8000000000001);memcpy(&bad.fuel,&nan,8);}CHECK(ps_validate(&bad,&c)==PS_INVALID);

 /* Recomputed-CRC attacks target decoded semantics and classification. */
 memcpy(copy,bytes,n);at=find_bytes(copy,n,"Zo\303\253");CHECK(at<n);copy[at]=(uint8_t)0xc0;copy[at+1]=(uint8_t)0x80;seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_INVALID));
 memcpy(copy,bytes,n);at=find_bytes(copy,n,"Zo\303\253");CHECK(at<n);copy[at+1]=0;seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_CORRUPT));
 memcpy(copy,bytes,n);at=find_bytes(copy,n,"ship:Sparrow");CHECK(at<n);copy[at+5]='X';seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_UNSUPPORTED));
 memcpy(copy,bytes,n);at=find_bytes(copy,n,"planet:Earth");CHECK(at<n);copy[at+7]='M';seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_UNSUPPORTED));
 /* First chunk: ID then name string; date follows its bytes. */
 memcpy(copy,bytes,n);at=64+12+16;at+=2+((size_t)copy[at]|(size_t)copy[at+1]<<8);wr32(copy+at,31);wr32(copy+at+4,2);seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_INVALID));
 /* Ship chunk starts immediately after first chunk. Poison fuel as NaN. */
 memcpy(copy,bytes,n);at=64+12+rd32(copy+72);at+=12+16;at+=2+((size_t)copy[at]|(size_t)copy[at+1]<<8);at+=2+((size_t)copy[at]|(size_t)copy[at+1]<<8);at+=2+((size_t)copy[at]|(size_t)copy[at+1]<<8);at+=4;memset(copy+at,0,8);copy[at+6]=0xf8;copy[at+7]=0x7f;seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_INVALID));
 /* Scope flags are at payload of fourth chunk. */
 memcpy(copy,bytes,n);at=64;for(i=0;i<3;i++)at+=12+rd32(copy+at+8);wr32(copy+at+12,1);seal(copy,n);CHECK(failure_keeps(copy,n,&c,PS_UNSUPPORTED));

 memcpy(copy,bytes,n);i=777;CHECK(ps_encode(&s,&c,1,copy,n-1,&i)==PS_INVALID);CHECK(i==777); /* publication is atomic */
 puts("status=pass");return 0;
}
