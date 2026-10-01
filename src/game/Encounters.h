/*
 * Encounters.h
 * The meetings on the map: which of the game's states a random meeting is
 * (DARKLAND.EXE, the chooser at file 0x5E1DB..0x5EC40, see docs/exe.md,
 * "Meetings on the map"). The hazard itself is MapViewer's.
 */
#pragma once

#include <functional>

// The states the chooser gives (DS:E48A); the cards of each are in the
// deck named by encounter_deck()
enum encounter_state {
    ENCOUNTER_NONE			= -1,	// nothing happens
    ENCOUNTER_THIEVES		= 0x24,	// $CITYT00
    ENCOUNTER_GARGOYLES		= 0x3D,	// $MeetG00
    ENCOUNTER_FRIAR			= 0x7F,	// $MeetG01
    ENCOUNTER_WILD_A0		= 0xA0,	// $meetw00 or $meetb00 (not known which)
    ENCOUNTER_WILD_A5		= 0xA5,
    ENCOUNTER_ALCHEMIST		= 0xA6,	// $MeetA00
    ENCOUNTER_ALCHEMIST_ARMED = 0xA7,	// $MeetA01
    ENCOUNTER_CAVE_DRAGON	= 0xC3,	// $MeetJ00 (the knight of a tournament)
    ENCOUNTER_CAVE			= 0xF0,	// $MeetC00
    ENCOUNTER_BANDITS		= 0x102,	// $MeetB01
    ENCOUNTER_SOLDIERS		= 0x103,	// $MeetB02
    ENCOUNTER_CARAVAN		= 0x104,	// $MeetM00
    ENCOUNTER_UNKNOWN_105	= 0x105,
    ENCOUNTER_PILGRIMS		= 0x106,	// $MeetP00
    ENCOUNTER_REFUGEES		= 0x107,	// $MeetP01
    ENCOUNTER_BISHOP		= 0x108,	// $MeetV00
    ENCOUNTER_TATZELWURM	= 0x111,	// $MeetT00
    ENCOUNTER_SPIDERS		= 0x112,	// $MeetG02
    ENCOUNTER_SCHRATS		= 0x113,	// $MeetS00
    ENCOUNTER_PEAT_BOG		= 0x116,	// $MeetP02
    ENCOUNTER_BLIZZARD		= 0x117,	// $MeetB03
    ENCOUNTER_HERMIT		= 0x118,	// $MeetH00
    ENCOUNTER_FLOOD			= 0x11B,	// $MeetS01
    ENCOUNTER_HUT			= 0x122,	// $MeetW01
    ENCOUNTER_TOLL			= 0x163		// $MeetH02
};

// The state of a meeting on a tile of the given type (the map's tile
// types, WorldMap::TileTypeAt()) in the given month (0 = January):
// ENCOUNTER_NONE if the game's chooser gives none. `random(n)` is 0..n-1.
int ChooseEncounter(int terrain, int month,
    const std::function<int(int)>& random);

// Whether the program plays the state
bool IsEncounterPlayed(int state);
