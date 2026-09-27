/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ES_DOS_WORLD_ACTIVE_H
#define ES_DOS_WORLD_ACTIVE_H
#include <stddef.h>
#include <stdint.h>
typedef struct WorldStore WorldStore;
typedef struct { int32_t index, parent; uint64_t planet; double distance, period, offset; const char *sprite; } WorldObject;
typedef struct { uint64_t commodity; int32_t price; } WorldTrade;
typedef struct {
    uint64_t id;
    const char *name, *display;
    uint32_t flags;
    double x, y, jump_range, arrival_hyper, arrival_jump, departure_hyper,
           departure_jump, habitable;
    uint32_t object_count, link_count, trade_count;
    WorldObject *objects;
    uint64_t *links;
    WorldTrade *trade;
    size_t allocation_bytes;
    void *allocation;
} ActiveSystem;
/* Open verifies the directory and all records. NULL on failure. */
WorldStore *world_open(const char *path);
void world_close(WorldStore *store);
/* A load requires a zero initialized output. Failure leaves it unchanged. */
int world_load_system(WorldStore *store, uint64_t id, ActiveSystem *out);
void world_release_system(ActiveSystem *system);
int world_find_system(WorldStore *store, const char *name, uint64_t *out);
size_t world_store_bytes(const WorldStore *store);
const char *world_error(const WorldStore *store);
#endif
