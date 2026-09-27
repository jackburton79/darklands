/*
 * SaveFile.h
 * Reader for the saved games (SAVES/DKSAVEn.SAV, SAVES/DEFAULT): the
 * party with its money, the date and where the party is. Only the parts
 * the game uses so far. See docs/formats.md.
 */
#pragma once

#include "Character.h"
#include "GameTime.h"

#include <string>
#include <vector>

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
    // Empty in DEFAULT, the new game template.
    const party&	Party() const				{ return fParty; }

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
    party			fParty;
};
