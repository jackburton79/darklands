#include "CityVisit.h"

#include "Character.h"
#include "CityFile.h"
#include "DescriptionFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
#include "ExeData.h"
#include "ListFile.h"
#include "ScreenSupport.h"

#include <stdexcept>

// What an option does
enum option_action {
    ACTION_UNLISTED = 0,		// zero-filled rest of a list: not implemented
    ACTION_NOT_IMPLEMENTED,
    ACTION_GO,					// to another screen, after `minutes`
    ACTION_LEAVE,				// back to the map
    ACTION_HIDE,				// never shown
    ACTION_TRADE,				// the trade screen, with merchant `target`
    ACTION_MASS,				// the church's options
    ACTION_CONFESSION,
    ACTION_DONATION,
    ACTION_ALTAR_BOY,
    ACTION_SLEEP,				// the inn's options
    ACTION_STABLES,
    ACTION_RESIDENCE,
    ACTION_REDEEM,				// the banks: `target` is the result
    ACTION_DEPOSIT,				// the number typed in; `target`: the bank
    ACTION_DISCUSS_TREATMENTS,	// the physician's options
    ACTION_ASK_AID,
    ACTION_COMPONENTS,
    ACTION_TREATMENT,
    ACTION_STUDENTS,
    ACTION_LEAVE_PHYSICIAN,		// at night: `target` 1 to apologize
    ACTION_APOLOGIZE
};

// Options that need the city to have something: a place slot, a harbor
// or a shop (its quality in the city record is not 0)
static const int kAlways			= -1;
static const int kNeedsHarbor		= CITY_PLACE_COUNT;
static const int kNeedsShop			= kNeedsHarbor + 1;	// + city_shop
// or a state of the party (DARKLAND.EXE, the church, file 0xB88C2):
// something to donate (a tenth of the purse, at least 10 pfennigs),
// a bad local reputation (-10 or less) for sanctuary
static const int kNeedsDonation		= -2;
static const int kNeedsBadReputation = -3;
// or a flag of the city record: the market's Leihhaus (1893:00FD)
static const int kNeedsPawnshop		= -4;
// or the price of a night at the inn
static const int kNeedsInnPrice		= -5;
// or a letter of credit to redeem, or two florins to buy one with
static const int kNeedsBankNotes	= -6;
static const int kNeedsFlorins		= -7;
// or the physician: in the city (small towns may have none), wounds to
// treat, a treatment offered
static const int kNeedsPhysician	= -8;
static const int kNeedsWounded		= -9;
static const int kNeedsTreatment	= -10;

// The game's day for some places (1367:072A): hour 5 to 18; the extra
// hour to reach a guild then (file 0xA47A5)
static bool
IsGameDay(const GameTime& clock)
{
    return clock.Hour() >= 5 && clock.Hour() <= 18;
}

// Special waiting times
static const int kUntilNight		= -1;	// "wait until nightfall"
static const int kUntilMorning		= -2;	// "camp here until morning"
static const int kAnHourMoreAtNight	= -3;	// an hour, two outside the
                                            // game's day

struct option_rule {
    int action;
    int target;					// screen, for ACTION_GO; merchant, for
                                // ACTION_TRADE
    int needs;					// kAlways, a city_place or kNeedsHarbor
    int minutes;				// game time the option takes
    int then;					// ACTION_TRADE: the screen after the
                                // trade, 0: the same
};

static const int kMaxOptions = 12;

struct screen_rules {
    const char* deck;			// NULL: see kNightScreens
    int card;
    const char* scene;			// picture shown first, or NULL
    option_rule options[kMaxOptions];	// in card order; the rest: not implemented
};

#define GO(screen)			{ ACTION_GO, CityVisit::screen, kAlways, 0 }
#define GO_IF(screen, needs) { ACTION_GO, CityVisit::screen, needs, 0 }
#define WAIT(screen, minutes) { ACTION_GO, CityVisit::screen, kAlways, minutes }
#define LEAVE				{ ACTION_LEAVE, 0, kAlways, 0 }
#define TODO				{ ACTION_NOT_IMPLEMENTED, 0, kAlways, 0 }
#define TODO_IF(needs)		{ ACTION_NOT_IMPLEMENTED, 0, needs, 0 }
#define HIDE				{ ACTION_HIDE, 0, kAlways, 0 }
#define TRADE(merchant)		{ ACTION_TRADE, merchant, kAlways, 0 }
#define TRADE_THEN(merchant, minutes, screen) \
    { ACTION_TRADE, merchant, kAlways, minutes, CityVisit::screen }
#define DO(action)			{ action, 0, kAlways, 0 }
#define DO_IF(action, needs) { action, 0, needs, 0 }

// The guilds' shops by day and by night: trading takes an hour, then
// the guild's card again (DARKLAND.EXE, e.g. file 0xCED6A, 0xD0833)
#define SHOP_OPTIONS(merchant, back) { \
        { ACTION_TRADE, merchant, kAlways, 60 },	/* buy and sell goods */ \
        TODO, TODO,							/* politics, the masters */ \
        HIDE, HIDE, HIDE,					/* the leader's secret */ \
        GO(back)							/* leave */ \
    }
#define NIGHT_SHOP_OPTIONS(merchant, back) { \
        { ACTION_TRADE, merchant, kAlways, 60 },	/* awaken somebody */ \
        TODO,								/* awaken the guild leader */ \
        HIDE, HIDE, HIDE,					/* the leader's home, saboteurs */ \
        TODO, TODO, TODO, TODO,				/* placeholders */ \
        GO(back)							/* leave */ \
    }

