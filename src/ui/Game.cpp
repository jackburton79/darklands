#include "Game.h"

#include "CharacterFile.h"
#include "CityFile.h"
#include "CityVisit.h"
#include "GameData.h"
#include "InfoView.h"
#include "LocationFile.h"
#include "MapViewer.h"
#include "SaveFile.h"
#include "ScreenSupport.h"

#include <iostream>
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
    fSeed = 0;
    fRandom.seed(std::random_device{}());
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
    // the new game's template: its date, its events and the locations'
    // state
    fReputations.assign(fData.Locations().CountLocations(), 0);
    try {
        const SaveFile template_(fData.PathFor("SAVES/DEFAULT"));
        fTime = template_.Date();
        fEvents = template_.Events();
        fLocationFlags = template_.LocationFlags();
    } catch (const std::exception&) {
        fTime = GameTime();		// no template: 1 January 1400
    }
    // DARKLAND.EXE takes it from the BIOS clock ticks (file 0x7C04F)
    fSeed = uint16(std::random_device()());
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
    fSeed = save.Seed();
    fReputations = save.Reputations();
    fReputations.resize(fData.Locations().CountLocations(), 0);
    fEvents = save.Events();
    fLocationFlags = save.LocationFlags();
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
    // the party recovers as time passes
    fTime.SetListener([this](bool newDay) {
        PassTime(fParty, newDay,
            [this](int n) { return int(fRandom() % uint32(n)); });
    });
    visit.SetSeed(fSeed);
    visit.SetReputations(&fReputations);
    visit.SetWorld(&fEvents, &fLocationFlags);
    MapViewer map(fData);
    map.SetClock(&fTime);
    InfoView info(fData);
    info.SetParty(&fParty);
    info.SetClock(&fTime);
    info.SetReputations(&fReputations);
    visit.SetInfoView(&info);
    map.SetInfoView(&info);

    GameWindow window("Darklands");
    int cityIndex = fCity;
    int screen = fScreen;
    map_position position = fPosition;
    for (;;) {
        if (cityIndex >= 0) {
            const CityVisit::result result = visit.Run(window, cityIndex,
                screen);
            if (result == CityVisit::PARTY_LOST) {
                std::cout << "The whole party has died: the game is over."
                    << std::endl;
                return;
            }
            if (result == CityVisit::QUIT)
                return;
            if (uint32(cityIndex) < fData.Cities().CountCities()) {
                const city& c = fData.Cities().CityAt(uint32(cityIndex));
                position = map_position{ c.x, c.y };
            } else {
                const location& l = fData.Locations().LocationAt(
                    uint32(cityIndex));
                position = map_position{ l.x, l.y };
            }
        }
        map.SetPartyPosition(position);
        cityIndex = map.Run(window);
        if (cityIndex < 0)
            return;
        screen = CityVisit::SCREEN_OUTSIDE;
    }
}
