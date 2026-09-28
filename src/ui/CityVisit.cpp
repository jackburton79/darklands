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
#include "TextSupport.h"

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
    ACTION_GUARDS_FIGHT,		// the guards who recognize a wanted party
    ACTION_GUARDS_TALK,
    ACTION_GUARDS_BRIBE,
    ACTION_CHALLENGE_RETURN,	// back where the guards met the party
    ACTION_TO_PRISON,			// the dungeon
    ACTION_BACK_TO_CELL,
    ACTION_PICK_LOCK,
    ACTION_CLIMB_WINDOW,
    ACTION_DIG,
    ACTION_SEDUCE,
    ACTION_PRAY,
    ACTION_AFTER_PRAYER,		// the magistrate, or the cell
    ACTION_WAIT_MAGISTRATE,
    ACTION_JAIL_FIGHT,			// the guardroom
    ACTION_TO_COURT,			// the magistrate
    ACTION_KEEP_SILENT,
    ACTION_PLEAD_INNOCENT,
    ACTION_CONFESS_GUILT,
    ACTION_TO_EXECUTION,		// the execution
    ACTION_SUBMIT,
    ACTION_BREAK_ROPES,
    ACTION_RESCUE,				// the rescues' roll
    ACTION_EXECUTION_FIGHT,
    ACTION_MOB_FIGHT,
    ACTION_TO_CHASE,			// the chase
    ACTION_CHALLENGE_RUN,
    ACTION_CHASE_RUN,
    ACTION_CHASE_FIGHT,
    ACTION_CHASE_AMBUSH,
    ACTION_CHASE_HIDE,
    ACTION_SAINT,				// invoke one of the card's saints
    ACTION_SAINT_RESCUE,		// the rescues after a saint's answer
    ACTION_EXIT_WALK,			// the gate from inside
    ACTION_EXIT_HIDE,
    ACTION_EXIT_FIGHT,
    ACTION_GATE_RETURN,			// "not leave the city just yet"
    ACTION_AFTER_SHOUT,			// the guards alerted: fight, or the gate
    ACTION_INNER_SEWER,			// the wall from inside; target 1: with
    ACTION_INNER_BRIBE,			// horses
    ACTION_INNER_ROPE,
    ACTION_INNER_CLIMB,
    ACTION_INNER_AFTER_DARK,
    ACTION_SALLY_CHALLENGE,
    ACTION_CALL_PRIEST,			// the priest in the dungeon
    ACTION_PRIEST_CONFESSION,
    ACTION_PRIEST_HELP,
    ACTION_PRIEST_GOOD_WORD,
    ACTION_AFTER_GOOD_WORD,		// the magistrate, or the cell
    ACTION_FROM_PRIEST,
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
// or the guards who recognize the party: their bribe in the purse, the
// party come from the side streets
static const int kNeedsGuardsBribe	= -29;
// or the dungeon: lockpicks (item 64), a woman standing
static const int kNeedsLockpicks	= -30;
static const int kNeedsWoman		= -31;
static const int kLockpickCode		= 64;	// in DARKLAND.LST
static const int kDaggerCode		= 7;
static const int kClubCode			= 15;
static const int kSkillArtifice		= 14;	// picking locks
// or a member standing who knows one of the card's saints (150B:168C)
static const int kNeedsSaint		= -32;
// or the wall from inside: by the option (horses, a rope, the marks)
static const int kNeedsInnerWall	= -33;
static const int kRopeCode			= 59;	// in DARKLAND.LST

// The game's timed marks used here (0E76:2930, 2A32)
static const int kMarkCharmFailed	= 0x0A;	// the gate's guards
static const int kMarkSlipFailed	= 0x0B;
static const int kMarkHailFailed	= 0x0C;	// the gate at night
static const int kMarkTalkFailed	= 0x0D;
static const int kMarkWallAlert		= 0x0F;	// the wall by day: guarded
static const int kMarkGrateFailed	= 0x10;
static const int kMarkWanted		= 0x11;	// after fighting the guards
static const int kMarkAlert			= 0x12;	// the gate's guards nervous
static const int kMarkGateFought	= 0x13;	// a fight at the gate lately
static const int kMarkSallyAlarm	= 0x22;	// the sally port's guard
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
        TODO,								/* potion */ \
        DO_IF(ACTION_SAINT, kNeedsSaint), \
        TODO,								/* attack */ \
        GO(SCREEN_OUTSIDE)					/* reconsider */ \
    }
#define WATCH_OPTIONS { \
        DO_IF(ACTION_PAY_FINE, kNeedsFine),	/* the fine of $Money1 */ \
        DO(ACTION_RUN),						/* run away */ \
        TODO,								/* potion */ \
        DO_IF(ACTION_SAINT, kNeedsSaint), \
        DO(ACTION_FIGHT)					/* attack them */ \
    }
#define CELL_OPTIONS { \
        DO_IF(ACTION_PICK_LOCK, kNeedsLockpicks), \
        DO(ACTION_CLIMB_WINDOW), \
        DO(ACTION_DIG),						/* with a spoon */ \
        DO_IF(ACTION_SEDUCE, kNeedsWoman),	/* the turnkey */ \
        DO(ACTION_CALL_PRIEST), \
        DO(ACTION_PRAY), \
        DO_IF(ACTION_SAINT, kNeedsSaint),	/* invoke a saint */ \
        DO(ACTION_WAIT_MAGISTRATE), \
        TODO								/* acid on the lock */ \
    }
#define COURT_OPTIONS { \
        DO(ACTION_KEEP_SILENT), \
        DO(ACTION_PLEAD_INNOCENT), \
        DO_IF(ACTION_SAINT, kNeedsSaint), \
        DO(ACTION_CONFESS_GUILT) \
    }
