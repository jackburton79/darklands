/*
 * Travel.h
 * Paths across the world map.
 */
#pragma once

#include "SupportDefs.h"

#include <vector>

class WorldMap;

struct map_position {
    uint16 x;
    uint16 y;

    bool operator==(const map_position& other) const
        { return x == other.x && y == other.y; }
    bool operator!=(const map_position& other) const
        { return !(*this == other); }
};

// Whether the party can walk on tile (x, y): anything but the sea and
// the major rivers, which DARKLAND.EXE lets the party enter only from
// water or with a special item (see docs/exe.md): rivers are crossed at
// fords and bridges.
bool IsPassable(const WorldMap& map, uint16 x, uint16 y);

// The fastest walkable path from `from` to `to` (A*), excluding `from`
// and including `to`; empty if `to` is unreachable, impassable (unless
// `anyGoal`, for a city on a river or the coast) or `from`. Moves go to
// the 8 neighbors of the staggered grid (4 diagonal, 2 horizontal, 2
// vertical, i.e. two rows up or down) and cost the game minutes of
// DARKLAND.EXE (see TravelHours()). The original game has no path
// finding: its party walks straight toward the destination.
std::vector<map_position> FindPath(const WorldMap& map,
    const map_position& from, const map_position& to, bool anyGoal = false);

// The game hours that walking from a tile to a neighbor takes, as
// DARKLAND.EXE counts them (see docs/exe.md): the party moves one pixel
// a frame horizontally and one every other frame vertically; every frame
// adds the minutes of the terrain under it to `accumulator`, and an hour
// passes, at most one a frame, whenever it reaches 60. Keep the same
// accumulator from step to step.
uint32 TravelHours(const WorldMap& map, const map_position& from,
    const map_position& to, int& accumulator);
