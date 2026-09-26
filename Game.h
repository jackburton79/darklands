/*
 * Game.h
 * The game so far: the party starts at the inn of a city (as in the
 * original: "the party is placed in a city somewhere", manual p. 11) or
 * where a saved game left it, goes through the city's cards, travels on
 * the world map and visits other cities.
 */
#pragma once

#include "Character.h"
#include "Travel.h"

#include <string>

class GameData;

class Game {
public:
    explicit		Game(GameData& data);

    // A new game with the characters of CHARACTR.TMP (the Quickstart
    // party), at the inn of city `startCity` (an index into DARKLAND.CTY;
    // -1 picks one at random). Throws if the data is missing.
    void			NewGame(int startCity = -1);
    // The party, its money and its position from a saved game: a path,
    // or a file name in the data directory's SAVES (e.g. "DKSAVE0.SAV").
    void			LoadGame(const std::string& fileName);

    // Opens the window and plays until the user quits.
    void			Run();

    const party&	Party() const	{ return fParty; }

private:
    GameData&		fData;
    party			fParty;
    int				fCity;			// the party is in this city, or -1
    int				fScreen;		// the city screen to start from
    map_position	fPosition;		// else on the map, here
};
