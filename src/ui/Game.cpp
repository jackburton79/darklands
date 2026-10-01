#include "Game.h"

#include "CardView.h"
#include "CharacterFile.h"
#include "CityFile.h"
#include "CityVisit.h"
#include "Encounters.h"
#include "GameData.h"
#include "InfoView.h"
#include "LocationFile.h"
#include "MapViewer.h"
#include "MenuBar.h"
#include "MsgFile.h"
#include "PartySelectView.h"
#include "ResidenceView.h"
#include "SaveFile.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <sys/stat.h>

// The saved games the Load dialog lists (the card's room)
static const size_t kLoadListSize	= 10;


Game::Game(GameData& data)
    :
    fData(data),
    fSelectParty(false),
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
    // the characters out of the party wait anywhere (their city is not
    // known); their pictures are those of the party's first member
    fRetired.clear();
    for (const character& who : save.Spare()) {
        retired_member spare;
        spare.member = who;
        spare.image = save.Party().images.empty() ? std::string("F60")
            : save.Party().images[0];
        spare.city = -1;
        fRetired.push_back(spare);
    }
    fSettings.difficulty = std::max(0, std::min(2, save.Difficulty()));
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
    const GameTime::listener recover = [this](bool newDay) {
        PassTime(fParty, newDay,
            [this](int n) { return int(fRandom() % uint32(n)); });
    };
    fTime.SetListener(recover);
    visit.SetSeed(fSeed);
    visit.SetRetired(&fRetired);
    visit.SetReputations(&fReputations);
    _PrepareWorld();
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
    bool meeting = false;		// the bandits of the map are on
    // Ctrl+S: in a city the game goes on at the inn; at another place,
    // at its arrival
    visit.SetSaveHandler([&](GameWindow& window) {
        if (meeting) {
            _SaveDialog(window, -1, map.PartyPosition(), 0x0C);
            return;
        }
        const location& here = fData.Locations().LocationAt(uint32(cityIndex));
        const bool city = uint32(cityIndex) < fData.Cities().CountCities();
        _SaveDialog(window, cityIndex, map_position{ here.x, here.y },
            city ? 0x1D : fEnterStates[size_t(cityIndex)]);
    });
    map.SetSaveHandler([&](GameWindow& window) {
        _SaveDialog(window, -1, map.PartyPosition(), 0x0C);
    });

    MenuBar menu(fData);
    menu.SetSettings(&fSettings);
    visit.SetMenuBar(&menu);
    visit.SetSettings(&fSettings);
    map.SetMenuBar(&menu);
    int screen = fScreen;
    map_position position = fPosition;
    // the menu's Load Saved Game: the game goes on from the saved one
    const std::function<bool(GameWindow&)> load = [&](GameWindow& where) {
        if (!_LoadDialog(where))
            return false;
        _PrepareWorld();
        fTime.SetListener(recover);
        visit.SetSeed(fSeed);
        visit.SetParty(&fParty);
        info.SetParty(&fParty);
        cityIndex = fCity;
        screen = fScreen;
        position = fPosition;
        return true;
    };
    const std::function<void(GameWindow&)> order = [&](GameWindow& where) {
        _OrderDialog(where);
        visit.SetParty(&fParty);
        info.SetParty(&fParty);
    };
    // C on the map: the camp of the wilderness (file 0x5E9A6): the danger
    // of a day starts at 3 for each member, plus the place's
    ResidenceView camp(fData);
    camp.SetClock(&fTime);
    camp.SetInfoView(&info);
    camp.SetMenuBar(&menu);
    map.SetCampHandler([&](GameWindow& where, int terrain) {
        camp.SetParty(&fParty);
        camp.SetCamp(3 * int(fParty.members.size()) + terrain);
        camp.SetSafe(visit.CampSafe());
        camp.SetEncounterFollows(true);
        camp.Run(where);
        if (!camp.Interrupted())
            return 0;
        // found: the soldiers or the bandits, by chance (file 0x5E1E5)
        visit.SetCampGuard(camp.Guard());
        visit.SetToll(false);
        meeting = true;
        visit.SetOnMap(true);
        const CityVisit::result result = visit.Run(where, map.NearestCity(),
            fRandom() % 2 == 0 ? CityVisit::SCREEN_CAMPJ_MEET
                : CityVisit::SCREEN_CAMPB_MEET);
        visit.SetOnMap(false);
        meeting = false;
        if (result == CityVisit::LOAD_GAME)
            return MapViewer::kLoadRequested;
        if (result == CityVisit::PARTY_LOST) {
            std::cout << "The whole party has died: the game is over."
                << std::endl;
            return -1;
        }
        return result == CityVisit::QUIT ? -1 : 0;
    });
    // The meetings on the way: the hazard is MapViewer's, the state the
    // chooser's (Encounters.h); the ones that are not played are skipped.
    // The reputation of the nearest place is what a win raises
    map.SetEncounterHandler([&](GameWindow& where, int place, int terrain,
            int state) {
        int screen = CityVisit::SCREEN_THIEVES_MAP_MEET;
        visit.SetToll(state == ENCOUNTER_TOLL);
        switch (state) {
            case ENCOUNTER_BANDITS:
            case ENCOUNTER_SOLDIERS:
                visit.SetBandits(state == ENCOUNTER_SOLDIERS, terrain);
                screen = CityVisit::SCREEN_BANDITS_MEET;
                break;
            case ENCOUNTER_PILGRIMS:
                screen = CityVisit::SCREEN_PILGRIMS_MEET;
                break;
            case ENCOUNTER_HERMIT:
                screen = CityVisit::SCREEN_HERMIT_MEET;
                break;
            case ENCOUNTER_BISHOP:
            case ENCOUNTER_TOLL:
                screen = CityVisit::SCREEN_TITHE_MEET;
                break;
            case ENCOUNTER_FRIAR:
                screen = CityVisit::SCREEN_FRIAR_MEET;
                break;
            case ENCOUNTER_CARAVAN:
                screen = CityVisit::SCREEN_CARAVAN_MEET;
                break;
            case ENCOUNTER_REFUGEES:
                screen = CityVisit::SCREEN_REFUGEES_MEET;
                break;
            default:
                break;
        }
        meeting = true;
        visit.SetOnMap(true);
        const CityVisit::result result = visit.Run(where, place, screen);
        visit.SetOnMap(false);
        meeting = false;
        if (result == CityVisit::LOAD_GAME)
            return MapViewer::kLoadRequested;
        if (result == CityVisit::PARTY_LOST) {
            std::cout << "The whole party has died: the game is over."
                << std::endl;
            return -1;
        }
        return result == CityVisit::QUIT ? -1 : 0;
    }, [&](int terrain) {
        const int state = ChooseEncounter(terrain, int(fTime.Month()),
            [this](int n) { return int(fRandom() % uint32(n)); });
        return IsEncounterPlayed(state) ? state : -1;
    }, [this](int n) { return int(fRandom() % uint32(n)); });
    visit.SetOrderHandler(order);
    map.SetOrderHandler(order);
    visit.SetLoadHandler(load);
    map.SetLoadHandler(load);

    GameWindow window("Darklands");
    if (fSelectParty) {
        PartySelectView select(fData);
        select.SetSheet(true);
        select.SetInfoView(&info);
        select.SetRoster(PartySelectView::MakeRoster(fParty, fRetired,
            PartySelectView::kEverywhere), false);
        if (select.Run(window) != PartySelectView::RESULT_BEGIN)
            return;
        PartySelectView::ApplyRoster(select.Roster(), fParty, fRetired,
            PartySelectView::kEverywhere);
        visit.SetParty(&fParty);
        info.SetParty(&fParty);
        fSelectParty = false;
    }
    for (;;) {
        if (cityIndex >= 0) {
            const CityVisit::result result = visit.Run(window, cityIndex,
                screen);
            if (result == CityVisit::LOAD_GAME)
                continue;
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
        const int place = map.Run(window);
        if (place == MapViewer::kLoadRequested)
            continue;
        cityIndex = place;
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
    // the members who retired keep their place in the world
    std::vector<character> spare;
    for (const retired_member& who : fRetired)
        spare.push_back(who.member);
    const saved_game game = { comment, fTime, fSeed, &fParty, location,
        position.x, position.y, state, &fEvents, &fReputations,
        &fLocationFlags, &fEnterStates, fSettings.difficulty, &spare };
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


void
Game::_PrepareWorld()
{
    const LocationFile& locations = fData.Locations();
    fLocationFlags.resize(locations.CountLocations(), 0);
    for (uint32 i = fEnterStates.size(); i < locations.CountLocations(); i++)
        fEnterStates.push_back(locations.LocationAt(i).enterState);
}


bool
Game::_LoadDialog(GameWindow& window)
{
    // the saved games (SAVES/DKSAVEn.SAV), the newest first
    struct saved {
        int number;
        std::string name;
        std::string label;
    };
    std::vector<saved> games;
    const std::string directory = fData.PathFor("SAVES");
    if (DIR* folder = ::opendir(directory.c_str())) {
        while (const dirent* entry = ::readdir(folder)) {
            const std::string name = entry->d_name;
            int number = 0;
            char tail[8] = "";
            if (std::sscanf(name.c_str(), "DKSAVE%d.%7s", &number, tail) != 2
                    || std::string(tail) != "SAV")
                continue;
            try {
                const SaveFile file(directory + "/" + name);
                games.push_back({ number, name, file.Label() });
            } catch (const std::exception&) {
                // not a saved game
            }
        }
        ::closedir(folder);
    }
    std::sort(games.begin(), games.end(),
        [](const saved& a, const saved& b) { return a.number > b.number; });
    // as many as the card has room for
    if (games.size() > kLoadListSize)
        games.resize(kLoadListSize);

    CardView view(fData);
    view.SetParty(&fParty);
    msg_card card = { 10, 10, 0, 240, 0, "" };
    card.text = Font::ToGameCharset(games.empty()
        ? "There are no saved games.\n" : "Load which game?\n");
    card.text += char(MSG_CODE_PARAGRAPH);
    card.text += char(MSG_CODE_PARAGRAPH);
    for (const saved& game : games) {
        card.text += char(MSG_CODE_OPTION);
        card.text += "...";
        card.text += char(MSG_CODE_OPTION_TEXT);
        card.text += Font::ToGameCharset(game.label + " (" + game.name + ")\n");
    }
    card.text += char(MSG_CODE_OPTION);
    card.text += "...";
    card.text += char(MSG_CODE_OPTION_TEXT);
    card.text += "go back.\n";
    view.SetCard(card, card_variables());
    const int choice = view.Run(window);
    if (choice < 0 || choice >= int(games.size()))
        return false;
    try {
        LoadGame(games[size_t(choice)].name);
    } catch (const std::exception& error) {
        card.text = Font::ToGameCharset(std::string(
            "The game could not be loaded: ") + error.what());
        view.SetCard(card, card_variables());
        view.Run(window);
        return false;
    }
    return true;
}


// Change Marching Order (the overlay at file 0x51050, 1EF8:051C): for each
// place in turn, from the first, the player picks one of the members left;
// the last one takes the last place. The leader stays the same member.
// The game's prompts ("Select ...") come from a table loaded at run time:
// these are made up.
void
Game::_OrderDialog(GameWindow& window)
{
    if (fParty.members.size() < 2)
        return;
    static const char* kPlaces[5] = { "first", "second", "third", "fourth",
        "fifth" };
    std::vector<size_t> left;
    for (size_t i = 0; i < fParty.members.size(); i++)
        left.push_back(i);
    std::vector<size_t> order;
    CardView view(fData);
    view.SetParty(&fParty);
    while (left.size() > 1) {
        msg_card card = { 10, 10, 0, 240, 0, "" };
        card.text = Font::ToGameCharset(std::string("Select the ")
            + kPlaces[order.size()] + " character.\n");
        card.text += char(MSG_CODE_PARAGRAPH);
        card.text += char(MSG_CODE_PARAGRAPH);
        for (size_t i : left) {
            card.text += char(MSG_CODE_OPTION);
            card.text += "...";
            card.text += char(MSG_CODE_OPTION_TEXT);
            card.text += Font::ToGameCharset(fParty.members[i].shortName)
                + "\n";
        }
        view.SetCard(card, card_variables());
        const int choice = view.Run(window);
        if (choice < 0 || choice >= int(left.size()))
            return;				// Esc: nothing changes
        order.push_back(left[size_t(choice)]);
        left.erase(left.begin() + choice);
    }
    order.push_back(left[0]);

    party changed = fParty;
    changed.members.clear();
    changed.images.clear();
    changed.colors.clear();
    for (size_t i = 0; i < order.size(); i++) {
        changed.members.push_back(fParty.members[order[i]]);
        if (order[i] < fParty.images.size())
            changed.images.push_back(fParty.images[order[i]]);
        if (order[i] < fParty.colors.size())
            changed.colors.push_back(fParty.colors[order[i]]);
        if (int(order[i]) == fParty.leader)
            changed.leader = int(i);
    }
    fParty = changed;
}