#define WATCH_CAUGHT_OPTIONS { \
        DO_IF(ACTION_PAY_FINE, kNeedsFine), \
        HIDE,								/* no running again */ \
        TODO, \
        DO_IF(ACTION_SAINT, kNeedsSaint), \
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
        GO(SCREEN_INNER_WALL)				// the city walls (file 0x96D18)
    } },
    // "The gate is heavily guarded..."
    { "SELEC00", 0, NULL, {
        DO(ACTION_EXIT_WALK),				// simply walk out
        DO(ACTION_EXIT_HIDE),				// hide among the people
        TODO,								// a potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
        DO(ACTION_EXIT_FIGHT),				// attack the guards
        GO(SCREEN_INNER_WALL),				// by way of the wall
        DO(ACTION_GATE_RETURN)				// not leave just yet
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
        GO_IF(SCREEN_PHYSICIAN, kNeedsPhysician),	// file 0x9F3F2
        GO(SCREEN_GROVE),
        GO_IF(SCREEN_SLUM, CITY_SLUMS),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET),
        WAIT(SCREEN_INNER_WALL, 60),		// a piece of wall (file 0x9F588)
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
    { "NIGHT00", 11, NULL, { DO(ACTION_TO_PRISON) } },
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
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
        DO(ACTION_FALL_BACK)
    } },
    { "CITYG00", 0, NULL, {
        HIDE, HIDE, HIDE,
        TODO,
        DO_IF(ACTION_SAINT, kNeedsSaint),
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
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
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
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
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
    // "I recognize them! They're wanted here -- arrest them all!" (state
    // 1, file 0x914FE)
    { "CHALL00", 0, NULL, {
        DO(ACTION_GUARDS_FIGHT),			// draw weapons and fight back
        DO(ACTION_CHALLENGE_RUN),			// run down a side street
        DO(ACTION_GUARDS_TALK),				// talk your way out
        DO_IF(ACTION_GUARDS_BRIBE, kNeedsGuardsBribe),	// $Money1
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
        WAIT(SCREEN_CHALLENGE_ARRESTED, 3 * 60)	// surrender (file 0x91E44)
    } },
    // "You defeat the guards utterly.", "...you flee down the street."
    { "CHALL00", 1, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { "CHALL00", 3, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    // "...you troop off to the dungeon." (state 0xD)
    { "CHALL00", 4, NULL, { DO(ACTION_TO_PRISON) } },
    // talked away: the decoy, the "test", the threat
    { "CHALL00", 5, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { "CHALL00", 6, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { "CHALL00", 7, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    // "They're stalling." "...Boys, capture those felons!"
    { "CHALL00", 8, NULL, { DO(ACTION_GUARDS_FIGHT) } },
    { "CHALL00", 9, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { "CHALL00", 10, NULL, { DO(ACTION_GUARDS_FIGHT) } },
    // "Dank tassels of moss festoon the walls..." (state 0xD, file
    // 0x988C6), the dark cell, the oubliette, Saint Lucy's light; the
    // options missing from the worse cells are placeholders there
    { "DUNGE00", 0, NULL, CELL_OPTIONS },
    { "DUNGE00", 1, NULL, CELL_OPTIONS },
    { "DUNGE00", 2, NULL, CELL_OPTIONS },
    { "DUNGE00", 3, NULL, CELL_OPTIONS },
    // "...confiscate the lockpicks, and hand out a sound beating."
    { "DUNGE00", 5, NULL, { DO(ACTION_BACK_TO_CELL) } },
    // "...Ahead is the guardroom." (file 0x999C2)
    { "DUNGE00", 6, NULL, { DO(ACTION_JAIL_FIGHT) } },
    // "...more guards rush after you." (state 0x7A, the chase)
    { "DUNGE00", 8, NULL, { DO(ACTION_TO_CHASE) } },
    { "DUNGE00", 9, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { "DUNGE00", 10, NULL, { DO(ACTION_TO_CHASE) } },
    { "DUNGE00", 11, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { "DUNGE00", 12, NULL, { GO(SCREEN_SIDE_STREET) } },	// "back alley"
    { "DUNGE00", 13, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { "DUNGE00", 22, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { "DUNGE00", 14, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "DUNGE00", 15, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { "DUNGE00", 17, NULL, { DO(ACTION_AFTER_PRAYER) } },
    { "DUNGE00", 16, NULL, { DO(ACTION_TO_COURT) } },
    // "...Then he says, 'Art thou guilty?'" (state 0x8C, file 0xFB4A0)
    { "MAGIS00", 0, NULL, COURT_OPTIONS },
    { "MAGIS00", 1, NULL, COURT_OPTIONS },
    { "MAGIS00", 4, NULL, { GO(SCREEN_SQUARE) } },	// "left in the town square"
    { "MAGIS00", 5, NULL, { DO(ACTION_TO_EXECUTION) } },
    { "MAGIS00", 6, NULL, { GO(SCREEN_SQUARE) } },
    { "MAGIS00", 7, NULL, { GO(SCREEN_SQUARE) } },
    { "MAGIS00", 8, NULL, { GO(SCREEN_SQUARE) } },
    { "MAGIS00", 9, NULL, { GO(SCREEN_SQUARE) } },
    // "In ominous stillness, a bare-chested, black-hooded executioner..."
    // (state 0x8D, file 0xFBEA8)
    { "EXECU01", 0, NULL, {
        DO(ACTION_SUBMIT),					// refuse to struggle
        DO(ACTION_BREAK_ROPES),
        DO_IF(ACTION_SAINT, kNeedsSaint)	// pray for deliverance
    } },
    { "EXECU01", 1, NULL, { GO(SCREEN_EXECUTION) } },	// the next one
    { "EXECU01", 6, NULL, { DO(ACTION_EXECUTION_FIGHT) } },
    { "EXECU01", 11, NULL, { DO(ACTION_RESCUE) } },
    { "EXECU01", 7, NULL, { GO(SCREEN_SQUARE) } },	// pardoned
    { "EXECU01", 8, NULL, { GO(SCREEN_CHURCH) } },	// "taken to the city church"
    { "EXECU01", 9, NULL, { GO(SCREEN_SQUARE) } },
    { "EXECU01", 10, NULL, { DO(ACTION_MOB_FIGHT) } },
    { "EXECU01", 12, NULL, { DO(ACTION_TO_CHASE) } },
    { "EXECU01", 13, NULL, { GO(SCREEN_EXECUTION) } },	// the block again
    // "Hearts pumping, you run down the street..." (state 0x7A, file
    // 0xF2112)
    { "CHASE00", 0, NULL, {
        DO(ACTION_CHASE_RUN),				// outdistance the guards
        DO(ACTION_CHASE_FIGHT),
        DO(ACTION_CHASE_AMBUSH),
        DO(ACTION_CHASE_HIDE),				// duck around a corner
        TODO,								// a potion
        WAIT(SCREEN_CHASE_CAUGHT, 3 * 60)	// surrender (file 0xF27AA)
    } },
    { "CHASE00", 1, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CHASE00", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CHASE00", 4, NULL, { DO(ACTION_TO_PRISON) } },
    { "CHASE00", 9, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CHASE00", 10, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "...poor old $ChosenOneName sneezes.", the ambush, overtaken
    { "CHASE00", 11, NULL, { DO(ACTION_CHASE_FIGHT) } },
    { "CHASE00", 12, NULL, { DO(ACTION_CHASE_FIGHT) } },
    { "CHASE00", 14, NULL, { DO(ACTION_CHASE_FIGHT) } },
    { "CHASE00", 15, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "Solemnly the priest enters your cell..." (state 0x83, file
    // 0xF675C)
    { "DUNGE01", 0, NULL, {
        DO(ACTION_PRIEST_CONFESSION),		// confess your sins
        DO(ACTION_PRIEST_HELP),				// help you escape
        DO(ACTION_PRIEST_GOOD_WORD),		// with the magistrate
        DO(ACTION_FROM_PRIEST)				// leave you alone
    } },
    { "DUNGE01", 1, NULL, { DO(ACTION_FROM_PRIEST) } },
    { "DUNGE01", 2, NULL, { GO(SCREEN_CHURCH) } },	// "escorts you to the city church"
    { "DUNGE01", 3, NULL, { DO(ACTION_FROM_PRIEST) } },
    { "DUNGE01", 4, NULL, { DO(ACTION_FROM_PRIEST) } },
    { "DUNGE01", 5, NULL, { DO(ACTION_AFTER_GOOD_WORD) } },
    { "DUNGE01", 6, NULL, { DO(ACTION_TO_COURT) } },
    // the dungeon's saints (file 0x9932A)
    { "DUNGE00", 7, NULL, { GO(SCREEN_SQUARE) } },	// "Soon you are outside."
    { "DUNGE00", 19, NULL, { GO(SCREEN_SQUARE) } },
    { "DUNGE00", 20, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "DUNGE00", 21, NULL, { GO(SCREEN_SQUARE) } },
    { "DUNGE00", 23, NULL, { DO(ACTION_BACK_TO_CELL) } },
    // the guards' saints (file 0x91C62)
    { "CHALL00", 15, NULL, { DO(ACTION_GUARDS_FIGHT) } },
    { "CHALL00", 16, NULL, { LEAVE } },	// "far from noisome, dangerous $PlaceName"
    { "CHALL00", 17, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { "CHALL00", 18, NULL, { GO(SCREEN_SIDE_STREET) } },
    // the gates' and walls' saints (files 0x92D40, 0x93B24, 0x9A392,
    // 0x9B0C4)
    { "CITYG01", 10, NULL, { GO(SCREEN_MAIN_STREET) } },
    { "CITYG01", 12, NULL, { GO(SCREEN_DAY_GATE) } },
    { "CITYG00", 10, NULL, { GO(SCREEN_MAIN_STREET) } },
    { "CITYG00", 11, NULL, { DO(ACTION_BACK_TO_GATE) } },
    { "CITYW00", 9, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW00", 10, NULL, { GO(SCREEN_DAY_WALL) } },
    { "CITYW01", 8, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW01", 9, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "CITYW01", 10, NULL, { DO(ACTION_BACK_TO_WALL) } },
    // the watch's saints (file 0xBF81E): "Night, what night?", and the
    // watch again without the saint
    { "NIGHT00", 6, NULL, { DO(ACTION_WATCH_RETURN) } },
    { "NIGHT00", 13, NULL, {
        DO_IF(ACTION_PAY_FINE, kNeedsFine),
        DO(ACTION_RUN),
        TODO,								// potion
        HIDE,								// the saint again (0xEE7C = 2)
        DO(ACTION_FIGHT)
    } },
    // the magistrate's (file 0xFB978) and the execution's (0xFC6B2)
    { "MAGIS00", 2, NULL, { GO(SCREEN_UNPLEADED) } },
    { "MAGIS00", 3, NULL, { GO(SCREEN_MAGISTRATE) } },
    { "EXECU01", 2, NULL, { DO(ACTION_SAINT_RESCUE) } },
    { "EXECU01", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { "EXECU01", 4, NULL, { DO(ACTION_RESCUE) } },
    // leaving through the gate (file 0xBC8C4)
    { "SELEC00", 1, NULL, { DO(ACTION_AFTER_SHOUT) } },
    { "SELEC00", 2, NULL, { LEAVE } },
    { "SELEC00", 7, NULL, { LEAVE } },
    { "SELEC00", 8, NULL, { GO(SCREEN_GATE) } },
    { "SELEC00", 9, NULL, { LEAVE } },
    { "SELEC00", 10, NULL, { GO(SCREEN_GATE) } },
    { "SELEC00", 11, NULL, { GO(SCREEN_GATE) } },
    { "SELEC00", 12, NULL, { DO(ACTION_TO_PRISON) } },
    // "You are near the great outer wall of $PlaceName." (file 0xBD916)
    { "SELEC01", 0, NULL, {
        { ACTION_INNER_SEWER, 0, kNeedsInnerWall, 0 },
        { ACTION_INNER_SEWER, 1, kNeedsInnerWall, 0 },	// abandon the horses
        { ACTION_INNER_BRIBE, 0, kNeedsInnerWall, 0 },	// $Money1
        { ACTION_INNER_ROPE, 0, kNeedsInnerWall, 0 },
        { ACTION_INNER_ROPE, 1, kNeedsInnerWall, 0 },
        { ACTION_INNER_CLIMB, 0, kNeedsInnerWall, 0 },
        { ACTION_INNER_CLIMB, 1, kNeedsInnerWall, 0 },
        WAIT(SCREEN_GATE, 60),				// look for a gate instead
        DO_IF(ACTION_SAINT, kNeedsSaint),
        GO(SCREEN_SIDE_STREET)				// return to the streets
    } },
    { "SELEC01", 1, NULL, { LEAVE } },
    { "SELEC01", 2, NULL, { GO(SCREEN_INNER_WALL) } },
    { "SELEC01", 3, NULL, { LEAVE } },
    { "SELEC01", 4, NULL, { DO(ACTION_SALLY_CHALLENGE) } },
    { "SELEC01", 5, NULL, { LEAVE } },
    { "SELEC01", 6, NULL, { GO(SCREEN_INNER_WALL) } },
    { "SELEC01", 7, NULL, { LEAVE } },
    { "SELEC01", 8, NULL, { LEAVE } },
    { "SELEC01", 9, NULL, { LEAVE } },
    { "SELEC01", 10, NULL, { LEAVE } },
    { "SELEC01", 11, NULL, { GO(SCREEN_INNER_WALL) } },
    { "SELEC01", 12, NULL, { DO(ACTION_INNER_AFTER_DARK) } },
    // "You stumble and trip frequently..."
    { "SIDES01", 1, NULL, { GO(SCREEN_INNER_WALL) } },
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
        WAIT(SCREEN_STUMBLING, 60)			// the city wall (file 0x975FE)
    } },
    // the gate at night: the same card 0 (the handler, file 0xBC8C4,
    // never shows cards 13..16, "The gate is closed for the night...")
    { NULL, 0, NULL, {} },
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
        GO_IF(SCREEN_PHYSICIAN, kNeedsPhysician),	// file 0x9F3F2
        GO(SCREEN_GROVE),
        TODO_IF(CITY_SLUMS),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET),
        WAIT(SCREEN_INNER_WALL, 60),		// a piece of wall (file 0x9F588)
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
    { NULL, 0, NULL, {} },					// the guards' challenge
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
#undef CELL_OPTIONS
#undef COURT_OPTIONS
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
    fBattleKind(BATTLE_WITH_WATCH),
    fCell(0),
    fTunnel(0),
    fTortures(0),
    fMagistrateComing(false),
    fChoosingSaint(false),
    fRescueSaint(-1),
    fGateReturn(SCREEN_MAIN_STREET),
    fGateShoutFight(false),
    fAfterDark(-1),
    fChallengeReturn(SCREEN_OUTSIDE),
    fChallengeReputation(0),
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
        if (fParty != NULL && fParty->members.empty())
            return PARTY_LOST;			// all executed
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
    if (fChoosingSaint) {
        // the saint list: a member and a saint, or none (the last line)
        fChoosingSaint = false;
        if (option < 0 || option >= int(fSaintChoices.size())) {
            _Show(fScreen, false);
            return true;
        }
        const std::pair<int, int> choice = fSaintChoices[size_t(option)];
        const int saint = _SaintsFor(fScreen)[size_t(choice.second)];
        const int screen = fScreen;
        _Show(_Invoke(choice.first, saint) > 0
            ? _SaintAnswered(screen, choice.second) : _SaintIgnored(screen));
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
        case ACTION_GUARDS_FIGHT:
            _FightGuards();
            return true;
        case ACTION_GUARDS_TALK:
            _Show(_TalkToGuards());
            return true;
        case ACTION_GUARDS_BRIBE:
            _Show(_BribeChallenge());
            return true;
        case ACTION_CHALLENGE_RETURN:
            _Show(fChallengeReturn);
            return true;
        case ACTION_TO_PRISON:
            _Show(_EnterPrison());
            return true;
        case ACTION_BACK_TO_CELL:
            _Show(SCREEN_CELL);
            return true;
        case ACTION_PICK_LOCK:
            _Show(_PickLock());
            return true;
        case ACTION_CLIMB_WINDOW:
            _Show(_ClimbWindow());
            return true;
        case ACTION_DIG:
            _Show(_Dig());
            return true;
        case ACTION_SEDUCE:
            _Show(_Seduce());
            return true;
        case ACTION_PRAY:
            _Show(_Pray());
            return true;
        case ACTION_AFTER_PRAYER:
            _Show(fMagistrateComing ? SCREEN_TO_MAGISTRATE : SCREEN_CELL);
            return true;
        case ACTION_WAIT_MAGISTRATE:
            _Show(_WaitForMagistrate());
            return true;
        case ACTION_JAIL_FIGHT:
            _FightJailGuards();
            return true;
        case ACTION_TO_COURT:
            _Show(_EnterCourt());
            return true;
        case ACTION_KEEP_SILENT:
            _Show(_KeepSilent());
            return true;
        case ACTION_PLEAD_INNOCENT:
            _Show(_Plead(false));
            return true;
        case ACTION_CONFESS_GUILT:
            _Show(_Plead(true));
            return true;
        case ACTION_TO_EXECUTION:
            _Show(SCREEN_EXECUTION);
            return true;
        case ACTION_SUBMIT:
        case ACTION_RESCUE:
            _Show(_Rescue());
            return true;
        case ACTION_BREAK_ROPES:
            _Show(_BreakRopes());
            return true;
        case ACTION_EXECUTION_FIGHT:
        case ACTION_MOB_FIGHT:
            _FightAtExecution();
            return true;
        case ACTION_CHALLENGE_RUN:
            // file 0x917EE: an hour, then the chase
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(_EnterChase());
            return true;
        case ACTION_TO_CHASE:
            _Show(_EnterChase());
            return true;
        case ACTION_CHASE_RUN:
            _Show(_OutrunGuards());
            return true;
        case ACTION_CHASE_FIGHT:
            _FightPursuers();
            return true;
        case ACTION_CHASE_AMBUSH:
            _Show(_Ambush());
            return true;
        case ACTION_CHASE_HIDE:
            _Show(_Hide());
            return true;
        case ACTION_SAINT:
            _ShowSaints();
            return true;
        case ACTION_SAINT_RESCUE:
            _Show(_Rescue(fRescueSaint));
            return true;
        case ACTION_EXIT_WALK: {
            const int next = _ExitWalk();
            if (next < 0)
                return false;
            _Show(next);
            return true;
        }
        case ACTION_EXIT_HIDE:
            _Show(_ExitHide());
            return true;
        case ACTION_EXIT_FIGHT:
            _FightAtGate(false);
            return true;
        case ACTION_AFTER_SHOUT:
            if (fGateShoutFight)
                _FightAtGate(true);
            else
                _Show(SCREEN_GATE);
            return true;
        case ACTION_GATE_RETURN:
            // file 0xBD53C: back to the previous state
            _Show(fGateReturn);
            return true;
        case ACTION_INNER_SEWER:
            _Show(_Sewer(rule.target == 1));
            return true;
        case ACTION_INNER_BRIBE:
            _Show(_BribeSally());
            return true;
        case ACTION_INNER_ROPE:
            _Show(_RopeDown(rule.target == 1));
            return true;
        case ACTION_INNER_CLIMB:
            _Show(_ClimbOver(rule.target == 1));
            return true;
        case ACTION_INNER_AFTER_DARK: {
            const int option = fAfterDark;
            fAfterDark = -1;
            _Show(option == 4 ? _RopeDown(false) : _ClimbOver(false));
            return true;
        }
        case ACTION_SALLY_CHALLENGE:
            _Show(_Challenge(SCREEN_INNER_WALL));
            return true;
        case ACTION_CALL_PRIEST:
            // file 0x991CA: three hours; the cell and the tunnel are kept
            if (fClock != NULL)
                fClock->AddHours(3);
            _Show(SCREEN_PRIEST);
            return true;
        case ACTION_PRIEST_CONFESSION:
            _Show(_ConfessToPriest());
            return true;
        case ACTION_PRIEST_HELP:
            _Show(_AskPriestForHelp());
            return true;
        case ACTION_PRIEST_GOOD_WORD:
            _Show(_AskGoodWord());
            return true;
        case ACTION_AFTER_GOOD_WORD:
            _Show(fMagistrateComing ? SCREEN_PRIEST_MAGISTRATE
                : _BackFromPriest());
            return true;
        case ACTION_FROM_PRIEST:
            _Show(_BackFromPriest());
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
    fChoosingSaint = false;
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
            || screen == SCREEN_NIGHT_WATCH_CAUGHT
            || screen == SCREEN_WATCH_UNANSWERED)
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
    // the dungeon's cell, the magistrate after the torture
    if (screen == SCREEN_CELL)
        screen = _CellScreen();
    if (screen == SCREEN_MAGISTRATE && fTortures > 0)
        screen = SCREEN_MAGISTRATE_AGAIN;
    // the gate from inside: where "not leave just yet" goes back to (the
    // previous state, DS:A88D); the wall's bribe
    if (screen == SCREEN_GATE && previous != SCREEN_GATE
            && previous != SCREEN_NOT_IMPLEMENTED
            && (previous < SCREEN_GATE_SHOUT || previous > SCREEN_STUMBLING))
        fGateReturn = previous;
    if (screen == SCREEN_INNER_WALL || screen == SCREEN_SALLY_BRIBED)
        fVariables["Money1"] = MoneyText(_InnerWallBribe());
    if (screen == SCREEN_CHALLENGE || screen == SCREEN_CHALLENGE_BRIBED
            || screen == SCREEN_CHALLENGE_REFUSED)
        fVariables["Money1"] = MoneyText(_ChallengeBribe());
    if (screen == SCREEN_OUTSIDE || screen == SCREEN_OUTSIDE_CAPITAL
            || screen == SCREEN_OUTSIDE_FREE
            || (screen == SCREEN_DAY_WALL && previous != SCREEN_DAY_WALL_FALL
                && previous != SCREEN_DAY_WALL_HELP
                && previous != SCREEN_DAY_WALL_SLIP_ALONE
                && previous != SCREEN_DAY_WALL_UNANSWERED)
            || (screen == SCREEN_NIGHT_WALL
                && previous != SCREEN_NIGHT_WALL_FALL
                && previous != SCREEN_NIGHT_WALL_HELP
                && previous != SCREEN_NIGHT_WALL_SLIP_ALONE
                && previous != SCREEN_SEWER_STUCK
                && previous != SCREEN_NIGHT_WALL_UNANSWERED))
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
        } else if (rule.needs == kNeedsSaint) {
            hide = !_SaintKnown(screen);
        } else if (rule.needs == kNeedsInnerWall) {
            // file 0xBD992: the grate tried (mark 0x10), the sally port's
            // alarm (0x22), the walls guarded (0x0F), a rope (item 59),
            // horses: the options without them or those abandoning them
            // (0E76:0DD8(-2, 0x2000)); the purse short of the bribe takes
            // away the second sewer option, as the game has it
            bool rope = false;
            if (fParty != NULL) {
                for (const character& member : fParty->members) {
                    for (const item& carried : member.items)
                        rope = rope || (carried.code & 0x0FFF) == kRopeCode;
                }
            }
            const bool horses = _HasHorses();
            const bool guarded = _Marked(kMarkWallAlert);
            switch (i) {
                case 0:
                    hide = horses || _Marked(kMarkGrateFailed);
                    break;
                case 1:
                    hide = !horses || _Marked(kMarkGrateFailed) || fParty == NULL
                        || TotalPfennigs(fParty->cash) < _InnerWallBribe();
                    break;
                case 2:
                    hide = _Marked(kMarkSallyAlarm) || guarded;
                    break;
                case 3:
                    hide = horses || !rope || guarded;
                    break;
                case 4:
                    hide = !horses || !rope || guarded;
                    break;
                case 5:
                    hide = horses || guarded;
                    break;
                default:
                    hide = !horses;
                    break;
            }
        } else if (rule.needs == kNeedsLockpicks) {
            bool lockpicks = false;
            if (fParty != NULL) {
                for (const character& member : fParty->members) {
                    for (const item& carried : member.items) {
                        lockpicks = lockpicks
                            || (carried.code & 0x0FFF) == kLockpickCode;
                    }
                }
            }
            hide = !lockpicks;
        } else if (rule.needs == kNeedsWoman) {
            hide = _Seductress() < 0;
        } else if (rule.needs == kNeedsGuardsBribe) {
            hide = fParty == NULL
                || TotalPfennigs(fParty->cash) < _ChallengeBribe()
                || fChallengeReturn != SCREEN_SIDE_STREET;
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
// party (state 1, $CHALL00)
int
CityVisit::_PayToll()
{
    if (int(fRandom() % 100) > _TollChance() || _Marked(kMarkWanted))
        return _Challenge();
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
// recognized (state 1); if random(100) is at most the
// chance, card 2, a lesson in Speak Common for the leader (1462:0132
// mode 1), the reputation up by 1, two hours and the main street; else
// no more tries for 12 hours (mark 0x0A), card 3, an hour, the gate
// (the leader's lesson of mode 0 then is not reproduced)
int
CityVisit::_CharmGuards()
{
    if (_Reputation() <= -10 || _Marked(kMarkWanted))
        return _Challenge();
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
// for the wanted an hour and state 1
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
        return _Challenge();
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


// A fall (1462:026A(member, 0, minWounds, amount), file 0x80C0A; amount
// 10 for a fall, 20 for the dungeon's beatings, 5 with no minimum for
// the wall from inside): Strength loses random(amount · Strength / 40 +
// 1) - 1, at least 0; Endurance random(amount · Endurance / 20 + 1) - 1,
// at least that and at least minWounds; neither more
// than the attribute's maximum (0E76:0B64); AddToAttribute() keeps them
// at 1 at least
void
CityVisit::_Fall(int member, int amount, int minWounds)
{
    character& c = fParty->members[size_t(member)];
    const int strength = c.attributes[ATTRIBUTE_STRENGTH];
    int lost = int(fRandom() % uint32(amount * strength / 40 + 1)) - 1;
    lost = std::min(std::max(lost, 0), int(c.maxAttributes[ATTRIBUTE_STRENGTH]));
    AddToAttribute(c, ATTRIBUTE_STRENGTH, -lost);
    const int endurance = c.attributes[ATTRIBUTE_ENDURANCE];
    int wounds = int(fRandom() % uint32(amount * endurance / 20 + 1)) - 1;
    wounds = std::min(std::max(std::max(wounds, lost), minWounds),
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
    _Mark(kMarkGrateFailed, 500);
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
    fBattleKind = BATTLE_WITH_WATCH;
    fFoes.clear();
    fFoes.push_back(foes{ 3, 1, int(fRandom() % 5) + 4 });
    fFoes.push_back(foes{ 0, 2, 1 });
    fPendingBattle = true;
    return -1;
}


// A battle with fFoes: enemies of DARKLAND.ENM at a variant of their
// group (e.g. the watch, file 0xBF3A2: die(5) + 3 of enemy 3, the
// "Guard" types, at variant 1 and one of enemy 0, "Sergeant", at
// variant 2, as TAC.TXT prints them). On a city map: which one the game
// picks (from the battlefield type) and where everybody starts are not
// decoded, so the map is one of ICITY.000..003 and the foes start near
// the party.
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
    for (const foes& group : fFoes) {
        const uint32 first = enemies.EnemyAt(uint32(group.enemy)).type;
        const int variants = std::max(1, int(enemies.TypeAt(first).variants));
        const uint32 type = first + uint32(std::min(group.variant, variants - 1));
        for (int i = 0; i < group.count; i++) {
            int x = 20;
            int y = 20;
            if (view.FindFreeCell(x, y))
                view.AddEnemy(type, x, y, 6);
        }
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
    if (fBattleKind == BATTLE_WITH_GATE_GUARDS) {
        _Show(_ResolveGuardBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_WITH_JAIL_GUARDS) {
        _Show(_ResolveJailBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_AT_EXECUTION) {
        _Show(_ResolveExecutionBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_WITH_PURSUERS) {
        _Show(_ResolveChaseBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_AT_GATE) {
        _Show(_ResolveGateBattle(outcome));
        return;
    }
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


// The guards recognize the party (state 1, file 0x914FE): from where it
// stands (DS:A88D, the previous state), an hour passes; the reputation
// then decides how long the party stays wanted after a fight
int
CityVisit::_Challenge(int from)
{
    if (from < 0)
        from = fScreen;
    fChallengeReturn = from == SCREEN_DAY_GATE_GUARDED ? SCREEN_DAY_GATE
        : from;
    fChallengeReputation = _Reputation();
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_CHALLENGE;
}


// The guards' price (file 0x9156C): city size / 2 florins, + |reputation
// / 20| for a negative reputation, at least one
uint32
CityVisit::_ChallengeBribe() const
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    int florins = size / 2;
    if (_Reputation() < 0)
        florins += std::abs(_Reputation() / 20);
    return uint32(std::max(1, florins)) * 240;
}


// Talking's chance (file 0x91978): the leader's Speak Common + Charisma
// + the reputation within 0..100 (1367:0028); 25 less while mark 0x13
// (a fight at the gate lately) is on
int
CityVisit::_ChallengeTalkChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    int chance = leader.skills[kSkillSpeakCommon]
        + leader.attributes[ATTRIBUTE_CHARISMA] + _Reputation();
    if (_Marked(kMarkGateFought))
        chance -= 25;
    return std::max(0, std::min(100, chance));
}


// The bribe's chance (file 0x91A6C): the reputation + the leader's
// Charisma + the bribe in groschen, within 0..100
int
CityVisit::_ChallengeBribeChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int chance = _Reputation() + leader.attributes[ATTRIBUTE_CHARISMA]
        + int(_ChallengeBribe() / 12);
    return std::max(0, std::min(100, chance));
}


// Talking (file 0x91838): if random(100) is at most the chance, a lesson
// in Speak Common for the leader (1462:0132 mode 1), card 5, 6 or 7 at
// random, an hour, back where the party was; card 7 names the leader's
// weapon (09C0:1E9B, 0E76:1FB8: not decoded, taken as the weapon in hand;
// without one card 5 or 6). Else card 8 and the fight (the leader's
// lesson of mode 0 is not reproduced).
int
CityVisit::_TalkToGuards()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fParty == NULL || fParty->members.empty()
            || random(100) > _ChallengeTalkChance())
        return SCREEN_CHALLENGE_TALK_FAILED;
    character& leader = fParty->members[size_t(fParty->leader)];
    TrainSkill(leader, kSkillSpeakCommon, 10, random);
    if (fClock != NULL)
        fClock->AddHours(1);
    const int pick = random(3);
    if (pick == 0)
        return SCREEN_CHALLENGE_DECOYED;
    if (pick == 1)
        return SCREEN_CHALLENGE_BLUFFED;
    const int weapon = leader.equipment[EQUIPMENT_WEAPON];
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (size_t code = 0; weapon != kNoEquipment && code < items.size();
            code++) {
        if (!items[code].name.empty() && items[code].type == weapon) {
            fVariables["NamedOneName"] = items[code].name;
            return SCREEN_CHALLENGE_COWED;
        }
    }
    return random(2) == 0 ? SCREEN_CHALLENGE_BLUFFED : SCREEN_CHALLENGE_DECOYED;
}


// Bribing (file 0x919E0): as the game has it, the guards take the offer
// (card 9, an hour, back where the party was) only if random(100) is at
// least the chance, and the purse stays as it was; else card 10 and the
// fight
int
CityVisit::_BribeChallenge()
{
    if (int(fRandom() % 100) < _ChallengeBribeChance())
        return SCREEN_CHALLENGE_REFUSED;
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_CHALLENGE_BRIBED;
}


// Fighting the guards (file 0x92370): clamp(the party's size (09C0:2161,
// 1462:1DE6), 7, random(7) + 2) of enemy 3 at variant 1 and the
// sergeant (enemy 0) at variant 2; the reputation falls by 15..24 with a
// chance of 100 - |reputation| % (0E76:19D0(location, -15, -25))
void
CityVisit::_FightGuards()
{
    const int size = fParty != NULL ? int(fParty->members.size()) : 1;
    const int guards = std::max(size, std::min(7, int(fRandom() % 7) + 2));
    _ChangeReputation(-24, -15);
    fBattleKind = BATTLE_WITH_GATE_GUARDS;
    fFoes.clear();
    fFoes.push_back(foes{ 3, 1, guards });
    fFoes.push_back(foes{ 0, 2, 1 });
    fPendingBattle = true;
}


// The guards' battle's result (file 0x9241E): won, card 1, the guards
// nervous (mark 0x12) for 2000 / city size hours; retreated, card 3
// and an hour (0E76:23E2 not decoded); lost, card 4 and three hours,
// then the dungeon (state 0xD). The party's result 1, card 2 ("you cut
// your way through", an hour, the side streets), has no BattleView
// outcome. Then the party is wanted (mark 0x11) for 120 hours, 240 with
// a reputation of -75 or less when the guards came.
int
CityVisit::_ResolveGuardBattle(int outcome)
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    int next = SCREEN_CHALLENGE_FLED;
    int hours = 1;
    if (outcome == BATTLE_WON) {
        _Mark(kMarkAlert, uint32(2000 / std::max(1, size)));
        next = SCREEN_CHALLENGE_WON;
        hours = 0;
    } else if (outcome == BATTLE_LOST) {
        next = SCREEN_CHALLENGE_ARRESTED;
        hours = 3;
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(hours));
    _Mark(kMarkWanted, fChallengeReputation <= -75 ? 240 : 120);
    return next;
}


// Entering the dungeon (file 0x988C6): the guards search the party
// (18E7:0854(-2)), the best cell, no tunnel yet
int
CityVisit::_EnterPrison()
{
    _Search();
    fCell = 0;
    fTunnel = 0;
    return SCREEN_CELL;
}


// The search (18E7:0854(-2), file 0x66484): the purse is emptied; each
// item goes if its quantity · weight is over 2, else if random(100) is
// under h / 2 (2) or h (less), h = (Agility + Stealth) / 2 of its
// owner. As the game has it, the nimbler lose more. What is gone is no
// longer in use.
void
CityVisit::_Search()
{
    if (fParty == NULL)
        return;
    fParty->cash = money{ 0, 0, 0 };
    for (character& member : fParty->members) {
        const int h = (member.attributes[ATTRIBUTE_AGILITY]
            + member.skills[kSkillStealth]) / 2;
        std::vector<item> kept;
        for (const item& carried : member.items) {
            const int bulk = carried.quantity * carried.weight;
            const int roll = int(fRandom() % 100);
            const bool taken = bulk > 2 || (bulk == 2 ? roll < h / 2
                : roll < h);
            if (!taken)
                kept.push_back(carried);
        }
        member.items = kept;
        for (uint8& slot : member.equipment) {
            bool left = false;
            for (const item& carried : member.items)
                left = left || carried.type == slot;
            if (!left)
                slot = kNoEquipment;
        }
    }
}


int
CityVisit::_CellScreen() const
{
    return SCREEN_CELL + std::max(0, std::min(fCell, 3));
}


// Caught trying to escape: from the best cell to the dark one, else to
// the oubliette; the tunnel is lost
void
CityVisit::_WorseCell()
{
    fCell = fCell == 0 ? 1 : 2;
    fTunnel = 0;
}


// A sound beating (1462:026A(-2, 0, 1, 20)): a fall of amount 20 for all
void
CityVisit::_Beating()
{
    if (fParty == NULL)
        return;
    for (int i = 0; i < int(fParty->members.size()); i++)
        _Fall(i, 20);
}


// A flogging (file 0xFB8F8): every member loses random(18) Endurance and
// random(12) Strength (0E76:0A72(-2, ...))
void
CityVisit::_Flogging()
{
    if (fParty == NULL)
        return;
    for (character& member : fParty->members) {
        AddToAttribute(member, ATTRIBUTE_ENDURANCE, -int(fRandom() % 18));
        AddToAttribute(member, ATTRIBUTE_STRENGTH, -int(fRandom() % 12));
    }
}


// An item of DARKLAND.LST for every member (18E7:0128(-2, code)), at its
// default quality; a weapon is taken in hand when the hand is empty
// (inferred)
void
CityVisit::_GiveEach(int code)
{
    if (fParty == NULL)
        return;
    for (character& member : fParty->members)
        _GiveTo(member, code);
}


// One item for a member (18E7:0128(member, code)); a weapon is taken in
// hand when the hand is empty (inferred)
void
CityVisit::_GiveTo(character& member, int code)
{
    const item_definition& definition = fData.Lists().Items()[size_t(code)];
    member.items.push_back(item{ uint16(code), uint8(definition.type),
        definition.quality, 1, definition.weight });
    if ((definition.flags & (ITEM_EDGED | ITEM_IMPACT | ITEM_POLEARM
            | ITEM_FLAIL)) != 0
            && member.equipment[EQUIPMENT_WEAPON] == kNoEquipment)
        member.equipment[EQUIPMENT_WEAPON] = uint8(definition.type);
}


// The best at Artifice (0E76:14A4(14))
int
CityVisit::_Picker() const
{
    return _BestSkill(kSkillArtifice);
}


// Picking the lock's chance (file 0x98D36): the picker's Artifice, + 50
// in the dark cell, - 50 in Saint Lucy's light, none in the oubliette,
// within 0..99 (1367:000A)
int
CityVisit::_PickChance() const
{
    if (fParty == NULL || fParty->members.empty() || fCell == 2)
        return 0;
    int chance = fParty->members[size_t(_Picker())].skills[kSkillArtifice];
    if (fCell == 1)
        chance += 50;
    else if (fCell == 3)
        chance -= 50;
    return std::max(0, std::min(99, chance));
}


// The climber (0E76:179C(2, 1)): as the game has it, a member whose
// Agility + Strength is over the best score so far becomes the climber,
// and the score becomes twice his Agility
int
CityVisit::_Climber(int* score) const
{
    int best = 0;
    int climber = 0;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const character& member = fParty->members[size_t(i)];
        if (member.attributes[ATTRIBUTE_AGILITY]
                + member.attributes[ATTRIBUTE_STRENGTH] > best) {
            best = 2 * member.attributes[ATTRIBUTE_AGILITY];
            climber = i;
        }
    }
    if (score != NULL)
        *score = best;
    return climber;
}


// The woman with the best Charisma (0E76:174A(5)), or -1
int
CityVisit::_Seductress() const
{
    int best = -1;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const character& member = fParty->members[size_t(i)];
        if (member.female && (best < 0 || member.attributes[ATTRIBUTE_CHARISMA]
                > fParty->members[size_t(best)].attributes[ATTRIBUTE_CHARISMA]))
            best = i;
    }
    return best;
}


// Picking the lock (file 0x98C1E): if random(100) is at most the chance,
// a lesson in Artifice for the picker (mode 1), an hour, a dagger for
// every member (18E7:0128(-2, 7)), card 6 and the guardroom; else card
// 5, a beating, three hours, the lockpicks taken (18E7:0668(-2, 64)), a
// lesson of mode 0 (not reproduced) and a worse cell
int
CityVisit::_PickLock()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CELL;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int picker = _Picker();
    _SetChosen(picker);
    if (random(100) <= _PickChance()) {
        TrainSkill(fParty->members[size_t(picker)], kSkillArtifice, 10,
            random);
        if (fClock != NULL)
            fClock->AddHours(1);
        _GiveEach(kDaggerCode);
        return SCREEN_LOCK_PICKED;
    }
    _Beating();
    if (fClock != NULL)
        fClock->AddHours(3);
    for (character& member : fParty->members) {
        std::vector<item> kept;
        for (const item& carried : member.items) {
            if ((carried.code & 0x0FFF) != kLockpickCode)
                kept.push_back(carried);
        }
        member.items = kept;
    }
    _WorseCell();
    return SCREEN_PICK_CAUGHT;
}


// Climbing to the window (file 0x98DAA; the best cell only): an hour;
// if random(100) is at most the climber's score / 3 (file 0x98E86), card
// 10, a lesson in Stealth for the climber (mode 1), a club for every
// member and the chase (state 0x7A); else card 11 and a worse cell
int
CityVisit::_ClimbWindow()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CELL;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fClock != NULL)
        fClock->AddHours(1);
    int score = 0;
    const int climber = _Climber(&score);
    _SetChosen(climber);
    if (random(100) <= std::min(100, score / 3)) {
        TrainSkill(fParty->members[size_t(climber)], kSkillStealth, 10,
            random);
        _GiveEach(kClubCode);
        return SCREEN_WINDOW_ESCAPED;
    }
    _WorseCell();
    return SCREEN_WINDOW_CAUGHT;
}


// Digging (file 0x98EBE): 12 hours; after 12 o'clock the magistrate
// sends for the party one time in nine (card 16). Else the tunnel grows
// by 12..17 % (50 in Saint Lucy's light): over 95, card 12, the
// reputation down by 1..6 (0E76:19D0) and the side streets; else found if
// random(100) is under a quarter of it (card 13, a beating, the tunnel
// lost, a worse cell: the light's cell is the oubliette's); else card 22
int
CityVisit::_Dig()
{
    if (fClock != NULL) {
        fClock->AddHours(12);
        if (fClock->Hour() > 12 && fRandom() % 9 == 1)
            return SCREEN_TO_MAGISTRATE;
    }
    fTunnel += fCell == 3 ? 50 : int(fRandom() % 6) + 12;
    if (fTunnel > 95) {
        fTunnel = 0;
        _ChangeReputation(-6, -1);
        return SCREEN_TUNNEL_DONE;
    }
    if (int(fRandom() % 100) < fTunnel / 4) {
        _Beating();
        fTunnel = 0;
        fCell = fCell == 3 ? 2 : fCell + 1;
        return SCREEN_TUNNEL_FOUND;
    }
    fVariables["Number1"] = std::to_string(fTunnel);
    return SCREEN_TUNNEL_PROGRESS;
}


// Seducing the turnkey (file 0x9909A; a woman in the best cell): if
// random(100) is at most her Charisma, two days, card 14, a lesson in
// Speak Common for her (mode 1), her Virtue down by 2, or 2..5 with a
// chance of 100 - Virtue % (0E76:18A8(virtue, 2, 6)), and the side
// streets; else two hours and card 15
int
CityVisit::_Seduce()
{
    const int woman = _Seductress();
    if (woman < 0)
        return SCREEN_CELL;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    character& her = fParty->members[size_t(woman)];
    _SetChosen(woman);
    if (random(100) > her.attributes[ATTRIBUTE_CHARISMA]) {
        if (fClock != NULL)
            fClock->AddHours(2);
        return SCREEN_SCOFFED;
    }
    if (fClock != NULL)
        fClock->AddHours(48);
    TrainSkill(her, kSkillSpeakCommon, 10, random);
    const int virtue = her.skills[kSkillVirtue];
    const int loss = random(100) <= std::abs(100 - virtue) ? 2 + random(4) : 2;
    her.skills[kSkillVirtue] = uint8(std::max(0, virtue - loss));
    return SCREEN_SEDUCED;
}


// Praying (file 0x99220): every member's divine favor + 2..11 (12..21 in
// Saint Lucy's light), card 17, 12 hours; then after 12 o'clock the
// magistrate one time in ten
int
CityVisit::_Pray()
{
    if (fParty != NULL) {
        for (character& member : fParty->members) {
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
                int(fRandom() % 10) + (fCell == 3 ? 12 : 2));
        }
    }
    fMagistrateComing = false;
    if (fClock != NULL) {
        fClock->AddHours(12);
        fMagistrateComing = fClock->Hour() > 12 && fRandom() % 10 == 1;
    }
    return SCREEN_PRAYED;
}


// Waiting (file 0x995A2): until 13 o'clock (1367:0716), then the
// magistrate if random(100) is at most 11
int
CityVisit::_WaitForMagistrate()
{
    if (fClock != NULL) {
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > 13 ? 13 - hour + 24 : 13 - hour));
    }
    return fRandom() % 100 <= 11 ? SCREEN_TO_MAGISTRATE : SCREEN_CELL;
}


// The guardroom (file 0x999C2): random(5) + 4 of enemy 3 at variant
// random(3) + |s| / 4 + 1 and the sergeant at variant random(3) + 1; s
// (09C0:1C1B, 1462:0470) is a measure of the party's strength, not
// reproduced (0)
void
CityVisit::_FightJailGuards()
{
    fBattleKind = BATTLE_WITH_JAIL_GUARDS;
    fFoes.clear();
    const int variant = int(fRandom() % 3) + 1;
    fFoes.push_back(foes{ 3, variant, int(fRandom() % 5) + 4 });
    fFoes.push_back(foes{ 0, int(fRandom() % 3) + 1, 1 });
    fPendingBattle = true;
}


// The guardroom's result (file 0x99A1B): won or fled, two hours, card 8
// and the chase; lost, two hours,
// card 9, the search again, a beating, the same cell
int
CityVisit::_ResolveJailBattle(int outcome)
{
    if (fClock != NULL)
        fClock->AddHours(2);
    if (outcome != BATTLE_LOST)
        return SCREEN_GUARDROOM_WON;
    _Search();
    _Beating();
    return SCREEN_RECAPTURED;
}


// Before the magistrate (file 0xFB4A0): no torture yet
int
CityVisit::_EnterCourt()
{
    fTortures = 0;
    return SCREEN_MAGISTRATE;
}


// Saying nothing (file 0xFB61C): three times the torture (card 1, six
// hours, every member loses random(18) Endurance and random(12)
// Strength); the fourth time card 4, an hour, free in the square
int
CityVisit::_KeepSilent()
{
    if (fTortures >= 3) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_UNPLEADED;
    }
    if (fClock != NULL)
        fClock->AddHours(6);
    _Flogging();
    fTortures++;
    return SCREEN_MAGISTRATE_AGAIN;
}


// Pleading (file 0xFB74C) or confessing (file 0xFBB02): the wanted mark
// is lifted (0E76:3CDE(0x11, ...), inferred); s = random(5) - 3 (random(3)
// - 2 confessing) + the reputation / 40 (+ 1462's 0xFBEA4, always 0):
// under 0 death (card 5, three hours, the execution), 0 a flogging (card
// 6, three hours), 1 a fine; pleading, s under -6 or over 1 acquits (card
// 9, an hour); then the square
int
CityVisit::_Plead(bool guilty)
{
    fMarks.erase(std::make_pair(kMarkWanted, fCity));
    const int s = guilty ? int(fRandom() % 3) - 2 + _Reputation() / 40
        : int(fRandom() % 5) - 3 + _Reputation() / 40;
    if (!guilty && (s < -6 || s > 1)) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_ACQUITTED;
    }
    if (guilty && (s < -5 || s > 5))
        return SCREEN_MAGISTRATE;		// cannot happen: nothing
    if (s < 0) {
        if (fClock != NULL)
            fClock->AddHours(3);
        return SCREEN_SENTENCED;
    }
    if (s == 0) {
        if (fClock != NULL)
            fClock->AddHours(3);
        _Flogging();
        return SCREEN_FLOGGED;
    }
    return _CourtFine();
}


// The fine (file 0xFB85A): random(3) + city size / 3 florins, at least
// one ($Money1); with that many florins in the purse, a day and card 7,
// else three hours, card 8 and a flogging. As the game has it, the fine
// is not taken.
int
CityVisit::_CourtFine()
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int florins = std::max(1, int(fRandom() % 3) + size / 3);
    fVariables["Money1"] = MoneyText(uint32(florins) * 240);
    if (fParty != NULL && florins <= fParty->cash.florins) {
        if (fClock != NULL)
            fClock->AddHours(24);
        return SCREEN_FINED;
    }
    if (fClock != NULL)
        fClock->AddHours(3);
    _Flogging();
    return SCREEN_FINED_FLOGGED;
}


// The rescues (1838:0CF6(saint), file 0xFC196; -1 without a saint),
// each tried in turn with random(n), n = 50 for St. Jude (0x50), else
// 100: the ruler's pardon if at most the reputation / 10 (card 7, the
// reputation set to -9, the square); the abbot if at most the best
// Virtue + Religion + Charisma + Speak Latin / 10 (card 8, the church;
// 0E76:360C(2, 0, location), not decoded, taken as true); the bankers if
// at most the florins in the purse (card 9, five more hours, the
// reputation -9, the square); the mob if at most |reputation / 5| (card
// 10, a weapon each by the best weapon skill, the fight). Two more, on
// DS:9082 (the city ruler's quest, state 0x84), are not reproduced. If
// none comes, a saint's answer decides (St. Alcuin: the pardon or the
// abbot; St. John Nepomuk: the pardon; St. Jude: any of the six, the
// quest not implemented); without one a member is beheaded (card 1)
// and the execution goes on. An hour passes first.
int
CityVisit::_Rescue(int saint)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_EXECUTION;
    const int reputation = _Reputation();
    int best = 0;
    for (const character& member : fParty->members) {
        best = std::max(best, member.skills[kSkillVirtue]
            + member.skills[kSkillReligion]
            + member.attributes[ATTRIBUTE_CHARISMA]
            + member.skills[kSkillSpeakLatin]);
    }
    const uint32 n = saint == 80 ? 50 : 100;
    int rescue = 6;
    if (int(fRandom() % n) <= reputation / 10)
        rescue = 0;
    else if (int(fRandom() % n) <= best / 10)
        rescue = 1;
    else if (int(fRandom() % n) <= fParty->cash.florins)
        rescue = 2;
    else if (int(fRandom() % n) <= std::abs(reputation / 5))
        rescue = 3;
    else if (saint == 5)
        rescue = int(fRandom() % 2);
    else if (saint == 78)
        rescue = 0;
    else if (saint == 80)
        rescue = int(fRandom() % 6);
    if (fClock != NULL)
        fClock->AddHours(1);
    switch (rescue) {
        case 0:
            if (fReputations != NULL && fCity < int(fReputations->size()))
                (*fReputations)[fCity] = -9;
            return SCREEN_PARDONED;
        case 1:
            return SCREEN_CLAIMED_BY_ABBOT;
        case 2:
            if (fClock != NULL)
                fClock->AddHours(5);
            if (fReputations != NULL && fCity < int(fReputations->size()))
                (*fReputations)[fCity] = -9;
            return SCREEN_BOUGHT_OFF;
        case 3: {
            // the weapon of the best weapon skill (0E76:01C0, file
            // 0xFC46A): a falchion (edged, bows, missiles), a mace
            // (impact, flails), a short spear (polearms, thrown)
            static const int kWeapons[kWeaponSkillCount]
                = { 4, 14, 14, 21, 21, 4, 4 };
            for (character& member : fParty->members) {
                int skill = 0;
                for (int s = 1; s < kWeaponSkillCount; s++) {
                    if (member.skills[s] > member.skills[skill])
                        skill = s;
                }
                _GiveTo(member, kWeapons[skill]);
            }
            return SCREEN_MOB;
        }
        case 4:
        case 5:
            // the city ruler's quest (state 0x84): not implemented
            fPreviousScreen = SCREEN_SQUARE;
            return SCREEN_NOT_IMPLEMENTED;
        default:
            break;
    }
    // file 0xFC4CE: a member at random (the next one standing)
    const int victim = int(fRandom() % fParty->members.size());
    _SetChosen(victim);
    RemoveMember(*fParty, size_t(victim));
    return SCREEN_BEHEADED;
}


// Breaking the ropes (file 0xFC5BC): if random(100) is at most the
// strongest's Strength (0E76:16FE(1)), card 6, a dagger each and the
// fight; else card 11 with a member at random, then the rescues
int
CityVisit::_BreakRopes()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_EXECUTION;
    const int strongest = _Strongest();
    if (int(fRandom() % 100)
            <= fParty->members[size_t(strongest)].attributes[ATTRIBUTE_STRENGTH]) {
        _SetChosen(strongest);
        _GiveEach(kDaggerCode);
        return SCREEN_ROPES_BROKEN;
    }
    _SetChosen(int(fRandom() % fParty->members.size()));
    return SCREEN_ROPES_HOLD;
}


// The fight at the execution (file 0xFC004): random(4) + s / 3 + 1 of
// enemy 3 at variant random(2) + |s| / 4 + 1 and enemy 23, the
// "Executioner", at variant s % 3 + 1 (s: see _FightJailGuards(), 0
// here); the reputation falls by 5..19 (0E76:1DFE)
void
CityVisit::_FightAtExecution()
{
    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        reputation = int16(std::max(-99, reputation - 5
            - int(fRandom() % 15)));
    }
    fBattleKind = BATTLE_AT_EXECUTION;
    fFoes.clear();
    const int variant = int(fRandom() % 2) + 1;
    fFoes.push_back(foes{ 3, variant, int(fRandom() % 4) + 1 });
    fFoes.push_back(foes{ 23, 1, 1 });
    fPendingBattle = true;
}


// Its result (file 0xFC0A4): won or fled, card 12, an hour and the
// chase; lost, card 13, three hours and
// the block again. Then the party is wanted (mark 0x11) for 240 hours,
// 480 with a reputation of -75 or less.
int
CityVisit::_ResolveExecutionBattle(int outcome)
{
    const int next = outcome == BATTLE_LOST ? SCREEN_EXECUTION_RECAPTURED
        : SCREEN_EXECUTION_ESCAPED;
    if (fClock != NULL)
        fClock->AddHours(outcome == BATTLE_LOST ? 3 : 1);
    _Mark(kMarkWanted, _Reputation() <= -75 ? 480 : 240);
    return next;
}


// The chase (state 0x7A, file 0xF2112): no time on entry; the
// reputation then decides how long the party stays wanted after a fight
int
CityVisit::_EnterChase()
{
    fChallengeReputation = _Reputation();
    return SCREEN_CHASE;
}


// Running's value (file 0xF238A): three times the slowest's speed
// (0E76:0656; the speed is the agility, the load is not kept)
int
CityVisit::_ChaseRunChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    return 3 * fParty->members[size_t(_Slowest())].attributes[ATTRIBUTE_AGILITY];
}


// The ambush's value (file 0xF249C): the leader's Streetwise + Stealth
// within 0..100
int
CityVisit::_AmbushChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    return std::min(100, leader.skills[kSkillStreetwise]
        + leader.skills[kSkillStealth]);
}


// Hiding's value (file 0xF25C4): the lowest Stealth (from 99), + 20
// outside the game's day, + 3 · the city's size, within 0..100
int
CityVisit::_HideChance() const
{
    int stealth = 99;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++)
        stealth = std::min(stealth, int(fParty->members[size_t(i)].skills[kSkillStealth]));
    if (fClock != NULL && !IsGameDay(*fClock))
        stealth += 20;
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    return std::max(0, std::min(100, stealth + 3 * size));
}


// Running (file 0xF22D0). As the game has it, the party gets away only
// if random(100) is at least the value: random(4) hours, card 15, a
// lesson in Streetwise for all (1462:0132(-2, 16, 1, 5)), the side
// streets; else card 14, an hour and the fight.
int
CityVisit::_OutrunGuards()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) < _ChaseRunChance()) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_OVERTAKEN;
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(random(4)));
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStreetwise, 1, 5, random);
    return SCREEN_OUTRUN;
}


