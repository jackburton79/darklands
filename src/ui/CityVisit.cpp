#include "CityVisit.h"

#include "BattleMap.h"
#include "BattleView.h"
#include "Catalog.h"
#include "Character.h"
#include "CityFile.h"
#include "DescriptionFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
#include "ExeData.h"
#include "ListFile.h"
#include "ScreenSupport.h"
#include "EnemyFile.h"
#include "Stream.h"

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
    ACTION_CACHE,				// the inn's cache: `then` after it
    ACTION_REDEEM,				// the banks: `target` is the result
    ACTION_DEPOSIT,				// the number typed in; `target`: the bank
    ACTION_DISCUSS_TREATMENTS,	// the physician's options
    ACTION_ASK_AID,
    ACTION_COMPONENTS,
    ACTION_TREATMENT,
    ACTION_STUDENTS,
    ACTION_LEAVE_PHYSICIAN,		// at night: `target` 1 to apologize
    ACTION_APOLOGIZE,
    ACTION_STONE,				// the alchemist's options
    ACTION_ALCHEMIST_SHOP,
    ACTION_SNEAK,				// the market at night
    ACTION_BRIBE,
    ACTION_PAY_FINE,			// the night watch
    ACTION_RUN,
    ACTION_FIGHT,				// attack the night watch
    ACTION_GATE_DAY,			// before the walls: to the gate by day
    ACTION_GATE_NIGHT,			// or at night
    ACTION_PAY_TOLL,			// the gate by day
    ACTION_CHARM_GUARDS,
    ACTION_SLIP_IN,
    ACTION_BACK_TO_GATE,		// the gate by day or at night, by the hour
    ACTION_HAIL_WATCH,			// the gate at night
    ACTION_TALK_TO_WATCH,
    ACTION_BRIBE_WATCH,
    ACTION_FALL_BACK,
    ACTION_WALL_DAY,			// before the walls: the wall by day, at night
    ACTION_WALL_NIGHT,
    ACTION_BRIBE_WALL,			// the wall
    ACTION_ROPE,
    ACTION_CLIMB,
    ACTION_GRATE,
    ACTION_BACK_TO_WALL,		// the wall by day or at night, by the hour
    ACTION_WATCH_RETURN,		// on where paying the fine would lead
    ACTION_NIGHT_WALK			// ACTION_GO, but the watch may stop the
                                // party outside the game's day
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
// or a cache at the inn (the location record's +0x18 is not -1)
static const int kNeedsCache		= -11;
// or the physician: in the city (small towns may have none), wounds to
// treat, a treatment offered
static const int kNeedsPhysician	= -8;
static const int kNeedsWounded		= -9;
static const int kNeedsTreatment	= -10;
// or the alchemist: in the city, the stone's price in the purse, a
// master (skill 25 or more) for the formulas
static const int kNeedsAlchemist	= -12;
static const int kNeedsStone		= -13;
static const int kNeedsMaster		= -14;
// or the night: no failed sneaking lately, the bribe in the purse, the
// fine in the purse
static const int kNeedsSneak		= -15;
static const int kNeedsBribe		= -16;
static const int kNeedsFine			= -17;
// or the gate: the toll in the purse, no failed attempt lately
static const int kNeedsToll			= -18;
static const int kNeedsCharm		= -19;
static const int kNeedsSlip			= -20;
static const int kNeedsHail			= -21;
static const int kNeedsTalk			= -22;
static const int kNeedsNightBribe	= -23;
// or the wall: its bribe in the purse; a rope, or none; no failed climb
// this stay, no alarm; no failed try at the sewer's grate lately
static const int kNeedsWallBribe	= -24;
static const int kNeedsRope			= -25;
static const int kNeedsNoRope		= -26;
static const int kNeedsClimb		= -27;
static const int kNeedsGrate		= -28;
static const int kRopeCode			= 59;	// in DARKLAND.LST

// The game's timed marks used here (0E76:2930, 2A32)
static const int kMarkCharmFailed	= 0x0A;	// the gate's guards
static const int kMarkSlipFailed	= 0x0B;
static const int kMarkHailFailed	= 0x0C;	// the gate at night
static const int kMarkTalkFailed	= 0x0D;
static const int kMarkWallAlert		= 0x0F;	// the wall by day: guarded
static const int kMarkGrateFailed	= 0x10;
static const int kMarkWanted		= 0x11;	// by the gate (inferred)
static const int kMarkAlert			= 0x12;	// the gate's guards nervous
static const int kMarkGuarded		= 0x17;	// the market is watched
static const int kMarkBribeRefused	= 0x19;
static const int kMarkSneakFailed	= 0x1A;
static const int kMarkWatchMet		= 0x40;

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

// The market at night and the night watch; potions, saints and fights
// are not implemented
#define NIGHT_MARKET_OPTIONS { \
        DO_IF(ACTION_SNEAK, kNeedsSneak),	/* sneak to the offices */ \
        DO_IF(ACTION_BRIBE, kNeedsBribe),	/* bribe them with $Money1 */ \
        TODO, TODO, TODO,					/* potion, saint, attack */ \
        GO(SCREEN_MAIN_STREET), \
        GO(SCREEN_SIDE_STREET) \
    }
#define OUTSIDE_OPTIONS { \
        DO(ACTION_GATE_DAY),				/* the main gate by day */ \
        DO(ACTION_GATE_NIGHT),				/* at night */ \
        DO(ACTION_WALL_DAY),				/* the wall by day */ \
        DO(ACTION_WALL_NIGHT),				/* at night */ \
        LEAVE								/* travel elsewhere */ \
    }
#define DAY_GATE_OPTIONS { \
        DO_IF(ACTION_PAY_TOLL, kNeedsToll),	/* the toll of $Money1 */ \
        DO_IF(ACTION_CHARM_GUARDS, kNeedsCharm), \
        DO_IF(ACTION_SLIP_IN, kNeedsSlip),	/* sneak in with the crowd */ \
        TODO, TODO, TODO,					/* potion, saint, attack */ \
        GO(SCREEN_OUTSIDE)					/* reconsider */ \
    }
#define WATCH_OPTIONS { \
        DO_IF(ACTION_PAY_FINE, kNeedsFine),	/* the fine of $Money1 */ \
        DO(ACTION_RUN),						/* run away */ \
        TODO, TODO,							/* potion, saint */ \
        DO(ACTION_FIGHT)					/* attack them */ \
    }
