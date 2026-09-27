/* Native CSV versus portable C damage kernel, runnable by host and DOS.
Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "damage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	FILE *input, *profile;
	char line[512], name[80], previous[80] = "", magic[32];
	double shield_damage, hull_damage, before_s, before_h, dealt_s, dealt_h, after_s, after_h;
	double max_error = 0.;
	int hit, destroyed, rows = 0, cases = 0, deaths = 0;
	DamageState state = {0., 0.};
	if(argc != 3 || !(input = fopen(argv[1], "r")) || !(profile = fopen(argv[2], "r")))
		return 2;
	if(!fgets(line, sizeof line, profile) || sscanf(line, "%31s", magic) != 1
		|| strcmp(magic, "ESDAMAGE1") || !fgets(line, sizeof line, profile)
		|| sscanf(line, "%lf %lf", &shield_damage, &hull_damage) != 2
		|| !fgets(line, sizeof line, input))
		return 3;
	if(fabs(shield_damage - 10.6) > 1e-12 || fabs(hull_damage - 6.6) > 1e-12)
		return 4;
	while(fgets(line, sizeof line, input))
	{
		double error;
		if(sscanf(line, "%79[^,],%d,%lf,%lf,%lf,%lf,%lf,%lf,%d", name, &hit,
			&before_s, &before_h, &dealt_s, &dealt_h, &after_s, &after_h, &destroyed) != 9)
			return 5;
		if(strcmp(name, previous))
		{
			if(hit != 1) return 6;
			state.shields = before_s;
			state.hull = before_h;
			strcpy(previous, name);
			++cases;
		}
		else if(fabs(state.shields - before_s) > 1e-10 || fabs(state.hull - before_h) > 1e-10)
			return 7;
		damage_hit(&state, shield_damage, hull_damage);
		error = fabs(state.shields - after_s);
		if(fabs(state.hull - after_h) > error) error = fabs(state.hull - after_h);
		if(fabs((before_s - state.shields) - dealt_s) > error)
			error = fabs((before_s - state.shields) - dealt_s);
		if(fabs((before_h - state.hull) - dealt_h) > error)
			error = fabs((before_h - state.hull) - dealt_h);
		if(error > max_error) max_error = error;
		if(error > 1e-10 || (state.hull < 0.) != destroyed)
		{
			fprintf(stderr, "FAIL case=%s hit=%d error=%.17g native_destroyed=%d local_hull=%.17g\n",
				name, hit, error, destroyed, state.hull);
			return 8;
		}
		deaths += destroyed;
		++rows;
	}
	if(ferror(input) || fclose(input) || fclose(profile) || rows != 18 || cases != 8 || deaths != 5)
		return 9;
	printf("status=pass\nrows=%d\ncases=%d\nmax_error=%.17g\n", rows, cases, max_error);
	return 0;
}