// The ambush (file 0xF23F0), the same way round: at least the value,
// lessons in Stealth and Streetwise for the leader (mode 1, 7) and card
// 12; else card 11 (the leader sneezes: the hiding's card, as the game
// has it) and lessons of mode 0 (not reproduced). The fight either way.
int
CityVisit::_Ambush()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CHASE;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) < _AmbushChance()) {
        _SetChosen(fParty->leader);
        return SCREEN_HIDING_FOUND;
    }
    character& leader = fParty->members[size_t(fParty->leader)];
    TrainSkill(leader, kSkillStealth, 7, random);
    TrainSkill(leader, kSkillStreetwise, 7, random);
    return SCREEN_AMBUSH;
}


// Hiding (file 0xF24E4), the same way round: at least the value, a
// lesson in Stealth for all (mode 1, 10), and by day a wait until 19
// o'clock (card 9), at night until 5 (card 10), then the side streets;
// else card 11 (the leader sneezes), a lesson of mode 0 and the fight
int
CityVisit::_Hide()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) < _HideChance()) {
        if (fParty != NULL && !fParty->members.empty())
            _SetChosen(fParty->leader);
        return SCREEN_HIDING_FOUND;
    }
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStealth, 1, 10, random);
    const bool day = fClock == NULL || IsGameDay(*fClock);
    if (fClock != NULL) {
        const int until = day ? 19 : 5;
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > until ? until - hour + 24
            : until - hour));
    }
    return day ? SCREEN_HIDDEN_TILL_NIGHT : SCREEN_HIDDEN_TILL_DAWN;
}