#define WATCH_CAUGHT_OPTIONS { \
        DO_IF(ACTION_PAY_FINE, kNeedsFine), \
        HIDE,								/* no running again */ \
        TODO, TODO, \
        DO(ACTION_FIGHT) \
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
    // "Before you lies the walled city of $PlaceName... It is ruled by
    // the $CityLordTitle for the $CityLordName. You suspect that you will
    // be $PlaceAttitude here." (DARKLAND.EXE state 4, file 0x941C0; the
    // game never uses $OUTSI00)
    { "CITYE00", 0, NULL, OUTSIDE_OPTIONS },
    // "Here you can enjoy the good food... of the $Inn common-room."
    // (DARKLAND.EXE, file 0xA6B5E; a wanted party gets SCREEN_UNWELCOME)
    { "URBAN00", 0, NULL, {
        TODO,								// local news and rumors
        DO_IF(ACTION_SLEEP, kNeedsInnPrice),	// a meal and sleep for $Money1
        DO(ACTION_RESIDENCE),				// take up residence
        DO(ACTION_STABLES),
        GO(SCREEN_STORE),					// store items (file 0xA720C)
        GO_IF(SCREEN_RECOVER, kNeedsCache),	// recover them (0xA72B8)
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
        { ACTION_NIGHT_WALK, CityVisit::SCREEN_PHYSICIAN, kNeedsPhysician, 60 },
        { ACTION_NIGHT_WALK, CityVisit::SCREEN_ALCHEMIST, kNeedsAlchemist, 60 },
        HIDE,								// jewelers
        { ACTION_NIGHT_WALK, CityVisit::SCREEN_ARTIFICER, kAlways,
            kAnHourMoreAtNight },			// tinkers
        { ACTION_NIGHT_WALK, CityVisit::SCREEN_CLOTHMAKER, kAlways,
            kAnHourMoreAtNight },
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
    // The market at night (in the day table too, for its cards): watched,
    // "a loud thump", "...they quickly run in your direction...", the
    // guard leader leads them away, "take money from scum like you?"
    { "MARKE01", 19, NULL, NIGHT_MARKET_OPTIONS },
    { "MARKE01", 2, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "MARKE01", 1, NULL, { GO(SCREEN_NIGHT_WATCH_MARKET) } },
    { "MARKE01", 9, NULL, { TODO } },			// into the offices
    { "MARKE01", 10, NULL, { GO(SCREEN_NIGHT_WATCH_MARKET) } },
    // "Who violates the curfew of $PlaceName?" (file 0xBF0BB)
    { "NIGHT00", 0, NULL, WATCH_OPTIONS },
    { "NIGHT00", 1, NULL, WATCH_OPTIONS },
    { "NIGHT00", 2, NULL, WATCH_OPTIONS },
    { "NIGHT00", 4, NULL, WATCH_CAUGHT_OPTIONS },
    // "Dashing down narrow lanes and alleys, you outdistance the night
    // watch."
    { "NIGHT00", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "When the night watch sees your naked steel, they gasp... and flee"
    { "NIGHT00", 7, NULL, { DO(ACTION_WATCH_RETURN) } },
    // "You look with regret on the unconscious and bleeding night watch."
    { "NIGHT00", 8, NULL, { DO(ACTION_WATCH_RETURN) } },
    // "You fall back around a corner, sheath your weapons..." (the side
    // streets: inferred)
    { "NIGHT00", 10, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "The night watch strips you of weapons, armor..." (the dungeon is
    // not implemented)
    { "NIGHT00", 11, NULL, { GO(SCREEN_NOT_IMPLEMENTED) } },
    // "You carefully select which items to leave with the innkeeper...",
    // "You sort through the various goods...": the cache, then an hour
    { "URBAN00", 5, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
    { "URBAN00", 6, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
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
    // "...the best alchemist in $PlaceName, $NamedOneName... You ask
    // about..." (file 0xD9B53); card 1 for the next questions
#define ALCHEMIST_OPTIONS { \
        DO_IF(ACTION_STONE, kNeedsStone),	/* a better stone for $Money1 */ \
        DO(ACTION_ALCHEMIST_SHOP),			/* purchasing $Text4 */ \
        TODO_IF(kNeedsMaster),				/* purchasing formulas */ \
        TODO,								/* trading formulas */ \
        TODO,								/* instruction in alchemy */ \
        TODO,								/* special tasks */ \
        HIDE, HIDE, HIDE,					/* placeholders */ \
        GO(SCREEN_CRAFTS)					/* trivialities, then leave */ \
    }
    { "ALCHE00", 0, NULL, ALCHEMIST_OPTIONS },
    { "ALCHE00", 1, NULL, ALCHEMIST_OPTIONS },
#undef ALCHEMIST_OPTIONS
    // "...none of you knows enough about alchemy", "Painful peril awaits
    // any who disturb my slumber", "...before I turn you into toads!"
    { "ALCHE00", 2, NULL, { GO(SCREEN_CRAFTS) } },
    { "ALCHE00", 3, NULL, { GO(SCREEN_CRAFTS) } },
    { "ALCHE00", 6, NULL, { GO(SCREEN_CRAFTS) } },
    // "...your abilities are beyond my own", "...improve your
    // philosopher's stone to quality $Number1"
    { "ALCHE00", 5, NULL, { GO(SCREEN_ALCHEMIST_AGAIN) } },
    { "ALCHE00", 7, NULL, { GO(SCREEN_ALCHEMIST_AGAIN) } },
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
    { "CITYE00", 5, NULL, OUTSIDE_OPTIONS },
    { "CITYE00", 6, NULL, OUTSIDE_OPTIONS },
    // "Since it's night, you wait until dawn...", "You wait until night
    // falls..."
    { "CITYE00", 1, NULL, { GO(SCREEN_DAY_GATE) } },
    { "CITYE00", 2, NULL, { GO(SCREEN_NIGHT_GATE) } },
    // "At the gate a line of people wait to enter... the overall cost
    // for your party will be $Money1." (state 2, file 0x925B0)
    { "CITYG01", 0, NULL, DAY_GATE_OPTIONS },
    { "CITYG01", 18, NULL, DAY_GATE_OPTIONS },
    { "CITYG01", 1, NULL, { GO(SCREEN_MAIN_STREET) } },
    { "CITYG01", 2, NULL, { GO(SCREEN_MAIN_STREET) } },
    { "CITYG01", 3, NULL, { DO(ACTION_BACK_TO_GATE) } },
    { "CITYG01", 4, NULL, { GO(SCREEN_MAIN_STREET) } },
    { "CITYG01", 5, NULL, { GO(SCREEN_OUTSIDE) } },
    // "In the dead of night you approach the closed gate..." (state 3,
    // file 0x93448)
    { "CITYG00", 0, NULL, {
        DO_IF(ACTION_HAIL_WATCH, kNeedsHail),	// rely on your fame
        DO_IF(ACTION_TALK_TO_WATCH, kNeedsTalk),	// talk your way inside
        DO_IF(ACTION_BRIBE_WATCH, kNeedsNightBribe),	// $Money1
        TODO, TODO,							// potion, saint
        DO(ACTION_FALL_BACK)
    } },
    { "CITYG00", 0, NULL, {
        HIDE, HIDE, HIDE,
        TODO, TODO,
        DO(ACTION_FALL_BACK)
    } },
    // "Holy Sacraments!... ushered into the city by a worshipful gateman"
    { "CITYG00", 1, NULL, { GO(SCREEN_MAIN_STREET) } },
    // "Nobody through the gates till dawn."
    { "CITYG00", 2, NULL, { DO(ACTION_BACK_TO_GATE) } },
    // "I recognize you. We've got a nice cozy dungeon cell waiting!"
    { "CITYG00", 3, NULL, { GO(SCREEN_NIGHT_GATE_ALERTED) } },
    // "...the watchman isn't very bright. ...He opens a sally port."
    { "CITYG00", 4, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "...$Money1, saying, 'Isn't this sufficient for our toll?'"
    { "CITYG00", 5, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "You retire to a quiet corner of the woods near the gate..."
    { "CITYG00", 12, NULL, { GO(SCREEN_OUTSIDE) } },
    // "You stare glumly at the least-guarded section of $PlaceName's
    // walls." (state 0xE, file 0x99AEC)
    { "CITYW00", 0, NULL, {
        DO_IF(ACTION_BRIBE_WALL, kNeedsWallBribe),	// $Money1 at a door
        { ACTION_ROPE, 0, kNeedsRope, 0 },	// $ChosenOneName and a rope
        { ACTION_CLIMB, 0, kNeedsNoRope, 0 },	// everybody, no rope
        TODO, TODO,							// potion, saint
        GO(SCREEN_OUTSIDE)					// fall back
    } },
    // "You finish your examination at night... wait until dawn"
    { "CITYE00", 3, NULL, { GO(SCREEN_DAY_WALL) } },
    { "CITYW00", 1, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW00", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW00", 4, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { "CITYW00", 11, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW00", 12, NULL, { GO(SCREEN_DAY_WALL_HELP) } },
    { "CITYW00", 13, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { "CITYW00", 12, NULL, { DO(ACTION_BACK_TO_WALL) } },	// nobody up
    // "Dark masses of stone loom over you." (state 0xF, file 0x9A7E0)
    { "CITYW01", 0, NULL, {
        { ACTION_ROPE, 0, kNeedsRope, 0 },
        { ACTION_CLIMB, 0, kNeedsClimb, 0 },
        DO_IF(ACTION_GRATE, kNeedsGrate),	// force a sewer grate
        TODO, TODO,							// potion, saint
        GO(SCREEN_OUTSIDE)					// fall back
    } },
    // "It's broad daylight when you finish... wait until dark"
    { "CITYE00", 4, NULL, { GO(SCREEN_NIGHT_WALL) } },
    { "CITYW01", 1, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW01", 2, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { "CITYW01", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW01", 11, NULL, { GO(SCREEN_NIGHT_WALL_HELP) } },
    { "CITYW01", 12, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { "CITYW01", 11, NULL, { DO(ACTION_BACK_TO_WALL) } },	// nobody up
    { "CITYW01", 4, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW01", 5, NULL, { DO(ACTION_BACK_TO_WALL) } },
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
        GO(SCREEN_STORE),
        GO_IF(SCREEN_RECOVER, kNeedsCache),
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
    // (DARKLAND.EXE, file 0xA0C62; card 19 when the market is watched)
    { "MARKE01", 0, NULL, NIGHT_MARKET_OPTIONS },
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
    { NULL, 0, NULL, {} },					// the market at night, the watch
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },					// fighting the watch
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { "URBAN01", 5, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
    { "URBAN01", 6, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
    { NULL, 0, NULL, {} },					// the alchemist
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
    { NULL, 0, NULL, {} },
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
    { NULL, 0, NULL, {} },					// before the walls, the gates,
    { NULL, 0, NULL, {} },					// the walls
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
#undef NIGHT_MARKET_OPTIONS
#undef WATCH_OPTIONS
#undef OUTSIDE_OPTIONS
#undef DAY_GATE_OPTIONS
#undef WATCH_CAUGHT_OPTIONS
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
    fPendingCache(false),
    fTreatmentOffered(false),
    fStoneOffered(false),
    fWatchReturn(SCREEN_NOT_IMPLEMENTED),
    fPendingBattle(false),
    fPartyLost(false),
    fWallFailed(false)
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
    fTrade.SetInfoView(info);
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
    fPendingBattle = false;
            _Show(fScreen, false);
        }
        if (fPendingCache) {
            fTrade.SetPlace(fCity, _Reputation());
            fTrade.SetCache(&fCaches[fCity]);
            fTrade.Run(window);
            fPendingCache = false;
            _Show(fScreen, false);
        }
        if (fPendingBattle) {
            _RunBattle(window);
            if (fPartyLost) {
                fPartyLost = false;
                return PARTY_LOST;
            }
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
    fPendingCache = false;
    // $ChosenOneName and its pronouns are the leader's unless an option
    // names another member
    if (fParty != NULL)
        AddPartyVariables(*fParty, fVariables);
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
        case ACTION_CACHE:
            if (fClock != NULL)
                fClock->AddMinutes(MinutesFor(rule, *fClock));
            fCaches[fCity];		// the location has a cache now
            fPendingCache = true;
            fScreen = rule.then;
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
        case ACTION_STONE:
            _Show(_Stone());
            return true;
        case ACTION_ALCHEMIST_SHOP:
            _Show(_AlchemistShop());
            return true;
        case ACTION_NIGHT_WALK: {
            // DARKLAND.EXE, e.g. file 0xA47A2 and 0xA4596: at night the
            // tinkers' and clothmakers' streets cost an hour more first;
            // then the watch stops the party if random(100) is over the
            // chance, and paying its fine leads back here
            if (fClock != NULL && !IsGameDay(*fClock)) {
                if (rule.minutes == kAnHourMoreAtNight)
                    fClock->AddHours(1);
                if (int(fRandom() % 100) > _NightWalkChance()) {
                    fWatchReturn = fScreen;
                    _Show(SCREEN_NIGHT_WATCH);
                    return true;
                }
            }
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(rule.target);
            return true;
        }
        case ACTION_SNEAK:
            _Show(_Sneak());
            return true;
        case ACTION_BRIBE:
            _Show(_BribeGuards());
            return true;
        case ACTION_PAY_FINE:
            _Show(_PayFine());
            return true;
        case ACTION_RUN:
            _Show(_RunFromWatch());
            return true;
        case ACTION_FIGHT: {
            const int next = _FightWatch();
            if (next >= 0)
                _Show(next);
            return true;
        }
        case ACTION_WATCH_RETURN:
            _Show(fWatchReturn);
            return true;
        case ACTION_GATE_DAY:
            _Show(_GoToGate(true));
            return true;
        case ACTION_GATE_NIGHT:
            _Show(_GoToGate(false));
            return true;
        case ACTION_PAY_TOLL:
            _Show(_PayToll());
            return true;
        case ACTION_CHARM_GUARDS:
            _Show(_CharmGuards());
            return true;
        case ACTION_SLIP_IN:
            _Show(_SlipIn());
            return true;
        case ACTION_BACK_TO_GATE:
            // states 2 or 3 by 1367:072A
            _Show(fClock == NULL || IsGameDay(*fClock) ? SCREEN_DAY_GATE
                : SCREEN_NIGHT_GATE);
            return true;
        case ACTION_HAIL_WATCH:
            _Show(_HailWatch());
            return true;
        case ACTION_TALK_TO_WATCH:
            _Show(_TalkToWatch());
            return true;
        case ACTION_BRIBE_WATCH:
            _Show(_BribeWatch());
            return true;
        case ACTION_WALL_DAY:
            _Show(_GoToWall(true));
            return true;
        case ACTION_WALL_NIGHT:
            _Show(_GoToWall(false));
            return true;
        case ACTION_BRIBE_WALL:
            _Show(_BribeWall());
            return true;
        case ACTION_ROPE:
            _Show(_ClimbWithRope(fScreen == SCREEN_DAY_WALL));
            return true;
        case ACTION_CLIMB:
            _Show(_ClimbAlone(fScreen == SCREEN_DAY_WALL));
            return true;
        case ACTION_GRATE:
            _Show(_ForceGrate());
            return true;
        case ACTION_BACK_TO_WALL:
            // states 0xE or 0xF by 1367:072A
            _Show(fClock == NULL || IsGameDay(*fClock) ? SCREEN_DAY_WALL
                : SCREEN_NIGHT_WALL);
            return true;
        case ACTION_FALL_BACK:
            // file 0x93BF2: card 12, an hour, before the walls
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(SCREEN_NIGHT_GATE_RETIRED);
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
        { "CityLordName", CITY_RULER },		// DARKLAND.EXE, file 0x8DCD5
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
    variables["CityLordTitle"] = CityLordTitle(c);
}


// $CityLordTitle (DARKLAND.EXE, file 0x8DC82): the ruler in its seat; in
// a city ruled for him, one of six officers, in a free city one of nine
// (tables at 290E:2323 and 233B), by the city's number (+0x56). The
// location's flag 0x80, which would switch to the city record's +0x5A,
// is not kept here.
/* static */
std::string
CityVisit::CityLordTitle(const city& c)
{
    static const char* kOfficers[6] = {
        "Vogt", "Erbvogt", "Obervogt", "Burggraf", "Richter", "Landhofmeister"
    };
    static const char* kCouncils[9] = {
        "alte Herr", "\xC3\x84ltere Herren", "Frager", "Losunger",
        "alte Losunger", "Oberste Hauptm\xC3\xA4nn", "Schultheiss",
        "Sch\xC3\xB6" "ff", "B\xC3\xBCrgermeister"
    };
    switch (c.rule) {
        case CITY_CAPITAL:
            return c.places[CITY_RULER];
        case CITY_RULED:
            return kOfficers[c.peopleSeed % 6];
        default:
            return kCouncils[c.peopleSeed % 9];
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
    const int previous = fScreen;
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
            _SetChosen(_BestHealer());
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
    // the alchemist (file 0xD9E30): angry for a while, not found by a
    // party that knows too little alchemy, closed at night; card 1 after
    // the first question
    if (screen == SCREEN_ALCHEMIST || screen == SCREEN_ALCHEMIST_AGAIN) {
        const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
        const std::map<int, uint32>::const_iterator angry
            = fAlchemistAngryUntil.find(fCity);
        // "found" if (the seed global + the location) % 30 is under the
        // best Alchemy, at least 10
        const bool found = int((fSeed + fCity) % 30)
            < std::max(10, std::min(_BestSkill(kSkillAlchemy), 99));
        if (screen == SCREEN_ALCHEMIST)
            fStoneOffered = false;
        if (angry != fAlchemistAngryUntil.end() && now < angry->second)
            screen = SCREEN_ALCHEMIST_ANGRY;
        else if (!found)
            screen = SCREEN_ALCHEMIST_UNKNOWN;
        else if (fClock != NULL && !IsGameDay(*fClock))
            screen = SCREEN_ALCHEMIST_NIGHT;
        fVariables["NamedOneName"] = _PersonName(uint16(_PeopleSeed() + 0x49));
        fVariables["Money1"] = MoneyText(_StonePrice());
        fVariables["Text4"] = _AlchemistSkill(true) >= 25 ? "Potions"
            : "Alchemical Components";
        if (fParty != NULL && !fParty->members.empty()) {
            int best = 0;
            for (size_t i = 1; i < fParty->members.size(); i++) {
                if (fParty->members[i].skills[kSkillAlchemy]
                        > fParty->members[best].skills[kSkillAlchemy])
                    best = int(i);
            }
            _SetChosen(best);
        }
    }
    // the market at night: watched for a while after trouble; the bribe
    if (screen == SCREEN_MARKET && fClock != NULL && fClock->IsNight()
            && _Marked(kMarkGuarded))
        screen = SCREEN_MARKET_GUARDED;
    if (screen == SCREEN_MARKET || screen == SCREEN_MARKET_GUARDED)
        fVariables["Money1"] = MoneyText(_Bribe());
    // the night watch: "Not you again" within 7 hours (0E76:2930(0x40, 7))
    if (screen == SCREEN_NIGHT_WATCH || screen == SCREEN_NIGHT_WATCH_MARKET) {
        if (screen == SCREEN_NIGHT_WATCH && _Marked(kMarkWatchMet))
            screen = SCREEN_NIGHT_WATCH_AGAIN;
        _Mark(kMarkWatchMet, 7);
    }
    if (screen == SCREEN_NIGHT_WATCH || screen == SCREEN_NIGHT_WATCH_MARKET
            || screen == SCREEN_NIGHT_WATCH_AGAIN
            || screen == SCREEN_NIGHT_WATCH_CAUGHT)
        fVariables["Money1"] = MoneyText(_Fine());
    // before the walls: the card of the city's rule (file 0x942E0), and
    // what the party expects there
    if (screen == SCREEN_OUTSIDE) {
        const int rule = fData.Cities().CityAt(uint32(fCity)).rule;
        if (rule == CITY_CAPITAL)
            screen = SCREEN_OUTSIDE_CAPITAL;
        else if (rule != CITY_RULED)
            screen = SCREEN_OUTSIDE_FREE;
    }
    if (screen == SCREEN_OUTSIDE || screen == SCREEN_OUTSIDE_CAPITAL
            || screen == SCREEN_OUTSIDE_FREE) {
        fVariables["PlaceAttitude"] = ReputationWord(_Reputation());
    }
    // the gate by day: nervous guards (mark 0x12: card 18), the toll
    if (screen == SCREEN_DAY_GATE && _Marked(kMarkAlert))
        screen = SCREEN_DAY_GATE_GUARDED;
    if (screen == SCREEN_DAY_GATE || screen == SCREEN_DAY_GATE_GUARDED)
        fVariables["Money1"] = MoneyText(_Toll());
    if (screen == SCREEN_NIGHT_GATE || screen == SCREEN_NIGHT_GATE_ALERTED
            || screen == SCREEN_NIGHT_GATE_BRIBED)
        fVariables["Money1"] = MoneyText(_NightBribe());
    // the wall: its bribe; a failed climb holds for this stay only (the
    // handler's disabled options), a new one begins before the walls or
    // when the day turns to night and back
    if (screen == SCREEN_DAY_WALL || screen == SCREEN_DAY_WALL_BRIBED)
        fVariables["Money1"] = MoneyText(_WallBribe());
    if (screen == SCREEN_OUTSIDE || screen == SCREEN_OUTSIDE_CAPITAL
            || screen == SCREEN_OUTSIDE_FREE
            || (screen == SCREEN_DAY_WALL && previous != SCREEN_DAY_WALL_FALL
                && previous != SCREEN_DAY_WALL_HELP
                && previous != SCREEN_DAY_WALL_SLIP_ALONE)
            || (screen == SCREEN_NIGHT_WALL
                && previous != SCREEN_NIGHT_WALL_FALL
                && previous != SCREEN_NIGHT_WALL_HELP
                && previous != SCREEN_NIGHT_WALL_SLIP_ALONE
                && previous != SCREEN_SEWER_STUCK))
        fWallFailed = false;
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
        else if (rule.needs == kNeedsCache)
            hide = fCaches.find(fCity) == fCaches.end();
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
        } else if (rule.needs == kNeedsAlchemist) {
            // file 0xA4368: in towns of size 3 or less where the city's
            // property 0x21 is even, of size 4 where it divides by 3
            const int number = int16(_PeopleSeed());
            hide = (c.size <= 3 && number % 2 == 0)
                || (c.size == 4 && number % 3 == 0);
        } else if (rule.needs == kNeedsSneak) {
            hide = _Marked(kMarkSneakFailed);
        } else if (rule.needs == kNeedsBribe) {
            hide = _Marked(kMarkBribeRefused) || fParty == NULL
                || TotalPfennigs(fParty->cash) < _Bribe();
        } else if (rule.needs == kNeedsFine) {
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < _Fine();
        } else if (rule.needs == kNeedsToll) {
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < _Toll();
        } else if (rule.needs == kNeedsCharm) {
            hide = _Marked(kMarkCharmFailed) || _Marked(kMarkWanted);
        } else if (rule.needs == kNeedsSlip) {
            hide = _Marked(kMarkSlipFailed);
        } else if (rule.needs == kNeedsHail) {
            hide = _Marked(kMarkHailFailed);
        } else if (rule.needs == kNeedsTalk) {
            hide = _Marked(kMarkTalkFailed);
        } else if (rule.needs == kNeedsWallBribe) {
            hide = fParty == NULL
                || TotalPfennigs(fParty->cash) < _WallBribe();
        } else if (rule.needs == kNeedsRope || rule.needs == kNeedsNoRope
                || rule.needs == kNeedsClimb) {
            // 0E76:0C76(-2, 0x3B): a rope (item 59) in the party
            bool rope = false;
            if (fParty != NULL) {
                for (const character& member : fParty->members) {
                    for (const item& carried : member.items)
                        rope = rope || (carried.code & 0x0FFF) == kRopeCode;
                }
            }
            const bool day = fScreen == SCREEN_DAY_WALL;
            hide = fWallFailed || (day && _Marked(kMarkWallAlert));
            if (rule.needs == kNeedsRope)
                hide = hide || !rope;
            else if (rule.needs == kNeedsNoRope)
                hide = hide || rope;
        } else if (rule.needs == kNeedsGrate) {
            hide = _Marked(kMarkGrateFailed);
        } else if (rule.needs == kNeedsNightBribe) {
            hide = fParty == NULL
                || TotalPfennigs(fParty->cash) < _NightBribe();
        } else if (rule.needs == kNeedsStone) {
            hide = fStoneOffered || fParty == NULL
                || TotalPfennigs(fParty->cash) < _StonePrice();
        } else if (rule.needs == kNeedsMaster)
            hide = _AlchemistSkill(true) < 25;
        else if (rule.needs == kNeedsWounded)
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


// The alchemist's skill (file 0xD9BBB): city size · 3 + (property 0x21
// + 4) % 41, and (property 0x21 + 9) % 11 + 10 more in a city with the
// flag 0x100 (0E76:1A7E(0x1B)); the shop (file 0xDA0CA) leaves this out
int
CityVisit::_AlchemistSkill(bool withBonus) const
{
    const city& c = fData.Cities().CityAt(uint32(fCity));
    int skill = c.size * 3 + (_PeopleSeed() + 4) % 41;
    if (withBonus && (c.flags & 0x100) != 0)
        skill += (_PeopleSeed() + 9) % 11 + 10;
    return skill;
}


// The stone he can make (file 0xDA00C): (property 0x21 + 4) % 25 + 1
int
CityVisit::_StoneQuality() const
{
    return (_PeopleSeed() + 4) % 25 + 1;
}


// Its price, in groschen (file 0xD9C62): (skill / 2 + 1) · (quality + 5)
// / 12, within 1..50 + (the seed global + location) % 20
uint32
CityVisit::_StonePrice() const
{
    const int limit = 50 + (fSeed + fCity) % 20;
    const int groschen = (_AlchemistSkill(true) / 2 + 1)
        * (_StoneQuality() + 5) / 12;
    return uint32(std::max(1, std::min(groschen, limit))) * 12;
}


// Whether he deigns to deal (file 0xD9F6C): random(100) at most the
// leader's charisma + reputation / 10 + property 0x21 % 31 + the
// leader's Speak Common / 3 + the best Alchemy / 2 - 15, within 0..99
int
CityVisit::_AlchemistChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[fParty->leader];
    const int chance = leader.attributes[ATTRIBUTE_CHARISMA]
        + _Reputation() / 10 + _PeopleSeed() % 31
        + leader.skills[kSkillSpeakCommon] / 3
        + _BestSkill(kSkillAlchemy) / 2 - 15;
    return std::max(0, std::min(chance, 99));
}


int
CityVisit::_BestSkill(int skill) const
{
    int best = 0;
    if (fParty != NULL) {
        for (const character& member : fParty->members)
            best = std::max(best, int(member.skills[skill]));
    }
    return best;
}


// A better philosopher's stone (file 0xD9FFE): offended if the chance
// fails (card 6, and no dealings for 60 hours); else the stone becomes
// his quality, paid, if it is better (card 7), or card 5
int
CityVisit::_Stone()
{
    fStoneOffered = true;
    if (int(fRandom() % 100) > _AlchemistChance()) {
        if (fClock != NULL)
            fAlchemistAngryUntil[fCity] = fClock->HourStamp() + 60;
        return SCREEN_ALCHEMIST_ANGRY;
    }
    const int quality = _StoneQuality();
    if (fParty == NULL || fParty->philosopherStone >= quality)
        return SCREEN_STONE_BEYOND;
    const uint32 price = _StonePrice();
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - price);
    fParty->philosopherStone = uint16(quality);
    fVariables["Number1"] = std::to_string(quality);
    fVariables["Money1"] = MoneyText(price);
    return SCREEN_STONE_IMPROVED;
}


// Purchasing (file 0xDA0CA): offended if the chance fails; else the trade
// screen, with potions if his skill (without the bonus) is over 24, else
// alchemical components
int
CityVisit::_AlchemistShop()
{
    if (int(fRandom() % 100) > _AlchemistChance()) {
        if (fClock != NULL)
            fAlchemistAngryUntil[fCity] = fClock->HourStamp() + 60;
        return SCREEN_ALCHEMIST_ANGRY;
    }
    fPendingTrade = _AlchemistSkill(false) > 24 ? MERCHANT_ALCHEMIST
        : MERCHANT_ALCHEMIST_COMPONENTS;
    return SCREEN_ALCHEMIST_AGAIN;
}


bool
CityVisit::_Marked(int kind) const
{
    const std::map<std::pair<int, int>, uint32>::const_iterator mark
        = fMarks.find(std::make_pair(kind, fCity));
    return mark != fMarks.end() && fClock != NULL
        && fClock->HourStamp() < mark->second;
}


// 0E76:2930 makes a mark for `hours`; 0E76:2A32 (extend) adds them to a
// mark still running
void
CityVisit::_Mark(int kind, uint32 hours, bool extend)
{
    if (fClock == NULL)
        return;
    uint32& until = fMarks[std::make_pair(kind, fCity)];
    const uint32 now = fClock->HourStamp();
    until = (extend && until > now ? until : now) + hours;
}


// The guards' price (file 0xA0CDC): max(4, city size - reputation / 10)
// · the party's size · 24 pfennigs
uint32
CityVisit::_Bribe() const
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int each = std::max(4, size - _Reputation() / 10);
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32(std::max(each * count * 24, 48));
}


// The watch's fine (file 0xBF160): (city size - reputation / 50 + the
// florins in the purse + 1) · the party's size, in pfennigs
uint32
CityVisit::_Fine() const
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int florins = fParty != NULL ? fParty->cash.florins : 0;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32(std::max(1, (size - _Reputation() / 50 + florins + 1) * count));
}


// Sneaking's chance (file 0xA109A): from 100, each member in turn brings
// it down to his Stealth if lower, then adds 30; 20 less when the market
// is watched
int
CityVisit::_SneakChance() const
{
    int chance = 100;
    if (fParty != NULL) {
        for (const character& member : fParty->members) {
            chance = std::min(chance, int(member.skills[kSkillStealth]));
            chance += 30;
        }
    }
    if (_Marked(kMarkGuarded))
        chance -= 20;
    return std::max(0, std::min(chance, 99));
}


// The member who falls behind (0E76:0656): the lowest agility (the game
// lowers it by the load carried: not kept here)
int
CityVisit::_Slowest() const
{
    int slowest = 0;
    for (size_t i = 1; fParty != NULL && i < fParty->members.size(); i++) {
        if (fParty->members[i].attributes[ATTRIBUTE_AGILITY]
                < fParty->members[slowest].attributes[ATTRIBUTE_AGILITY])
            slowest = int(i);
    }
    return slowest;
}


// Sneaking past the guards (file 0xA0F96): into the offices (state
// 0x101, not implemented) with a lesson in Stealth; else, if the roll is
// under twice the chance and 95, the watch only hears you (card 2, an
// hour, the side streets, the market watched 72 hours more); else the
// guards come (card 1, then the watch), no sneaking for 12 hours
int
CityVisit::_Sneak()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int chance = _SneakChance();
    const int roll = random(100);
    if (roll <= chance) {
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStealth, 1, 10, random);
        return SCREEN_NOT_IMPLEMENTED;
    }
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStealth, 0, 10, random);
    if (roll < 2 * chance && roll < 95) {
        _Mark(kMarkGuarded, 72, true);
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_MARKET_STUMBLE;
    }
    _Mark(kMarkSneakFailed, 12);
    _Mark(kMarkGuarded, 32, true);
    fWatchReturn = SCREEN_NOT_IMPLEMENTED;		// the offices
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(_Slowest());
    return SCREEN_MARKET_ALARM;
}


// Bribing (file 0xA10FC): taken unless the local reputation is -10 or
// less (card 9, into the offices); else refused (card 10), the market
// watched 72 hours more, and the watch
int
CityVisit::_BribeGuards()
{
    if (_Reputation() > -10 && fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _Bribe());
        return SCREEN_MARKET_BRIBED;
    }
    _Mark(kMarkGuarded, 72, true);
    fWatchReturn = SCREEN_NOT_IMPLEMENTED;
    return SCREEN_MARKET_REFUSED;
}


// Paying the fine (file 0xBF5C0): an hour, then on as before
int
CityVisit::_PayFine()
{
    if (fParty != NULL)
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _Fine());
    if (fClock != NULL)
        fClock->AddHours(1);
    return fWatchReturn;
}


// Running (file 0xBF61A): if random(100) is at most (the slowest member's
// agility + the best Streetwise) / 2, the party escapes (the local
// reputation falls by 1 with a chance of 100 - |reputation| %, an hour,
// card 3, the side streets); else the slowest falls behind (card 4, no
// running again). Either way a small chance of Streetwise.
int
CityVisit::_RunFromWatch()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int slowest = _Slowest();
    const int chance = fParty == NULL || fParty->members.empty() ? 0
        : (fParty->members[slowest].attributes[ATTRIBUTE_AGILITY]
            + _BestSkill(kSkillStreetwise)) / 2;
    if (random(100) <= chance) {
        if (fReputations != NULL && fCity >= 0
                && fCity < int(fReputations->size())) {
            int16& reputation = (*fReputations)[fCity];
            if (random(100) <= 100 - std::abs(int(reputation)))
                reputation = int16(std::max(-99, reputation - 1));
        }
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStreetwise, 7, 5, random);
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_WATCH_ESCAPED;
    }
    if (fParty != NULL) {
        TrainParty(*fParty, kSkillStreetwise, 0, 5, random);
        if (!fParty->members.empty())
            _SetChosen(slowest);
    }
    return SCREEN_NIGHT_WATCH_CAUGHT;
}


// To the main gate (file 0x94398, 0x94444): by day, at night waiting
// for dawn (7 o'clock, card 1); at night, by day waiting for the night
// (20 o'clock, card 2)
int
CityVisit::_GoToGate(bool byDay)
{
    const bool day = fClock == NULL || IsGameDay(*fClock);
    if (byDay == day || fClock == NULL)
        return byDay ? SCREEN_DAY_GATE : SCREEN_NIGHT_GATE;
    const int now = fClock->Hour() * 60 + fClock->Minute();
    const int until = (byDay ? 7 : 20) * 60;
    fClock->AddMinutes(uint32((until - now + 24 * 60) % (24 * 60)));
    return byDay ? SCREEN_WAIT_DAWN : SCREEN_WAIT_NIGHT;
}


// The toll (file 0x92642): (city size / 3 + 1) pfennigs per member
uint32
CityVisit::_Toll() const
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32((size / 3 + 1) * count);
}


// Paying's chance (file 0x92916): none for the wanted (mark 0x11) or a
// reputation of -10 or less; else 100, or 100 + the (negative)
// reputation, twice while the guards are nervous (mark 0x12), within
// 1..99
int
CityVisit::_TollChance() const
{
    const int reputation = _Reputation();
    if (reputation <= -10 || _Marked(kMarkWanted))
        return 0;
    if (reputation >= 0)
        return 100;
    const int factor = _Marked(kMarkAlert) ? 2 : 1;
    return std::max(1, std::min(99, 100 + factor * reputation));
}


// Befriending the guards' chance (file 0x92A62): none as for paying;
// else the reputation / 2 + the leader's Charisma or Speak Common,
// whichever is higher (1367:0084), within 1..99
int
CityVisit::_CharmChance() const
{
    if (_Reputation() <= -10 || _Marked(kMarkWanted) || fParty == NULL
            || fParty->members.empty()) {
        return 0;
    }
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int best = std::max(int(leader.attributes[ATTRIBUTE_CHARISMA]),
        int(leader.skills[kSkillSpeakCommon]));
    return std::max(1, std::min(99, _Reputation() / 2 + best));
}


// Slipping in's chance (file 0x92B8A): (the party's average speed,
// 0E76:060E, + average Streetwise, 0E76:1600) / 2, halved for the
// wanted, within 1..99. The speed is the agility lowered by the load,
// which is not kept here: the agility.
int
CityVisit::_SlipChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 1;
    int agility = 0;
    int streetwise = 0;
    for (const character& member : fParty->members) {
        agility += member.attributes[ATTRIBUTE_AGILITY];
        streetwise += member.skills[kSkillStreetwise];
    }
    const int count = int(fParty->members.size());
    int chance = (agility / count + streetwise / count) / 2;
    if (_Marked(kMarkWanted))
        chance /= 2;
    return std::max(1, std::min(99, chance));
}


// Paying (file 0x928B8): if random(100) is at most the chance, the toll,
// an hour, card 1 and the main street; else the guards recognize the
// party (state 1, $CHALL00: not implemented)
int
CityVisit::_PayToll()
{
    if (int(fRandom() % 100) > _TollChance() || _Marked(kMarkWanted))
        return SCREEN_NOT_IMPLEMENTED;
    if (fParty != NULL)
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _Toll());
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_TOLL_PAID;
}


