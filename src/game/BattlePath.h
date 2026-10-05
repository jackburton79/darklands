/*
 * BattlePath.h
 * Paths across a battlefield map (BattleMap): which steps a figure can
 * take between cells, and the shortest path between two cells. The
 * game's own rules of movement are not decoded yet; these follow the
 * map: closed cells and walls block.
 */
#pragma once

#include "SupportDefs.h"

#include <vector>

class BattleMap;

struct battle_position {
    int x;
    int y;

    bool operator==(const battle_position& other) const
        { return x == other.x && y == other.y; }
    bool operator!=(const battle_position& other) const
        { return !(*this == other); }
};

// Whether a figure can step from (x, y) to its neighbor (x + dx, y + dy),
// dx and dy in -1..1: the target must be open, with no wall on either
// side of the edge between them; diagonally, both cells beside the step
// must be reachable that way too.
bool CanStep(const BattleMap& map, int x, int y, int dx, int dy);

// The shortest path from `from` to `to` (a straight step costs 2, a
// diagonal one 3), excluding `from` and including `to`; empty if `to`
// cannot be reached or is `from`. The `occupied` cells cannot be
// entered.
std::vector<battle_position> FindBattlePath(const BattleMap& map,
    const battle_position& from, const battle_position& to,
    const std::vector<battle_position>& occupied);

// Whether a shot can go from one cell to another: along the straight line
// between the centers of the cells, no closed cell, no wall of type 1..11
// on the edges it crosses (the game's own test, file 0x4874C, takes the
// walls below type 12 as blocking and also the objects' footprints and the
// figures in the way; the footprints are not reproduced) and none of the
// `obstacles` (the other figures) on the way.
bool HasLineOfFire(const BattleMap& map, const battle_position& from,
    const battle_position& to, const std::vector<battle_position>& obstacles);