// The fight (file 0xF2A40): random(5) + 3 of enemy 3 at variant 2 and
// random(4) + 3 of enemy 0 ("Sergeant") at variant 2; the reputation
// -1..-5 (0E76:19D0)
void
CityVisit::_FightPursuers()
{
    _ChangeReputation(-5, -1);
    fBattleKind = BATTLE_WITH_PURSUERS;
    fFoes.clear();
    fFoes.push_back(foes{ 3, 2, int(fRandom() % 5) + 3 });
    fFoes.push_back(foes{ 0, 2, int(fRandom() % 4) + 3 });
    fPendingBattle = true;
}


// Its result (file 0xF2AAB): won, card 1 and the guards nervous for
// 2000 / size hours (0E76:2D5C(-2, location, 0x1E, 0x12, ...),
// inferred); fled, card 3 and an hour; both then the side streets;
// lost, card 4, three hours and the dungeon. Result 1 (card 2, "you cut
// your way through") has no BattleView outcome. Then the party is
// wanted (mark 0x11) for 120 hours, 240 at -75 or less.
int
CityVisit::_ResolveChaseBattle(int outcome)
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    int next = SCREEN_CHASE_FLED;
    int hours = 1;
    if (outcome == BATTLE_WON) {
        _Mark(kMarkAlert, uint32(2000 / std::max(1, size)));
        next = SCREEN_CHASE_WON;
        hours = 0;
    } else if (outcome == BATTLE_LOST) {
        next = SCREEN_CHASE_CAUGHT;
        hours = 3;
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(hours));
    _Mark(kMarkWanted, fChallengeReputation <= -75 ? 240 : 120);
    return next;
}


