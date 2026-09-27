/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOS_COLLISION_MASK_H
#define DOS_COLLISION_MASK_H
#include <stdint.h>
#define MASK_MAX_OUTLINES 16
#define MASK_MAX_POINTS 1024
typedef struct { double x,y; } MaskPoint;
typedef struct {
    double radius;
    unsigned outlines, points;
    unsigned ends[MASK_MAX_OUTLINES];
    MaskPoint vertices[MASK_MAX_POINTS];
} CollisionMask;
/* Query positions are relative to the target centre; v is segment displacement.
 * Return 0 when starting inside, 1 for no hit, otherwise first entry fraction.
 * Preserve upstream half-open polygon-edge conventions. No ship/body overlap
 * resolution, faction filters, moving-target policy or damage is implied. */
int mask_contains(const CollisionMask *mask, MaskPoint p, uint16_t facing);
double mask_collide(const CollisionMask *mask, MaskPoint p, MaskPoint v, uint16_t facing);
int masks_load(const char *path, CollisionMask *masks, unsigned count);
#endif
