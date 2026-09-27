/* Ordinary Energy Blaster damage for an unprotected training target.
Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef ES_DOS_DAMAGE_H
#define ES_DOS_DAMAGE_H

typedef struct {
	double shields;
	double hull;
} DamageState;

/* No piercing, permeability, disruption, protection, relative damage, or repair.
   Hull may become negative; the native destroyed condition is hull < 0. */
void damage_hit(DamageState *state, double shield_damage, double hull_damage);

#endif