// Asking the priest's help (file 0xF6A86): the leader's Virtue +
// Religion + Charisma + Speak Latin, / 10, + 20 while bit 0x40 of the
// location's byte +0x14 is set (location property 0x20; not kept here);
// 0 if not over 0, else within 1..99
int
CityVisit::_PriestChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int chance = (leader.skills[kSkillVirtue] + leader.skills[kSkillReligion]
        + leader.attributes[ATTRIBUTE_CHARISMA]
        + leader.skills[kSkillSpeakLatin]) / 10;
    return chance <= 0 ? 0 : std::max(1, std::min(99, chance));
}


// The confession (file 0xF68B4): a lesson in Virtue for the leader (mode
// 1), his divine favor + random(10) + the reputation / 20, three hours,
// card 1
int
CityVisit::_ConfessToPriest()
{
    if (fParty == NULL || fParty->members.empty())
        return _BackFromPriest();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    character& leader = fParty->members[size_t(fParty->leader)];
    TrainSkill(leader, kSkillVirtue, 10, random);
    AddToAttribute(leader, ATTRIBUTE_DIVINE_FAVOR,
        random(10) + _Reputation() / 20);
    if (fClock != NULL)
        fClock->AddHours(3);
    return SCREEN_PRIEST_CONFESSION;
}