// A change of the local reputation by low..high (0E76:19D0), taken with
// a chance of 100 - |reputation| %
void
CityVisit::_ChangeReputation(int low, int high)
{
    if (fReputations == NULL || fCity < 0 || fCity >= int(fReputations->size()))
        return;
    int16& reputation = (*fReputations)[fCity];
    if (int(fRandom() % 100) > 100 - std::abs(int(reputation)))
        return;
    const int change = low + (high > low ? int(fRandom() % uint32(high - low + 1))
        : 0);
    reputation = int16(std::max(-99, std::min(99, reputation + change)));
}


// Befriending the guards (file 0x9298A): the wanted or disliked are
// recognized (state 1, not implemented); if random(100) is at most the
// chance, card 2, a lesson in Speak Common for the leader (1462:0132
// mode 1), the reputation up by 1, two hours and the main street; else
// no more tries for 12 hours (mark 0x0A), card 3, an hour, the gate
// (the leader's lesson of mode 0 then is not reproduced)
int
CityVisit::_CharmGuards()
{
    if (_Reputation() <= -10 || _Marked(kMarkWanted))
        return SCREEN_NOT_IMPLEMENTED;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) <= _CharmChance()) {
        if (fParty != NULL && !fParty->members.empty()) {
            TrainSkill(fParty->members[size_t(fParty->leader)],
                kSkillSpeakCommon, 10, random);
        }
        _ChangeReputation(1, 1);
        if (fClock != NULL)
            fClock->AddHours(2);
        return SCREEN_GUARDS_CHARMED;
    }
    _Mark(kMarkCharmFailed, 12);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_GUARDS_UNMOVED;
}


