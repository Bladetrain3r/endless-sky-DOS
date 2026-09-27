/* Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "damage.h"

void damage_hit(DamageState *state, double shield_damage, double hull_damage)
{
	double shield_fraction = 0.;
	double actual_shield_damage = 0.;
	if(state->shields > 0.)
	{
		shield_fraction = 1.;
		actual_shield_damage = shield_damage;
		if(actual_shield_damage > state->shields)
			shield_fraction = state->shields / actual_shield_damage;
	}
	actual_shield_damage *= shield_fraction;
	state->hull -= hull_damage * (1. - shield_fraction);
	state->shields -= actual_shield_damage;
	if(state->shields < 0.)
		state->shields = 0.;
}