// Help to escape (file 0xF6970): if random(100) is at most the chance, a
// day passes; over 25 the Church claims the party (card 2, the church),
// else the leader gets lockpicks or, one time in two, an Eater Water
// ($NamedOneName; card 3), back to the cell. Else an hour, card 4 and
// the cell the party returns to is the oubliette from the best cell,
// the dark one from the others (as the game has it).
int
CityVisit::_AskPriestForHelp()
{
    if (fParty == NULL || fParty->members.empty())
        return _BackFromPriest();
    const int chance = _PriestChance();
    if (int(fRandom() % 100) > chance) {
        if (fClock != NULL)
            fClock->AddHours(1);
        fCell = fCell < 1 ? 2 : 1;
        return SCREEN_PRIEST_OUTRAGED;
    }
    if (fClock != NULL)
        fClock->AddHours(24);
    if (chance > 25)
        return SCREEN_PRIEST_RELEASED;
    static const int kEaterWaterCode = 99;
    const int code = fRandom() % 2 == 1 ? kLockpickCode : kEaterWaterCode;
    _GiveTo(fParty->members[size_t(fParty->leader)], code);
    fVariables["NamedOneName"] = fData.Lists().Items()[size_t(code)].name;
    return SCREEN_PRIEST_SMUGGLED;
}


// A good word with the magistrate (file 0xF6B2C): card 5, 12 hours, then
// after 12 o'clock the magistrate one time in ten (card 6). The game
// works out a score (Religion, Speak Latin, Virtue, the reputation) and
// never uses it.
int
CityVisit::_AskGoodWord()
{
    fMagistrateComing = false;
    if (fClock != NULL) {
        fClock->AddHours(12);
        fMagistrateComing = fClock->Hour() > 12 && fRandom() % 10 == 1;
    }
    return SCREEN_PRIEST_GOOD_WORD;
}


// Back in the dungeon (state 0xD) from state 0x83: the search again
// (the handler's entry, file 0x98953), the cell and the tunnel kept
int
CityVisit::_BackFromPriest()
{
    _Search();
    return SCREEN_CELL;
}


// A card's saints (DS:EE4B, four words, set by each state's handler)
std::vector<int>
CityVisit::_SaintsFor(int screen) const
{
    std::vector<int> saints;
    if (screen >= SCREEN_CELL && screen <= SCREEN_LIT_CELL) {
        // file 0x9896E: Bathildis, Dismas, Peter, and by the cell
        // Reinold, Lucy (the dark cell, its light) or Jude (0x98A50)
        static const int kCellSaints[4] = { 114, 87, 80, 87 };
        saints = { 14, 35, 108, kCellSaints[screen - SCREEN_CELL] };
    }
    // the guards (file 0x9163D): Christina, Genevieve, Godfrey, Reinold
    if (screen == SCREEN_CHALLENGE)
        saints = { 21, 54, 61, 114 };
    // the gate by day (file 0x9273D): Lutgardis; at night (0x9357D) and
    // the wall by day (0x99CAB) Lutgardis, Milburga; the wall at night
    // (0x9A902) Christina too
    if (screen == SCREEN_DAY_GATE || screen == SCREEN_DAY_GATE_GUARDED)
        saints = { 89 };
    if (screen == SCREEN_NIGHT_GATE || screen == SCREEN_NIGHT_GATE_ALERTED
            || screen == SCREEN_DAY_WALL)
        saints = { 89, 97 };
    if (screen == SCREEN_NIGHT_WALL)
        saints = { 21, 89, 97 };
    // the night watch (file 0xBF1D6): Raphael, Finbar, Lucy, Odilia
    if (screen >= SCREEN_NIGHT_WATCH && screen <= SCREEN_NIGHT_WATCH_CAUGHT)
        saints = { 111, 49, 87, 100 };
    // the magistrate (file 0xFB518): Devota, Lawrence; the execution
    // (0xFBF17): Alcuin, Gregory Thaumaturgus, John Nepomuk, Jude
    if (screen == SCREEN_MAGISTRATE || screen == SCREEN_MAGISTRATE_AGAIN)
        saints = { 34, 84 };
    if (screen == SCREEN_EXECUTION)
        saints = { 5, 63, 78, 80 };
    // leaving: the gate (file 0xBC94B) and the wall (0xBDACB):
    // Christina, Lutgardis, Milburga
    if (screen == SCREEN_GATE || screen == SCREEN_INNER_WALL)
        saints = { 21, 89, 97 };
    return saints;
}


