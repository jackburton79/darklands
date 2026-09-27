#include "CityVisit.h"

#include "Character.h"
#include "CityFile.h"
#include "DescriptionFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
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
    ACTION_SLEEP,				// the inn's options
    ACTION_STABLES
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

// Special waiting times
static const int kUntilNight		= -1;	// "wait until nightfall"
static const int kUntilMorning		= -2;	// "camp here until morning"

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

// The guilds' shops by day and by night
#define SHOP_OPTIONS(merchant) { \
        TRADE(merchant),					/* buy and sell goods */ \
        TODO, TODO,							/* politics, the masters */ \
        HIDE, HIDE, HIDE,					/* the leader's secret */ \
        GO(SCREEN_ARMS_CRAFTS)				/* leave */ \
    }
#define NIGHT_SHOP_OPTIONS(merchant) { \
        TRADE(merchant),					/* awaken somebody to trade */ \
        TODO,								/* awaken the guild leader */ \
        HIDE, HIDE, HIDE,					/* the leader's home, saboteurs */ \
        TODO, TODO, TODO, TODO,				/* placeholders */ \
        GO(SCREEN_ARMS_CRAFTS)				/* leave */ \
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
        TODO,								// take up residence
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
        TODO, TODO, TODO,					// Fugger, Medici, Hanse
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
    { "CIVCR00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// physician, astrologists,
                                            // jewelers, tinkers, clothmakers
        TODO, TODO,							// placeholders
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
    { "SWORD00", 0, NULL, SHOP_OPTIONS(MERCHANT_SWORDSMITH) },
    { "BLACK00", 0, NULL, SHOP_OPTIONS(MERCHANT_BLACKSMITH) },
    { "ARMOR00", 0, NULL, SHOP_OPTIONS(MERCHANT_ARMORER) },
    { "BOWYE00", 0, NULL, SHOP_OPTIONS(MERCHANT_BOWYER) },
    // The church's results: "the Mass is sung", "the next Mass will be
    // at $NamedOneName", confession, the priest's thanks for small,
    // middling and large donations
    { "CITYC00", 2, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 4, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 3, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 5, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 6, NULL, { GO(SCREEN_CHURCH) } },
    { "CITYC00", 7, NULL, { GO(SCREEN_CHURCH) } },
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
        TODO,								// take up residence
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
    { "CITYC01", 0, NULL, {
        TODO, TODO, TODO,					// mass, altar boy, sanctuary
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
    // "Peering down the narrow streets, dimly lit with lamplight..."
    { "CIVCR00", 1, NULL, {
        TODO, TODO, TODO, TODO, TODO,
        TODO, TODO,
        GO(SCREEN_ARMS_CRAFTS),
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_MAIN_STREET)
    } },
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
    { "SWORD01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_SWORDSMITH) },
    { "BLACK01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_BLACKSMITH) },
    { "ARMOR01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_ARMORER) },
    { "BOWYE01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_BOWYER) },
    { NULL, 0, NULL, {} },					// the church's results
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
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
    fRandom(std::random_device()())
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
}


void
CityVisit::SetInfoView(InfoView* info)
{
    fInfo = info;
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
        case ACTION_MASS:
            _Show(_Mass());
            return true;
        case ACTION_CONFESSION:
            _Show(_Confession());
            return true;
        case ACTION_DONATION:
            _Show(_Donation());
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
    fScreen = screen;
    if (screen == SCREEN_INN)
        fVariables["Money1"] = MoneyText(_InnPrice());
    else if (fParty != NULL)	// what the church asks for a donation
        fVariables["Money1"] = MoneyText(TotalPfennigs(fParty->cash) / 10);
    const screen_rules& rules = RulesFor(screen, fNight);
    if (rules.deck == NULL) {
        fView.SetCard(fNotImplementedCard, fVariables);
        fView.SetScene("");
        return;
    }
    fView.SetCard(fData.Messages(rules.deck).CardAt(uint32(rules.card)),
        fVariables, _HiddenOptions(screen));
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
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < _InnPrice();
        else if (rule.needs == kNeedsPawnshop)
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


int
CityVisit::_Reputation() const
{
    if (fReputations == NULL || fCity < 0 || fCity >= int(fReputations->size()))
        return 0;
    return (*fReputations)[fCity];
}


// Mass: said at some hours only, more of them in bigger cities; every
// member gains Religion / 8 + Speak Latin / 35 + 1 divine favor, and it
// lasts until the start of the bell after the next one. Otherwise the
// priest tells when the next Mass is.
int
CityVisit::_Mass()
{
    if (fClock == NULL || fParty == NULL)
        return SCREEN_NO_MASS;
    // the smallest city size with a Mass, by bell (1 Matins .. 8
    // Compline); 99: none
    static const int kMassSize[9] = { 99, 99, 0, 5, 6, 7, 4, 6, 99 };
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int bell = fClock->Hour() / 3 + 1;
    if (size < kMassSize[bell]) {
        const int next = bell >= 3 && bell <= 5 && size > 3 ? 18 : 6;
        fVariables["NamedOneName"] = GameTime(1400, 0, 1, uint16(next)).BellName();
        return SCREEN_NO_MASS;
    }
    for (character& member : fParty->members) {
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
            member.skills[kSkillReligion] / 8
                + member.skills[kSkillSpeakLatin] / 35 + 1);
    }
    const int end = (bell + 1) * 3;
    fClock->AddHours(uint32((end - fClock->Hour() + 24) % 24));
    return SCREEN_MASS;
}


// Confession: the leader gains random(5) + Religion / 10 + 2 divine
// favor; it takes 11 hours, less with a good local reputation (the game
// may also raise Virtue: not reproduced)
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
CityVisit::_InnPrice() const
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
    const uint32 price = _InnPrice();
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
