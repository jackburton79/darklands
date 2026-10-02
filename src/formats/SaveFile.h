/*
 * SaveFile.h
 * Reader and writer for the saved games (SAVES/DKSAVEn.SAV,
 * SAVES/DEFAULT): the party with its money, the date and where the party
 * is, the events and the locations' state. Only the parts the game uses
 * so far; a file written keeps the other bytes of the one read. See
 * docs/formats.md.
 */
#pragma once

#include "Character.h"
#include "EventFile.h"
#include "GameTime.h"

#include <map>
#include <string>
#include <vector>

class LocationFile;

// The items left at the inns, by location (index into DARKLAND.LOC)
typedef std::map<int, std::vector<cache_item> > cache_map;

// What Write() puts into a saved game
struct saved_game {
    std::string label;				// the comment, at most 22 characters
    GameTime date;
    uint16 seed;
    const party* members;
    int location;					// index into DARKLAND.LOC, -1: none
    uint16 x;						// map tile
    uint16 y;
    uint16 state;					// DS:A772: where the game goes on
    const std::vector<world_event>* events;
    const std::vector<int16>* reputations;		// by location
    const std::vector<uint8>* locationFlags;
    const std::vector<uint16>* enterStates;
    int difficulty;					// DS:906A: 0 basic, 1 standard, 2 expert
    const std::vector<character>* spare;	// the characters not in the party
    const cache_map* caches;		// NULL: the file's own are kept
};

class SaveFile {
public:
    explicit		SaveFile(const std::string& fileName);	// throws on error

    const std::string& Label() const			{ return fLabel; }
    const std::string& LocationName() const		{ return fLocationName; }
    const GameTime&	Date() const				{ return fDate; }
    // The game's seed global (DS:9C4A): with a city's number it names its
    // people and makes their skills. A new game takes it from the clock.
    uint16			Seed() const				{ return fSeed; }
    // Index into DARKLAND.LOC, or -1 in the wilderness.
    int				Location() const			{ return fLocation; }
    uint16			X() const					{ return fX; }	// map tile
    uint16			Y() const					{ return fY; }

    const std::vector<character>& Characters() const	{ return fCharacters; }
    // The state of each location of DARKLAND.LOC: the party's reputation
    // there (-99..99) and its flags (war, fair...: not decoded). Empty if
    // the file has no location array.
    const std::vector<int16>& Reputations() const	{ return fReputations; }
    const std::vector<uint8>& LocationFlags() const	{ return fLocationFlags; }
    // The state the game enters there (+0x0C, see LocationFile.h)
    const std::vector<uint16>& EnterStates() const	{ return fEnterStates; }
    // The items left at the inns: the 198 bytes that end a saved game are
    // CACHE.TMP (docs/formats.md), and a location's word +0x18 is its
    // cache's number, -1 for none
    const cache_map& Caches() const				{ return fCaches; }
    // The game's events (see EventFile.h)
    const std::vector<world_event>& Events() const	{ return fEvents; }
    // Empty in DEFAULT, the new game template.
    const party&	Party() const				{ return fParty; }
    // The characters of the world that are not in the party (the records
    // after the party's; DKSAVE0 has none)
    const std::vector<character>& Spare() const		{ return fSpare; }
    // DS:A772, the state the game goes on from (0x0C the map, 0x1D the
    // inn...), and the difficulty (DS:906A: 0 basic, 1 standard, 2
    // expert)
    uint16			State() const				{ return fState; }
    int				Difficulty() const			{ return fDifficulty; }

    // Writes a saved game: this file's bytes with the fields of `game`;
    // the locations this file lacks come from `locations`. Throws on
    // error.
    void			Write(const std::string& fileName, const saved_game& game,
                        const LocationFile& locations) const;

private:
    std::string		fLabel;
    std::string		fLocationName;
    GameTime		fDate;
    uint16			fSeed;
    int				fLocation;
    uint16			fX;
    uint16			fY;
    std::vector<character>	fCharacters;
    std::vector<int16>	fReputations;
    std::vector<uint8>	fLocationFlags;
    std::vector<uint16>	fEnterStates;
    cache_map		fCaches;
    std::vector<world_event> fEvents;
    party			fParty;
    std::vector<character> fSpare;
    uint16			fState;
    int				fDifficulty;
    std::vector<uint8> fBytes;
};
