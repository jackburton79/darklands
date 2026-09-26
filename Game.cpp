#include "Game.h"

#include "CharacterFile.h"
#include "CityFile.h"
#include "CityVisit.h"
#include "GameData.h"
#include "InfoView.h"
#include "MapViewer.h"
#include "SaveFile.h"
#include "ScreenSupport.h"

#include <random>
#include <stdexcept>
#include <sys/stat.h>


Game::Game(GameData& data)
    :
    fData(data),
    fCity(-1),
    fScreen(CityVisit::SCREEN_START),
    fPosition{ 0, 0 }
{
    fParty.leader = 0;
    fParty.cash = money{ 0, 0, 0 };
}


void
Game::NewGame(int startCity)
{
    const CityFile& cities = fData.Cities();
    if (cities.CountCities() == 0)
        throw std::runtime_error("Game: no cities");
    if (startCity < 0 || startCity >= int(cities.CountCities())) {
        std::random_device seed;
        std::uniform_int_distribution<int> pick(0, int(cities.CountCities()) - 1);
        startCity = pick(seed);
    }
    // CHARACTR.TMP has no money: the characters' funds are pooled at the
    // start of the game (manual p. 15), where from is unknown
    fParty = CharacterFile(fData.PathFor("CHARACTR.TMP")).Party();
    try {
        fTime = SaveFile(fData.PathFor("SAVES/DEFAULT")).Date();
    } catch (const std::exception&) {
        fTime = GameTime();		// no template: 1 January 1400
    }
    fCity = startCity;
    fScreen = CityVisit::SCREEN_START;
}


void
Game::LoadGame(const std::string& fileName)
{
    struct stat st;
    const std::string path = ::stat(fileName.c_str(), &st) == 0
        ? fileName : fData.PathFor("SAVES/" + fileName);
    const SaveFile save(path);
    if (save.Party().members.empty())
        throw std::runtime_error("Game: no party in " + fileName);
    fParty = save.Party();
    fTime = save.Date();
    // the cities are the first locations of DARKLAND.LOC; in a city the
    // game goes on in the main street (the saved screen is not decoded)
    if (save.Location() >= 0
            && save.Location() < int(fData.Cities().CountCities())) {
        fCity = save.Location();
        fScreen = CityVisit::SCREEN_MAIN_STREET;
    } else {
        fCity = -1;
        fPosition = map_position{ save.X(), save.Y() };
    }
}


void
Game::Run()
{
    if (fParty.members.empty())
        NewGame();

    // load everything before opening the window
    CityVisit visit(fData);
    visit.SetParty(&fParty);
    visit.SetClock(&fTime);
    MapViewer map(fData);
    map.SetClock(&fTime);
    InfoView info(fData);
    info.SetParty(&fParty);
    info.SetClock(&fTime);
    visit.SetInfoView(&info);
    map.SetInfoView(&info);

    GameWindow window("Darklands");
    int cityIndex = fCity;
    int screen = fScreen;
    map_position position = fPosition;
    for (;;) {
        if (cityIndex >= 0) {
            if (visit.Run(window, cityIndex, screen) == CityVisit::QUIT)
                return;
            const city& c = fData.Cities().CityAt(uint32(cityIndex));
            position = map_position{ c.x, c.y };
        }
        map.SetPartyPosition(position);
        cityIndex = map.Run(window);
        if (cityIndex < 0)
            return;
        screen = CityVisit::SCREEN_OUTSIDE;
    }
}
