/*
 * CityVisitInternal.h
 * What the parts of CityVisit share: what an option does, the screens'
 * rules (the tables are in CityScreens.cpp) and the constants of the
 * options' conditions and of the marks.
 */
#pragma once

#include "CityFile.h"
#include "CityVisit.h"
#include "GameTime.h"


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
    ACTION_INN_NEWS,			// news and rumors
    ACTION_NEWS,				// from `target`, after `minutes` (kHalfSize:
                                // city size / 2 hours)
    ACTION_NEWS_RETURN,
    ACTION_INN_RAID,
    ACTION_NOTICES,
    ACTION_AFFAIRS,
    ACTION_GOSSIP,
    ACTION_NEWS_NEXT,			// the news' next card, or the menu
    ACTION_BANK_TASKS,			// `target`: the patron, 8 the Fuggers, 6
                                // the Medici
    ACTION_QUEST_OFFER,			// after the purse: the task
    ACTION_QUEST_RETURN,		// back to the patron
    ACTION_TOWER_SIEGE,			// the robber knight's tower
    ACTION_TOWER_ASK,
    ACTION_TOWER_DUEL,
    ACTION_TOWER_SNEAK,
    ACTION_TOWER_STORM,
    ACTION_TOWER_FIGHT_KNIGHT,	// after a card: the battles
    ACTION_TOWER_FIGHT_MEN,
    ACTION_TOWER_INSIDE,		// the audience (0x95) or inside (0x94)
    ACTION_AFTER_CARD,			// fAfterCard, or the map
    ACTION_SLUM_LODGING,		// live very cheaply in the slum
    ACTION_SLUM_CAMP,			// then the residence screen
    ACTION_MEET_THIEVES,		// state 0x24
    ACTION_THIEVES_GROVEL,
    ACTION_THIEVES_TALK,
    ACTION_THIEVES_SCARE,
    ACTION_THIEVES_RUN,
    ACTION_THIEVES_FIGHT,
    ACTION_THIEVES_RETURN,		// back where the party was (DS:E7D8)
    ACTION_SHELL_PAY,			// the shell game
    ACTION_SHELL_PICK,			// target: 0 right, 1 middle, 2 left
    ACTION_SHELL_LEAVE,
    ACTION_GROVE,				// target: the option (3: after the nap)
    ACTION_MONKS_PRAY,			// the monastery
    ACTION_MONKS_LIBRARY,
    ACTION_MONKS_TUTORING,
    ACTION_MONKS_HEALING,
    ACTION_MONASTERY_ANSWER,	// after card 4: fMonkAnswer
    ACTION_MONASTERY_BACK,		// its card again
    ACTION_MONKS_NIGHT_PRAY,
    ACTION_MONKS_NIGHT_HELP,
    ACTION_MONKS_NIGHT_SANCTUARY,
    ACTION_ABBESS,				// state 0xB4: not implemented
    ACTION_BANK_REWARD,			// target: the patron
    ACTION_PATRON_THANKS,		// state 0x91
    ACTION_THANKS_RETURN,
    ACTION_CALL_PRIEST,			// the priest in the dungeon
    ACTION_PRIEST_CONFESSION,
    ACTION_PRIEST_HELP,
    ACTION_PRIEST_GOOD_WORD,
    ACTION_AFTER_GOOD_WORD,		// the magistrate, or the cell
    ACTION_FROM_PRIEST,
    ACTION_NIGHT_WALK,			// ACTION_GO, but the watch may stop the
                                // party outside the game's day
    ACTION_TO_GROVE				// the streets' way to the grove
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
static const int kSkillWoodwise		= 18;
// or a member standing who knows one of the card's saints (150B:168C)
static const int kNeedsSaint		= -32;
// or the wall from inside: by the option (horses, a rope, the marks)
static const int kNeedsInnerWall	= -33;
// or the special jobs' first rumor: the city's property 0x21 a multiple
// of 20 and no mark 0x65 (file 0xE3FED)
static const int kNeedsJobRumor		= -34;
// or the game's events: unrest here (0E76:3470(2, location)), its people
// here (0E76:360C(2, 0, location))
static const int kNeedsUnrestHere	= -35;
static const int kNeedsRebelsHere	= -36;
// or the banks' tasks (file 0xC42C3): no task of theirs running from this
// city (0E76:353E(10 or 3, patron, location)), no refusal lately
// (0E76:392C(7, patron, location)), a reputation of 0 or more
static const int kNeedsFuggerTasks	= -37;
static const int kNeedsMediciTasks	= -38;
// or the tower (file 0xFF79B): asking to come inside needs no mark 0x26;
// storming it needs allies (0E76:360C(3, 5 or 0x27, place))
static const int kNeedsTowerWelcome	= -39;
static const int kNeedsTowerAllies	= -40;
// the shell game's "pay him and play": more than a groschen in the purse
// (file 0x110D02)
static const int kNeedsGroschen		= -41;
// the monastery: the prayers (not while mark 0x30, if the purse can pay),
// the library (not while mark 0x34), the abbess (not while mark 0x31)
static const int kNeedsMonksPrayers	= -42;
static const int kNeedsLibrary		= -43;
static const int kNeedsAbbess		= -44;
// tutoring: not while the city has teachers (0E76:3878(0x28)) or after a
// refusal (mark 0x33)
static const int kNeedsTutoring		= -47;
// a bank's reward for a robber knight (0E76:3404(3, patron, location))
static const int kNeedsFuggerReward	= -45;
static const int kNeedsMediciReward	= -46;
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
static const int kMarkTowerAsked	= 0x26;	// the tower's options taken
static const int kMarkShellGame		= 0x2F;	// the shell game man met
static const int kMarkMonksPrayed	= 0x30;	// the monastery (file 0xB9835)
static const int kMarkAbbess		= 0x31;
static const int kMarkMonastery		= 0x32;	// been here lately
static const int kMarkMonksNoTutors	= 0x33;
static const int kMarkLibrary		= 0x34;
static const int kMarkMonksBothered	= 0x35;	// at night

// The game's day for some places (1367:072A): hour 5 to 18; the extra
// hour to reach a guild then (file 0xA47A5)
inline bool
IsGameDay(const GameTime& clock)
{
    return clock.Hour() >= 5 && clock.Hour() <= 18;
}

// Special waiting times
static const int kAnHourMoreAtNight	= -3;	// an hour, two outside the
                                            // game's day
static const int kHalfSize			= -4;	// city size / 2 hours

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


// The tables of city screens, by day and by night (CityScreens.cpp)
extern const screen_rules kScreens[CityVisit::SCREEN_COUNT];
extern const screen_rules kNightScreens[CityVisit::SCREEN_COUNT];

// The octile distance on the map (1462:271A: rows count a third)
int MapDistance(int x1, int y1, int x2, int y2);