// 150B:168C: a member standing who knows one of the card's saints
bool
CityVisit::_SaintKnown(int screen) const
{
    if (fParty == NULL)
        return false;
    for (const character& member : fParty->members) {
        for (int saint : _SaintsFor(screen)) {
            if (KnowsSaint(member, saint))
                return true;
        }
    }
    return false;
}


// The saint list (file 0x8E252, the card's text replaced): a line for
// each member and each of the card's saints he knows; one more to give
// up (inferred)
void
CityVisit::_ShowSaints()
{
    fSaintChoices.clear();
    const std::vector<int> saints = _SaintsFor(fScreen);
    const std::vector<std::string>& names = fData.Lists().Saints();
    std::string text = "Which saint do you call upon?\n";
    text += char(MSG_CODE_PARAGRAPH);
    text += char(MSG_CODE_PARAGRAPH);
    for (int m = 0; fParty != NULL && m < int(fParty->members.size()); m++) {
        const character& member = fParty->members[size_t(m)];
        for (size_t i = 0; i < saints.size(); i++) {
            if (!KnowsSaint(member, saints[i]))
                continue;
            fSaintChoices.push_back(std::make_pair(m, int(i)));
            text += char(MSG_CODE_OPTION);
            text += "...";
            text += char(MSG_CODE_OPTION_TEXT);
            text += Font::ToGameCharset(member.shortName + " calls upon "
                + names[size_t(saints[i])] + ".") + "\n";
        }
    }
    text += char(MSG_CODE_OPTION);
    text += "...";
    text += char(MSG_CODE_OPTION_TEXT);
    text += "call upon none of them.\n";
    msg_card card = fNotImplementedCard;
    card.text = text;
    fView.SetCard(card, fVariables);
    fChoosingSaint = true;
}


// An invocation (0E76:2180, 1462:0000 of overlay 0x22, file 0x6B7D0):
// the chance is the saint's base + (Virtue - its Virtue) / 2 (165C:0000,
// file 0x82940), 0 under its Virtue or while the divine favor is under
// its cost. If random(100) is at most the chance, the saint answers
// (its own effect, mode 8, is not reproduced) and the divine favor
// falls by the cost; else by the cost, and by half of it more if
// random(99) is under the chance - 66. The game first shows the saint
// (0x6B9FC), where one can give up: not reproduced. Returns 1 or 0.
int
CityVisit::_Invoke(int member, int saint)
{
    if (fExe == NULL)
        fExe.reset(new ExeData(fData.PathFor("DARKLAND.EXE")));
    character& c = fParty->members[size_t(member)];
    const exe_saint& rule = fExe->Saints()[size_t(saint)];
    const int favor = c.attributes[ATTRIBUTE_DIVINE_FAVOR];
    const int virtue = c.skills[kSkillVirtue];
    const int cost = favor < int(rule.cost) ? 0 : int(rule.cost);
    const int chance = virtue < int(rule.minVirtue) || cost < int(rule.cost)
        ? 0 : int(rule.base) + (virtue - int(rule.minVirtue)) / 2;
    _SetChosen(member);
    fVariables["NamedOneName"] = fData.Lists().Saints()[size_t(saint)];
    if (int(fRandom() % 100) <= chance) {
        AddToAttribute(c, ATTRIBUTE_DIVINE_FAVOR, -cost);
        return 1;
    }
    if (int(fRandom() % 99) < chance - 66)
        AddToAttribute(c, ATTRIBUTE_DIVINE_FAVOR, -(cost / 2));
    AddToAttribute(c, ATTRIBUTE_DIVINE_FAVOR, -cost);
    return 0;
}


// A saint answered: the card's own outcome, by the saint's place in its
// list
int
CityVisit::_SaintAnswered(int screen, int index)
{
    if (screen >= SCREEN_CELL && screen <= SCREEN_LIT_CELL) {
        // the dungeon (file 0x9932A): three hours; Bathildis (card 7)
        // pays with half the purse, Dismas and Peter (card 19) with the
        // reputation (-1..-8), both to the square; Reinold (card 20) to
        // the side streets, Lucy (card 3) lights the dark cell, Jude
        // (card 21) to the square, the reputation +2..+7
        if (fClock != NULL)
            fClock->AddHours(3);
        if (index == 0) {
            if (fParty != NULL)
                fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) / 2);
            return SCREEN_BATHILDIS;
        }
        if (index <= 2) {
            _ChangeReputation(-8, -1);
            return SCREEN_WALL_CRACKED;
        }
        if (fCell == 0)
            return SCREEN_REINOLD_CLIMB;
        if (fCell == 1) {
            fCell = 3;
            return SCREEN_LIT_CELL;
        }
        if (fCell == 2) {
            _ChangeReputation(2, 7);
            return SCREEN_EARTHQUAKE;
        }
        return SCREEN_CELL;				// Lucy again: nothing
    }
    // with a good reputation (over -10) the answer raises it, else it
    // lowers it (0E76:19D0 with the signs reversed)
    const bool liked = _Reputation() > -10;
    switch (screen) {
        case SCREEN_CHALLENGE:
            // file 0x91CC2: an hour; Christina (card 16, +-3..9) takes the
            // party away from the city; Genevieve, Godfrey (card 17,
            // +2..6) stop the guards; Reinold (card 18, +-2..6) walks up
            // a wall to the side streets
            if (fClock != NULL)
                fClock->AddHours(1);
            if (index == 0) {
                _ChangeReputation(liked ? 3 : -9, liked ? 9 : -3);
                return SCREEN_CHRISTINA_LIFTS;
            }
            if (index <= 2) {
                _ChangeReputation(2, 6);
                return SCREEN_GUARDS_AT_PEACE;
            }
            _ChangeReputation(liked ? 2 : -6, liked ? 6 : -2);
            return SCREEN_REINOLD_WALKS;
        case SCREEN_DAY_GATE:
        case SCREEN_DAY_GATE_GUARDED:
            // file 0x92D62: card 10, +4..12 (-3..9), the main street
            _ChangeReputation(liked ? 4 : -9, liked ? 12 : -3);
            return SCREEN_GATE_LIFTED;
        case SCREEN_NIGHT_GATE:
        case SCREEN_NIGHT_GATE_ALERTED:
            // file 0x93B46: card 10, +-2..6, the main street
            _ChangeReputation(liked ? 2 : -6, liked ? 6 : -2);
            return SCREEN_NIGHT_GATE_LIFTED;
        case SCREEN_DAY_WALL:
            // file 0x9A3C0: card 9, +1..4, the side streets
            _ChangeReputation(1, 4);
            return SCREEN_DAY_WALL_LIFTED;
        case SCREEN_NIGHT_WALL:
            // file 0x9B0ED: card 8 (Christina) or 9, an hour, the side
            // streets
            if (fClock != NULL)
                fClock->AddHours(1);
            return index == 0 ? SCREEN_NIGHT_WALL_CHRISTINA
                : SCREEN_NIGHT_WALL_LIFTED;
        case SCREEN_NIGHT_WATCH:
        case SCREEN_NIGHT_WATCH_MARKET:
        case SCREEN_NIGHT_WATCH_AGAIN:
        case SCREEN_NIGHT_WATCH_CAUGHT:
            // file 0xBF844: an hour, +5..10, card 6, on as after the fine
            if (fClock != NULL)
                fClock->AddHours(1);
            _ChangeReputation(5, 10);
            return SCREEN_WATCH_SUNLIGHT;
        case SCREEN_MAGISTRATE:
        case SCREEN_MAGISTRATE_AGAIN:
            // file 0xFB9A1: card 2, the torture (every member loses
            // random(20) Endurance, the leader keeps 1), 12 hours, card 4
            // (free, the square)
            if (fParty != NULL) {
                for (character& member : fParty->members) {
                    AddToAttribute(member, ATTRIBUTE_ENDURANCE,
                        -int(fRandom() % 20));
                }
            }
            if (fClock != NULL)
                fClock->AddHours(12);
            return SCREEN_COURT_SAINT;
        case SCREEN_EXECUTION:
            // file 0xFC6DB: Gregory (card 3) brings a storm, three hours,
            // the side streets; the others (card 2) help the rescues
            if (index == 1) {
                if (fClock != NULL)
                    fClock->AddHours(3);
                return SCREEN_STORM;
            }
            fRescueSaint = _SaintsFor(screen)[size_t(index)];
            return SCREEN_EXECUTION_SAINT;
        case SCREEN_GATE:
            // file 0xBD184: card 7, an hour, +-2..8, out of the city
            if (fClock != NULL)
                fClock->AddHours(1);
            _ChangeReputation(liked ? 2 : -8, liked ? 8 : -2);
            return SCREEN_GATE_SAINT;
        case SCREEN_INNER_WALL:
            // file 0xBE84F: card 10, an hour, out of the city
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_INNER_SAINT;
        default:
            return screen;
    }
}


// No answer: the dungeon's card 23, six hours in all
int
CityVisit::_SaintIgnored(int screen)
{
    if (screen >= SCREEN_CELL && screen <= SCREEN_LIT_CELL) {
        if (fClock != NULL)
            fClock->AddHours(6);
        return SCREEN_NO_ANSWER;
    }
    switch (screen) {
        case SCREEN_CHALLENGE:
            return SCREEN_CHALLENGE_UNANSWERED;		// card 15, the fight
        case SCREEN_DAY_GATE:
        case SCREEN_DAY_GATE_GUARDED:
            return SCREEN_GATE_UNANSWERED;			// card 12
        case SCREEN_NIGHT_GATE:
        case SCREEN_NIGHT_GATE_ALERTED:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_NIGHT_GATE_UNANSWERED;	// card 11, an hour
        case SCREEN_DAY_WALL:
            return SCREEN_DAY_WALL_UNANSWERED;		// card 10
        case SCREEN_NIGHT_WALL:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_NIGHT_WALL_UNANSWERED;	// card 10, an hour
        case SCREEN_NIGHT_WATCH:
        case SCREEN_NIGHT_WATCH_MARKET:
        case SCREEN_NIGHT_WATCH_AGAIN:
        case SCREEN_NIGHT_WATCH_CAUGHT:
            return SCREEN_WATCH_UNANSWERED;			// card 13
        case SCREEN_MAGISTRATE:
        case SCREEN_MAGISTRATE_AGAIN:
            return SCREEN_COURT_UNANSWERED;			// card 3
        case SCREEN_EXECUTION:
            return SCREEN_EXECUTION_UNANSWERED;		// card 4, the rescues
        case SCREEN_GATE:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_GATE_SAINT_UNANSWERED;	// card 8, an hour
        case SCREEN_INNER_WALL:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_INNER_SAINT_UNANSWERED;	// card 11, an hour
        default:
            return screen;
    }
}