// Slipping in with the crowd (file 0x92ADE): if random(100) is at most
// the chance, card 4, a lesson in Streetwise for all (1462:0132(-2, 16,
// 1, 10)), an hour, the main street; else no more tries for 12 hours
// (mark 0x0B), a lesson of mode 0, and card 5 back before the walls, or
// for the wanted an hour and state 1 (not implemented)
int
CityVisit::_SlipIn()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) <= _SlipChance()) {
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStreetwise, 1, 10, random);
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_SLIPPED_IN;
    }
    _Mark(kMarkSlipFailed, 12);
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStreetwise, 0, 10, random);
    if (_Marked(kMarkWanted)) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_NOT_IMPLEMENTED;
    }
    return SCREEN_SLIP_NOTICED;
}


// The gate at night's bribe (file 0x934C6): (city size / 3 + 1) · the
// party's size (DS:A67E) · 18 / 10 pfennigs; for a party with a
// negative local reputation (100 - reputation) / 33 pfennigs instead, as
// the game has it
uint32
CityVisit::_NightBribe() const
{
    const int reputation = _Reputation();
    if (reputation < 0)
        return uint32((100 - reputation) / 33);
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32((size / 3 + 1) * count * 18 / 10);
}


// Hailing the watch (file 0x936C0): recognized (card 3, no time) with a
// reputation of -10 or less; else let in (card 1, no time, the main
// street) if random(100) is under the reputation / 2 + the fame within
// 0..100 (file 0x937A8, 1367:0028); else no more tries for 12 hours
// (mark 0x0C), card 2 and an hour
int
CityVisit::_HailWatch()
{
    const int reputation = _Reputation();
    if (reputation <= -10)
        return SCREEN_NIGHT_GATE_ALARM;
    const int fame = fParty != NULL ? std::min(100, int(fParty->fame)) : 0;
    const int chance = std::max(0, reputation / 2 + fame);
    if (int(fRandom() % 100) < chance)
        return SCREEN_NIGHT_GATE_OPENED;
    _Mark(kMarkHailFailed, 12);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_NIGHT_GATE_SHUT;
}


