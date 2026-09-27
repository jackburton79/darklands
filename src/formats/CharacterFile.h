/*
 * CharacterFile.h
 * Reader for CHARACTR.TMP, the characters of a new game: the Quickstart
 * party in the game data. See docs/formats.md.
 */
#pragma once

#include "Character.h"

#include <string>
#include <vector>

class CharacterFile {
public:
    explicit		CharacterFile(const std::string& fileName);	// throws on error

    const std::vector<character>& Characters() const	{ return fCharacters; }
    // The party formed from them (no money: see docs/formats.md).
    const party&	Party() const						{ return fParty; }

private:
    std::vector<character>	fCharacters;
    party			fParty;
};
