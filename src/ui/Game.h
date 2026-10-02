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
#include "GameSettings.h"
#include "GameTime.h"
#include "Travel.h"

#include <map>
#include <random>
#include <string>
#include <vector>

class GameData;
class GameWindow;

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

    // Before the game begins, the party selection screen (manual pp. 11-12)
    // lets the player choose among the characters.
    void			SetSelectParty(bool select)	{ fSelectParty = select; }

    // Opens the window and plays until the user quits.
    void			Run();

    const party&	Party() const	{ return fParty; }
    const GameTime&	Time() const	{ return fTime; }
    // The Game menu's settings (the menu bar changes them; a saved game
    // holds the difficulty)
    const game_settings& Settings() const	{ return fSettings; }

    // Saves the game as the first free SAVES/DKSAVEn.SAV, as DARKLAND.EXE
    // does (file 0x7505A), with a comment; returns the file's name.
    // `location` is where the party is (-1: on the map), `state` the
    // game's state to go on from (DS:A772: 0x0C the map, 0x1D the inn).
    // Throws on error.
    std::string		Save(const std::string& comment, int location,
                        const map_position& position, uint16 state);

private:
    // The locations' state, from DARKLAND.LOC where the game had none
    void			_PrepareWorld();
    // The menu's Load Saved Game: lists the saved games, loads the one
    // chosen; false if none was
    bool			_LoadDialog(GameWindow& window);
    // The menu's Change Marching Order: asks who goes first, second...
    void			_OrderDialog(GameWindow& window);
    // Ctrl+S: asks for the comment ("Save Game Comment:", file 0x74CD6),
    // saves and says where
    void			_SaveDialog(GameWindow& window, int location,
                        const map_position& position, uint16 state);

    GameData&		fData;
    party			fParty;
    GameTime		fTime;
    game_settings	fSettings;
    uint16			fSeed;			// the game's seed global (DS:9C4A)
    std::mt19937	fRandom;
    std::vector<int16> fReputations;	// by location of DARKLAND.LOC
    std::vector<world_event> fEvents;	// the game's events
    std::vector<retired_member> fRetired;	// the members who retired
    bool			fSelectParty;
    std::vector<uint8> fLocationFlags;	// by location: its state (+0x14)
    std::vector<uint16> fEnterStates;	// by location: its arrival (+0x0C)
    std::map<int, std::vector<cache_item> > fCaches;	// left at the inns
    int				fCity;			// the party is in this city, or -1
    int				fScreen;		// the city screen to start from
    map_position	fPosition;		// else on the map, here
    std::string		fTemplate;		// the saved game the others are made from
};
