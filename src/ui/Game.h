/*
 * Game.h
 * The game so far: the party starts at the inn of a city (as in the
 * original: "the party is placed in a city somewhere", manual p. 11) or
 * where a saved game left it, goes through the city's cards, travels on
 * the world map and visits other cities.
 */
#pragma once

#include "Character.h"
#include "EventFile.h"
#include "GameTime.h"
#include "Travel.h"

#include <random>
#include <string>
#include <vector>

class GameData;

class Game {
public:
    explicit		Game(GameData& data);

    // A new game with the characters of CHARACTR.TMP (the Quickstart
    // party), at the inn of city `startCity` (an index into DARKLAND.CTY;
    // -1 picks one at random), on the date of SAVES/DEFAULT, the new
    // game template (28 May 1400, Terce). Throws if the data is missing.
    void			NewGame(int startCity = -1);
    // The party, its money, its position and the date from a saved game:
    // a path, or a file name in the data directory's SAVES (e.g.
    // "DKSAVE0.SAV").
    void			LoadGame(const std::string& fileName);

    // Opens the window and plays until the user quits.
    void			Run();

    const party&	Party() const	{ return fParty; }
    const GameTime&	Time() const	{ return fTime; }

private:
    GameData&		fData;
    party			fParty;
    GameTime		fTime;
    uint16			fSeed;			// the game's seed global (DS:9C4A)
    std::mt19937	fRandom;
    std::vector<int16> fReputations;	// by location of DARKLAND.LOC
    std::vector<world_event> fEvents;	// the game's events
    std::vector<uint8> fLocationFlags;	// by location: its state (+0x14)
    std::vector<uint16> fEnterStates;	// by location: its arrival (+0x0C)
    int				fCity;			// the party is in this city, or -1
    int				fScreen;		// the city screen to start from
    map_position	fPosition;		// else on the map, here
};