// Walking out of the gate (file 0xBCD30): an hour and out of the city;
// after a fight at the gate lately (mark 0x13), four times in ten card 1
// and the fight. 09C0:20F3 (1462:00BA), not decoded, would lead there
// too (taken as false).
int
CityVisit::_ExitWalk()
{
    if (_Marked(kMarkGateFought) && fRandom() % 100 < 40) {
        fGateShoutFight = true;
        return SCREEN_GATE_SHOUT;
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    return -1;
}


// Hiding among the people's chance (file 0xBCEBE): the party's average
// Agility + Streetwise (0E76:05EE(m, 2), 01A0(m, 16)), 25 less after a
// fight at the gate lately, within 0..100
int
CityVisit::_ExitHideChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    int sum = 0;
    for (const character& member : fParty->members) {
        sum += member.attributes[ATTRIBUTE_AGILITY]
            + member.skills[kSkillStreetwise];
    }
    int chance = sum / int(fParty->members.size());
    if (_Marked(kMarkGateFought))
        chance -= 25;
    return std::max(0, std::min(100, chance));
}


// Hiding among the people (file 0xBCDDE): if random(100) is at most the
// chance, card 2, a lesson in Streetwise for all (09C0:1F63(-2, 16, 1,
// 5)), an hour, out of the city; else a lesson of mode 0, an hour, card
// 1, the reputation -1..-4, the gate again
int
CityVisit::_ExitHide()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fClock != NULL)
        fClock->AddHours(1);
    if (random(100) <= _ExitHideChance()) {
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStreetwise, 1, 5, random);
        return SCREEN_GATE_SLIPPED;
    }
    _ChangeReputation(-4, -1);
    fGateShoutFight = false;
    return SCREEN_GATE_SHOUT;
}


// The fight at the gate (file 0xBCAB4 after card 1, 0xBD27A attacking):
// battlefield 0x2B, random(4) + |s| / 4 + 3 of enemy 3 at variant |s| /
// 4 + 1 and the sergeant at variant random(3) + 1 (s: see
// _FightJailGuards(), 0 here); after card 1 while the guards are nervous
// (mark 0x12) 7 of them at variant 5 and the sergeant at 5. The
// reputation falls by 40, or at -40 or less by 3, or 3..8 with a chance
// of 100 - |reputation| % (0E76:18A8(reputation, 3, 9)).
void
CityVisit::_FightAtGate(bool nervous)
{
    fFoes.clear();
    if (nervous && _Marked(kMarkAlert)) {
        fFoes.push_back(foes{ 3, 5, 7 });
        fFoes.push_back(foes{ 0, 5, 1 });
    } else {
        fFoes.push_back(foes{ 3, 1, int(fRandom() % 4) + 3 });
        fFoes.push_back(foes{ 0, int(fRandom() % 3) + 1, 1 });
    }
    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        int loss = 40;
        if (reputation <= -40) {
            loss = int(fRandom() % 100) <= std::abs(100 - int(reputation))
                ? 3 + int(fRandom() % 6) : 3;
        }
        reputation = int16(std::max(-99, reputation - loss));
    }
    fBattleKind = BATTLE_AT_GATE;
    fPendingBattle = true;
}


// Its result (file 0xBCBA9): a fight at the gate is remembered for 24
// hours (mark 0x13); won, the guards nervous (mark 0x12) for 120 hours,
// card 9, an hour, out of the city; fled, card 10, an hour, the gate;
// lost, card 11 (the bodies dumped in an alley), the gate (the game's
// result 4, card 12 and the dungeon, is its surrender: no BattleView
// outcome). Then wanted (mark 0x11) for 120 hours, 240 at -75 or less.
int
CityVisit::_ResolveGateBattle(int outcome)
{
    _Mark(kMarkGateFought, 24);
    int next = SCREEN_GATE_DUMPED;
    if (outcome == BATTLE_WON) {
        _Mark(kMarkAlert, 120);
        next = SCREEN_GATE_DASHED;
    } else if (outcome != BATTLE_LOST)
        next = SCREEN_GATE_FLED;
    if (fClock != NULL && next != SCREEN_GATE_DUMPED)
        fClock->AddHours(1);
    _Mark(kMarkWanted, _Reputation() <= -75 ? 240 : 120);
    return next;
}


// Horses among the party's items (the item flag 0x2000, 0E76:0DD8)
bool
CityVisit::_HasHorses() const
{
    if (fParty == NULL)
        return false;
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (const character& member : fParty->members) {
        for (const item& carried : member.items) {
            const size_t code = carried.code & 0x0FFF;
            if (code < items.size() && (items[code].flags & ITEM_HORSE) != 0)
                return true;
        }
    }
    return false;
}


// The horses left behind (09C0:202B(-2, 0x2000, 0))
void
CityVisit::_LeaveHorses()
{
    if (fParty == NULL)
        return;
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (character& member : fParty->members) {
        std::vector<item> kept;
        for (const item& carried : member.items) {
            const size_t code = carried.code & 0x0FFF;
            if (code >= items.size() || (items[code].flags & ITEM_HORSE) == 0)
                kept.push_back(carried);
        }
        member.items = kept;
    }
}


// The sally port's guard (file 0xBD9E3): (city size / 3 + 1) · the
// party's size · 2 pfennigs, or (100 - reputation) / 33 for a negative
// reputation; twice that after a fight at the gate lately
uint32
CityVisit::_InnerWallBribe() const
{
    const int size = fData.Cities().CityAt(uint32(fCity)).size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    int bribe = (size / 3 + 1) * count * 2;
    if (_Reputation() < 0)
        bribe = (100 - _Reputation()) / 33;
    if (_Marked(kMarkGateFought))
        bribe *= 2;
    return uint32(bribe);
}


// The sewer's chance (file 0xBDD9E): the member with the best Agility +
// Strength, that sum · 8 / 10 within 0..99
int
CityVisit::_SewerChance(int* member) const
{
    int best = 0;
    int chosen = 0;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const character& c = fParty->members[size_t(i)];
        const int sum = c.attributes[ATTRIBUTE_AGILITY]
            + c.attributes[ATTRIBUTE_STRENGTH];
        if (sum > best) {
            best = sum;
            chosen = i;
        }
    }
    if (member != NULL)
        *member = chosen;
    return std::max(0, std::min(99, best * 8 / 10));
}


// The rope's and the climb's chance (file 0xBE1A8): the lowest Stealth
// of the party (0E76:1396(15)), halved after a fight at the gate lately
int
CityVisit::_WallStealth(int* member) const
{
    int lowest = 199;
    int chosen = 0;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const int stealth = fParty->members[size_t(i)].skills[kSkillStealth];
        if (stealth < lowest) {
            lowest = stealth;
            chosen = i;
        }
    }
    if (member != NULL)
        *member = chosen;
    if (_Marked(kMarkGateFought))
        lowest /= 2;
    return lowest;
}


// The sewer (file 0xBDC88, 0xBDE1A with the horses): if random(100) is
// under the chance, card 1 (the grate broken by $ChosenOneName), the
// horses left, out of the city; else mark 0x10 for 500 hours
// (0E76:2C4E), a positive reputation -2..-4, city size / 3 hours, card 2
int
CityVisit::_Sewer(bool horses)
{
    (void)horses;						// both leave the horses
    int member = 0;
    const int chance = _SewerChance(&member);
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(member);
    if (int(fRandom() % 100) < chance) {
        _LeaveHorses();
        return SCREEN_SEWER_OUT;
    }
    _Mark(kMarkGrateFailed, 500);
    if (_Reputation() > 0)
        _ChangeReputation(-4, -2);
    if (fClock != NULL)
        fClock->AddHours(uint32(fData.Cities().CityAt(uint32(fCity)).size / 3));
    return SCREEN_SEWER_BLOCKED;
}


// The sally port (file 0xBDF9A): over -10 and not wanted, the bribe
// paid, card 3, an hour, the horses left, out of the city; else card 4,
// mark 0x22 for 48 hours (0E76:2C4E), no time, and the guards' challenge
int
CityVisit::_BribeSally()
{
    if (_Reputation() > -10 && !_Marked(kMarkWanted)) {
        if (fParty != NULL) {
            const uint32 purse = TotalPfennigs(fParty->cash);
            const uint32 bribe = _InnerWallBribe();
            fParty->cash = MoneyFromPfennigs(purse > bribe ? purse - bribe : 0);
        }
        if (fClock != NULL)
            fClock->AddHours(1);
        _LeaveHorses();
        return SCREEN_SALLY_BRIBED;
    }
    _Mark(kMarkSallyAlarm, 48);
    return SCREEN_SALLY_ALARM;
}


// Down a rope (file 0xBE0B8, 0xBE1EC with the horses: by day card 12 and
// a wait until 19 o'clock first): if random(100) is at most the chance,
// a rope used (18E7:04D4(-2, 59)), a lesson in Stealth for all (mode 7),
// card 5, the horses left, three hours, out of the city; else card 6,
// an hour, the walls guarded for 24 hours (mark 0x0F)
int
CityVisit::_RopeDown(bool afterDark)
{
    if (afterDark && fClock != NULL && IsGameDay(*fClock)) {
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > 19 ? 19 - hour + 24 : 19 - hour));
        fAfterDark = 4;
        return SCREEN_WAIT_FOR_DARK;
    }
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) > _WallStealth(NULL)) {
        if (fClock != NULL)
            fClock->AddHours(1);
        _Mark(kMarkWallAlert, 24);
        return SCREEN_WALL_SPOTTED;
    }
    bool used = false;
    for (character& member : fParty->members) {
        for (size_t i = 0; !used && i < member.items.size(); i++) {
            if ((member.items[i].code & 0x0FFF) != kRopeCode)
                continue;
            if (member.items[i].quantity > 1)
                member.items[i].quantity--;
            else
                member.items.erase(member.items.begin() + long(i));
            used = true;
        }
    }
    TrainParty(*fParty, kSkillStealth, 7, 10, random);
    _LeaveHorses();
    if (fClock != NULL)
        fClock->AddHours(3);
    return SCREEN_ROPE_DOWN;
}


// Over the wall (file 0xBE35A, 0xBE562 with the horses: the roll first,
// then by day card 12 and the wait): if the roll, random(100), is at
// most the chance, a lesson in Stealth for all (mode 1), every member
// whose speed · 3 (0E76:06DE; the agility here) is under the roll falls
// (1462:026A(m, 0, 0, 5)), card 7 (nobody), 8 (one) or 9, the horses
// left, three hours, out of the city; else card 6, an hour, a lesson of
// mode 0, the walls guarded for 24 hours
int
CityVisit::_ClimbOver(bool afterDark)
{
    if (afterDark && fClock != NULL && IsGameDay(*fClock)) {
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > 19 ? 19 - hour + 24 : 19 - hour));
        fAfterDark = 6;
        return SCREEN_WAIT_FOR_DARK;
    }
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int roll = random(100);
    if (roll > _WallStealth(NULL) || fParty == NULL) {
        if (fClock != NULL)
            fClock->AddHours(1);
        _Mark(kMarkWallAlert, 24);
        return SCREEN_WALL_SPOTTED;
    }
    TrainParty(*fParty, kSkillStealth, 1, 10, random);
    int fallen = 0;
    for (int i = 0; i < int(fParty->members.size()); i++) {
        if (3 * fParty->members[size_t(i)].attributes[ATTRIBUTE_AGILITY] < roll) {
            _Fall(i, 5, 0);
            fallen++;
        }
    }
    _LeaveHorses();
    if (fClock != NULL)
        fClock->AddHours(3);
    return fallen == 0 ? SCREEN_OVER_WALL
        : fallen == 1 ? SCREEN_OVER_WALL_ONE_FELL : SCREEN_OVER_WALL_FALLS;
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
