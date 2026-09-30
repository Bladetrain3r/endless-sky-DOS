/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ESDOS_PERSIST_STATE_H
#define ESDOS_PERSIST_STATE_H
#include <stddef.h>
#include <stdint.h>
#define PS_TEXT 256
#define PS_COMMODITIES 10
#define PS_SALES 16
#define PS_MAX_FILE (64u+65536u)
typedef enum {
 PS_OK=0, PS_MISSING, PS_CORRUPT, PS_UNSUPPORTED, PS_INVALID,
 PS_IO, PS_CONFLICT, PS_STALE, PS_EXHAUSTED, PS_MEMORY
} PSResult;
typedef struct { char key[PS_TEXT]; int32_t tons; int64_t basis; } PSCargo;
typedef struct { char system[PS_TEXT],commodity[PS_TEXT]; int32_t delta; } PSSale;
typedef struct {
 uint8_t pilot_id[16],ship_id[16],flagship_id[16];
 char name[PS_TEXT],system[PS_TEXT],planet[PS_TEXT];
 int32_t day,month,year;
 char model[PS_TEXT],variant[PS_TEXT],ship_name[PS_TEXT];
 int32_t crew;
 double fuel,shields,hull;
 int64_t credits;
 uint32_t cargo_count,sale_count,scope_flags;
 PSCargo cargo[PS_COMMODITIES]; PSSale sales[PS_SALES];
} PSState;
typedef struct {
 uint8_t content[32];
 int32_t cargo_capacity,max_crew;
 double max_fuel,max_shields,max_hull;
} PSContext;
/* One stock Sparrow in Sol at Earth/Luna. No arbitrary loadout/fleet/mission
 * state can enter this type; nonempty scope/variant is rejected explicitly. */
PSResult ps_validate(const PSState *state,const PSContext *context);
const char *ps_error(PSResult result);
#endif