// Talking the way inside (file 0x9380A): recognized with a reputation of
// -40 or less; else through a sally port (card 4, a lesson in Speak
// Common for the leader, an hour, the side streets) if random(100) is
// under (the leader's Speak Common + 2 · Intelligence) / 2 (file
// 0x938EE); else no more tries for 12 hours (mark 0x0D), card 2 and an
// hour. The game's lesson here has mode 7 (and 0 on failure), taken as
// the usual one; the failure's is not reproduced.
int
CityVisit::_TalkToWatch()
{
    if (_Reputation() <= -40)
        return SCREEN_NIGHT_GATE_ALARM;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    int chance = 0;
    if (fParty != NULL && !fParty->members.empty()) {
        const character& leader = fParty->members[size_t(fParty->leader)];
        chance = (leader.skills[kSkillSpeakCommon]
            + 2 * leader.attributes[ATTRIBUTE_INTELLIGENCE]) / 2;
    }
    if (random(100) < chance) {
        if (fParty != NULL && !fParty->members.empty()) {
            TrainSkill(fParty->members[size_t(fParty->leader)],
                kSkillSpeakCommon, 10, random);
        }
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_NIGHT_GATE_TALKED;
    }
    _Mark(kMarkTalkFailed, 12);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_NIGHT_GATE_SHUT;
}


