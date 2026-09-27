/* Native collision oracle linked against unchanged Endless Sky objects.
Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "Angle.h"
#include "image/ImageBuffer.h"
#include "image/ImageFileData.h"
#include "image/Mask.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
struct Fixture {
	const char *name;
	Mask mask;
	int width;
	int height;
};

uint64_t rng = 0x5a17d3eab921460full;
uint64_t Next()
{
	rng ^= rng >> 12;
	rng ^= rng << 25;
	rng ^= rng >> 27;
	return rng * 2685821657736338717ull;
}
double Unit() { return (Next() >> 11) * 0x1.0p-53; }
double Between(double lo, double hi) { return lo + (hi - lo) * Unit(); }

Angle Facing(int steps) { return Angle(steps * (360. / 65536.)); }

void Case(ostream &out, int id, const Mask &mask, int steps, Point start, Point vector)
{
	const Angle facing = Facing(steps);
	const double fraction = mask.Collide(start, vector, facing);
	if(!isfinite(fraction) || fraction < 0. || fraction > 1.)
		throw runtime_error("invalid collision fraction");
	out << id << '\t' << steps << '\t' << start.X() << '\t' << start.Y()
		<< '\t' << vector.X() << '\t' << vector.Y() << '\t'
		<< mask.Contains(start, facing) << '\t' << fraction << '\n';
}

Fixture Load(const char *name)
{
	const string path = string("/src/images/ship/") + name + ".png";
	ImageBuffer image;
	if(image.Read(ImageFileData(path)) != 1)
		throw runtime_error("image read failed: " + path);
	Fixture result{name, {}, image.Width(), image.Height()};
	result.mask.Create(image, 0, path);
	if(!result.mask.IsLoaded() || result.mask.Radius() <= 0.)
		throw runtime_error("mask creation failed: " + path);
	return result;
}
}

int main(int argc, char **argv)
{
	try
	{
		if(argc != 3)
			throw runtime_error("usage: oracle-native MASKS TESTS");
		array<Fixture, 3> fixtures = {Load("sparrow"), Load("star barge"), Load("falcon")};
		ofstream masks(argv[1]);
		ofstream tests(argv[2]);
		if(!masks || !tests)
			throw runtime_error("output open failed");
		masks << setprecision(17);
		tests << setprecision(17);
		masks << "ESMASK1 " << fixtures.size() << '\n';
		tests << "mask_id\tangle_steps\tsx\tsy\tvx\tvy\tcontains\tcollision_fraction\n";
		uint64_t cases = 0;
		for(size_t id = 0; id < fixtures.size(); ++id)
		{
			const Mask &mask = fixtures[id].mask;
			const auto &outlines = mask.Outlines();
			size_t points = 0;
			for(const auto &outline : outlines) points += outline.size();
			masks << id << ' ' << outlines.size() << ' ' << points << ' ' << mask.Radius() << '\n';
			for(const auto &outline : outlines)
			{
				masks << outline.size() << '\n';
				for(Point p : outline) masks << p.X() << ' ' << p.Y() << '\n';
			}
			auto emit = [&](int steps, Point start, Point vector)
			{
				Case(tests, static_cast<int>(id), mask, steps, start, vector);
				++cases;
			};
			const double radius = mask.Radius();
			const array<int, 12> headings = {0, 1, 2, 257, 4096, 8192, 16384, 24576,
				32768, 40960, 49152, 65535};
			for(int heading : headings)
			{
				const Angle facing = Facing(heading);
				for(Point p : {Point(), Point(radius * 2., 0.), Point(0., radius * 2.)})
				{
					const Point rotated = facing.Rotate(p);
					emit(heading, rotated, Point());
					emit(heading, rotated, facing.Rotate(Point(-radius * 4., 0.)));
				}
				for(int i = 0; i < 80; ++i)
				{
					const Point start(Between(-2. * radius, 2. * radius), Between(-2. * radius, 2. * radius));
					const Point vector(Between(-4. * radius, 4. * radius), Between(-4. * radius, 4. * radius));
					emit(heading, start, vector);
				}
			}
			// Use actual exported vertices and edge midpoints. These cases deliberately
			// retain exact-boundary results, even where rounding is sensitive.
			for(const auto &outline : outlines)
				for(size_t i = 0; i < outline.size(); ++i)
				{
					const Point vertex = outline[i];
					const Point midpoint = (vertex + outline[(i + 1) % outline.size()]) * .5;
					for(Point p : {vertex, midpoint})
					{
						emit(0, p, Point());
						emit(0, p, Point(radius * 2., 0.));
						emit(0, Point(p.X() - radius * 2., p.Y()), Point(radius * 4., 0.));
						for(int heading : {8192, 16384, 32768})
						{
							const Angle facing = Facing(heading);
							emit(heading, facing.Rotate(p), Point());
						}
					}
				}
			for(int i = 0; i < 2400; ++i)
			{
				const int heading = static_cast<int>(Next() & 65535);
				const Point start(Between(-2. * radius, 2. * radius), Between(-2. * radius, 2. * radius));
				const Point vector(Between(-4. * radius, 4. * radius), Between(-4. * radius, 4. * radius));
				emit(heading, start, vector);
			}
			cout << id << ' ' << fixtures[id].name << ' ' << fixtures[id].width << 'x'
				<< fixtures[id].height << " outlines=" << outlines.size()
				<< " points=" << points << " radius=" << setprecision(17) << radius << '\n';
		}
		if(!masks || !tests) throw runtime_error("output write failed");
		cout << "cases=" << cases << '\n';
		return 0;
	}
	catch(const exception &error)
	{
		cerr << error.what() << '\n';
		return 1;
	}
}