// The option lists are those of the cards (see `darklands --messages`);
// options whose text is a placeholder ("5", "...this option should be
// hidden") are hidden by CardView. The docks need a harbor (inferred:
// DARKLAND.CTY only knows sea ports). The scene pictures other than
// MAIN-ST.PIC are chosen by what they show (inferred).
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
    // (DARKLAND.EXE, file 0xA6B5E; a wanted party gets SCREEN_UNWELCOME)
    { "URBAN00", 0, NULL, {
        TODO,								// local news and rumors
        DO_IF(ACTION_SLEEP, kNeedsInnPrice),	// a meal and sleep for $Money1
        DO(ACTION_RESIDENCE),				// take up residence
        DO(ACTION_STABLES),
        TODO,								// store items
        HIDE,								// recover them: none stored
        TODO,								// the party's composition
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Looking down the main street of $PlaceName, you set off toward..."
    { "MAINS01", 0, "MAIN-ST.PIC", {
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// craft guilds and side alleys
        GO_IF(SCREEN_INN, CITY_INN),
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_GROVE),
        GO(SCREEN_GATE)
    } },
    // "The side streets of $PlaceName are full of people..."
    { "SIDES00", 0, "XSIDE.PIC", {
        GO(SCREEN_MAIN_STREET),
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// crafts district, inns...
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        GO(SCREEN_GROVE),
        GO(SCREEN_OTHER),
        TODO								// the city walls
    } },
    // "The gate is heavily guarded..."
    { "SELEC00", 0, NULL, {
        LEAVE,								// simply walk out
        TODO, TODO, TODO, TODO, TODO,		// hide, potion, saint, fight, wall
        GO(SCREEN_MAIN_STREET)				// not leave just yet
    } },
    // "Storing your gear, you eat a hearty meal, then take eight hours
    // of well-deserved sleep." (the game lets nine hours pass)
    { "URBAN00", 2, NULL, { WAIT(SCREEN_INN, 9 * 60) } },
    // "The $citySquare, the main city square of $PlaceName..."
    { "CITYS00", 0, "XTOWN.PIC", {
        TODO,								// notices and gossip
        GO_IF(SCREEN_TOWN_HALL, CITY_TOWN_HALL),
        TODO,								// the prison
        GO_IF(SCREEN_BARRACKS, CITY_ARMORY),
        GO_IF(SCREEN_UNIVERSITY, CITY_UNIVERSITY),
        GO(SCREEN_CHURCHES),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Looming overhead are the great battlements of the $fortress..."
    { "CITYF00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// audience, clerk, saint, dungeon
        TODO, TODO, TODO,					// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $marketplace... is the bustling center of all business"
    // (DARKLAND.EXE, segment 1893: the merchants open the trade screen
    // directly; the game may first offer a quest, or bring guards on a
    // wanted party, and draws an event after trading: not reproduced)
    { "MARKE00", 0, NULL, {
        TRADE(MERCHANT_GOODS),				// everyday items
        TRADE(MERCHANT_FOREIGN),			// the foreign traders
        TRADE(MERCHANT_HERBALIST),			// the pharmacists' stalls
        // Fugger, Medici, Hanse: the game offers them where the location
        // record's bytes +0x15, +0x16, +0x17 are not 0, which they never
        // are in the game data
        GO(SCREEN_FUGGER),
        GO(SCREEN_MEDICI),
        GO(SCREEN_HANSE),
        { ACTION_TRADE, MERCHANT_PAWNSHOP, kNeedsPawnshop, 0 },
        HIDE,								// placeholder
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The tall spires of a gothic church arrow into the sky..."
    { "CHURC00", 0, "XCHURCH.PIC", {
        GO_IF(SCREEN_CATHEDRAL, CITY_CATHEDRAL),
        GO_IF(SCREEN_CHURCH, CITY_CHURCH),
        TODO,								// placeholder
        GO_IF(SCREEN_MONASTERY, CITY_MONASTERY),
        GO_IF(SCREEN_UNIVERSITY, CITY_UNIVERSITY),
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Gargoyles leer overhead as you approach the famed $cathedral."
    { "CATHE00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO, TODO, TODO,	// mass, priest, donate...
        GO(SCREEN_CHURCHES),				// leave the cathedral
        HIDE								// a relic as a quest reward
    } },
    // "You come to the $cityChurch, the church of $PlaceName."
    { "CITYC00", 0, NULL, {
        DO(ACTION_MASS),
        DO(ACTION_CONFESSION),
        TODO,								// talk to a priest
        DO_IF(ACTION_DONATION, kNeedsDonation),	// give $Money1
        TODO_IF(kNeedsBadReputation),		// seek sanctuary
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Now you are before the city's monastery."
    { "MONAS00", 0, NULL, {
        TODO, TODO,							// study, prayers
        TODO, TODO, TODO, TODO,				// placeholders
        GO(SCREEN_CHURCHES)					// leave the monastery
    } },
    // "At the $university... the snobbish staff prefers to speak Latin"
    { "UNIVE00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// saints, formulae, stone...
        TODO, TODO, TODO,					// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The entrance... of the $councilHall for $PlaceName is well guarded."
    { "COUNC00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO, TODO,	// audience, clerk, dungeon...
        TODO, TODO,							// placeholders
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $cityBarracks is the armory of $PlaceName..."
    { "CITYB00", 0, NULL, {
        TODO, TODO,							// training, recruits
        HIDE,								// ask $NamedOneName to join
        TODO, TODO,							// placeholders
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Navigating through the narrow streets, you seek..."
    { "BUSIN00", 0, NULL, {
        GO(SCREEN_ARMS_CRAFTS),
        GO(SCREEN_CRAFTS),
        GO_IF(SCREEN_INN, CITY_INN),
        TODO,								// a physician
        GO(SCREEN_GROVE),
        GO_IF(SCREEN_SLUM, CITY_SLUMS),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET),
        TODO,								// a piece of city wall
        GO(SCREEN_GATE)
    } },
    // "...the picture signs that proclaim $PlaceName's guilds and crafts"
    // (DARKLAND.EXE, file 0xA42D5: the jewelers are never offered by
    // day; the physician and the alchemist are missing from some small
    // towns, by a rule on the game's year; going to a guild takes an
    // hour)
    { "CIVCR00", 0, NULL, {
        { ACTION_GO, CityVisit::SCREEN_PHYSICIAN, kNeedsPhysician, 60 },
        TODO,								// astrologists
        HIDE,								// jewelers
        WAIT(SCREEN_ARTIFICER, kAnHourMoreAtNight),	// tinkers
        WAIT(SCREEN_CLOTHMAKER, kAnHourMoreAtNight),
        HIDE, HIDE,							// placeholders
        GO(SCREEN_ARMS_CRAFTS),
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_MAIN_STREET)
    } },
    // "...signs with pictures portray the various guilds and crafts."
    { "MILCR00", 0, NULL, {
        TODO,								// Soldier's Road
        GO_IF(SCREEN_BLACKSMITH, kNeedsShop + SHOP_BLACKSMITH),
        GO_IF(SCREEN_SWORDSMITH, kNeedsShop + SHOP_SWORDSMITH),
        GO_IF(SCREEN_ARMORER, kNeedsShop + SHOP_ARMORER),
        GO_IF(SCREEN_BOWYER, kNeedsShop + SHOP_BOWYER),	// and gunsmiths
        TODO,								// placeholder
        GO(SCREEN_OTHER),					// a specific building
        GO(SCREEN_CRAFTS),
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_MAIN_STREET)
    } },
    // "You pause in a small grove of trees..."
    { "CITYG05", 0, "XGROVE1.PIC", {
        WAIT(SCREEN_GROVE, 60),				// an hour
        WAIT(SCREEN_GROVE, 3 * 60),			// a bell
        WAIT(SCREEN_GROVE, kUntilNight),
        TODO, TODO, TODO, TODO, TODO,		// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $slum of $PlaceName is full of paupers, drifters, thieves..."
    { "SLUMD00", 0, NULL, {
        WAIT(SCREEN_SLUM, 60),				// rest for an hour
        TODO,								// listen to the rumors
        TODO, TODO, TODO, TODO, TODO, TODO,	// placeholders
        TODO,								// live very cheaply
        GO(SCREEN_SIDE_STREET)
    } },
    // "The craft tied to the piers and wharves have many destinations."
    { "DOCKS00", 0, "DOCKDAY.PIC", {
        HIDE, HIDE, HIDE, HIDE, HIDE,		// boats: the destinations and
                                            // fares come from the game
        TODO,								// placeholder
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Trudging along back streets and alleys, you head for..."
    { "OTHER00", 0, NULL, {
        HIDE, HIDE,							// the homes of people you met
        TODO, TODO, TODO, TODO, TODO, TODO,	// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The sounds of hammers ringing on steel... fill Swordsmith's Lane."
    // The shops of the four arms-making guilds have the same options;
    // the guild politics and the leader's secrets belong to quests.
    { "SWORD00", 0, NULL, SHOP_OPTIONS(MERCHANT_SWORDSMITH, SCREEN_ARMS_CRAFTS) },
    { "BLACK00", 0, NULL, SHOP_OPTIONS(MERCHANT_BLACKSMITH, SCREEN_ARMS_CRAFTS) },
    { "ARMOR00", 0, NULL, SHOP_OPTIONS(MERCHANT_ARMORER, SCREEN_ARMS_CRAFTS) },
    { "BOWYE00", 0, NULL, SHOP_OPTIONS(MERCHANT_BOWYER, SCREEN_ARMS_CRAFTS) },
    // "Tinkers' Square..."; the clothmakers' guild
    { "ARTIF00", 0, NULL, SHOP_OPTIONS(MERCHANT_ARTIFICER, SCREEN_CRAFTS) },
    { "CLOTH00", 0, NULL, SHOP_OPTIONS(MERCHANT_CLOTHMAKER, SCREEN_CRAFTS) },
    // The church's results: "the Mass is sung", "the next Mass will be
    // at $NamedOneName", confession, the priest's thanks for small,
    // middling and large donations
    { "CITYC00", 2, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 4, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 3, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 5, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 6, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 7, NULL, { GO(SCREEN_CHURCH) } },
    // The church at night: "Finally, the Mass is sung", "the next Mass
    // will not be sung until $NamedOneName", the altar boy's answer
    { "CITYC01", 2, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC01", 1, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC01", 3, NULL, { GO(SCREEN_CHURCH) } },
    // "...the innkeeper carefully bows. 'Sirs, most regrettably, I fear
    // that we have no room.'" The game offers neither the meal nor the
    // room, nor the storage.
    { "URBAN00", 3, NULL, {
        TODO,								// talk, daring the guards
        HIDE, HIDE,							// eat and rest, a room
        DO(ACTION_STABLES),
        HIDE, HIDE,							// store, recover items
        TODO,								// the party's composition
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // The stablemaster's horses and mules: an hour, then the trade
    { "URBAN00", 1, NULL, { TRADE_THEN(MERCHANT_STABLES, 60, SCREEN_INN) } },
    // the same, "whether any of your mounts are for sale"
    { "URBAN00", 7, NULL, { TRADE_THEN(MERCHANT_STABLES, 60, SCREEN_INN) } },
    // The banks (DARKLAND.EXE, file 0xC41E7 and 0xC6253): letters of
    // credit; the tasks, rewards and politics are not implemented
#define BANK_OPTIONS(redeemed, deposit, tasks) { \
        { ACTION_REDEEM, CityVisit::redeemed, kNeedsBankNotes, 0 }, \
        GO_IF(deposit, kNeedsFlorins),		/* a letter of credit */ \
        tasks,								/* special tasks */ \
        HIDE,								/* a reward */ \
        TODO,								/* politics */ \
        HIDE,								/* "unused" */ \
        GO(SCREEN_MARKET), \
        GO(SCREEN_SIDE_STREET) \
    }
    { "FUGGE00", 0, NULL, BANK_OPTIONS(SCREEN_FUGGER_REDEEMED,
        SCREEN_FUGGER_DEPOSIT, TODO) },
    { "MEDIC00", 0, NULL, BANK_OPTIONS(SCREEN_MEDICI_REDEEMED,
        SCREEN_MEDICI_DEPOSIT, TODO) },
    // "In the rich, wood-paneled offices of the Hanseatic League..."
    { "HANSE00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// tasks, rewards, politics
        HIDE, HIDE,							// placeholders
        TODO,								// chat with the clerks
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_MARKET)					// the main door (state 0x15)
    } },
    // "...the guards grip their weapons and watch you carefully": the
    // same, with no tasks (a reputation under 0)
    { "FUGGE00", 2, NULL, BANK_OPTIONS(SCREEN_FUGGER_REDEEMED,
        SCREEN_FUGGER_DEPOSIT, HIDE) },
    { "MEDIC00", 2, NULL, BANK_OPTIONS(SCREEN_MEDICI_REDEEMED,
        SCREEN_MEDICI_DEPOSIT, HIDE) },
    // "...counts out from the purse the full amount, $Money1. Then he
    // deducts $Money2 from the pile."
    { "FUGGE00", 6, NULL, { GO(SCREEN_FUGGER) } },
    { "MEDIC00", 6, NULL, { GO(SCREEN_MEDICI) } },
    // "You pool your resources and give the clerk enough coins for a note
    // worth..." (then "Deposit how many Florins?")
    { "FUGGE00", 3, NULL, { { ACTION_DEPOSIT, CityVisit::SCREEN_FUGGER, kAlways, 0 } } },
    { "MEDIC00", 3, NULL, { { ACTION_DEPOSIT, CityVisit::SCREEN_MEDICI, kAlways, 0 } } },
#undef BANK_OPTIONS
    // "Among various guilds and merchant townhouses, you find the home of
    // $NamedOneName, a respected physician..." (file 0xA2E6A)
    { "PHYSI00", 0, NULL, {
        DO(ACTION_DISCUSS_TREATMENTS),		// try to determine his skill
        DO_IF(ACTION_ASK_AID, kNeedsWounded),	// his aid in healing wounds
        DO(ACTION_STUDENTS),				// be his students
        DO(ACTION_COMPONENTS),				// alchemical components
        DO_IF(ACTION_TREATMENT, kNeedsTreatment),	// pay $Money1
        GO(SCREEN_CRAFTS),					// leave
        HIDE								// (night only)
    } },
    // "...a chamberpot's load of offal" (a reputation of -40 or less);
    // then the district (state 0x14, $BUSIN00)
    { "PHYSI00", 3, NULL, { GO(SCREEN_DISTRICT) } },
    // "...since $Number1 of you suffer, the overall cost will be $Money1"
    { "PHYSI00", 2, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...decides that $NamedOneName has $Text1 skill"
    { "PHYSI00", 8, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...unable to yet determine the competence of this person"
    { "PHYSI00", 9, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...$NamedOneName is a complete idiot" (and the party leaves)
    { "PHYSI00", 10, NULL, { GO(SCREEN_DISTRICT) } },
    // "I have no need for additional medicines"
    { "PHYSI00", 12, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...the physician uses leeches to draw out the vile humors"
    { "PHYSI00", 13, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...your purse lacks enough money for everyone"
    { "PHYSI00", 14, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...I will take some of you as students for $Money1 daily"
    { "PHYSI00", 4, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "I already have $Number1 apprentices"
    { "PHYSI00", 5, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "Your skills in healing match my own"
    { "PHYSI00", 6, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "I am unable to take any students"
    { "PHYSI00", 11, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "Among the dark townhouses... he peers at you through a crack in
    // the door" (outside the game's day, file 0xA2EDB)
    { "PHYSI00", 1, NULL, {
        HIDE,
        DO_IF(ACTION_ASK_AID, kNeedsWounded),
        HIDE,
        DO(ACTION_COMPONENTS),
        DO_IF(ACTION_TREATMENT, kNeedsTreatment),
        DO(ACTION_LEAVE_PHYSICIAN),			// apologize and leave
        DO(ACTION_APOLOGIZE)				// and give him two groschen
    } },
    // "As you walk away, the physician loudly curses you."
    { "PHYSI00", 7, NULL, { GO(SCREEN_CRAFTS) } },
    // not a game card: see the constructor
    { NULL, 0, NULL, {
        TODO								// go back (handled by Choose())
    } }
};

// At night (see GameTime::IsNight()) these screens show other cards;
// a NULL deck: the same as by day
static const screen_rules kNightScreens[CityVisit::SCREEN_COUNT] = {
    { NULL, 0, NULL, {} },					// start
    { NULL, 0, NULL, {} },					// outside
    // "Mellow lanterns and a warm fire make the $Inn..." (file 0xA7545)
    { "URBAN01", 0, NULL, {
        TODO,								// local news and rumors
        DO_IF(ACTION_SLEEP, kNeedsInnPrice),
        DO(ACTION_RESIDENCE),				// take up residence
        DO(ACTION_STABLES),
        TODO,								// store items
        HIDE,								// recover them
        TODO,								// the party's composition
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Darkness covers the main street of $PlaceName..." (same options)
    { "MAINS02", 0, "XNMAIN.PIC", {
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// crafts district
        GO_IF(SCREEN_INN, CITY_INN),
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_GROVE),					// a small grove
        GO(SCREEN_GATE)
    } },
    // "Tiny gleams from occasional windows..." (another order)
    { "SIDES01", 0, NULL, {
        GO(SCREEN_MAIN_STREET),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// crafts district, inns...
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        GO(SCREEN_GROVE),					// a dark grove
        GO(SCREEN_OTHER),
        TODO								// the city wall
    } },
    // "The gate is closed for the night..." The game replaces options
    // that do not apply with lines like "1 not available"
    { "SELEC00", 13, NULL, {
        TODO,								// talk the guards into it
        HIDE,								// "1 not available"
        HIDE,								// "2 alc not available"
        TODO,								// call upon a saint
        HIDE,								// "4 combat not available"
        TODO,								// see how well the walls are guarded
        GO(SCREEN_MAIN_STREET)				// not leave the city just yet
    } },
    { "URBAN01", 2, NULL, { WAIT(SCREEN_INN, 9 * 60) } },	// sleep
    // "Amid the dark shadows of the city square..."
    { "CITYS01", 0, NULL, {
        TODO,								// read the notices
        GO_IF(SCREEN_TOWN_HALL, CITY_TOWN_HALL),
        TODO,								// the prison
        TODO_IF(CITY_ARMORY),				// the barracks: no night card
        GO_IF(SCREEN_UNIVERSITY, CITY_UNIVERSITY),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Flickering torchlight highlights the stone walls of the $fortress"
    { "CITYF01", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO, TODO,	// bribes, dungeon...
        TODO, TODO,							// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $marketplace... is almost empty at night."
    { "MARKE01", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// sneak, bribe, potion, saint,
                                            // attack
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Gothic spires are black spikes in the night sky."
    { "CHURC01", 0, "XNCHRCH.PIC", {
        GO_IF(SCREEN_CATHEDRAL, CITY_CATHEDRAL),
        GO_IF(SCREEN_CHURCH, CITY_CHURCH),
        TODO,								// the $hospital: no place slot
        GO_IF(SCREEN_MONASTERY, CITY_MONASTERY),
        GO_IF(SCREEN_UNIVERSITY, CITY_UNIVERSITY),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "...votive candles cast the only light"
    { "CATHE01", 0, NULL, {
        TODO, TODO, TODO, TODO,				// mass, priest, relic, sanctuary
        GO(SCREEN_CHURCHES),				// leave the cathedral
        HIDE								// a relic as a quest reward
    } },
    // "You come to $cityChurch... It is dark."
    // (DARKLAND.EXE, file 0xB9249)
    { "CITYC01", 0, NULL, {
        DO(ACTION_MASS),
        DO(ACTION_ALTAR_BOY),
        TODO_IF(kNeedsBadReputation),		// seek sanctuary
        GO(SCREEN_CHURCHES)					// leave the church
    } },
    // "It is dark at the monastery."
    { "MONAS01", 0, "XNMONK.PIC", {
        TODO,								// prayers
        TODO, TODO, TODO, TODO,				// placeholders
        GO(SCREEN_CHURCHES)					// leave the monastery
    } },
    { NULL, 0, NULL, {} },					// university
    // "The $councilHall doors are locked..."
    { "COUNC01", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO, TODO,	// bribes, dungeon...
        TODO,								// placeholder
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    { NULL, 0, NULL, {} },					// barracks: not reached at night
    // BUSIN00 has no night card: the day one, without the slum (its night
    // card is a stub, "This is the slum at night. It isn't done yet.")
    { "BUSIN00", 0, NULL, {
        GO(SCREEN_ARMS_CRAFTS),
        GO(SCREEN_CRAFTS),
        GO_IF(SCREEN_INN, CITY_INN),
        TODO,								// a physician
        GO(SCREEN_GROVE),
        TODO_IF(CITY_SLUMS),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET),
        TODO,								// a piece of city wall
        GO(SCREEN_GATE)
    } },
    // (the game shows the crafts' day card at night too: $CIVCR00 card 1
    // and $CIVCR01 are not used)
    { NULL, 0, NULL, {} },
    // "Walking along the dark streets, you peer down each one..."
    { "MILCR00", 1, NULL, {
        TODO,
        GO_IF(SCREEN_BLACKSMITH, kNeedsShop + SHOP_BLACKSMITH),
        GO_IF(SCREEN_SWORDSMITH, kNeedsShop + SHOP_SWORDSMITH),
        GO_IF(SCREEN_ARMORER, kNeedsShop + SHOP_ARMORER),
        GO_IF(SCREEN_BOWYER, kNeedsShop + SHOP_BOWYER),
        TODO,
        GO(SCREEN_OTHER),
        GO(SCREEN_CRAFTS),
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_MAIN_STREET)
    } },
    // "The moonlight filters down through a quiet stand of trees."
    { "CITYG06", 0, "XGROVE27.PIC", {
        WAIT(SCREEN_GROVE, 60),				// an hour
        WAIT(SCREEN_GROVE, 3 * 60),			// a bell
        WAIT(SCREEN_GROVE, kUntilMorning),	// camp until morning
        TODO, TODO, TODO, TODO, TODO,		// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    { NULL, 0, NULL, {} },					// slum: not reached at night
    // "Some activity still proceeds on the docks of $PlaceName..."
    { "DOCKS01", 0, "XDOCK.PIC", {
        TODO,								// which boats are sailing
        TODO, TODO,							// escape by boat
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    { NULL, 0, NULL, {} },					// other locations
    // "The swordsmiths' courtyards are silent..."
    { "SWORD01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_SWORDSMITH, SCREEN_ARMS_CRAFTS) },
    { "BLACK01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_BLACKSMITH, SCREEN_ARMS_CRAFTS) },
    { "ARMOR01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_ARMORER, SCREEN_ARMS_CRAFTS) },
    { "BOWYE01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_BOWYER, SCREEN_ARMS_CRAFTS) },
    { "ARTIF01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_ARTIFICER, SCREEN_CRAFTS) },
    { "CLOTH01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_CLOTHMAKER, SCREEN_CRAFTS) },
    { NULL, 0, NULL, {} },					// the church's results
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },					// the church's night results
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { "URBAN01", 3, NULL, {					// "no rooms available"
        TODO,
        HIDE, HIDE,
        DO(ACTION_STABLES),
        HIDE, HIDE,
        TODO,
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The stableboy assures you..." (no trade at night)
    { "URBAN01", 1, NULL, { GO(SCREEN_INN) } },
    { NULL, 0, NULL, {} },					// stables, sale: not at night
    { NULL, 0, NULL, {} },					// the physician
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },					// the banks and the League
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} }					// not implemented
};

#undef GO
#undef GO_IF
#undef WAIT
#undef LEAVE
#undef TODO
#undef TODO_IF
#undef HIDE
#undef TRADE
#undef TRADE_THEN
#undef DO
#undef DO_IF
#undef SHOP_OPTIONS
#undef NIGHT_SHOP_OPTIONS


// The game minutes of a rule: its own, or until night or morning.
static uint32
MinutesFor(const option_rule& rule, const GameTime& clock)
{
    if (rule.minutes >= 0)
        return uint32(rule.minutes);
    if (rule.minutes == kAnHourMoreAtNight)
        return IsGameDay(clock) ? 60 : 120;
    const int now = clock.Hour() * 60 + clock.Minute();
    const int target = (rule.minutes == kUntilNight
        ? GameTime::kNightStart : GameTime::kNightEnd) * 60;
    return uint32((target - now + 24 * 60) % (24 * 60));
}


static const screen_rules&
RulesFor(int screen, bool night)
{
    if (night && kNightScreens[screen].deck != NULL)
        return kNightScreens[screen];
    return kScreens[screen];
}


static const option_rule&
RuleFor(const screen_rules& rules, int option)
{
    static const option_rule kNotImplemented = { ACTION_NOT_IMPLEMENTED, 0,
        kAlways, 0 };
    if (option < 0 || option >= kMaxOptions
            || rules.options[option].action == ACTION_UNLISTED)
        return kNotImplemented;
    return rules.options[option];
}


// #pragma mark - CityVisit


CityVisit::CityVisit(GameData& data)
    :
    fData(data),
    fView(data),
    fTrade(data),
    fPendingTrade(-1),
    fParty(NULL),
    fClock(NULL),
    fInfo(NULL),
    fReputations(NULL),
    fNight(false),
    fCity(-1),
    fScreen(SCREEN_START),
    fPreviousScreen(SCREEN_START),
    fRandom(std::random_device()()),
    fSeed(0),
    fResidence(data),
    fPendingResidence(false),
    fTreatmentOffered(false)
{
    // every screen has a day card (a miscounted table would leave some
    // zero-filled), and the decks load: missing files show up right away
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if ((kScreens[i].deck == NULL) != (i == SCREEN_NOT_IMPLEMENTED))
            throw std::logic_error("CityVisit: screen table out of order");
    }
    for (const screen_rules* table : { kScreens, kNightScreens }) {
        for (int i = 0; i < SCREEN_COUNT; i++) {
            if (table[i].deck != NULL)
                fData.Messages(table[i].deck).CardAt(uint32(table[i].card));
        }
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


void
CityVisit::SetParty(party* members)
{
    fParty = members;
    fView.SetParty(members);
    fTrade.SetParty(members);
    fResidence.SetParty(members);
}


void
CityVisit::SetInfoView(InfoView* info)
{
    fInfo = info;
    fResidence.SetInfoView(info);
    fView.SetInfoView(info);
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
        if (fPendingTrade >= 0) {
            const int reputation = fReputations != NULL
                && fCity < int(fReputations->size()) ? (*fReputations)[fCity] : 0;
            fTrade.SetPlace(fCity, reputation);
            // the same stock all day long
            const uint32 day = fClock != NULL ? uint32(fClock->Year()) * 400
                + fClock->Month() * 32 + fClock->Day() : 0;
            fTrade.SetMerchant(merchant_kind(fPendingTrade),
                uint32(fCity) * 7919 + uint32(fPendingTrade) * 104729 + day);
            fTrade.Run(window);
            fPendingTrade = -1;
            _Show(fScreen, false);
        }
        if (fPendingResidence) {
            // DARKLAND.EXE, file 0xA709A: the residence, then the inn;
            // a day costs the inn's price
            std::map<int, city_tutor>::const_iterator tutor
                = fTutors.find(fCity);
            fResidence.SetClock(fClock);
            fResidence.SetPlace(fCity, _Reputation(), InnPrice(),
                tutor != fTutors.end() ? &tutor->second : NULL);
            fResidence.Run(window);
            fPendingResidence = false;
            _Show(SCREEN_INN, false);
        }
    }
}


void
CityVisit::Enter(int cityIndex, int screen)
{
    if (screen < 0 || screen >= SCREEN_COUNT)
        throw std::out_of_range("CityVisit::Enter(): invalid screen");
    fCity = cityIndex;
    if (fInfo != NULL) {
        const city& c = fData.Cities().CityAt(uint32(cityIndex));
        fInfo->SetPosition(map_position{ c.x, c.y });
    }
    fVariables.clear();
    AddCityVariables(fData, cityIndex, fVariables);
    if (fParty != NULL)
        AddPartyVariables(*fParty, fVariables);
    fPreviousScreen = screen;
    _Show(screen);
}


bool
CityVisit::Choose(int option)
{
    fPendingTrade = -1;
    fPendingResidence = false;
    if (fScreen == SCREEN_NOT_IMPLEMENTED) {
        _Show(fPreviousScreen, false);
        return true;
    }
    const option_rule& rule = RuleFor(RulesFor(fScreen, fNight), option);
    switch (rule.action) {
        case ACTION_GO:
            if (fClock != NULL)
                fClock->AddMinutes(MinutesFor(rule, *fClock));
            _Show(rule.target);
            return true;
        case ACTION_LEAVE:
            return false;
        case ACTION_TRADE:
            if (fClock != NULL)
                fClock->AddMinutes(MinutesFor(rule, *fClock));
            fPendingTrade = rule.target;
            if (rule.then != 0)
                fScreen = rule.then;	// shown after the trade
            return true;
        case ACTION_SLEEP:
            _Show(_Sleep());
            return true;
        case ACTION_STABLES:
            _Show(_Stables());
            return true;
        case ACTION_RESIDENCE:
            fPendingResidence = true;
            return true;
        case ACTION_REDEEM:
            _Show(_Redeem(rule.target));
            return true;
        case ACTION_DEPOSIT:
            _Deposit();
            _Show(rule.target);
            return true;
        case ACTION_DISCUSS_TREATMENTS:
            _Show(_DiscussTreatments());
            return true;
        case ACTION_ASK_AID:
            _Show(_AskAid());
            return true;
        case ACTION_COMPONENTS:
            _Show(_Components());
            return true;
        case ACTION_TREATMENT:
            _Show(_Treatment());
            return true;
        case ACTION_STUDENTS:
            _Show(_Students());
            return true;
        case ACTION_LEAVE_PHYSICIAN:
            _Show(_LeavePhysician(false));
            return true;
        case ACTION_APOLOGIZE:
            _Show(_LeavePhysician(true));
            return true;
        case ACTION_MASS:
            _Show(_Mass());
            return true;
        case ACTION_CONFESSION:
            _Show(_Confession());
            return true;
        case ACTION_DONATION:
            _Show(_Donation());
            return true;
        case ACTION_ALTAR_BOY:
            _Show(_AltarBoy());
            return true;
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
CityVisit::AddPartyVariables(const party& members, card_variables& variables)
{
    if (members.members.empty())
        return;
    static const char* kOrdinals[kMaxPartySize] = {
        "One", "Two", "Three", "Four", "Five"
    };
    // who the game "chooses" for a scene is unknown: the leader first
    std::vector<const character*> chosen;
    chosen.push_back(&members.members[members.leader]);
    for (size_t i = 0; i < members.members.size(); i++) {
        if (int(i) != members.leader)
            chosen.push_back(&members.members[i]);
    }
    for (size_t i = 0; i < chosen.size() && i < size_t(kMaxPartySize); i++)
        variables[std::string("Chosen") + kOrdinals[i] + "Name"] = chosen[i]->shortName;
    variables["LeaderName"] = chosen[0]->shortName;

    const bool female = chosen[0]->female;
    variables["he"] = female ? "she" : "he";
    variables["He"] = female ? "She" : "He";
    variables["his"] = female ? "her" : "his";
    variables["His"] = female ? "Her" : "His";
    variables["him"] = female ? "her" : "him";
    variables["himself"] = female ? "herself" : "himself";
}


void
CityVisit::_Show(int screen, bool withScene)
{
    fScreen = screen;
    fNight = fClock != NULL && fClock->IsNight();
    if (fClock != NULL) {
        fVariables["CurrentBell"] = fClock->BellName();
        fVariables["MonthName"] = fClock->MonthName();
    }
    // a wanted party is not welcome at the inn (DARKLAND.EXE: a
    // reputation of -40 or less)
    if (screen == SCREEN_INN && _Reputation() <= -40)
        screen = SCREEN_UNWELCOME;
    // the physician shuts his door to a wanted party
    if (screen == SCREEN_PHYSICIAN && _Reputation() <= -40)
        screen = SCREEN_PHYSICIAN_SHUT;
    else if (screen == SCREEN_PHYSICIAN && fClock != NULL
            && !IsGameDay(*fClock))
        screen = SCREEN_PHYSICIAN_NIGHT;
    if (screen == SCREEN_PHYSICIAN || screen == SCREEN_PHYSICIAN_SHUT
            || screen == SCREEN_PHYSICIAN_NIGHT) {
        if (fScreen == SCREEN_CRAFTS)	// a new visit
            fTreatmentOffered = false;
        _PhysicianSkill();
        // his name: the city's property 0x21 + 800
        fVariables["NamedOneName"] = _PersonName(uint16(_PeopleSeed() + 0x320));
        if (fParty != NULL && !fParty->members.empty())
            fVariables["ChosenOneName"]
                = fParty->members[_BestHealer()].shortName;
    }
    // the master banker and the League's master: the city's number + 8
    // (the Fuggers, file 0xC4280), + 6 (the Medici, file 0xC62E9), + 7
    // (the Hanse, file 0xC7BBF)
    if (screen >= SCREEN_FUGGER && screen <= SCREEN_MEDICI_DEPOSIT) {
        const bool fugger = screen == SCREEN_FUGGER
            || screen == SCREEN_FUGGER_COLD || screen == SCREEN_FUGGER_REDEEMED
            || screen == SCREEN_FUGGER_DEPOSIT;
        const uint16 number = fData.Cities().CityAt(uint32(fCity)).peopleSeed;
        fVariables["NamedOneName"] = _PersonName(uint16(number
            + (fugger ? 8 : screen == SCREEN_HANSE ? 7 : 6)));
    }
    // the banks are cold to a party with a bad local reputation
    if (screen == SCREEN_FUGGER && _Reputation() < 0)
        screen = SCREEN_FUGGER_COLD;
    else if (screen == SCREEN_MEDICI && _Reputation() < 0)
        screen = SCREEN_MEDICI_COLD;
    fScreen = screen;
    if (screen == SCREEN_INN)
        fVariables["Money1"] = MoneyText(InnPrice());
    else if (fParty != NULL && screen == SCREEN_CHURCH)	// the donation
        fVariables["Money1"] = MoneyText(TotalPfennigs(fParty->cash) / 10);
    else if (fParty != NULL && (screen == SCREEN_FUGGER
            || screen == SCREEN_MEDICI || screen == SCREEN_FUGGER_COLD
            || screen == SCREEN_MEDICI_COLD))	// the letter of credit
        fVariables["Money1"] = MoneyText(uint32(fParty->bankNotes) * 240);
    const screen_rules& rules = RulesFor(screen, fNight);
    if (rules.deck == NULL) {
        fView.SetCard(fNotImplementedCard, fVariables);
        fView.SetScene("");
        return;
    }
    fView.SetCard(fData.Messages(rules.deck).CardAt(uint32(rules.card)),
        fVariables, _HiddenOptions(screen));
    if ((screen == SCREEN_FUGGER_DEPOSIT || screen == SCREEN_MEDICI_DEPOSIT)
            && fParty != NULL) {
        // DARKLAND.EXE: the purse's florins to start with, 10 digits
        fView.SetPrompt("Deposit how many Florins?",
            std::to_string(fParty->cash.florins), 10);
    }
    fView.SetScene(rules.scene != NULL ? rules.scene : "", withScene);
}


std::vector<int>
CityVisit::_HiddenOptions(int screen) const
{
    const city& c = fData.Cities().CityAt(uint32(fCity));
    std::vector<int> hidden;
    const screen_rules& rules = RulesFor(screen, fNight);
    for (int i = 0; i < kMaxOptions; i++) {
        const option_rule& rule = RuleFor(rules, i);
        bool hide = rule.action == ACTION_HIDE;
        if (rule.needs == kNeedsDonation)
            hide = fParty == NULL || TotalPfennigs(fParty->cash) / 10 < 10;
        else if (rule.needs == kNeedsBadReputation)
            hide = _Reputation() > -10;
        else if (rule.needs == kNeedsInnPrice)
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < InnPrice();
        else if (rule.needs == kNeedsBankNotes)
            hide = fParty == NULL || fParty->bankNotes == 0;
        else if (rule.needs == kNeedsFlorins)
            hide = fParty == NULL || fParty->cash.florins < 2;
        else if (rule.needs == kNeedsPhysician) {
            // DARKLAND.EXE, file 0xA433D: in towns of size 3 or less, by
            // the city's number and the year
            // (a signed 16-bit remainder)
            hide = c.size <= 3 && int16(_PeopleSeed() + (fClock != NULL
                ? fClock->Year() : 1400)) % 3 == 0;
        } else if (rule.needs == kNeedsWounded)
            hide = _Wounded() == 0;
        else if (rule.needs == kNeedsTreatment) {
            const std::map<int, uint32>::const_iterator treated
                = fTreatedUntil.find(fCity);
            hide = !fTreatmentOffered || (fClock != NULL
                && treated != fTreatedUntil.end()
                && fClock->HourStamp() < treated->second);
        } else if (rule.needs == kNeedsPawnshop)
            hide = (c.flags & CITY_HAS_PAWNSHOP) == 0;
        else if (rule.needs >= kNeedsShop)
            hide = c.shopQuality[rule.needs - kNeedsShop] == 0;
        else if (rule.needs == kNeedsHarbor)
            hide = c.harbor == CITY_HARBOR_NONE;
        else if (rule.needs != kAlways)
            hide = c.places[rule.needs].empty();
        if (hide)
            hidden.push_back(i);
    }
    return hidden;
}


// 1367:0DB4 adds the seed global to `seed`
std::string
CityVisit::_PersonName(uint16 seed)
{
    if (fNames == NULL)
        fNames.reset(new ExeData(fData.PathFor("DARKLAND.EXE")));
    return fNames->MaleName(uint16(fSeed + seed));
}


uint16
CityVisit::_PeopleSeed() const
{
    return uint16(fData.Cities().CityAt(uint32(fCity)).peopleSeed + fSeed);
}


int
CityVisit::_Reputation() const
{
    if (fReputations == NULL || fCity < 0 || fCity >= int(fReputations->size()))
        return 0;
    return (*fReputations)[fCity];
}


// Mass: said at some hours only, more of them in bigger cities; every
// member gains divine favor, Religion / 8 + Speak Latin / 35 + 1 by day
// (1838:0214), Religion / 60 + Speak Latin / 40 + 1 at night (file
// 0xB93AA), and it lasts until the start of the bell after the next
// one. Otherwise the priest, or the altar boy, tells when the next Mass
// is.
int
CityVisit::_Mass()
{
    const bool night = fNight;		// the church's night card
    if (fClock == NULL || fParty == NULL)
        return night ? SCREEN_NIGHT_NO_MASS : SCREEN_NO_MASS;
    // the smallest city size with a Mass, by bell (1 Matins .. 8
    // Compline); 99: none
    static const int kMassSize[9] = { 99, 99, 0, 5, 6, 7, 4, 6, 99 };
    static const int kNightMassSize[9] = { 99, 7, 0, 5, 99, 99, 4, 6, 99 };
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int bell = fClock->Hour() / 3 + 1;
    if (size < (night ? kNightMassSize : kMassSize)[bell]) {
        int next = 6;
        if (night)
            next = bell == 3 && size > 3 ? 18 : 6;
        else
            next = bell >= 3 && bell <= 5 && size > 3 ? 18 : 6;
        fVariables["NamedOneName"] = GameTime(1400, 0, 1, uint16(next)).BellName();
        return night ? SCREEN_NIGHT_NO_MASS : SCREEN_NO_MASS;
    }
    for (character& member : fParty->members) {
        const int religion = member.skills[kSkillReligion];
        const int latin = member.skills[kSkillSpeakLatin];
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, night
            ? religion / 60 + latin / 40 + 1 : religion / 8 + latin / 35 + 1);
    }
    const int end = (bell + 1) * 3;
    fClock->AddHours(uint32((end - fClock->Hour() + 24) % 24));
    return night ? SCREEN_NIGHT_MASS : SCREEN_MASS;
}


// The altar boy at night (file 0xB9572) names the next Mass, by bell and
// city size. (At Terce, Sexts and Compline the game reads an unset
// variable; the church shows its night card only at other bells.)
int
CityVisit::_AltarBoy()
{
    int next = 6;
    if (fClock != NULL) {
        const int size = fData.Cities().CityAt(uint32(fCity)).size;
        switch (fClock->Hour() / 3 + 1) {
            case 1:
                next = size >= 7 ? 0 : 6;
                break;
            case 3:
                next = size >= 5 ? 9 : size == 4 ? 18 : 6;
                break;
            case 6:
                next = size >= 4 ? 18 : 6;
                break;
            case 7:
                next = size >= 6 ? 21 : 6;
                break;
            default:
                break;
        }
    }
    fVariables["NamedOneName"] = GameTime(1400, 0, 1, uint16(next)).BellName();
    return SCREEN_ALTAR_BOY;
}


// Confession: the leader gains random(5) + Religion / 10 + 2 divine
// favor; it takes 11 hours, less with a good local reputation; if
// random(100) <= Religion and random(100) <= 25, a chance of a point of
// Virtue (1462:0132, see TrainSkill())
int
CityVisit::_Confession()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CONFESSION;
    const int reputation = _Reputation();
    const int hours = reputation >= 0 ? 11 - reputation / 10
        : 12 + reputation / 20;
    if (fClock != NULL)
        fClock->AddHours(uint32(std::max(hours, 0)));
    character& leader = fParty->members[fParty->leader];
    // 0x9C0:1F63(-1, Virtue, 1, 10, -1): a lesson in Virtue
    if (int(fRandom() % 100) <= leader.skills[kSkillReligion]
            && int(fRandom() % 100) <= 25) {
        TrainSkill(leader, kSkillVirtue, 10,
            [this](int n) { return int(fRandom() % uint32(n)); });
    }
    AddToAttribute(leader, ATTRIBUTE_DIVINE_FAVOR,
        int(fRandom() % 5) + leader.skills[kSkillReligion] / 10 + 2);
    return SCREEN_CONFESSION;
}


// Donation: a tenth of the purse, a pfennig of which buys a sixth of a
// divine favor point per member. The most religious member gets back all
// the favor it lacks, then the others in turn while points remain. Over
// 600 pfennigs, every member gains Religion / 30 Virtue. An hour passes.
int
CityVisit::_Donation()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_SMALL_DONATION;
    const uint32 purse = TotalPfennigs(fParty->cash);
    const int amount = int(purse / 10);
    fParty->cash = MoneyFromPfennigs(purse - uint32(amount));

    std::vector<character>& members = fParty->members;
    const int count = int(members.size());
    int points = amount / (count * 6);
    // the game means the most religious member here, but uses its Religion
    // as a member index (a bug of the original)
    int first = 0;
    for (int i = 1; i < count; i++) {
        if (members[i].skills[kSkillReligion] > members[first].skills[kSkillReligion])
            first = i;
    }
    for (int k = 0; k < count && (k == 0 || points > 0); k++) {
        character& member = members[(first + k) % count];
        const int lacking = std::max(0, std::min(99,
            member.maxAttributes[ATTRIBUTE_DIVINE_FAVOR]
                - member.attributes[ATTRIBUTE_DIVINE_FAVOR]));
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, lacking);
        points -= lacking;
    }
    if (amount > 600) {
        for (character& member : members) {
            member.skills[kSkillVirtue] = uint8(std::min(99,
                member.skills[kSkillVirtue] + member.skills[kSkillReligion] / 30));
        }
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    if (amount < 120)
        return SCREEN_SMALL_DONATION;
    return amount < 600 ? SCREEN_DONATION : SCREEN_LARGE_DONATION;
}


// The price of a meal and a night for the party (DARKLAND.EXE,
// 1462:1D2C), in pfennigs: the city size + 1, a third more for a
// suspect party (reputation -10 or less), 30% less for a respected one
// (10 or more), half for a local hero (50 or more); times the party's
// size. (The game raises it by 8/3 or 5/3 with some location states,
// which are not kept here.)
uint32
CityVisit::InnPrice() const
{
    const int reputation = _Reputation();
    int price = fData.Cities().CityAt(uint32(fCity)).size + 1;
    if (reputation <= -10)
        price = price * 4 / 3;
    else if (reputation >= 50)
        price = price / 2;
    else if (reputation >= 10)
        price = price * 7 / 10;
    price = std::max(price, 1);
    if (fParty != NULL)
        price *= int(fParty->members.size());
    return uint32(std::max(1, std::min(price, 1000)));
}


// A meal and a night (file 0xA6F70): every member gets back all the
// endurance it lacks, and a point of strength; the time passes when the
// card is left
int
CityVisit::_Sleep()
{
    if (fParty == NULL)
        return SCREEN_SLEEP;
    const uint32 price = InnPrice();
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - price);
    for (character& member : fParty->members) {
        member.attributes[ATTRIBUTE_ENDURANCE]
            = member.maxAttributes[ATTRIBUTE_ENDURANCE];
        if (member.attributes[ATTRIBUTE_STRENGTH]
                < member.maxAttributes[ATTRIBUTE_STRENGTH])
            AddToAttribute(member, ATTRIBUTE_STRENGTH, 1);
    }
    return SCREEN_SLEEP;
}


// The stables (file 0xA7120): by day the stablemaster shows his horses
// and mules, asking about the party's mounts unless every member has one
// (0E76:1326); at night the stableboy only says to come back
int
CityVisit::_Stables()
{
    if (fNight || fParty == NULL)
        return SCREEN_STABLES;
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (const character& member : fParty->members) {
        bool mounted = false;
        for (const item& carried : member.items) {
            if (carried.code < items.size()
                    && (items[carried.code].flags & ITEM_HORSE) != 0)
                mounted = true;
        }
        if (!mounted)
            return SCREEN_STABLES_SALE;
    }
    return SCREEN_STABLES;
}


// Redeeming the letter of credit: its florins go to the purse, less
// 6 pfennigs a florin
int
CityVisit::_Redeem(int result)
{
    if (fParty == NULL)
        return result;
    const uint32 notes = fParty->bankNotes;
    const uint32 fee = notes * 6;
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
        + notes * 240 - fee);
    fParty->bankNotes = 0;
    fVariables["Money1"] = MoneyText(notes * 240);
    fVariables["Money2"] = MoneyText(fee);
    return result;
}


// Buying a letter of credit: the florins typed in, at most 500 and what
// the purse holds in florins (not counting the smaller coins), without a
// fee
void
CityVisit::_Deposit()
{
    if (fParty == NULL)
        return;
    const std::string& typed = fView.PromptText();
    uint32 amount = 0;
    for (char c : typed)
        amount = std::min(amount * 10 + uint32(c - '0'), 500u);
    amount = std::min(amount, uint32(fParty->cash.florins));
    amount = std::min(amount, uint32(0xFFFF - fParty->bankNotes));
    fParty->cash.florins -= uint16(amount);
    fParty->bankNotes += uint16(amount);
}


CityVisit::~CityVisit()
{
}


// The physician's skill, made when the party first meets him (file
// 0xA2F1A): (the city's property 0x21 % 10) · (city size + random(4)
// - 3), within 1..99
int
CityVisit::_PhysicianSkill()
{
    std::map<int, int>::const_iterator found = fPhysicianSkill.find(fCity);
    if (found != fPhysicianSkill.end())
        return found->second;
    const city& c = fData.Cities().CityAt(uint32(fCity));
    const int skill = (_PeopleSeed() % 10)
        * (c.size + int(fRandom() % 4) - 3);
    return fPhysicianSkill[fCity] = std::max(1, std::min(skill, 99));
}


// The members whose strength is under its maximum
int
CityVisit::_Wounded() const
{
    int wounded = 0;
    if (fParty != NULL) {
        for (const character& member : fParty->members) {
            if (member.attributes[ATTRIBUTE_STRENGTH]
                    < member.maxAttributes[ATTRIBUTE_STRENGTH])
                wounded++;
        }
    }
    return wounded;
}


// skill / 10 + 12 pfennigs for each wounded member (file 0xA33A6)
uint32
CityVisit::_TreatmentPrice()
{
    return uint32((_PhysicianSkill() / 10 + 12) * _Wounded());
}


// The member best at healing, who speaks with the physician (0E76:14A4)
int
CityVisit::_BestHealer() const
{
    int best = 0;
    for (size_t i = 1; fParty != NULL && i < fParty->members.size(); i++) {
        if (fParty->members[i].skills[kSkillHealing]
                > fParty->members[best].skills[kSkillHealing])
            best = int(i);
    }
    return best;
}


// Discussing treatments (file 0xA31D6): an hour; the best healer
// judges the physician if random(100) is at most his intelligence, half
// his charisma and the physician's skill (file 0xA333C)
int
CityVisit::_DiscussTreatments()
{
    const int skill = _PhysicianSkill();
    if (fClock != NULL)
        fClock->AddHours(1);
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_PHYSICIAN_UNSURE;
    const character& healer = fParty->members[_BestHealer()];
    const int chance = healer.attributes[ATTRIBUTE_INTELLIGENCE]
        + healer.attributes[ATTRIBUTE_CHARISMA] / 2 + skill;
    if (int(fRandom() % 100) > chance)
        return SCREEN_PHYSICIAN_UNSURE;
    if (skill <= 1)
        return SCREEN_PHYSICIAN_IDIOT;
    static const char* kWords[] = { "Poor", "Modest", "Good", "Very Good",
        "Excellent" };
    fVariables["Text1"] = kWords[std::min(skill / 20, 4)];
    return SCREEN_PHYSICIAN_SKILL;
}


// Asking his aid (file 0xA3388): an hour, then his price, and the
// treatment is offered
int
CityVisit::_AskAid()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    fVariables["Number1"] = std::to_string(_Wounded());
    fVariables["Money1"] = MoneyText(_TreatmentPrice());
    fTreatmentOffered = true;
    return SCREEN_PHYSICIAN_PRICE;
}


// Alchemical components (file 0xA35E8): he trades if random(100) is at
// most the leader's Speak Common, charisma, (the city's number + month)
// % 30 and the local reputation, within 0..75 (file 0xA365C); an hour
int
CityVisit::_Components()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_PHYSICIAN_NO_TRADE;
    const character& leader = fParty->members[fParty->leader];
    const int month = fClock != NULL ? fClock->Month() : 0;
    const int chance = std::max(0, std::min(75,
        leader.skills[kSkillSpeakCommon] + leader.attributes[ATTRIBUTE_CHARISMA]
            + int(uint16(_PeopleSeed() + month) % 30) + _Reputation()));
    if (int(fRandom() % 100) > chance)
        return SCREEN_PHYSICIAN_NO_TRADE;
    if (fClock != NULL)
        fClock->AddHours(1);
    fPendingTrade = MERCHANT_PHYSICIAN;
    return SCREEN_PHYSICIAN;
}


// The treatment (file 0xA36C0): paid, an hour, and every wounded member
// gains skill / 30 strength (at least 1; an idiot's treatment takes 1 or
// 2); then he treats no one for 20 hours (0E76:2930)
int
CityVisit::_Treatment()
{
    if (fParty == NULL)
        return SCREEN_PHYSICIAN;
    const uint32 price = _TreatmentPrice();
    const uint32 purse = TotalPfennigs(fParty->cash);
    if (purse < price)
        return SCREEN_PHYSICIAN_POOR;
    fParty->cash = MoneyFromPfennigs(purse - price);
    const int skill = _PhysicianSkill();
    if (fClock != NULL)
        fClock->AddHours(1);
    for (character& member : fParty->members) {
        if (member.attributes[ATTRIBUTE_STRENGTH]
                >= member.maxAttributes[ATTRIBUTE_STRENGTH])
            continue;
        const int amount = skill > 1 ? std::max(1, std::min(skill / 30, 99))
            : int(fRandom() % 2) - 2;
        AddToAttribute(member, ATTRIBUTE_STRENGTH, amount);
    }
    if (fClock != NULL)
        fTreatedUntil[fCity] = fClock->HourStamp() + 20;
    return SCREEN_PHYSICIAN_TREATED;
}


// Asking to be his students (file 0xA349C): nothing to learn from him if
// the best healer is better (and he is no idiot); no again for 30 hours
// after a no; he takes students for skill / 5 + 10 pfennigs a day where
// (the city's property 0x21 + year) % 3 is not 0 (a teacher of healing
// up to 60, for 168 hours); else he has 1..4 apprentices already
int
CityVisit::_Students()
{
    const int skill = _PhysicianSkill();
    int best = 0;
    if (fParty != NULL && !fParty->members.empty())
        best = fParty->members[_BestHealer()].skills[kSkillHealing];
    if (best > skill && skill > 1)
        return SCREEN_PHYSICIAN_NOTHING;
    const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
    const std::map<int, uint32>::const_iterator refused
        = fNoStudentsUntil.find(fCity);
    if (refused != fNoStudentsUntil.end() && now < refused->second)
        return SCREEN_PHYSICIAN_NO_STUDENTS;
    const int year = fClock != NULL ? fClock->Year() : 1400;
    if (int16(_PeopleSeed() + year) % 3 != 0 && skill > 1) {
        const uint32 fee = uint32(skill / 5 + 10);
        fVariables["Money1"] = MoneyText(fee);
        // the teacher the game makes charges 60 pfennigs a day, not the
        // fee the card shows, with a level of 50 (0E76:2C4E's arguments)
        if (fTutors.find(fCity) == fTutors.end()
                || fTutors[fCity].until <= now)
            fTutors[fCity] = city_tutor{ kSkillHealing, 50, 60, now + 168 };
        return SCREEN_PHYSICIAN_TUTOR;
    }
    fVariables["Number1"] = std::to_string(fRandom() % 4 + 1);
    fNoStudentsUntil[fCity] = now + 30;
    return SCREEN_PHYSICIAN_APPRENTICES;
}


// Leaving the physician (file 0xA3902, 0xA39B6): at night, apologizing
// with two groschen for the trouble (to the district), or else, half the
// time
// (random(100) <= 50), he curses the party and the local reputation
// falls by 1..4 (card 7)
int
CityVisit::_LeavePhysician(bool apologize)
{
    if (apologize) {
        if (fParty != NULL) {
            const uint32 purse = TotalPfennigs(fParty->cash);
            fParty->cash = MoneyFromPfennigs(purse - std::min(purse, 24u));
        }
        return SCREEN_DISTRICT;
    }
    if (int(fRandom() % 100) > 50)
        return SCREEN_CRAFTS;
    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        reputation = int16(std::max(-99, reputation - int(fRandom() % 4) - 1));
    }
    return SCREEN_PHYSICIAN_CURSES;
}
