/*
 * Game.h
 * The game so far: the party starts at the inn of a city (as in the
 * original: "the party is placed in a city somewhere", manual p. 11),
 * goes through the city's cards, travels on the world map and visits
 * other cities.
 */
#pragma once

class GameData;

class Game {
public:
    explicit		Game(GameData& data);

    // Opens the window and plays until the user quits. `startCity` is
    // an index into DARKLAND.CTY; -1 picks one at random.
    void			Run(int startCity = -1);

private:
    GameData&		fData;
};
