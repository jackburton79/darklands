/*
 * BattleMap.h
 * Reader for the battlefield maps, the entries of IMAPS.CAT ("ICITY.000",
 * "IWILDGEN.101"...): compressed like the sprites (Lzexe.h), 13308
 * bytes of two 40 x 40 grids and records. Only the grids are read; what
 * their bytes mean is partly inferred. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <vector>

class Stream;

struct battle_cell {
    uint8 bytes[4];		// the first grid (byte 3 bit 7: a building's
                        // inside, 1..8: a wall; byte 0: an object?)
    uint8 ground;		// the second grid (0x10: open ground?)
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
