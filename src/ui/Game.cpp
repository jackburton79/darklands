#include "Game.h"

#include "CardView.h"
#include "CharacterFile.h"
#include "CityFile.h"
#include "CityVisit.h"
#include "GameData.h"
#include "InfoView.h"
#include "LocationFile.h"
#include "MapViewer.h"
#include "SaveFile.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

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
        fTemplate = fData.PathFor("SAVES/DEFAULT");
        const SaveFile template_(fTemplate);
        fTime = template_.Date();
        fEvents = template_.Events();
        fLocationFlags = template_.LocationFlags();
        fEnterStates = template_.EnterStates();
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
    fTemplate = path;
    fParty = save.Party();
    fTime = save.Date();
    fSeed = save.Seed();
    fReputations = save.Reputations();
    fReputations.resize(fData.Locations().CountLocations(), 0);
    fEvents = save.Events();
    fLocationFlags = save.LocationFlags();
    fEnterStates = save.EnterStates();
    // the cities are the first locations of DARKLAND.LOC; in a city the
    // game goes on at the inn if it was saved there (DS:A772 0x1D, 0x1E
    // at night), else in the main street
    if (save.Location() >= 0
            && save.Location() < int(fData.Cities().CountCities())) {
        fCity = save.Location();
        fScreen = save.State() == 0x1D || save.State() == 0x1E
            ? CityVisit::SCREEN_INN : CityVisit::SCREEN_MAIN_STREET;
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
    // the locations' state, from DARKLAND.LOC where the file had none
    const LocationFile& locations = fData.Locations();
    fLocationFlags.resize(locations.CountLocations(), 0);
    for (uint32 i = fEnterStates.size(); i < locations.CountLocations(); i++)
        fEnterStates.push_back(locations.LocationAt(i).enterState);
    visit.SetWorld(&fEvents, &fLocationFlags, &fEnterStates);
    MapViewer map(fData);
    map.SetClock(&fTime);
    InfoView info(fData);
    info.SetParty(&fParty);
    info.SetClock(&fTime);
    info.SetReputations(&fReputations);
    visit.SetInfoView(&info);
    map.SetInfoView(&info);
    int cityIndex = fCity;
    // Ctrl+S: in a city the game goes on at the inn; at another place,
    // at its arrival
    visit.SetSaveHandler([&](GameWindow& window) {
        const location& here = fData.Locations().LocationAt(uint32(cityIndex));
        const bool city = uint32(cityIndex) < fData.Cities().CountCities();
        _SaveDialog(window, cityIndex, map_position{ here.x, here.y },
            city ? 0x1D : fEnterStates[size_t(cityIndex)]);
    });
    map.SetSaveHandler([&](GameWindow& window) {
        _SaveDialog(window, -1, map.PartyPosition(), 0x0C);
    });

    GameWindow window("Darklands");
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


std::string
Game::Save(const std::string& comment, int location,
    const map_position& position, uint16 state)
{
    const SaveFile template_(fTemplate.empty()
        ? fData.PathFor("SAVES/DEFAULT") : fTemplate);
    const std::string directory = fData.PathFor("SAVES");
    ::mkdir(directory.c_str(), 0755);
    std::string name;
    struct stat st;
    for (int n = 0; ; n++) {
        name = "DKSAVE" + std::to_string(n) + ".SAV";
        if (::stat((directory + "/" + name).c_str(), &st) != 0)
            break;
    }
    const saved_game game = { comment, fTime, fSeed, &fParty, location,
        position.x, position.y, state, &fEvents, &fReputations,
        &fLocationFlags, &fEnterStates };
    template_.Write(directory + "/" + name, game, fData.Locations());
    return name;
}


void
Game::_SaveDialog(GameWindow& window, int location,
    const map_position& position, uint16 state)
{
    CardView view(fData);
    view.SetParty(&fParty);
    msg_card card = { 10, 10, 0, 240, 0, "Save the game." };
    view.SetCard(card, card_variables());
    view.SetPrompt("Save Game Comment:", "Darklands", 22, false);
    if (view.Run(window) < 0)
        return;
    std::string text;
    try {
        const std::string name = Save(LocationFile::DecodeName(
            view.PromptText().data(), view.PromptText().size()), location,
            position, state);
        text = "The game is saved as " + name + ".";
    } catch (const std::exception& error) {
        text = std::string("The game could not be saved: ") + error.what();
    }
    card.text = Font::ToGameCharset(text);
    view.SetCard(card, card_variables());
    view.Run(window);
}
