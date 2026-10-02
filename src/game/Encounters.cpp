#include "Encounters.h"

#include <cstddef>

// 1462 of overlay 0x1E, file 0x5E1DB..: the tile type picks a table and
// the range of random(); a number past a table gives the thieves (0x24),
// except where noted. kWinter stands for the check of file 0x5E48A: in
// the months April to October nothing, else the blizzard (0x117).
static const int kWinter = -2;

struct encounter_table {
    int range;			// random(range)
    int count;			// entries
    int rest;			// the state for a number past the entries
    const int* states;
};

static const int kTable4[14] = { 0x112, 0x112, kWinter, kWinter, kWinter,
    0x116, 0x116, 0xA0, 0xA0, 0x102, 0x102, 0x103, 0x103, 0x111 };
static const int kTable5[14] = { 0x112, 0x112, 0xA5, 0xA5, 0x107, 0x107,
    0xA0, 0xA0, 0x102, 0x102, 0x103, 0x103, 0x111, 0x118 };
static const int kTable7[25] = { 0xA0, 0xA0, 0x102, 0x102, 0x103, 0x103,
    0x106, 0x106, 0x106, 0x107, 0x107, 0x7F, 0x7F, 0x7F, 0x104, 0x104,
    0x104, 0x104, 0x105, 0x105, 0x108, 0x108, 0x108, kWinter, kWinter };
static const int kTable9[16] = { 0xA0, 0xA0, 0xA5, 0xA5, 0x102, 0x102,
    0x103, 0x103, 0x106, 0x106, 0x106, 0x107, 0x107, 0x112, kWinter,
    kWinter };
static const int kTable11[14] = { 0xA0, 0xA0, 0xA5, 0xA5, 0x102, 0x102,
    0x103, 0x103, 0x113, 0x112, 0x122, kWinter, kWinter, 0x118 };
static const int kTable15[14] = { 0xA0, 0xA0, 0xA5, 0xA5, 0x111, 0x113,
    0x112, 0x112, 0x122, 0x122, kWinter, kWinter, kWinter, 0x118 };
static const int kTable17[13] = { 0xA0, 0xA0, 0x102, 0x102, 0x103, 0x103,
    0x111, 0x112, 0x112, kWinter, kWinter, kWinter, 0x118 };
static const int kTable23[27] = { 0xA0, 0xA0, 0xA6, 0xA6, 0xA7, 0xA7,
    0x3D, 0xC3, 0x102, 0x102, 0x103, 0x103, 0x106, 0x106, 0x106, 0x107,
    0x107, 0x7F, 0x7F, 0x104, 0x104, 0x104, 0x104, 0x105, 0x105, 0x163,
    0x108 };

// The tile types 4 and 5.. : the first entries of a row are the tile
// type 4; 5 and 6 share the second table, and so on
static const encounter_table kTables[] = {
    { 17, 14, 0x122, kTable4 },		// tile type 4
    { 17, 14, 0x122, kTable5 },		// 5, 6
    { 26, 25, 0x24, kTable7 },		// 7, 8
    { 17, 16, 0x24, kTable9 },		// 9, 10
    { 16, 14, 0x24, kTable11 },		// 11..14
    { 16, 14, 0x24, kTable15 },		// 15, 16
    { 15, 13, 0x24, kTable17 },		// 17..20
    { 29, 27, 0x24, kTable23 }		// 23
};


static int
Winter(int month)
{
    return month >= 3 && month <= 9 ? ENCOUNTER_NONE : 0x117;
}


int
ChooseEncounter(int terrain, int month, const std::function<int(int)>& random)
{
    const encounter_table* table = NULL;
    switch (terrain) {
        case 3: {
            // file 0x58D4: random(9): 0..1 the spiders, 2..4 the winter,
            // 5..6 the peat bog, 7..8 the flood
            const int n = random(9);
            if (n < 2)
                return 0x112;
            if (n < 5)
                return Winter(month);
            return n < 7 ? 0x116 : 0x11B;
        }
        case 4:
            table = &kTables[0];
            break;
        case 5: case 6:
            table = &kTables[1];
            break;
        case 7: case 8:
            table = &kTables[2];
            break;
        case 9: case 10:
            table = &kTables[3];
            break;
        case 11: case 12: case 13: case 14:
            table = &kTables[4];
            break;
        case 15: case 16:
            table = &kTables[5];
            break;
        case 17: case 18: case 19: case 20:
            table = &kTables[6];
            break;
        case 21: case 22: {
            // file 0xAF8: random(9): 0..2 the cave, 3 the tatzelwurm, else
            // the winter in the months before April and after August
            const int n = random(9);
            if (n < 3)
                return 0xF0;
            if (n == 3)
                return 0x111;
            return month < 4 || month > 8 ? 0x117 : ENCOUNTER_NONE;
        }
        case 23:
            table = &kTables[7];
            break;
        default:
            return ENCOUNTER_NONE;
    }
    const int n = random(table->range);
    if (n >= table->count)
        return table->rest;
    const int state = table->states[n];
    return state == kWinter ? Winter(month) : state;
}


bool
IsEncounterPlayed(int state)
{
    switch (state) {
        case ENCOUNTER_THIEVES:
        case ENCOUNTER_BANDITS:
        case ENCOUNTER_SOLDIERS:
        case ENCOUNTER_FRIAR:
        case ENCOUNTER_PILGRIMS:
        case ENCOUNTER_BISHOP:
        case ENCOUNTER_HERMIT:
        case ENCOUNTER_TOLL:
        case ENCOUNTER_CARAVAN:
        case ENCOUNTER_REFUGEES:
        case ENCOUNTER_WOLVES:
        case ENCOUNTER_BOARS:
        case ENCOUNTER_PEAT_BOG:
        case ENCOUNTER_BLIZZARD:
        case ENCOUNTER_FLOOD:
            return true;
        default:
            return false;
    }
}