// Bribing the watchman (file 0x93948): recognized with a reputation of
// -10 or less; else paid (card 5, an hour, the side streets)
int
CityVisit::_BribeWatch()
{
    if (_Reputation() <= -10)
        return SCREEN_NIGHT_GATE_ALARM;
    if (fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
            - _NightBribe());
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_NIGHT_GATE_BRIBED;
}


// To the wall (file 0x944E6, 0x9457A): searching for the best spot
// takes city size / 2 + 1 hours by day (card 3 and a wait for dawn if
// the night came meanwhile), size / 2 + 2 at night (the game waits for
// midnight when it is before it, card 4 by day)
int
CityVisit::_GoToWall(bool byDay)
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    if (fClock == NULL)
        return byDay ? SCREEN_DAY_WALL : SCREEN_NIGHT_WALL;
    if (byDay) {
        fClock->AddHours(uint32(size / 2 + 1));
        if (IsGameDay(*fClock))
            return SCREEN_DAY_WALL;
        const int now = fClock->Hour() * 60 + fClock->Minute();
        fClock->AddMinutes(uint32((7 * 60 - now + 24 * 60) % (24 * 60)));
        return SCREEN_WALL_DAWN;
    }
    const int hours = size / 2 + 2;
    const int hour = fClock->Hour();
    if (hour + hours <= 24 && hour != 0) {
        const int now = hour * 60 + fClock->Minute();
        fClock->AddMinutes(uint32(24 * 60 - now));
        return SCREEN_WALL_DUSK;
    }
    fClock->AddHours(uint32(hours));
    return SCREEN_NIGHT_WALL;
}


