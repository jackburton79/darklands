#include "CityVisit.h"

#include "CityFile.h"
#include "DescriptionFile.h"
#include "GameData.h"
#include "ScreenSupport.h"

#include <stdexcept>

// What an option does
enum option_action {
    ACTION_UNLISTED = 0,		// zero-filled rest of a list: not implemented
    ACTION_NOT_IMPLEMENTED,
    ACTION_GO,					// to another screen
    ACTION_LEAVE,				// back to the map
    ACTION_HIDE					// never shown
};

// Options that need the city to have something
static const int kAlways			= -1;
static const int kNeedsHarbor		= CITY_PLACE_COUNT;	// else a place slot

struct option_rule {
    int action;
    int target;					// screen, for ACTION_GO
    int needs;					// kAlways, a city_place or kNeedsHarbor
};

static const int kMaxOptions = 12;

struct screen_rules {
    const char* deck;
    int card;
    const char* scene;			// picture shown first, or NULL
    option_rule options[kMaxOptions];	// in card order; the rest: not implemented
};

#define GO(screen)			{ ACTION_GO, CityVisit::screen, kAlways }
#define GO_IF(screen, needs) { ACTION_GO, CityVisit::screen, needs }
#define LEAVE				{ ACTION_LEAVE, 0, kAlways }
#define TODO				{ ACTION_NOT_IMPLEMENTED, 0, kAlways }
#define TODO_IF(needs)		{ ACTION_NOT_IMPLEMENTED, 0, needs }
#define HIDE				{ ACTION_HIDE, 0, kAlways }

// The option lists are those of the cards (see `darklands --messages`).
// The docks need a harbor: inferred, DARKLAND.CTY only knows sea ports.
static const screen_rules kScreens[CityVisit::SCREEN_COUNT] = {
    // "You gather around the comfortable fire at the $Inn..."
    { "PARTY02", 0, NULL, {
        GO(SCREEN_INN),						// spend some time here
        LEAVE,								// immediately leave the city
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET),
        HIDE								// "test mines", a leftover
    } },
    // "Before you lies the $PlaceName, $PlaceDesc."
    { "OUTSI00", 0, NULL, {
        GO(SCREEN_MAIN_STREET),				// main gate in daytime
        TODO,								// sneak over the wall
        TODO,								// main gate at night
        TODO,								// sally port at night
        LEAVE								// turn away
    } },
    // "Here you can enjoy the good food... of the $Inn common-room."
    { "URBAN00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO, TODO, TODO,	// news, meal, residence...
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Looking down the main street of $PlaceName, you set off toward..."
    { "MAINS01", 0, "MAIN-ST.PIC", {
        TODO_IF(CITY_SQUARE),
        TODO_IF(CITY_CASTLE),
        TODO_IF(CITY_MARKET),
        TODO,								// the churches
        TODO,								// craft guilds and side alleys
        GO_IF(SCREEN_INN, CITY_INN),
        TODO_IF(kNeedsHarbor),				// wharves and docks
        GO(SCREEN_SIDE_STREET),
        TODO,								// a scenic grove
        GO(SCREEN_GATE)
    } },
    // "The side streets of $PlaceName are full of people..."
    { "SIDES00", 0, NULL, {
        GO(SCREEN_MAIN_STREET),
        TODO_IF(CITY_SQUARE),
        TODO_IF(CITY_CASTLE),
        TODO_IF(CITY_MARKET),
        TODO,								// the churches
        TODO,								// crafts district, inns...
        TODO_IF(kNeedsHarbor),				// docks and wharves
        TODO,								// a scenic grove
        TODO,								// other locations
        TODO								// the city walls
    } },
    // "The gate is heavily guarded..."
    { "SELEC00", 0, NULL, {
        LEAVE,								// simply walk out
        TODO, TODO, TODO, TODO, TODO,		// hide, potion, saint, fight, wall
        GO(SCREEN_MAIN_STREET)				// not leave just yet
    } },
    // not a game card: see the constructor
    { NULL, 0, NULL, {
        TODO								// go back (handled by Choose())
    } }
};

#undef GO
#undef GO_IF
#undef LEAVE
#undef TODO
#undef TODO_IF
#undef HIDE


static const option_rule&
RuleFor(int screen, int option)
{
    static const option_rule kNotImplemented = { ACTION_NOT_IMPLEMENTED, 0,
        kAlways };
    if (option < 0 || option >= kMaxOptions
            || kScreens[screen].options[option].action == ACTION_UNLISTED)
        return kNotImplemented;
    return kScreens[screen].options[option];
}


// #pragma mark - CityVisit


CityVisit::CityVisit(GameData& data)
    :
    fData(data),
    fView(data),
    fCity(-1),
    fScreen(SCREEN_START),
    fPreviousScreen(SCREEN_START)
{
    // load the decks up front, so missing files are reported right away
    for (const screen_rules& screen : kScreens) {
        if (screen.deck != NULL)
            fData.Messages(screen.deck).CardAt(uint32(screen.card));
    }

    // in the game's character set, with the cards' control codes
    fNotImplementedCard.textTop = 10;
    fNotImplementedCard.textLeft = 10;
    fNotImplementedCard.unknown1 = 0;
    fNotImplementedCard.textRight = 240;
    fNotImplementedCard.unknown2 = 0;
    fNotImplementedCard.text = std::string("This is not implemented yet.\n")
        + char(MSG_CODE_PARAGRAPH) + char(MSG_CODE_PARAGRAPH)
        + char(MSG_CODE_OPTION) + "..." + char(MSG_CODE_OPTION_TEXT)
        + "go back.\n";
}


