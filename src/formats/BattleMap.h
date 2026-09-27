/*
 * BattleMap.h
 * Reader for the battlefield maps, the entries of IMAPS.CAT ("ICITY.000",
 * "IWILDGEN.101"...): compressed like the sprites (Lzexe.h), 13308
 * bytes of two 40 x 40 grids and records. Only the grids are read.
 * See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <vector>

class Stream;

// The sides of a cell, in the order of its bytes. DARKLAND.EXE removes a
// wall from both cells it separates: byte 0 with byte 1 of the cell at
// y + 1, byte 2 with byte 3 of the cell at x - 1 (file 0x5222A).
enum battle_side {
    SIDE_NEXT_ROW = 0,		// toward y + 1
    SIDE_PREVIOUS_ROW,		// toward y - 1
    SIDE_PREVIOUS_COLUMN,	// toward x - 1
    SIDE_NEXT_COLUMN,		// toward x + 1
    SIDE_COUNT
};

struct battle_cell {
    uint8 bytes[SIDE_COUNT];
    uint8 ground;			// the second grid

    // The wall on a side, as seen from this cell (0: none; 1 rock, 2 a
    // house, 6 masonry... see docs/formats.md)
    int		Wall(battle_side side) const	{ return bytes[side] & 0x0F; }
    // An object standing in the cell (0: none), e.g. trees
    int		Object() const			{ return bytes[SIDE_NEXT_ROW] >> 4; }
    // Closed cells (inside houses, rock) cannot be entered; their ground
    // is 0
    bool	Closed() const			{ return (bytes[SIDE_NEXT_COLUMN] & 0x80) != 0; }
};

class BattleMap {
public:
    static const int	kSize = 40;		// cells per side, borders included

    // Reads the whole stream (not owned; it must support Size()).
    // Throws std::runtime_error on invalid data.
    explicit		BattleMap(Stream* stream);

    const battle_cell& CellAt(int x, int y) const;	// throws on bad x, y

private:
    std::vector<battle_cell> fCells;	// row by row
};