// The wall's bribe (file 0x99C1E): (city size / 3 + 1) · the party's
// size · 14 / 10 pfennigs, or (100 - reputation) / 33 for a negative
// reputation, as at the gate by night
uint32
CityVisit::_WallBribe() const
{
    const int reputation = _Reputation();
    if (reputation < 0)
        return uint32((100 - reputation) / 33);
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32((size / 3 + 1) * count * 14 / 10);
}


// A climber's score (file 0x99BCE): (2 · speed + Stealth) / 2, the speed
// being the agility (the load is not kept)
int
CityVisit::_ClimberScore(int member) const
{
    const character& c = fParty->members[size_t(member)];
    return (2 * c.attributes[ATTRIBUTE_AGILITY] + c.skills[kSkillStealth]) / 2;
}


// The best climber from `first` on (file 0x99BB9, 0x9A87C), the first
// member being the default
int
CityVisit::_BestClimber(int first) const
{
    int best = 0;
    if (fParty == NULL || fParty->members.empty())
        return 0;
    int score = _ClimberScore(0);
    for (int i = first; i < int(fParty->members.size()); i++) {
        if (_ClimberScore(i) > score) {
            score = _ClimberScore(i);
            best = i;
        }
    }
    return best;
}


// Climbing alone's chance (file 0x9A210): the weakest climber's score
int
CityVisit::_WeakestClimber() const
{
    int weakest = 0;
    if (fParty == NULL)
        return 0;
    for (int i = 1; i < int(fParty->members.size()); i++) {
        if (_ClimberScore(i) < _ClimberScore(weakest))
            weakest = i;
    }
    return weakest;
}


// The strongest member (file 0x9AF58), who tries the sewer's grate
int
CityVisit::_Strongest() const
{
    int strongest = 0;
    if (fParty == NULL)
        return 0;
    for (int i = 1; i < int(fParty->members.size()); i++) {
        if (fParty->members[size_t(i)].attributes[ATTRIBUTE_STRENGTH]
                > fParty->members[size_t(strongest)].attributes[ATTRIBUTE_STRENGTH])
            strongest = i;
    }
    return strongest;
}


// A fall (1462:026A(member, 0, 1, 10), file 0x80C0A): Strength loses
// random(10 · Strength / 40 + 1) - 1, at least 0; Endurance random(10 ·
// Endurance / 20 + 1) - 1, at least that and at least 1; neither more
// than the attribute's maximum (0E76:0B64); AddToAttribute() keeps them
// at 1 at least
void
CityVisit::_Fall(int member)
{
    character& c = fParty->members[size_t(member)];
    const int strength = c.attributes[ATTRIBUTE_STRENGTH];
    int lost = int(fRandom() % uint32(10 * strength / 40 + 1)) - 1;
    lost = std::min(std::max(lost, 0), int(c.maxAttributes[ATTRIBUTE_STRENGTH]));
    AddToAttribute(c, ATTRIBUTE_STRENGTH, -lost);
    const int endurance = c.attributes[ATTRIBUTE_ENDURANCE];
    int wounds = int(fRandom() % uint32(10 * endurance / 20 + 1)) - 1;
    wounds = std::min(std::max(std::max(wounds, lost), 1),
        int(c.maxAttributes[ATTRIBUTE_ENDURANCE]));
    AddToAttribute(c, ATTRIBUTE_ENDURANCE, -wounds);
}


// Bribing a guard at a door (file 0x99E76): paid, card 1, an hour, the
// side streets
int
CityVisit::_BribeWall()
{
    if (fParty != NULL)
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _WallBribe());
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_DAY_WALL_BRIBED;
}


// Up the rope (file 0x99F0E, 0x9AAC4): the best climber
// ($ChosenOneName) climbs if random(100) is under his score: card 3 (1
// at night), a lesson in Stealth for him, an hour, the side streets;
// else he falls (card 4, or 2), a lesson of mode 0 (not reproduced), an
// hour, and no more climbing this stay
int
CityVisit::_ClimbWithRope(bool byDay)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_OUTSIDE;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    // by day the game looks from the first member, at night from the
    // second with the first as the default: the same one
    const int climber = _BestClimber(1);
    _SetChosen(climber);
    if (fClock != NULL)
        fClock->AddHours(1);
    if (random(100) < _ClimberScore(climber)) {
        TrainSkill(fParty->members[size_t(climber)], kSkillStealth, 10, random);
        return byDay ? SCREEN_DAY_WALL_ROPE : SCREEN_NIGHT_WALL_ROPE;
    }
    _Fall(climber);
    fWallFailed = true;
    return byDay ? SCREEN_DAY_WALL_FALL : SCREEN_NIGHT_WALL_FALL;
}


// Everybody climbing alone (file 0x9A024, 0x9ABD4): if random(100) is
// under the weakest one's score, all are up (card 11, 3 at night), a
// lesson in Stealth for all, an hour, the side streets; else each whose
// score is at most that roll falls (card 12, 11 at night; hurt as by
// _Fall()), those up come back down to help (card 13, 12), an hour, and
// no more climbing this stay. The game shows the card of every fallen
// and every climber; here the first fallen's, and one for those up.
int
CityVisit::_ClimbAlone(bool byDay)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_OUTSIDE;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int weakest = _WeakestClimber();
    const int roll = random(100);
    if (fClock != NULL)
        fClock->AddHours(1);
    if (roll < _ClimberScore(weakest)) {
        TrainParty(*fParty, kSkillStealth, 7, 10, random);
        return byDay ? SCREEN_DAY_WALL_CLIMBED : SCREEN_NIGHT_WALL_CLIMBED;
    }
    int fallen = -1;
    int up = -1;
    for (int i = 0; i < int(fParty->members.size()); i++) {
        if (_ClimberScore(i) <= roll) {
            _Fall(i);
            if (fallen < 0)
                fallen = i;
        } else if (up < 0)
            up = i;
    }
    fWallFailed = true;
    _SetChosen(fallen >= 0 ? fallen : weakest);
    if (up < 0)
        return byDay ? SCREEN_DAY_WALL_SLIP_ALONE : SCREEN_NIGHT_WALL_SLIP_ALONE;
    return byDay ? SCREEN_DAY_WALL_SLIP : SCREEN_NIGHT_WALL_SLIP;
}


