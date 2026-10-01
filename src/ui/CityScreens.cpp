// The tables of city screens, by day and by night: the card each shows
// and what each of its options does (see CityVisitInternal.h)

#include "CityVisitInternal.h"

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
#include "LocationFile.h"
#include "ScreenSupport.h"
#include "EnemyFile.h"
#include "Stream.h"
#include "TextSupport.h"

#include <cstring>
#include <stdexcept>


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
#define TOWER_OPTIONS { \
        DO(ACTION_TOWER_SIEGE),				/* lay siege */ \
        TODO,								/* alchemy */ \
        DO_IF(ACTION_TOWER_ASK, kNeedsTowerWelcome),	/* come inside */ \
        DO(ACTION_TOWER_DUEL),				/* single combat */ \
        DO(ACTION_TOWER_SNEAK),				/* sneak in after dark */ \
        DO_IF(ACTION_SAINT, kNeedsSaint),	/* a saint */ \
        DO_IF(ACTION_TOWER_STORM, kNeedsTowerAllies),	/* storm it */ \
        LEAVE								/* go away (state 0xC) */ \
    }
// The monastery by day (file 0xB9802: the options shown)
#define MONASTERY_OPTIONS { \
        DO_IF(ACTION_MONKS_PRAY, kNeedsMonksPrayers),	/* $Money1 for prayers */ \
        DO_IF(ACTION_MONKS_TUTORING, kNeedsTutoring),	/* tutoring */ \
        DO_IF(ACTION_MONKS_LIBRARY, kNeedsLibrary),	/* the saints' books */ \
        DO_IF(ACTION_MONKS_HEALING, kNeedsAbbess),	/* healing */ \
        GO(SCREEN_MONASTERY_NO_SANCTUARY), \
        HIDE, HIDE, HIDE,					/* problems, abbot, prayer */ \
        GO(SCREEN_CHURCHES)					/* leave */ \
    }
// The shell game (file 0x110CFB): pay and play, or walk away; the
// shuffled shells' cards offer only the three shells
#define SHELL_OPTIONS { \
        DO_IF(ACTION_SHELL_PAY, kNeedsGroschen), \
        DO(ACTION_SHELL_LEAVE), \
        HIDE, HIDE, HIDE \
    }
