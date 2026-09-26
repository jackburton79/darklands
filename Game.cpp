#include "Game.h"

#include "CityFile.h"
#include "CityVisit.h"
#include "GameData.h"
#include "MapViewer.h"
#include "ScreenSupport.h"

#include <random>
#include <stdexcept>


Game::Game(GameData& data)
    :
    fData(data)
{
}


void
Game::Run(int startCity)
{
    const CityFile& cities = fData.Cities();
    if (cities.CountCities() == 0)
        throw std::runtime_error("Game: no cities");
    if (startCity < 0 || startCity >= int(cities.CountCities())) {
        std::random_device seed;
        std::uniform_int_distribution<int> pick(0, int(cities.CountCities()) - 1);
        startCity = pick(seed);
    }

    // load everything before opening the window
    CityVisit visit(fData);
    MapViewer map(fData);

    GameWindow window("Darklands");
    int cityIndex = startCity;
    int screen = CityVisit::SCREEN_START;
    for (;;) {
        if (visit.Run(window, cityIndex, screen) == CityVisit::QUIT)
            return;
        const city& c = cities.CityAt(uint32(cityIndex));
        map.SetPartyPosition(map_position{ c.x, c.y });
        cityIndex = map.Run(window);
        if (cityIndex < 0)
            return;
        screen = CityVisit::SCREEN_OUTSIDE;
    }
}