// The sewer's grate (file 0x9AE3A): the strongest ($ChosenOneName) loses
// 3 Endurance; if random(100) is under 1.6 · his Strength (file
// 0x9AF58): card 4, a positive local reputation falls by 2..4 (the
// smell?), city size / 3 hours, the side streets; else card 5, an hour,
// and a mark 0x10 made by 0E76:2D5C(..., 0x10, 3, 99...) (taken as 3
// hours without trying again)
int
CityVisit::_ForceGrate()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_OUTSIDE;
    const int strongest = _Strongest();
    character& c = fParty->members[size_t(strongest)];
    _SetChosen(strongest);
    AddToAttribute(c, ATTRIBUTE_ENDURANCE, -3);
    if (int(fRandom() % 100) < c.attributes[ATTRIBUTE_STRENGTH] * 16 / 10) {
        if (_Reputation() > 0)
            _ChangeReputation(-4, -2);
        if (fClock != NULL)
            fClock->AddHours(uint32(fData.Cities().CityAt(uint32(fCity)).size / 3));
        return SCREEN_SEWER;
    }
    _Mark(kMarkGrateFailed, 3);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_SEWER_STUCK;
}


// Attacking the watch (file 0xBF914): they flee (card 7) if random(100)
// is at most the chance: |reputation| / 10 + the party's average
// Charisma / 10 (0E76:1800) + the leader's best weapon skill / 2
// (0E76:01C0) + the fame / 20 (0E76:1326), 0 under 75, at most 90. Else
// (file 0xBF3A2) the local reputation falls by 15..24 with a chance of
// 100 - |reputation| % (0E76:19D0), and the battle begins.
int
CityVisit::_FightWatch()
{
    if (fParty == NULL || fParty->members.empty())
        return fWatchReturn;
    int charisma = 0;
    for (const character& member : fParty->members)
        charisma += member.attributes[ATTRIBUTE_CHARISMA];
    charisma /= int(fParty->members.size());
    const character& leader = fParty->members[size_t(fParty->leader)];
    int weaponSkill = 0;
    for (int s = 0; s < kWeaponSkillCount; s++)
        weaponSkill = std::max(weaponSkill, int(leader.skills[s]));
    int chance = std::abs(_Reputation()) / 10 + charisma / 10
        + weaponSkill / 2 + fParty->fame / 20;
    if (chance < 75)
        chance = 0;
    chance = std::min(chance, 90);
    if (int(fRandom() % 100) <= chance)
        return SCREEN_WATCH_SCARED;

    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        if (int(fRandom() % 100) <= 100 - std::abs(int(reputation))) {
            reputation = int16(std::max(-99,
                reputation - 15 - int(fRandom() % 10)));
        }
    }
    fPendingBattle = true;
    return -1;
}


// The battle with the watch (file 0xBF3A2): die(5) + 3 of enemy 3 (the
// "Guard" types) at variant 1 and one of enemy 0 ("Sergeant") at
// variant 2, as TAC.TXT prints them. On a city map: which one the game
// picks (from the battlefield type) and where everybody starts are not
// decoded, so the map is one of ICITY.000..003 and the watch starts
// near the party.
void
CityVisit::_RunBattle(GameWindow& window)
{
    fPendingBattle = false;
    BattleView view(fData);
    {
        std::unique_ptr<Catalog> maps(fData.OpenCatalog("IMAPS.CAT"));
        const std::string name = "ICITY.00" + std::to_string(fRandom() % 4);
        std::unique_ptr<Stream> stream(maps->GetStream(name));
        if (!stream)
            throw std::runtime_error("CityVisit: no map " + name);
        view.SetMap(std::unique_ptr<BattleMap>(new BattleMap(stream.get())),
            name);
    }
    for (size_t i = 0; i < fParty->members.size(); i++) {
        int x = 12;
        int y = 20;
        if (view.FindFreeCell(x, y)) {
            view.AddPartyMember(int(i), fParty->members[i], fParty->images[i],
                i < fParty->colors.size() ? fParty->colors[i]
                    : std::vector<uint8>(), x, y, 2);
        }
    }
    const EnemyFile& enemies = fData.Enemies();
    const uint32 guard = enemies.EnemyAt(3).type + 1;
    const uint32 sergeant = enemies.EnemyAt(0).type + 2;
    const int guards = int(fRandom() % 5) + 4;
    for (int i = 0; i <= guards; i++) {
        int x = 20;
        int y = 20;
        if (view.FindFreeCell(x, y))
            view.AddEnemy(i < guards ? guard : sergeant, x, y, 6);
    }
    view.Scroll(0, 12);
    const battle_outcome outcome = view.Run(window);

    // the wounded keep their wounds, the dead leave the party; with
    // nobody left the game is over (the game's end sequence,
    // 09C0:18F1(0x12), is not decoded)
    std::vector<fighter> fighters;
    for (size_t i = 0; i < fParty->members.size(); i++)
        fighters.push_back(view.FigureFighter(int(i)));
    AfterBattle(*fParty, fighters);
    if (fParty->members.empty()) {
        fPartyLost = true;
        return;
    }
    // the winners loot the bodies ("Loot Bodies" in the battle's menu)
    if (outcome == BATTLE_WON) {
        fLoot = view.Loot();
        fTrade.SetPlace(fCity, _Reputation());
        fTrade.SetLoot(&fLoot, money{ 0, 0, 0 });
        fTrade.Run(window);
        fLoot.clear();
    }
    ResolveBattle(outcome);
}


// file 0xBF43C: won, card 8 then on as before; lost, card 11 (the
// dungeon); fled, card 10
void
CityVisit::ResolveBattle(int outcome)
{
    fPendingBattle = false;
    switch (outcome) {
        case BATTLE_WON:
            _Show(SCREEN_WATCH_BEATEN);
            break;
        case BATTLE_LOST:
            _Show(SCREEN_WATCH_PRISON);
            break;
        default:
            _Show(SCREEN_WATCH_RETREAT);
            break;
    }
}


// $ChosenOneName and the pronouns that go with it ($he, $his...)
void
CityVisit::_SetChosen(int member)
{
    if (fParty == NULL || member < 0 || member >= int(fParty->members.size()))
        return;
    const character& chosen = fParty->members[member];
    fVariables["ChosenOneName"] = chosen.shortName;
    const bool female = chosen.female;
    fVariables["he"] = female ? "she" : "he";
    fVariables["He"] = female ? "She" : "He";
    fVariables["his"] = female ? "her" : "his";
    fVariables["His"] = female ? "Her" : "His";
    fVariables["him"] = female ? "her" : "him";
    fVariables["himself"] = female ? "herself" : "himself";
}


// Walking at night unseen (file 0xA4596): the party's average Stealth
// (0E76:1600) + the best Streetwise - a hazard, within 1..99; the hazard
// (1462:0000(15, 1, 5)) is random(14) within 1..15, more where the
// location's state or marks say so (not kept here)
int
CityVisit::_NightWalkChance()
{
    if (fParty == NULL || fParty->members.empty())
        return 1;
    int stealth = 0;
    for (const character& member : fParty->members)
        stealth += member.skills[kSkillStealth];
    stealth /= int(fParty->members.size());
    const int hazard = std::max(1, std::min(int(fRandom() % 14), 15));
    return std::max(1, std::min(stealth + _BestSkill(kSkillStreetwise)
        - hazard, 99));
}