CityVisit::result
CityVisit::Run(GameWindow& window, int cityIndex, int screen)
{
    Enter(cityIndex, screen);
    for (;;) {
        const int option = fView.Run(window);
        if (option < 0)
            return QUIT;
        if (!Choose(option))
            return LEAVE_CITY;
    }
}


void
CityVisit::Enter(int cityIndex, int screen)
{
    if (screen < 0 || screen >= SCREEN_COUNT)
        throw std::out_of_range("CityVisit::Enter(): invalid screen");
    fCity = cityIndex;
    fVariables.clear();
    AddCityVariables(fData, cityIndex, fVariables);
    AddPartyVariables(fVariables);
    fPreviousScreen = screen;
    _Show(screen);
}


bool
CityVisit::Choose(int option)
{
    if (fScreen == SCREEN_NOT_IMPLEMENTED) {
        _Show(fPreviousScreen, false);
        return true;
    }
    const option_rule& rule = RuleFor(fScreen, option);
    switch (rule.action) {
        case ACTION_GO:
            _Show(rule.target);
            return true;
        case ACTION_LEAVE:
            return false;
        default:
            fPreviousScreen = fScreen;
            _Show(SCREEN_NOT_IMPLEMENTED);
            return true;
    }
}


/* static */
void
CityVisit::AddCityVariables(GameData& data, int cityIndex,
    card_variables& variables)
{
    // place variables by DARKLAND.CTY slot: inferred from the names (the
    // list of variable names is in DARKLAND.EXE) and the texts
    static const struct {
        const char* name;
        int place;
    } kPlaceVariables[] = {
        { "CityLordTitle", CITY_RULER },
        { "citySquare", CITY_SQUARE }, { "councilHall", CITY_TOWN_HALL },
        { "fortress", CITY_CASTLE }, { "cathedral", CITY_CATHEDRAL },
        { "cityChurch", CITY_CHURCH }, { "marketplace", CITY_MARKET },
        { "imperialMint", CITY_MINT_SQUARE }, { "slum", CITY_SLUMS },
        { "cityBarracks", CITY_ARMORY }, { "pawnshop", CITY_PAWNSHOP },
        { "monastery", CITY_MONASTERY }, { "Inn", CITY_INN },
        { "inn", CITY_INN }, { "university", CITY_UNIVERSITY }
    };
    const city& c = data.Cities().CityAt(uint32(cityIndex));
    variables["PlaceName"] = c.shortName;
    const DescriptionFile& descriptions = data.CityDescriptions();
    if (uint32(cityIndex) < descriptions.CountDescriptions())
        variables["PlaceDesc"] = descriptions.DescriptionAt(uint32(cityIndex));
    for (const auto& variable : kPlaceVariables) {
        if (!c.places[variable.place].empty())
            variables[variable.name] = c.places[variable.place];
    }
}


/* static */
void
CityVisit::AddPartyVariables(card_variables& variables)
{
    static const char* kNames[] = { "Gretchen", "Gunther", "Hans", "Ebhard" };
    static const char* kOrdinals[] = { "One", "Two", "Three", "Four" };
    for (int i = 0; i < 4; i++)
        variables[std::string("Chosen") + kOrdinals[i] + "Name"] = kNames[i];
    variables["LeaderName"] = kNames[0];
    variables["he"] = "she";
    variables["He"] = "She";
    variables["his"] = "her";
    variables["His"] = "Her";
    variables["him"] = "her";
    variables["himself"] = "herself";
}


void
CityVisit::_Show(int screen, bool withScene)
{
    fScreen = screen;
    const screen_rules& rules = kScreens[screen];
    if (rules.deck == NULL) {
        fView.SetCard(fNotImplementedCard, fVariables);
        fView.SetScene("");
        return;
    }
    fView.SetCard(fData.Messages(rules.deck).CardAt(uint32(rules.card)),
        fVariables, _HiddenOptions(screen));
    fView.SetScene(withScene && rules.scene != NULL ? rules.scene : "");
}


std::vector<int>
CityVisit::_HiddenOptions(int screen) const
{
    const city& c = fData.Cities().CityAt(uint32(fCity));
    std::vector<int> hidden;
    for (int i = 0; i < kMaxOptions; i++) {
        const option_rule& rule = RuleFor(screen, i);
        bool hide = rule.action == ACTION_HIDE;
        if (rule.needs == kNeedsHarbor)
            hide = c.harbor == CITY_HARBOR_NONE;
        else if (rule.needs != kAlways)
            hide = c.places[rule.needs].empty();
        if (hide)
            hidden.push_back(i);
    }
    return hidden;
}
