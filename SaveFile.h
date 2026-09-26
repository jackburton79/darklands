/*
 * SaveFile.h
 * Reader for the saved games (SAVES/DKSAVEn.SAV, SAVES/DEFAULT): the
 * party with its money, the date and where the party is. Only the parts
 * the game uses so far. See docs/formats.md.
 */
#pragma once

#include "Character.h"

#include <string>
#include <vector>

struct game_date {
    uint16 year;
    uint16 month;			// 0-based (inferred)
    uint16 day;
    uint16 hour;
};

class SaveFile {
public:
    explicit		SaveFile(const std::string& fileName);	// throws on error

    const std::string& Label() const			{ return fLabel; }
    const std::string& LocationName() const		{ return fLocationName; }
    const game_date& Date() const				{ return fDate; }
    // Index into DARKLAND.LOC, or -1 in the wilderness.
    int				Location() const			{ return fLocation; }
    uint16			X() const					{ return fX; }	// map tile
    uint16			Y() const					{ return fY; }

    const std::vector<character>& Characters() const	{ return fCharacters; }
    // Empty in DEFAULT, the new game template.
    const party&	Party() const				{ return fParty; }

private:
    std::string		fLabel;
    std::string		fLocationName;
    game_date		fDate;
    int				fLocation;
    uint16			fX;
    uint16			fY;
    std::vector<character>	fCharacters;
    party			fParty;
};
