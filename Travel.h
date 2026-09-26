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

// Whether the party can walk on tile (x, y): anything but open sea.
bool IsPassable(const WorldMap& map, uint16 x, uint16 y);

// Shortest walkable path from `from` to `to` (A*), excluding `from` and
// including `to`; empty if `to` is unreachable, impassable or `from`.
// Moves go to the 8 neighbors of the staggered grid (4 diagonal, 2
// horizontal, 2 vertical, i.e. two rows up or down) and cost the pixel
// distance between the tile centers.
std::vector<map_position> FindPath(const WorldMap& map,
    const map_position& from, const map_position& to);