#define SHELLS { \
        HIDE, HIDE, \
        { ACTION_SHELL_PICK, 0, kAlways, 0 },	/* the right-hand shell */ \
        { ACTION_SHELL_PICK, 1, kAlways, 0 },	/* the center one */ \
        { ACTION_SHELL_PICK, 2, kAlways, 0 }	/* the left-hand one */ \
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
static const screen_rules kDayScreens[] = {
    // "You gather around the comfortable fire at the $Inn..."
    { CityVisit::SCREEN_START, "PARTY02", 0, NULL, {
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
    { CityVisit::SCREEN_OUTSIDE, "CITYE00", 0, NULL, OUTSIDE_OPTIONS },
    // "Here you can enjoy the good food... of the $Inn common-room."
    // (DARKLAND.EXE, file 0xA6B5E; a wanted party gets SCREEN_UNWELCOME)
    { CityVisit::SCREEN_INN, "URBAN00", 0, NULL, {
        DO(ACTION_INN_NEWS),				// local news and rumors
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
    { CityVisit::SCREEN_MAIN_STREET, "MAINS01", 0, "MAIN-ST.PIC", {
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// craft guilds and side alleys
        GO_IF(SCREEN_INN, CITY_INN),
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        GO(SCREEN_SIDE_STREET),
        DO(ACTION_TO_GROVE),
        GO(SCREEN_GATE)
    } },
    // "The side streets of $PlaceName are full of people..."
    { CityVisit::SCREEN_SIDE_STREET, "SIDES00", 0, "XSIDE.PIC", {
        GO(SCREEN_MAIN_STREET),
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// crafts district, inns...
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        DO(ACTION_TO_GROVE),
        GO(SCREEN_OTHER),
        GO(SCREEN_INNER_WALL)				// the city walls (file 0x96D18)
    } },
    // "The gate is heavily guarded..."
    { CityVisit::SCREEN_GATE, "SELEC00", 0, NULL, {
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
    { CityVisit::SCREEN_SLEEP, "URBAN00", 2, NULL, { WAIT(SCREEN_INN, 9 * 60) } },
    // "The $citySquare, the main city square of $PlaceName..."
    { CityVisit::SCREEN_SQUARE, "CITYS00", 0, "XTOWN.PIC", {
        { ACTION_NEWS, CityVisit::SCREEN_SQUARE, kAlways, 60 },	// notices,
                                            // gossip (file 0x9DB66)
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
    // (DARKLAND.EXE, state 0x1B, file 0xA4B0E: it shows the audience, the
    // clerk and the saint; the dungeon's options, which the card also
    // has, are always hidden)
    { CityVisit::SCREEN_FORTRESS, "CITYF00", 0, NULL, {
        TODO, TODO, TODO,					// audience, clerk, saint
        HIDE, HIDE,							// the dungeon: prisoners, in
        HIDE, HIDE, HIDE,					// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $marketplace... is the bustling center of all business"
    // (DARKLAND.EXE, segment 1893: the merchants open the trade screen
    // directly; the game may first offer a quest, or bring guards on a
    // wanted party, and draws an event after trading: not reproduced)
    { CityVisit::SCREEN_MARKET, "MARKE00", 0, NULL, {
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
    { CityVisit::SCREEN_CHURCHES, "CHURC00", 0, "XCHURCH.PIC", {
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
    { CityVisit::SCREEN_CATHEDRAL, "CATHE00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO, TODO, TODO,	// mass, priest, donate...
        GO(SCREEN_CHURCHES),				// leave the cathedral
        HIDE								// a relic as a quest reward
    } },
    // "You come to the $cityChurch, the church of $PlaceName."
    { CityVisit::SCREEN_CHURCH, "CITYC00", 0, NULL, {
        DO(ACTION_MASS),
        DO(ACTION_CONFESSION),
        TODO,								// talk to a priest
        DO_IF(ACTION_DONATION, kNeedsDonation),	// give $Money1
        TODO_IF(kNeedsBadReputation),		// seek sanctuary
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "An elderly, clear-eyed monk bows and asks your business." (state
    // 0x36, file 0xB97B2; the cards $MONAS00, $MONAS01 are never used)
    { CityVisit::SCREEN_MONASTERY, "CITYM00", 0, NULL, MONASTERY_OPTIONS },
    // "At the $university... the snobbish staff prefers to speak Latin"
    { CityVisit::SCREEN_UNIVERSITY, "UNIVE00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// saints, formulae, stone...
        TODO, TODO, TODO,					// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The entrance... of the $councilHall for $PlaceName is well guarded."
    // (state 0x2B, file 0xB1A98: the weapons training is never offered)
    { CityVisit::SCREEN_TOWN_HALL, "COUNC00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// audience, clerk, saint, dungeon
        HIDE, HIDE, HIDE,					// training, placeholders
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $cityBarracks is the armory of $PlaceName..."
    { CityVisit::SCREEN_BARRACKS, "CITYB00", 0, NULL, {
        TODO, TODO,							// training, recruits
        HIDE,								// ask $NamedOneName to join
        TODO, TODO,							// placeholders
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Navigating through the narrow streets, you seek..."
    { CityVisit::SCREEN_DISTRICT, "BUSIN00", 0, NULL, {
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
    { CityVisit::SCREEN_CRAFTS, "CIVCR00", 0, NULL, {
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
    { CityVisit::SCREEN_ARMS_CRAFTS, "MILCR00", 0, NULL, {
        // Soldier's Road (file 0xA217E): the arms outfitter, two hours
        { ACTION_TRADE, MERCHANT_ARMS_OUTFITTER, kAlways, 120 },
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
    { CityVisit::SCREEN_GROVE, "CITYG05", 0, "XGROVE1.PIC", {
        { ACTION_GROVE, 0, kAlways, 0 },	// an hour
        { ACTION_GROVE, 1, kAlways, 0 },	// a bell
        { ACTION_GROVE, 2, kAlways, 0 },	// until nightfall
        TODO, TODO, TODO, TODO, TODO,		// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $slum of $PlaceName is full of paupers, drifters, thieves..."
    { CityVisit::SCREEN_SLUM, "SLUMD00", 0, NULL, {
        WAIT(SCREEN_SLUM_REST, 60),			// rest for an hour
        { ACTION_NEWS, CityVisit::SCREEN_SLUM, kAlways, kHalfSize },	// the
                                            // rumors (file 0xAB15A)
        TODO, TODO, TODO, TODO, TODO, TODO,	// placeholders
        DO(ACTION_SLUM_LODGING),			// live very cheaply
        GO(SCREEN_SIDE_STREET)
    } },
    // "The craft tied to the piers and wharves have many destinations."
    { CityVisit::SCREEN_DOCKS, "DOCKS00", 0, "DOCKDAY.PIC", {
        HIDE, HIDE, HIDE, HIDE, HIDE,		// boats: the destinations and
                                            // fares come from the game
        TODO,								// placeholder
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Trudging along back streets and alleys, you head for..."
    { CityVisit::SCREEN_OTHER, "OTHER00", 0, NULL, {
        HIDE, HIDE,							// the homes of people you met
        TODO, TODO, TODO, TODO, TODO, TODO,	// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The sounds of hammers ringing on steel... fill Swordsmith's Lane."
    // The shops of the four arms-making guilds have the same options;
    // the guild politics and the leader's secrets belong to quests.
    { CityVisit::SCREEN_SWORDSMITH, "SWORD00", 0, NULL, SHOP_OPTIONS(MERCHANT_SWORDSMITH, SCREEN_ARMS_CRAFTS) },
    { CityVisit::SCREEN_BLACKSMITH, "BLACK00", 0, NULL, SHOP_OPTIONS(MERCHANT_BLACKSMITH, SCREEN_ARMS_CRAFTS) },
    { CityVisit::SCREEN_ARMORER, "ARMOR00", 0, NULL, SHOP_OPTIONS(MERCHANT_ARMORER, SCREEN_ARMS_CRAFTS) },
    { CityVisit::SCREEN_BOWYER, "BOWYE00", 0, NULL, SHOP_OPTIONS(MERCHANT_BOWYER, SCREEN_ARMS_CRAFTS) },
    // "Tinkers' Square..."; the clothmakers' guild
    { CityVisit::SCREEN_ARTIFICER, "ARTIF00", 0, NULL, SHOP_OPTIONS(MERCHANT_ARTIFICER, SCREEN_CRAFTS) },
    { CityVisit::SCREEN_CLOTHMAKER, "CLOTH00", 0, NULL, SHOP_OPTIONS(MERCHANT_CLOTHMAKER, SCREEN_CRAFTS) },
    // The church's results: "the Mass is sung", "the next Mass will be
    // at $NamedOneName", confession, the priest's thanks for small,
    // middling and large donations
    { CityVisit::SCREEN_MASS, "CITYC00", 2, NULL, { GO(SCREEN_CHURCH) } },
    { CityVisit::SCREEN_NO_MASS, "CITYC00", 4, NULL, { GO(SCREEN_CHURCH) } },
    { CityVisit::SCREEN_CONFESSION, "CITYC00", 3, NULL, { GO(SCREEN_CHURCH) } },
    { CityVisit::SCREEN_SMALL_DONATION, "CITYC00", 5, NULL, { GO(SCREEN_CHURCH) } },
    { CityVisit::SCREEN_DONATION, "CITYC00", 6, NULL, { GO(SCREEN_CHURCH) } },
    { CityVisit::SCREEN_LARGE_DONATION, "CITYC00", 7, NULL, { GO(SCREEN_CHURCH) } },
    // The church at night: "Finally, the Mass is sung", "the next Mass
    // will not be sung until $NamedOneName", the altar boy's answer
    { CityVisit::SCREEN_NIGHT_MASS, "CITYC01", 2, NULL, { GO(SCREEN_CHURCH) } },
    { CityVisit::SCREEN_NIGHT_NO_MASS, "CITYC01", 1, NULL, { GO(SCREEN_CHURCH) } },
    { CityVisit::SCREEN_ALTAR_BOY, "CITYC01", 3, NULL, { GO(SCREEN_CHURCH) } },
    // "...the innkeeper carefully bows. 'Sirs, most regrettably, I fear
    // that we have no room.'" The game offers neither the meal nor the
    // room, nor the storage.
    { CityVisit::SCREEN_UNWELCOME, "URBAN00", 3, NULL, {
        DO(ACTION_INN_NEWS),				// talk, daring the guards
        HIDE, HIDE,							// eat and rest, a room
        DO(ACTION_STABLES),
        HIDE, HIDE,							// store, recover items
        TODO,								// the party's composition
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // The stablemaster's horses and mules: an hour, then the trade
    { CityVisit::SCREEN_STABLES, "URBAN00", 1, NULL, { TRADE_THEN(MERCHANT_STABLES, 60, SCREEN_INN) } },
    // the same, "whether any of your mounts are for sale"
    { CityVisit::SCREEN_STABLES_SALE, "URBAN00", 7, NULL, { TRADE_THEN(MERCHANT_STABLES, 60, SCREEN_INN) } },
    // The market at night (in the day table too, for its cards): watched,
    // "a loud thump", "...they quickly run in your direction...", the
    // guard leader leads them away, "take money from scum like you?"
    { CityVisit::SCREEN_MARKET_GUARDED, "MARKE01", 19, NULL, NIGHT_MARKET_OPTIONS },
    { CityVisit::SCREEN_MARKET_STUMBLE, "MARKE01", 2, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_MARKET_ALARM, "MARKE01", 1, NULL, { GO(SCREEN_NIGHT_WATCH_MARKET) } },
    { CityVisit::SCREEN_MARKET_BRIBED, "MARKE01", 9, NULL, { TODO } },			// into the offices
    { CityVisit::SCREEN_MARKET_REFUSED, "MARKE01", 10, NULL, { GO(SCREEN_NIGHT_WATCH_MARKET) } },
    // "Who violates the curfew of $PlaceName?" (file 0xBF0BB)
    { CityVisit::SCREEN_NIGHT_WATCH, "NIGHT00", 0, NULL, WATCH_OPTIONS },
    { CityVisit::SCREEN_NIGHT_WATCH_MARKET, "NIGHT00", 1, NULL, WATCH_OPTIONS },
    { CityVisit::SCREEN_NIGHT_WATCH_AGAIN, "NIGHT00", 2, NULL, WATCH_OPTIONS },
    { CityVisit::SCREEN_NIGHT_WATCH_CAUGHT, "NIGHT00", 4, NULL, WATCH_CAUGHT_OPTIONS },
    // "Dashing down narrow lanes and alleys, you outdistance the night
    // watch."
    { CityVisit::SCREEN_WATCH_ESCAPED, "NIGHT00", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "When the night watch sees your naked steel, they gasp... and flee"
    { CityVisit::SCREEN_WATCH_SCARED, "NIGHT00", 7, NULL, { DO(ACTION_WATCH_RETURN) } },
    // "You look with regret on the unconscious and bleeding night watch."
    { CityVisit::SCREEN_WATCH_BEATEN, "NIGHT00", 8, NULL, { DO(ACTION_WATCH_RETURN) } },
    // "You fall back around a corner, sheath your weapons..." (the side
    // streets: inferred)
    { CityVisit::SCREEN_WATCH_RETREAT, "NIGHT00", 10, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "The night watch strips you of weapons, armor..." (the dungeon is
    // not implemented)
    { CityVisit::SCREEN_WATCH_PRISON, "NIGHT00", 11, NULL, { DO(ACTION_TO_PRISON) } },
    // "You carefully select which items to leave with the innkeeper...",
    // "You sort through the various goods...": the cache, then an hour
    { CityVisit::SCREEN_STORE, "URBAN00", 5, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
    { CityVisit::SCREEN_RECOVER, "URBAN00", 6, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
    // The banks (DARKLAND.EXE, file 0xC41E7 and 0xC6253): letters of
    // credit, the tasks against robber knights and their rewards; the
    // other tasks and politics are not implemented
#define FUGGER_TASKS { ACTION_BANK_TASKS, 8, kNeedsFuggerTasks, 0 }
#define MEDICI_TASKS { ACTION_BANK_TASKS, 6, kNeedsMediciTasks, 0 }
#define FUGGER_REWARD { ACTION_BANK_REWARD, 8, kNeedsFuggerReward, 0 }
#define MEDICI_REWARD { ACTION_BANK_REWARD, 6, kNeedsMediciReward, 0 }
#define BANK_OPTIONS(redeemed, deposit, tasks, reward) { \
        { ACTION_REDEEM, CityVisit::redeemed, kNeedsBankNotes, 0 }, \
        GO_IF(deposit, kNeedsFlorins),		/* a letter of credit */ \
        tasks,								/* special tasks */ \
        reward,								/* a reward */ \
        TODO,								/* politics */ \
        HIDE,								/* "unused" */ \
        GO(SCREEN_MARKET), \
        GO(SCREEN_SIDE_STREET) \
    }
    { CityVisit::SCREEN_FUGGER, "FUGGE00", 0, NULL, BANK_OPTIONS(SCREEN_FUGGER_REDEEMED,
        SCREEN_FUGGER_DEPOSIT, FUGGER_TASKS, FUGGER_REWARD) },
    { CityVisit::SCREEN_MEDICI, "MEDIC00", 0, NULL, BANK_OPTIONS(SCREEN_MEDICI_REDEEMED,
        SCREEN_MEDICI_DEPOSIT, MEDICI_TASKS, MEDICI_REWARD) },
    // "In the rich, wood-paneled offices of the Hanseatic League..."
    { CityVisit::SCREEN_HANSE, "HANSE00", 0, NULL, {
        TODO, TODO, TODO, TODO, TODO,		// tasks, rewards, politics
        HIDE, HIDE,							// placeholders
        TODO,								// chat with the clerks
        GO(SCREEN_SIDE_STREET),
        GO(SCREEN_MARKET)					// the main door (state 0x15)
    } },
    // "...the guards grip their weapons and watch you carefully": the
    // same, with no tasks (a reputation under 0)
    { CityVisit::SCREEN_FUGGER_COLD, "FUGGE00", 2, NULL, BANK_OPTIONS(SCREEN_FUGGER_REDEEMED,
        SCREEN_FUGGER_DEPOSIT, HIDE, FUGGER_REWARD) },
    { CityVisit::SCREEN_MEDICI_COLD, "MEDIC00", 2, NULL, BANK_OPTIONS(SCREEN_MEDICI_REDEEMED,
        SCREEN_MEDICI_DEPOSIT, HIDE, MEDICI_REWARD) },
    // "...counts out from the purse the full amount, $Money1. Then he
    // deducts $Money2 from the pile."
    { CityVisit::SCREEN_FUGGER_REDEEMED, "FUGGE00", 6, NULL, { GO(SCREEN_FUGGER) } },
    { CityVisit::SCREEN_MEDICI_REDEEMED, "MEDIC00", 6, NULL, { GO(SCREEN_MEDICI) } },
    // "You pool your resources and give the clerk enough coins for a note
    // worth..." (then "Deposit how many Florins?")
    { CityVisit::SCREEN_FUGGER_DEPOSIT, "FUGGE00", 3, NULL, { { ACTION_DEPOSIT, CityVisit::SCREEN_FUGGER, kAlways, 0 } } },
    { CityVisit::SCREEN_MEDICI_DEPOSIT, "MEDIC00", 3, NULL, { { ACTION_DEPOSIT, CityVisit::SCREEN_MEDICI, kAlways, 0 } } },
#undef BANK_OPTIONS
#undef FUGGER_REWARD
#undef MEDICI_REWARD
    // "Among various guilds and merchant townhouses, you find the home of
    // $NamedOneName, a respected physician..." (file 0xA2E6A)
    { CityVisit::SCREEN_PHYSICIAN, "PHYSI00", 0, NULL, {
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
    { CityVisit::SCREEN_PHYSICIAN_SHUT, "PHYSI00", 3, NULL, { GO(SCREEN_DISTRICT) } },
    // "...since $Number1 of you suffer, the overall cost will be $Money1"
    { CityVisit::SCREEN_PHYSICIAN_PRICE, "PHYSI00", 2, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...decides that $NamedOneName has $Text1 skill"
    { CityVisit::SCREEN_PHYSICIAN_SKILL, "PHYSI00", 8, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...unable to yet determine the competence of this person"
    { CityVisit::SCREEN_PHYSICIAN_UNSURE, "PHYSI00", 9, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...$NamedOneName is a complete idiot" (and the party leaves)
    { CityVisit::SCREEN_PHYSICIAN_IDIOT, "PHYSI00", 10, NULL, { GO(SCREEN_DISTRICT) } },
    // "I have no need for additional medicines"
    { CityVisit::SCREEN_PHYSICIAN_NO_TRADE, "PHYSI00", 12, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...the physician uses leeches to draw out the vile humors"
    { CityVisit::SCREEN_PHYSICIAN_TREATED, "PHYSI00", 13, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...your purse lacks enough money for everyone"
    { CityVisit::SCREEN_PHYSICIAN_POOR, "PHYSI00", 14, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "...I will take some of you as students for $Money1 daily"
    { CityVisit::SCREEN_PHYSICIAN_TUTOR, "PHYSI00", 4, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "I already have $Number1 apprentices"
    { CityVisit::SCREEN_PHYSICIAN_APPRENTICES, "PHYSI00", 5, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "Your skills in healing match my own"
    { CityVisit::SCREEN_PHYSICIAN_NOTHING, "PHYSI00", 6, NULL, { GO(SCREEN_PHYSICIAN) } },
    // "I am unable to take any students"
    { CityVisit::SCREEN_PHYSICIAN_NO_STUDENTS, "PHYSI00", 11, NULL, { GO(SCREEN_PHYSICIAN) } },
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
    { CityVisit::SCREEN_ALCHEMIST, "ALCHE00", 0, NULL, ALCHEMIST_OPTIONS },
    { CityVisit::SCREEN_ALCHEMIST_AGAIN, "ALCHE00", 1, NULL, ALCHEMIST_OPTIONS },
#undef ALCHEMIST_OPTIONS
    // "...none of you knows enough about alchemy", "Painful peril awaits
    // any who disturb my slumber", "...before I turn you into toads!"
    { CityVisit::SCREEN_ALCHEMIST_UNKNOWN, "ALCHE00", 2, NULL, { GO(SCREEN_CRAFTS) } },
    { CityVisit::SCREEN_ALCHEMIST_NIGHT, "ALCHE00", 3, NULL, { GO(SCREEN_CRAFTS) } },
    { CityVisit::SCREEN_ALCHEMIST_ANGRY, "ALCHE00", 6, NULL, { GO(SCREEN_CRAFTS) } },
    // "...your abilities are beyond my own", "...improve your
    // philosopher's stone to quality $Number1"
    { CityVisit::SCREEN_STONE_BEYOND, "ALCHE00", 5, NULL, { GO(SCREEN_ALCHEMIST_AGAIN) } },
    { CityVisit::SCREEN_STONE_IMPROVED, "ALCHE00", 7, NULL, { GO(SCREEN_ALCHEMIST_AGAIN) } },
    // "Among the dark townhouses... he peers at you through a crack in
    // the door" (outside the game's day, file 0xA2EDB)
    { CityVisit::SCREEN_PHYSICIAN_NIGHT, "PHYSI00", 1, NULL, {
        HIDE,
        DO_IF(ACTION_ASK_AID, kNeedsWounded),
        HIDE,
        DO(ACTION_COMPONENTS),
        DO_IF(ACTION_TREATMENT, kNeedsTreatment),
        DO(ACTION_LEAVE_PHYSICIAN),			// apologize and leave
        DO(ACTION_APOLOGIZE)				// and give him two groschen
    } },
    // "As you walk away, the physician loudly curses you."
    { CityVisit::SCREEN_PHYSICIAN_CURSES, "PHYSI00", 7, NULL, { GO(SCREEN_CRAFTS) } },
    { CityVisit::SCREEN_OUTSIDE_CAPITAL, "CITYE00", 5, NULL, OUTSIDE_OPTIONS },
    { CityVisit::SCREEN_OUTSIDE_FREE, "CITYE00", 6, NULL, OUTSIDE_OPTIONS },
    // "Since it's night, you wait until dawn...", "You wait until night
    // falls..."
    { CityVisit::SCREEN_WAIT_DAWN, "CITYE00", 1, NULL, { GO(SCREEN_DAY_GATE) } },
    { CityVisit::SCREEN_WAIT_NIGHT, "CITYE00", 2, NULL, { GO(SCREEN_NIGHT_GATE) } },
    // "At the gate a line of people wait to enter... the overall cost
    // for your party will be $Money1." (state 2, file 0x925B0)
    { CityVisit::SCREEN_DAY_GATE, "CITYG01", 0, NULL, DAY_GATE_OPTIONS },
    { CityVisit::SCREEN_DAY_GATE_GUARDED, "CITYG01", 18, NULL, DAY_GATE_OPTIONS },
    { CityVisit::SCREEN_TOLL_PAID, "CITYG01", 1, NULL, { GO(SCREEN_MAIN_STREET) } },
    { CityVisit::SCREEN_GUARDS_CHARMED, "CITYG01", 2, NULL, { GO(SCREEN_MAIN_STREET) } },
    { CityVisit::SCREEN_GUARDS_UNMOVED, "CITYG01", 3, NULL, { DO(ACTION_BACK_TO_GATE) } },
    { CityVisit::SCREEN_SLIPPED_IN, "CITYG01", 4, NULL, { GO(SCREEN_MAIN_STREET) } },
    { CityVisit::SCREEN_SLIP_NOTICED, "CITYG01", 5, NULL, { GO(SCREEN_OUTSIDE) } },
    // "In the dead of night you approach the closed gate..." (state 3,
    // file 0x93448)
    { CityVisit::SCREEN_NIGHT_GATE, "CITYG00", 0, NULL, {
        DO_IF(ACTION_HAIL_WATCH, kNeedsHail),	// rely on your fame
        DO_IF(ACTION_TALK_TO_WATCH, kNeedsTalk),	// talk your way inside
        DO_IF(ACTION_BRIBE_WATCH, kNeedsNightBribe),	// $Money1
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
        DO(ACTION_FALL_BACK)
    } },
    { CityVisit::SCREEN_NIGHT_GATE_ALERTED, "CITYG00", 0, NULL, {
        HIDE, HIDE, HIDE,
        TODO,
        DO_IF(ACTION_SAINT, kNeedsSaint),
        DO(ACTION_FALL_BACK)
    } },
    // "Holy Sacraments!... ushered into the city by a worshipful gateman"
    { CityVisit::SCREEN_NIGHT_GATE_OPENED, "CITYG00", 1, NULL, { GO(SCREEN_MAIN_STREET) } },
    // "Nobody through the gates till dawn."
    { CityVisit::SCREEN_NIGHT_GATE_SHUT, "CITYG00", 2, NULL, { DO(ACTION_BACK_TO_GATE) } },
    // "I recognize you. We've got a nice cozy dungeon cell waiting!"
    { CityVisit::SCREEN_NIGHT_GATE_ALARM, "CITYG00", 3, NULL, { GO(SCREEN_NIGHT_GATE_ALERTED) } },
    // "...the watchman isn't very bright. ...He opens a sally port."
    { CityVisit::SCREEN_NIGHT_GATE_TALKED, "CITYG00", 4, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "...$Money1, saying, 'Isn't this sufficient for our toll?'"
    { CityVisit::SCREEN_NIGHT_GATE_BRIBED, "CITYG00", 5, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "You retire to a quiet corner of the woods near the gate..."
    { CityVisit::SCREEN_NIGHT_GATE_RETIRED, "CITYG00", 12, NULL, { GO(SCREEN_OUTSIDE) } },
    // "You stare glumly at the least-guarded section of $PlaceName's
    // walls." (state 0xE, file 0x99AEC)
    { CityVisit::SCREEN_DAY_WALL, "CITYW00", 0, NULL, {
        DO_IF(ACTION_BRIBE_WALL, kNeedsWallBribe),	// $Money1 at a door
        { ACTION_ROPE, 0, kNeedsRope, 0 },	// $ChosenOneName and a rope
        { ACTION_CLIMB, 0, kNeedsNoRope, 0 },	// everybody, no rope
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
        GO(SCREEN_OUTSIDE)					// fall back
    } },
    // "You finish your examination at night... wait until dawn"
    { CityVisit::SCREEN_WALL_DAWN, "CITYE00", 3, NULL, { GO(SCREEN_DAY_WALL) } },
    { CityVisit::SCREEN_DAY_WALL_BRIBED, "CITYW00", 1, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_DAY_WALL_ROPE, "CITYW00", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_DAY_WALL_FALL, "CITYW00", 4, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { CityVisit::SCREEN_DAY_WALL_CLIMBED, "CITYW00", 11, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_DAY_WALL_SLIP, "CITYW00", 12, NULL, { GO(SCREEN_DAY_WALL_HELP) } },
    { CityVisit::SCREEN_DAY_WALL_HELP, "CITYW00", 13, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { CityVisit::SCREEN_DAY_WALL_SLIP_ALONE, "CITYW00", 12, NULL, { DO(ACTION_BACK_TO_WALL) } },	// nobody up
    // "Dark masses of stone loom over you." (state 0xF, file 0x9A7E0)
    { CityVisit::SCREEN_NIGHT_WALL, "CITYW01", 0, NULL, {
        { ACTION_ROPE, 0, kNeedsRope, 0 },
        { ACTION_CLIMB, 0, kNeedsClimb, 0 },
        DO_IF(ACTION_GRATE, kNeedsGrate),	// force a sewer grate
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
        GO(SCREEN_OUTSIDE)					// fall back
    } },
    // "It's broad daylight when you finish... wait until dark"
    { CityVisit::SCREEN_WALL_DUSK, "CITYE00", 4, NULL, { GO(SCREEN_NIGHT_WALL) } },
    { CityVisit::SCREEN_NIGHT_WALL_ROPE, "CITYW01", 1, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_NIGHT_WALL_FALL, "CITYW01", 2, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { CityVisit::SCREEN_NIGHT_WALL_CLIMBED, "CITYW01", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_NIGHT_WALL_SLIP, "CITYW01", 11, NULL, { GO(SCREEN_NIGHT_WALL_HELP) } },
    { CityVisit::SCREEN_NIGHT_WALL_HELP, "CITYW01", 12, NULL, { DO(ACTION_BACK_TO_WALL) } },
    { CityVisit::SCREEN_NIGHT_WALL_SLIP_ALONE, "CITYW01", 11, NULL, { DO(ACTION_BACK_TO_WALL) } },	// nobody up
    { CityVisit::SCREEN_SEWER, "CITYW01", 4, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_SEWER_STUCK, "CITYW01", 5, NULL, { DO(ACTION_BACK_TO_WALL) } },
    // "I recognize them! They're wanted here -- arrest them all!" (state
    // 1, file 0x914FE)
    { CityVisit::SCREEN_CHALLENGE, "CHALL00", 0, NULL, {
        DO(ACTION_GUARDS_FIGHT),			// draw weapons and fight back
        DO(ACTION_CHALLENGE_RUN),			// run down a side street
        DO(ACTION_GUARDS_TALK),				// talk your way out
        DO_IF(ACTION_GUARDS_BRIBE, kNeedsGuardsBribe),	// $Money1
        TODO,								// potion
        DO_IF(ACTION_SAINT, kNeedsSaint),
        WAIT(SCREEN_CHALLENGE_ARRESTED, 3 * 60)	// surrender (file 0x91E44)
    } },
    // "You defeat the guards utterly.", "...you flee down the street."
    { CityVisit::SCREEN_CHALLENGE_WON, "CHALL00", 1, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { CityVisit::SCREEN_CHALLENGE_FLED, "CHALL00", 3, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    // "...you troop off to the dungeon." (state 0xD)
    { CityVisit::SCREEN_CHALLENGE_ARRESTED, "CHALL00", 4, NULL, { DO(ACTION_TO_PRISON) } },
    // talked away: the decoy, the "test", the threat
    { CityVisit::SCREEN_CHALLENGE_DECOYED, "CHALL00", 5, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { CityVisit::SCREEN_CHALLENGE_BLUFFED, "CHALL00", 6, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { CityVisit::SCREEN_CHALLENGE_COWED, "CHALL00", 7, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    // "They're stalling." "...Boys, capture those felons!"
    { CityVisit::SCREEN_CHALLENGE_TALK_FAILED, "CHALL00", 8, NULL, { DO(ACTION_GUARDS_FIGHT) } },
    { CityVisit::SCREEN_CHALLENGE_BRIBED, "CHALL00", 9, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { CityVisit::SCREEN_CHALLENGE_REFUSED, "CHALL00", 10, NULL, { DO(ACTION_GUARDS_FIGHT) } },
    // "Dank tassels of moss festoon the walls..." (state 0xD, file
    // 0x988C6), the dark cell, the oubliette, Saint Lucy's light; the
    // options missing from the worse cells are placeholders there
    { CityVisit::SCREEN_CELL, "DUNGE00", 0, NULL, CELL_OPTIONS },
    { CityVisit::SCREEN_DARK_CELL, "DUNGE00", 1, NULL, CELL_OPTIONS },
    { CityVisit::SCREEN_OUBLIETTE, "DUNGE00", 2, NULL, CELL_OPTIONS },
    { CityVisit::SCREEN_LIT_CELL, "DUNGE00", 3, NULL, CELL_OPTIONS },
    // "...confiscate the lockpicks, and hand out a sound beating."
    { CityVisit::SCREEN_PICK_CAUGHT, "DUNGE00", 5, NULL, { DO(ACTION_BACK_TO_CELL) } },
    // "...Ahead is the guardroom." (file 0x999C2)
    { CityVisit::SCREEN_LOCK_PICKED, "DUNGE00", 6, NULL, { DO(ACTION_JAIL_FIGHT) } },
    // "...more guards rush after you." (state 0x7A, the chase)
    { CityVisit::SCREEN_GUARDROOM_WON, "DUNGE00", 8, NULL, { DO(ACTION_TO_CHASE) } },
    { CityVisit::SCREEN_RECAPTURED, "DUNGE00", 9, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { CityVisit::SCREEN_WINDOW_ESCAPED, "DUNGE00", 10, NULL, { DO(ACTION_TO_CHASE) } },
    { CityVisit::SCREEN_WINDOW_CAUGHT, "DUNGE00", 11, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { CityVisit::SCREEN_TUNNEL_DONE, "DUNGE00", 12, NULL, { GO(SCREEN_SIDE_STREET) } },	// "back alley"
    { CityVisit::SCREEN_TUNNEL_FOUND, "DUNGE00", 13, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { CityVisit::SCREEN_TUNNEL_PROGRESS, "DUNGE00", 22, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { CityVisit::SCREEN_SEDUCED, "DUNGE00", 14, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_SCOFFED, "DUNGE00", 15, NULL, { DO(ACTION_BACK_TO_CELL) } },
    { CityVisit::SCREEN_PRAYED, "DUNGE00", 17, NULL, { DO(ACTION_AFTER_PRAYER) } },
    { CityVisit::SCREEN_TO_MAGISTRATE, "DUNGE00", 16, NULL, { DO(ACTION_TO_COURT) } },
    // "...Then he says, 'Art thou guilty?'" (state 0x8C, file 0xFB4A0)
    { CityVisit::SCREEN_MAGISTRATE, "MAGIS00", 0, NULL, COURT_OPTIONS },
    { CityVisit::SCREEN_MAGISTRATE_AGAIN, "MAGIS00", 1, NULL, COURT_OPTIONS },
    { CityVisit::SCREEN_UNPLEADED, "MAGIS00", 4, NULL, { GO(SCREEN_SQUARE) } },	// "left in the town square"
    { CityVisit::SCREEN_SENTENCED, "MAGIS00", 5, NULL, { DO(ACTION_TO_EXECUTION) } },
    { CityVisit::SCREEN_FLOGGED, "MAGIS00", 6, NULL, { GO(SCREEN_SQUARE) } },
    { CityVisit::SCREEN_FINED, "MAGIS00", 7, NULL, { GO(SCREEN_SQUARE) } },
    { CityVisit::SCREEN_FINED_FLOGGED, "MAGIS00", 8, NULL, { GO(SCREEN_SQUARE) } },
    { CityVisit::SCREEN_ACQUITTED, "MAGIS00", 9, NULL, { GO(SCREEN_SQUARE) } },
    // "In ominous stillness, a bare-chested, black-hooded executioner..."
    // (state 0x8D, file 0xFBEA8)
    { CityVisit::SCREEN_EXECUTION, "EXECU01", 0, NULL, {
        DO(ACTION_SUBMIT),					// refuse to struggle
        DO(ACTION_BREAK_ROPES),
        DO_IF(ACTION_SAINT, kNeedsSaint)	// pray for deliverance
    } },
    { CityVisit::SCREEN_BEHEADED, "EXECU01", 1, NULL, { GO(SCREEN_EXECUTION) } },	// the next one
    { CityVisit::SCREEN_ROPES_BROKEN, "EXECU01", 6, NULL, { DO(ACTION_EXECUTION_FIGHT) } },
    { CityVisit::SCREEN_ROPES_HOLD, "EXECU01", 11, NULL, { DO(ACTION_RESCUE) } },
    { CityVisit::SCREEN_PARDONED, "EXECU01", 7, NULL, { GO(SCREEN_SQUARE) } },	// pardoned
    { CityVisit::SCREEN_CLAIMED_BY_ABBOT, "EXECU01", 8, NULL, { GO(SCREEN_CHURCH) } },	// "taken to the city church"
    { CityVisit::SCREEN_BOUGHT_OFF, "EXECU01", 9, NULL, { GO(SCREEN_SQUARE) } },
    { CityVisit::SCREEN_MOB, "EXECU01", 10, NULL, { DO(ACTION_MOB_FIGHT) } },
    { CityVisit::SCREEN_EXECUTION_ESCAPED, "EXECU01", 12, NULL, { DO(ACTION_TO_CHASE) } },
    { CityVisit::SCREEN_EXECUTION_RECAPTURED, "EXECU01", 13, NULL, { GO(SCREEN_EXECUTION) } },	// the block again
    // "Hearts pumping, you run down the street..." (state 0x7A, file
    // 0xF2112)
    { CityVisit::SCREEN_CHASE, "CHASE00", 0, NULL, {
        DO(ACTION_CHASE_RUN),				// outdistance the guards
        DO(ACTION_CHASE_FIGHT),
        DO(ACTION_CHASE_AMBUSH),
        DO(ACTION_CHASE_HIDE),				// duck around a corner
        TODO,								// a potion
        WAIT(SCREEN_CHASE_CAUGHT, 3 * 60)	// surrender (file 0xF27AA)
    } },
    { CityVisit::SCREEN_CHASE_WON, "CHASE00", 1, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_CHASE_FLED, "CHASE00", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_CHASE_CAUGHT, "CHASE00", 4, NULL, { DO(ACTION_TO_PRISON) } },
    { CityVisit::SCREEN_HIDDEN_TILL_NIGHT, "CHASE00", 9, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_HIDDEN_TILL_DAWN, "CHASE00", 10, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "...poor old $ChosenOneName sneezes.", the ambush, overtaken
    { CityVisit::SCREEN_HIDING_FOUND, "CHASE00", 11, NULL, { DO(ACTION_CHASE_FIGHT) } },
    { CityVisit::SCREEN_AMBUSH, "CHASE00", 12, NULL, { DO(ACTION_CHASE_FIGHT) } },
    { CityVisit::SCREEN_OVERTAKEN, "CHASE00", 14, NULL, { DO(ACTION_CHASE_FIGHT) } },
    { CityVisit::SCREEN_OUTRUN, "CHASE00", 15, NULL, { GO(SCREEN_SIDE_STREET) } },
    // "Solemnly the priest enters your cell..." (state 0x83, file
    // 0xF675C)
    { CityVisit::SCREEN_PRIEST, "DUNGE01", 0, NULL, {
        DO(ACTION_PRIEST_CONFESSION),		// confess your sins
        DO(ACTION_PRIEST_HELP),				// help you escape
        DO(ACTION_PRIEST_GOOD_WORD),		// with the magistrate
        DO(ACTION_FROM_PRIEST)				// leave you alone
    } },
    { CityVisit::SCREEN_PRIEST_CONFESSION, "DUNGE01", 1, NULL, { DO(ACTION_FROM_PRIEST) } },
    { CityVisit::SCREEN_PRIEST_RELEASED, "DUNGE01", 2, NULL, { GO(SCREEN_CHURCH) } },	// "escorts you to the city church"
    { CityVisit::SCREEN_PRIEST_SMUGGLED, "DUNGE01", 3, NULL, { DO(ACTION_FROM_PRIEST) } },
    { CityVisit::SCREEN_PRIEST_OUTRAGED, "DUNGE01", 4, NULL, { DO(ACTION_FROM_PRIEST) } },
    { CityVisit::SCREEN_PRIEST_GOOD_WORD, "DUNGE01", 5, NULL, { DO(ACTION_AFTER_GOOD_WORD) } },
    { CityVisit::SCREEN_PRIEST_MAGISTRATE, "DUNGE01", 6, NULL, { DO(ACTION_TO_COURT) } },
    // the dungeon's saints (file 0x9932A)
    { CityVisit::SCREEN_BATHILDIS, "DUNGE00", 7, NULL, { GO(SCREEN_SQUARE) } },	// "Soon you are outside."
    { CityVisit::SCREEN_WALL_CRACKED, "DUNGE00", 19, NULL, { GO(SCREEN_SQUARE) } },
    { CityVisit::SCREEN_REINOLD_CLIMB, "DUNGE00", 20, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_EARTHQUAKE, "DUNGE00", 21, NULL, { GO(SCREEN_SQUARE) } },
    { CityVisit::SCREEN_NO_ANSWER, "DUNGE00", 23, NULL, { DO(ACTION_BACK_TO_CELL) } },
    // the guards' saints (file 0x91C62)
    { CityVisit::SCREEN_CHALLENGE_UNANSWERED, "CHALL00", 15, NULL, { DO(ACTION_GUARDS_FIGHT) } },
    { CityVisit::SCREEN_CHRISTINA_LIFTS, "CHALL00", 16, NULL, { LEAVE } },	// "far from noisome, dangerous $PlaceName"
    { CityVisit::SCREEN_GUARDS_AT_PEACE, "CHALL00", 17, NULL, { DO(ACTION_CHALLENGE_RETURN) } },
    { CityVisit::SCREEN_REINOLD_WALKS, "CHALL00", 18, NULL, { GO(SCREEN_SIDE_STREET) } },
    // the gates' and walls' saints (files 0x92D40, 0x93B24, 0x9A392,
    // 0x9B0C4)
    { CityVisit::SCREEN_GATE_LIFTED, "CITYG01", 10, NULL, { GO(SCREEN_MAIN_STREET) } },
    { CityVisit::SCREEN_GATE_UNANSWERED, "CITYG01", 12, NULL, { GO(SCREEN_DAY_GATE) } },
    { CityVisit::SCREEN_NIGHT_GATE_LIFTED, "CITYG00", 10, NULL, { GO(SCREEN_MAIN_STREET) } },
    { CityVisit::SCREEN_NIGHT_GATE_UNANSWERED, "CITYG00", 11, NULL, { DO(ACTION_BACK_TO_GATE) } },
    { CityVisit::SCREEN_DAY_WALL_LIFTED, "CITYW00", 9, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_DAY_WALL_UNANSWERED, "CITYW00", 10, NULL, { GO(SCREEN_DAY_WALL) } },
    { CityVisit::SCREEN_NIGHT_WALL_CHRISTINA, "CITYW01", 8, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_NIGHT_WALL_LIFTED, "CITYW01", 9, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_NIGHT_WALL_UNANSWERED, "CITYW01", 10, NULL, { DO(ACTION_BACK_TO_WALL) } },
    // the watch's saints (file 0xBF81E): "Night, what night?", and the
    // watch again without the saint
    { CityVisit::SCREEN_WATCH_SUNLIGHT, "NIGHT00", 6, NULL, { DO(ACTION_WATCH_RETURN) } },
    { CityVisit::SCREEN_WATCH_UNANSWERED, "NIGHT00", 13, NULL, {
        DO_IF(ACTION_PAY_FINE, kNeedsFine),
        DO(ACTION_RUN),
        TODO,								// potion
        HIDE,								// the saint again (0xEE7C = 2)
        DO(ACTION_FIGHT)
    } },
    // the magistrate's (file 0xFB978) and the execution's (0xFC6B2)
    { CityVisit::SCREEN_COURT_SAINT, "MAGIS00", 2, NULL, { GO(SCREEN_UNPLEADED) } },
    { CityVisit::SCREEN_COURT_UNANSWERED, "MAGIS00", 3, NULL, { GO(SCREEN_MAGISTRATE) } },
    { CityVisit::SCREEN_EXECUTION_SAINT, "EXECU01", 2, NULL, { DO(ACTION_SAINT_RESCUE) } },
    { CityVisit::SCREEN_STORM, "EXECU01", 3, NULL, { GO(SCREEN_SIDE_STREET) } },
    { CityVisit::SCREEN_EXECUTION_UNANSWERED, "EXECU01", 4, NULL, { DO(ACTION_RESCUE) } },
    // leaving through the gate (file 0xBC8C4)
    { CityVisit::SCREEN_GATE_SHOUT, "SELEC00", 1, NULL, { DO(ACTION_AFTER_SHOUT) } },
    { CityVisit::SCREEN_GATE_SLIPPED, "SELEC00", 2, NULL, { LEAVE } },
    { CityVisit::SCREEN_GATE_SAINT, "SELEC00", 7, NULL, { LEAVE } },
    { CityVisit::SCREEN_GATE_SAINT_UNANSWERED, "SELEC00", 8, NULL, { GO(SCREEN_GATE) } },
    { CityVisit::SCREEN_GATE_DASHED, "SELEC00", 9, NULL, { LEAVE } },
    { CityVisit::SCREEN_GATE_FLED, "SELEC00", 10, NULL, { GO(SCREEN_GATE) } },
    { CityVisit::SCREEN_GATE_DUMPED, "SELEC00", 11, NULL, { GO(SCREEN_GATE) } },
    { CityVisit::SCREEN_GATE_ARRESTED, "SELEC00", 12, NULL, { DO(ACTION_TO_PRISON) } },
    // "You are near the great outer wall of $PlaceName." (file 0xBD916)
    { CityVisit::SCREEN_INNER_WALL, "SELEC01", 0, NULL, {
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
    { CityVisit::SCREEN_SEWER_OUT, "SELEC01", 1, NULL, { LEAVE } },
    { CityVisit::SCREEN_SEWER_BLOCKED, "SELEC01", 2, NULL, { GO(SCREEN_INNER_WALL) } },
    { CityVisit::SCREEN_SALLY_BRIBED, "SELEC01", 3, NULL, { LEAVE } },
    { CityVisit::SCREEN_SALLY_ALARM, "SELEC01", 4, NULL, { DO(ACTION_SALLY_CHALLENGE) } },
    { CityVisit::SCREEN_ROPE_DOWN, "SELEC01", 5, NULL, { LEAVE } },
    { CityVisit::SCREEN_WALL_SPOTTED, "SELEC01", 6, NULL, { GO(SCREEN_INNER_WALL) } },
    { CityVisit::SCREEN_OVER_WALL, "SELEC01", 7, NULL, { LEAVE } },
    { CityVisit::SCREEN_OVER_WALL_ONE_FELL, "SELEC01", 8, NULL, { LEAVE } },
    { CityVisit::SCREEN_OVER_WALL_FALLS, "SELEC01", 9, NULL, { LEAVE } },
    { CityVisit::SCREEN_INNER_SAINT, "SELEC01", 10, NULL, { LEAVE } },
    { CityVisit::SCREEN_INNER_SAINT_UNANSWERED, "SELEC01", 11, NULL, { GO(SCREEN_INNER_WALL) } },
    { CityVisit::SCREEN_WAIT_FOR_DARK, "SELEC01", 12, NULL, { DO(ACTION_INNER_AFTER_DARK) } },
    // "You stumble and trip frequently..."
    { CityVisit::SCREEN_STUMBLING, "SIDES01", 1, NULL, { GO(SCREEN_INNER_WALL) } },
    // "Eyes and ears open, you..." (state 0x66, file 0xE3A70)
    { CityVisit::SCREEN_NEWS, "CITYN00", 0, NULL, {
        DO(ACTION_NOTICES),					// the official notices (0x6D)
        DO(ACTION_AFFAIRS),					// elsewhere in the Empire (0x6E)
        DO(ACTION_GOSSIP),					// the situation here (0xAE)
        GO(SCREEN_JOBS),					// special jobs (0x67)
        { ACTION_GOSSIP, 0, kNeedsUnrestHere, 0 },	// politics: unrest
                                            // here (0E76:3470(2, location))
        HIDE, HIDE, HIDE, HIDE,				// placeholders
        DO(ACTION_NEWS_RETURN)				// have learned what you can
    } },
    // "...A squad of city guardsmen leap into the common room!"
    { CityVisit::SCREEN_INN_RAID, "URBAN00", 4, NULL, { DO(ACTION_INN_RAID) } },
    // the notices (state 0x6D, file 0xE97C2)
    { CityVisit::SCREEN_NOTICES_EXPLAINED, "OFFIC00", 4, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_NOTICES_TOO_DARK, "OFFIC00", 5, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_NOTICE_CURFEW, "OFFIC00", 0, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_NOTICE_CURFEW_LORD, "OFFIC00", 6, NULL, { DO(ACTION_NEWS_NEXT) } },
    // "...nobody has any travellers' tales" (state 0x6E, file 0xE9FE4)
    { CityVisit::SCREEN_AFFAIRS_NONE, "AFFAI00", 3, NULL, { DO(ACTION_NEWS_NEXT) } },
    // the gossip (state 0xAE, file 0x10F96E)
    { CityVisit::SCREEN_GOSSIP_NOTHING, "SITUA01", 0, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_JOKES, "SITUA01", 5, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_DULL, "SITUA01", 6, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_NOTHING_EVER, "SITUA01", 7, NULL, { DO(ACTION_NEWS_NEXT) } },
    // "After a few casual conversations, you learn that..." (state
    // 0x67, file 0xE3F7A): the employers' leads are quests, not
    // implemented; 1 and 2 need an event of kind 2 here (0E76:360C)
    { CityVisit::SCREEN_JOBS, "SPECI00", 0, NULL, {
        TODO_IF(kNeedsJobRumor),			// a well-placed personage
        TODO_IF(kNeedsRebelsHere),			// an aristocrat, friend of
        TODO_IF(kNeedsRebelsHere),			// the ruler; people with a
                                            // grudge (0E76:360C(2, 0, here))
        HIDE,
        HIDE, HIDE, HIDE, HIDE, HIDE,		// "info 4." ... "info 8."
        GO(SCREEN_NEWS)						// nothing more to hear
    } },
    // the news of the world (the game's events and the locations'
    // state; see _Notices(), _Affairs(), _Gossip())
    { CityVisit::SCREEN_NOTICE_PRICES, "OFFIC00", 1, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_NOTICE_SIEGE, "OFFIC00", 2, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_NOTICE_ASSEMBLY, "OFFIC00", 3, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_AFFAIRS_OVERTHROWN, "AFFAI00", 1, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_AFFAIRS_CRUSHED, "AFFAI00", 2, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_AFFAIRS_UNREST, "AFFAI00", 4, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_AFFAIRS_DRAGON, "AFFAI00", 5, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_AFFAIRS_MINES, "AFFAI00", 17, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_PRICES, "SITUA01", 1, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_RATS, "SITUA01", 2, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_POLITICS, "SITUA01", 3, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_TRAITORS, "SITUA01", 8, NULL, { DO(ACTION_NEWS_NEXT) } },
    { CityVisit::SCREEN_GOSSIP_NEW_RULERS, "SITUA01", 9, NULL, { DO(ACTION_NEWS_NEXT) } },
    // the banks' special tasks (file 0xC473E, 0xC677E): the purse, or
    // "busy all week"
    { CityVisit::SCREEN_FUGGER_TASK, "FUGGE00", 1, NULL, { DO(ACTION_QUEST_OFFER) } },
    { CityVisit::SCREEN_MEDICI_TASK, "MEDIC00", 1, NULL, { DO(ACTION_QUEST_OFFER) } },
    { CityVisit::SCREEN_FUGGER_BUSY, "FUGGE00", 10, NULL, { GO(SCREEN_FUGGER) } },
    { CityVisit::SCREEN_MEDICI_BUSY, "MEDIC00", 10, NULL, { GO(SCREEN_MEDICI) } },
    // the robber knight's offer (state 0x90, file 0xFE800), then where
    // his castle is; back to the patron
    { CityVisit::SCREEN_ROBBER_FUGGER, "RAUBI00", 6, NULL, { GO(SCREEN_ROBBER_WHEREABOUTS) } },
    { CityVisit::SCREEN_ROBBER_MEDICI, "RAUBI00", 8, NULL, { GO(SCREEN_ROBBER_WHEREABOUTS) } },
    { CityVisit::SCREEN_ROBBER_WHEREABOUTS, "RAUBI00", 14, NULL, { DO(ACTION_QUEST_RETURN) } },
    // "Its spire outlined against the sky, the tower of the robber knight
    // $NamedOneName is impressive." (state 0x93, file 0xFF716), or "...the
    // raubritter has constructed a rude fort."
    { CityVisit::SCREEN_TOWER, "RAUBI03", 0, NULL, TOWER_OPTIONS },
    { CityVisit::SCREEN_FORT, "RAUBI03", 24, NULL, TOWER_OPTIONS },
    // the tower's cards (file 0xFFD5A...), in the order of the screens
    { CityVisit::SCREEN_SIEGE, "RAUBI03", 1, NULL, { DO(ACTION_AFTER_CARD) } },	// the siege
    { CityVisit::SCREEN_SIEGE_ATTACK, "RAUBI03", 7, NULL, { DO(ACTION_TOWER_FIGHT_KNIGHT) } },
    { CityVisit::SCREEN_SIEGE_RETURN, "RAUBI03", 8, NULL, { DO(ACTION_TOWER_FIGHT_MEN) } },
    { CityVisit::SCREEN_SIEGE_SALLY, "RAUBI03", 9, NULL, { DO(ACTION_TOWER_FIGHT_MEN) } },
    { CityVisit::SCREEN_TOWER_REFUSED, "RAUBI03", 10, NULL, { DO(ACTION_AFTER_CARD) } },	// turned away
    { CityVisit::SCREEN_TOWER_WELCOME, "RAUBI03", 11, NULL, { { ACTION_TOWER_INSIDE, 0x95, kAlways, 0 } } },
    { CityVisit::SCREEN_TOWER_ATTACK, "RAUBI03", 12, NULL, { DO(ACTION_TOWER_FIGHT_MEN) } },
    { CityVisit::SCREEN_DUEL, "RAUBI03", 13, NULL, { DO(ACTION_TOWER_FIGHT_KNIGHT) } },
    { CityVisit::SCREEN_DUEL_MEN, "RAUBI03", 14, NULL, { DO(ACTION_TOWER_FIGHT_MEN) } },
    { CityVisit::SCREEN_TOWER_SAINT, "RAUBI03", 15, NULL, { { ACTION_TOWER_INSIDE, 0x95, kAlways, 0 } } },
    { CityVisit::SCREEN_SNEAK_IN, "RAUBI03", 16, NULL, { { ACTION_TOWER_INSIDE, 0x94, kAlways, 0 } } },
    { CityVisit::SCREEN_SNEAK_HEARD, "RAUBI03", 17, NULL, { DO(ACTION_TOWER_FIGHT_MEN) } },
    { CityVisit::SCREEN_REINOLD_WINDOW, "RAUBI03", 18, NULL, { { ACTION_TOWER_INSIDE, 0x94, kAlways, 0 } } },
    { CityVisit::SCREEN_TOWER_UNANSWERED, "RAUBI03", 19, NULL, { DO(ACTION_AFTER_CARD) } },	// no answer
    { CityVisit::SCREEN_KNIGHT_SLAIN, "RAUBI03", 20, NULL, { LEAVE } },		// the knight slain
    { CityVisit::SCREEN_DRIVEN_OFF, "RAUBI03", 21, NULL, { DO(ACTION_AFTER_CARD) } },	// driven off
    { CityVisit::SCREEN_LEFT_FOR_DEAD, "RAUBI03", 22, NULL, { LEAVE } },		// left for dead
    { CityVisit::SCREEN_MEN_BEATEN, "RAUBI03", 23, NULL, { DO(ACTION_AFTER_CARD) } },	// his men beaten
    // the slum (file 0xAB104, 0xAB49E): an hour's rest; living there
    { CityVisit::SCREEN_SLUM_REST, "SLUMD00", 5, NULL, { GO(SCREEN_SLUM) } },
    { CityVisit::SCREEN_SLUM_ROOM, "SLUMD00", 2, NULL, { DO(ACTION_SLUM_CAMP) } },
    { CityVisit::SCREEN_SLUM_SHANTY, "SLUMD00", 3, NULL, { DO(ACTION_SLUM_CAMP) } },
    { CityVisit::SCREEN_SLUM_DISTURBED, "SLUMD00", 7, NULL, { DO(ACTION_MEET_THIEVES) } },
    // "Suddenly alert, $ChosenOneName senses danger nearby..." (state
    // 0x24, file 0xAC140)
    { CityVisit::SCREEN_THIEVES, "CITYT00", 1, NULL, {
        DO(ACTION_THIEVES_GROVEL),			// offer all your possessions
        DO(ACTION_THIEVES_TALK),			// your street sense
        DO(ACTION_THIEVES_SCARE),			// armed and dangerous
        DO_IF(ACTION_SAINT, kNeedsSaint),
        DO(ACTION_THIEVES_RUN),
        TODO,								// alchemy
        DO(ACTION_THIEVES_FIGHT)			// attack them first
    } },
    { CityVisit::SCREEN_THIEVES_ROBBED, "CITYT00", 3, NULL, { DO(ACTION_THIEVES_RETURN) } },	// robbed
    { CityVisit::SCREEN_THIEVES_TALKED, "CITYT00", 4, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_UNCONVINCED, "CITYT00", 5, NULL, { DO(ACTION_THIEVES_FIGHT) } },
    { CityVisit::SCREEN_THIEVES_SCARED, "CITYT00", 6, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_UNIMPRESSED, "CITYT00", 7, NULL, { DO(ACTION_THIEVES_FIGHT) } },
    { CityVisit::SCREEN_THIEVES_SAINT, "CITYT00", 8, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_UNANSWERED, "CITYT00", 9, NULL, { DO(ACTION_THIEVES_FIGHT) } },
    { CityVisit::SCREEN_THIEVES_OUTRIDDEN, "CITYT00", 10, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_ELUDED, "CITYT00", 11, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_CAUGHT, "CITYT00", 12, NULL, { DO(ACTION_THIEVES_FIGHT) } },
    { CityVisit::SCREEN_THIEVES_SLAIN, "CITYT00", 15, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_LEFT_FOR_DEAD, "CITYT00", 16, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_THANKED, "CITYT00", 17, NULL, { DO(ACTION_THIEVES_RETURN) } },
    { CityVisit::SCREEN_THIEVES_BLESSED, "CITYT00", 18, NULL, { DO(ACTION_THIEVES_RETURN) } },
    // "Your eye is caught by a sleek-skulled little man with three walnut
    // half-shells..." (state 0xB2, file 0x110C20)
    { CityVisit::SCREEN_SHELL_GAME, "SHELL00", 0, NULL, SHELL_OPTIONS },
    { CityVisit::SCREEN_SHELL_LOST_RIGHT, "SHELL00", 1, NULL, SHELL_OPTIONS },
    { CityVisit::SCREEN_SHELL_LOST_MIDDLE, "SHELL00", 2, NULL, SHELL_OPTIONS },
    { CityVisit::SCREEN_SHELL_LOST_LEFT, "SHELL00", 3, NULL, SHELL_OPTIONS },
    { CityVisit::SCREEN_SHELL_RIGHT, "SHELL00", 4, NULL, SHELLS },
    { CityVisit::SCREEN_SHELL_MIDDLE, "SHELL00", 5, NULL, SHELLS },
    { CityVisit::SCREEN_SHELL_LEFT, "SHELL00", 6, NULL, SHELLS },
    { CityVisit::SCREEN_SHELL_WON, "SHELL00", 7, NULL, SHELL_OPTIONS },
    // the grove's waits (file 0xAA50A, 0xAAAEA at night)
    { CityVisit::SCREEN_GROVE_HOUR, "CITYG05", 1, NULL, { GO(SCREEN_GROVE) } },
    { CityVisit::SCREEN_GROVE_BELL, "CITYG05", 2, NULL, { GO(SCREEN_GROVE) } },
    { CityVisit::SCREEN_GROVE_NAP, "CITYG05", 3, NULL, { { ACTION_GROVE, 3, kAlways, 0 } } },
    { CityVisit::SCREEN_GROVE_AWAKE, "CITYG05", 4, NULL, { GO(SCREEN_GROVE) } },
    { CityVisit::SCREEN_GROVE_MOONLIGHT, "CITYG06", 1, NULL, { GO(SCREEN_GROVE) } },
    { CityVisit::SCREEN_GROVE_DOZE, "CITYG06", 2, NULL, { GO(SCREEN_GROVE) } },
    { CityVisit::SCREEN_GROVE_CAMP, "CITYG06", 3, NULL, { GO(SCREEN_GROVE_MORNING) } },
    { CityVisit::SCREEN_GROVE_MORNING, "CITYG06", 5, NULL, { GO(SCREEN_GROVE) } },
    // the monastery's answers (file 0xB9B3A...)
    { CityVisit::SCREEN_MONASTERY_AGAIN, "CITYM00", 5, NULL, MONASTERY_OPTIONS },
    { CityVisit::SCREEN_MONASTERY_REFUSED, "CITYM00", 1, NULL, { GO(SCREEN_CHURCHES) } },	// refused
    { CityVisit::SCREEN_MONKS_MASS, "CITYM00", 2, NULL, { GO(SCREEN_MONKS_PRAYED) } },
    { CityVisit::SCREEN_MONKS_PRAYED, "CITYM00", 3, NULL, { DO(ACTION_MONASTERY_BACK) } },
    { CityVisit::SCREEN_MONKS_INQUIRE, "CITYM00", 4, NULL, { DO(ACTION_MONASTERY_ANSWER) } },
    { CityVisit::SCREEN_MONKS_BUSY, "CITYM00", 7, NULL, { DO(ACTION_MONASTERY_BACK) } },
    { CityVisit::SCREEN_LIBRARY_CLOSED, "CITYM00", 16, NULL, { DO(ACTION_MONASTERY_BACK) } },
    { CityVisit::SCREEN_ABBESS_REFUSED, "CITYM00", 10, NULL, { GO(SCREEN_CHURCHES) } },
    { CityVisit::SCREEN_ABBESS_PHYSICIAN, "CITYM00", 11, NULL, { GO(SCREEN_CHURCHES) } },
    { CityVisit::SCREEN_MONASTERY_NO_SANCTUARY, "CITYM00", 12, NULL, { GO(SCREEN_CHURCHES) } },
    { CityVisit::SCREEN_MONKS_TUTORS, "CITYM00", 8, NULL, { DO(ACTION_MONASTERY_BACK) } },	// teachers
    { CityVisit::SCREEN_MONKS_NO_TUTORS, "CITYM00", 6, NULL, { DO(ACTION_MONASTERY_BACK) } },	// none
    { CityVisit::SCREEN_MONASTERY_NIGHT_REFUSED, "CITYM01", 1, NULL, { GO(SCREEN_CHURCHES) } },
    { CityVisit::SCREEN_MONKS_NIGHT_PRAYED, "CITYM01", 2, NULL, { GO(SCREEN_CHURCHES) } },
    { CityVisit::SCREEN_MONKS_NIGHT_UNMOVED, "CITYM01", 3, NULL, { GO(SCREEN_CHURCHES) } },
    { CityVisit::SCREEN_MONKS_NIGHT_ABBESS, "CITYM01", 5, NULL, { DO(ACTION_ABBESS) } },
    { CityVisit::SCREEN_MONKS_NIGHT_NO_HELP, "CITYM01", 6, NULL, { GO(SCREEN_CHURCHES) } },
    { CityVisit::SCREEN_MONKS_NIGHT_NO_SANCTUARY, "CITYM01", 8, NULL, { GO(SCREEN_CHURCHES) } },
    // the banks' reward (file 0xC4940, 0xC692E), then the patron's thanks
    // (state 0x91, file 0xFEB8C)
    { CityVisit::SCREEN_FUGGER_REWARD, "FUGGE00", 7, NULL, { DO(ACTION_PATRON_THANKS) } },
    { CityVisit::SCREEN_MEDICI_REWARD, "MEDIC00", 7, NULL, { DO(ACTION_PATRON_THANKS) } },
    { CityVisit::SCREEN_ROBBER_AVENGED, "RAUBI01", 5, NULL, { DO(ACTION_THANKS_RETURN) } },
    { CityVisit::SCREEN_ROBBER_REASON, "RAUBI01", 8, NULL, { DO(ACTION_THANKS_RETURN) } },
    // not a game card: see the constructor
    { CityVisit::SCREEN_NOT_IMPLEMENTED, NULL, 0, NULL, {
        TODO								// go back (handled by Choose())
    } }
};

// At night (see GameTime::IsNight()) these screens show other cards;
// a NULL deck: the same as by day
static const screen_rules kNightScreens[] = {
    // "Mellow lanterns and a warm fire make the $Inn..." (file 0xA7545)
    { CityVisit::SCREEN_INN, "URBAN01", 0, NULL, {
        DO(ACTION_INN_NEWS),				// local news and rumors
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
    { CityVisit::SCREEN_MAIN_STREET, "MAINS02", 0, "XNMAIN.PIC", {
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// crafts district
        GO_IF(SCREEN_INN, CITY_INN),
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        GO(SCREEN_SIDE_STREET),
        DO(ACTION_TO_GROVE),				// a small grove
        GO(SCREEN_GATE)
    } },
    // "Tiny gleams from occasional windows..." (another order)
    { CityVisit::SCREEN_SIDE_STREET, "SIDES01", 0, NULL, {
        GO(SCREEN_MAIN_STREET),
        GO_IF(SCREEN_FORTRESS, CITY_CASTLE),
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO_IF(SCREEN_MARKET, CITY_MARKET),
        GO(SCREEN_CHURCHES),
        GO(SCREEN_DISTRICT),				// crafts district, inns...
        GO_IF(SCREEN_DOCKS, kNeedsHarbor),
        DO(ACTION_TO_GROVE),				// a dark grove
        GO(SCREEN_OTHER),
        WAIT(SCREEN_STUMBLING, 60)			// the city wall (file 0x975FE)
    } },
    // SCREEN_GATE has no night row: the same card 0 (the handler, file
    // 0xBC8C4, never shows cards 13..16, "The gate is closed for the
    // night...")
    { CityVisit::SCREEN_SLEEP, "URBAN01", 2, NULL, { WAIT(SCREEN_INN, 9 * 60) } },	// sleep
    // "Amid the dark shadows of the city square..."
    { CityVisit::SCREEN_SQUARE, "CITYS01", 0, NULL, {
        TODO,								// read the notices
        GO_IF(SCREEN_TOWN_HALL, CITY_TOWN_HALL),
        TODO,								// the prison
        TODO_IF(CITY_ARMORY),				// the barracks: no night card
        GO_IF(SCREEN_UNIVERSITY, CITY_UNIVERSITY),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Flickering torchlight highlights the stone walls of the $fortress"
    // (state 0x1C, file 0xA6240: all its options but the two ways out are
    // hidden, the bribes of $Money1 and $Money2 are never offered)
    { CityVisit::SCREEN_FORTRESS, "CITYF01", 0, NULL, {
        HIDE, HIDE, HIDE, HIDE,				// bribes, talk, saint,
        HIDE, HIDE,							// sneak, potion
        HIDE, HIDE,							// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The $marketplace... is almost empty at night."
    // (DARKLAND.EXE, file 0xA0C62; card 19 when the market is watched)
    { CityVisit::SCREEN_MARKET, "MARKE01", 0, NULL, NIGHT_MARKET_OPTIONS },
    // "Gothic spires are black spikes in the night sky."
    { CityVisit::SCREEN_CHURCHES, "CHURC01", 0, "XNCHRCH.PIC", {
        GO_IF(SCREEN_CATHEDRAL, CITY_CATHEDRAL),
        GO_IF(SCREEN_CHURCH, CITY_CHURCH),
        TODO,								// the $hospital: no place slot
        GO_IF(SCREEN_MONASTERY, CITY_MONASTERY),
        GO_IF(SCREEN_UNIVERSITY, CITY_UNIVERSITY),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "...votive candles cast the only light"
    { CityVisit::SCREEN_CATHEDRAL, "CATHE01", 0, NULL, {
        TODO, TODO, TODO, TODO,				// mass, priest, relic, sanctuary
        GO(SCREEN_CHURCHES),				// leave the cathedral
        HIDE								// a relic as a quest reward
    } },
    // "You come to $cityChurch... It is dark."
    // (DARKLAND.EXE, file 0xB9249)
    { CityVisit::SCREEN_CHURCH, "CITYC01", 0, NULL, {
        DO(ACTION_MASS),
        DO(ACTION_ALTAR_BOY),
        TODO_IF(kNeedsBadReputation),		// seek sanctuary
        GO(SCREEN_CHURCHES)					// leave the church
    } },
    // "At night the monastery is mostly dark..." (state 0x38, file
    // 0xBBAAC)
    { CityVisit::SCREEN_MONASTERY, "CITYM01", 0, "XNMONK.PIC", {
        DO_IF(ACTION_MONKS_NIGHT_PRAY, kNeedsMonksPrayers),
        HIDE, HIDE,
        DO_IF(ACTION_MONKS_NIGHT_HELP, kNeedsAbbess),	// "we perish!"
        DO(ACTION_MONKS_NIGHT_SANCTUARY),
        HIDE, HIDE,							// a problem, the abbot
        HIDE,								// sneak in (not offered)
        GO(SCREEN_CHURCHES)					// go elsewhere
    } },
    // "The $councilHall doors are locked..." (state 0x2C, file 0xB3446: its
    // bribes, as the fortress's, are never offered)
    { CityVisit::SCREEN_TOWN_HALL, "COUNC01", 0, NULL, {
        HIDE, HIDE, HIDE, HIDE, HIDE, HIDE,	// bribes, dungeon...
        HIDE,								// placeholder
        GO_IF(SCREEN_SQUARE, CITY_SQUARE),
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // BUSIN00 has no night card: the day one, without the slum (its night
    // card is a stub, "This is the slum at night. It isn't done yet.")
    { CityVisit::SCREEN_DISTRICT, "BUSIN00", 0, NULL, {
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
    // SCREEN_CRAFTS has no night row (the game shows the crafts' day card
    // at night too: $CIVCR00 card 1 and $CIVCR01 are not used)
    // "Walking along the dark streets, you peer down each one..."
    { CityVisit::SCREEN_ARMS_CRAFTS, "MILCR00", 1, NULL, {
        { ACTION_TRADE, MERCHANT_ARMS_OUTFITTER, kAlways, 120 },
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
    { CityVisit::SCREEN_GROVE, "CITYG06", 0, "XGROVE27.PIC", {
        { ACTION_GROVE, 0, kAlways, 0 },	// an hour
        { ACTION_GROVE, 1, kAlways, 0 },	// a bell
        { ACTION_GROVE, 2, kAlways, 0 },	// camp until morning
        TODO, TODO, TODO, TODO, TODO,		// placeholders
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "Some activity still proceeds on the docks of $PlaceName..."
    { CityVisit::SCREEN_DOCKS, "DOCKS01", 0, "XDOCK.PIC", {
        TODO,								// which boats are sailing
        TODO, TODO,							// escape by boat
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The swordsmiths' courtyards are silent..."
    { CityVisit::SCREEN_SWORDSMITH, "SWORD01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_SWORDSMITH, SCREEN_ARMS_CRAFTS) },
    { CityVisit::SCREEN_BLACKSMITH, "BLACK01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_BLACKSMITH, SCREEN_ARMS_CRAFTS) },
    { CityVisit::SCREEN_ARMORER, "ARMOR01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_ARMORER, SCREEN_ARMS_CRAFTS) },
    { CityVisit::SCREEN_BOWYER, "BOWYE01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_BOWYER, SCREEN_ARMS_CRAFTS) },
    { CityVisit::SCREEN_ARTIFICER, "ARTIF01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_ARTIFICER, SCREEN_CRAFTS) },
    { CityVisit::SCREEN_CLOTHMAKER, "CLOTH01", 0, NULL, NIGHT_SHOP_OPTIONS(MERCHANT_CLOTHMAKER, SCREEN_CRAFTS) },
    { CityVisit::SCREEN_UNWELCOME, "URBAN01", 3, NULL, {					// "no rooms available"
        DO(ACTION_INN_NEWS),
        HIDE, HIDE,
        DO(ACTION_STABLES),
        HIDE, HIDE,
        TODO,
        GO(SCREEN_MAIN_STREET),
        GO(SCREEN_SIDE_STREET)
    } },
    // "The stableboy assures you..." (no trade at night)
    { CityVisit::SCREEN_STABLES, "URBAN01", 1, NULL, { GO(SCREEN_INN) } },
    { CityVisit::SCREEN_STORE, "URBAN01", 5, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
    { CityVisit::SCREEN_RECOVER, "URBAN01", 6, NULL, { { ACTION_CACHE, 0, kAlways, 60, CityVisit::SCREEN_INN } } },
    { CityVisit::SCREEN_INN_RAID, "URBAN01", 4, NULL, { DO(ACTION_INN_RAID) } },	// the guards at the inn
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
#undef TOWER_OPTIONS
#undef SHELL_OPTIONS
#undef MONASTERY_OPTIONS
#undef SHELLS
#undef CELL_OPTIONS
#undef COURT_OPTIONS
#undef DO
#undef DO_IF
#undef SHOP_OPTIONS
#undef NIGHT_SHOP_OPTIONS


namespace {

// The tables by screen: each screen has one row by day and at most one by
// night (a screen without a night row shows its day card); the rows come
// in any order
struct screen_index {
    const screen_rules* day[CityVisit::SCREEN_COUNT];
    const screen_rules* night[CityVisit::SCREEN_COUNT];

    screen_index()
    {
        memset(day, 0, sizeof(day));
        memset(night, 0, sizeof(night));
        _Fill(day, kDayScreens, sizeof(kDayScreens) / sizeof(kDayScreens[0]));
        _Fill(night, kNightScreens,
            sizeof(kNightScreens) / sizeof(kNightScreens[0]));
        // every screen has a day card, but the one that is not implemented
        for (int i = 0; i < CityVisit::SCREEN_COUNT; i++) {
            if (day[i] == NULL)
                throw std::logic_error("CityVisit: a screen has no row");
            if ((day[i]->deck == NULL)
                    != (i == CityVisit::SCREEN_NOT_IMPLEMENTED))
                throw std::logic_error("CityVisit: a screen has no card");
        }
    }

private:
    static void _Fill(const screen_rules** table, const screen_rules* rows,
        size_t count)
    {
        for (size_t i = 0; i < count; i++) {
            const int screen = rows[i].screen;
            if (screen < 0 || screen >= CityVisit::SCREEN_COUNT)
                throw std::logic_error("CityVisit: a row of no screen");
            if (table[screen] != NULL)
                throw std::logic_error("CityVisit: a screen has two rows");
            table[screen] = &rows[i];
        }
    }
};


const screen_index&
Index()
{
    static const screen_index index;
    return index;
}

}	// namespace


const screen_rules&
DayRules(int screen)
{
    return *Index().day[screen];
}


const screen_rules*
NightRules(int screen)
{
    return Index().night[screen];
}


void
CheckScreenTables()
{
    Index();
}
