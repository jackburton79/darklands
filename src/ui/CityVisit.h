/*
 * CityVisit.h
 * The party in a city: which card follows which. The original game
 * keeps this logic in DARKLAND.EXE, so the flow is rebuilt from the card
 * texts: the start at the inn, the arrival from the map, the streets,
 * the city's places (square, fortress, market, churches, guilds...) and
 * the gate. What happens inside the places (trade, training, audiences,
 * masses...) is not implemented yet: those options show a "not
 * implemented" card. Options for places the city does not have are
 * hidden.
 *
 * Run() shows the cards in a window; Enter(), Choose() and the view work
 * without one, for testing.
 */
#pragma once

#include "CardView.h"
#include "EventFile.h"
#include "MsgFile.h"
#include "ResidenceView.h"
#include "TradeView.h"

#include <map>
#include <memory>
#include <random>
#include <functional>
#include <string>
#include <vector>

class ExeData;
class GameData;
struct city;
class GameTime;
class GameWindow;
class InfoView;
struct party;

class CityVisit {
public:
    enum screen_id {
        SCREEN_START = 0,		// the game starts at the inn: $PARTY02.MSG
        SCREEN_OUTSIDE,			// arriving from the map: $OUTSI00.MSG
        SCREEN_INN,				// $URBAN00.MSG
        SCREEN_MAIN_STREET,		// $MAINS01.MSG, $MAINS02.MSG
        SCREEN_SIDE_STREET,		// $SIDES00.MSG, $SIDES01.MSG
        SCREEN_GATE,			// leaving through the gate: $SELEC00.MSG
        SCREEN_SLEEP,			// a meal and a night's sleep at the inn
        SCREEN_SQUARE,			// $CITYS00.MSG, $CITYS01.MSG
        SCREEN_FORTRESS,		// $CITYF00.MSG, $CITYF01.MSG
        SCREEN_MARKET,			// $MARKE00.MSG, $MARKE01.MSG
        SCREEN_CHURCHES,		// the religious quarter: $CHURC00/01.MSG
        SCREEN_CATHEDRAL,		// $CATHE00.MSG, $CATHE01.MSG
        SCREEN_CHURCH,			// $CITYC00.MSG, $CITYC01.MSG
        SCREEN_MONASTERY,		// $CITYM00.MSG, $CITYM01.MSG
        SCREEN_UNIVERSITY,		// $UNIVE00.MSG
        SCREEN_TOWN_HALL,		// $COUNC00.MSG, $COUNC01.MSG
        SCREEN_BARRACKS,		// $CITYB00.MSG
        SCREEN_DISTRICT,		// crafts district, inns, slum: $BUSIN00.MSG
        SCREEN_CRAFTS,			// guilds and crafts: $CIVCR00.MSG
        SCREEN_ARMS_CRAFTS,		// arms-making guilds: $MILCR00.MSG
        SCREEN_GROVE,			// where to wait: $CITYG05.MSG, $CITYG06.MSG
        SCREEN_SLUM,			// $SLUMD00.MSG
        SCREEN_DOCKS,			// $DOCKS00.MSG, $DOCKS01.MSG
        SCREEN_OTHER,			// "other locations you remember": $OTHER00
        SCREEN_SWORDSMITH,		// the guilds' shops: $SWORD00/01.MSG,
        SCREEN_BLACKSMITH,		// $BLACK00/01.MSG, $ARMOR00/01.MSG,
        SCREEN_ARMORER,			// $BOWYE00/01.MSG
        SCREEN_BOWYER,
        SCREEN_ARTIFICER,		// the crafts' guilds: $ARTIF00/01.MSG,
        SCREEN_CLOTHMAKER,		// $CLOTH00/01.MSG
        SCREEN_MASS,			// the church's result cards, $CITYC00.MSG
        SCREEN_NO_MASS,			// cards 2..7
        SCREEN_CONFESSION,
        SCREEN_SMALL_DONATION,
        SCREEN_DONATION,
        SCREEN_LARGE_DONATION,
        SCREEN_NIGHT_MASS,		// the church at night: $CITYC01.MSG cards
        SCREEN_NIGHT_NO_MASS,	// 2, 1 and 3
        SCREEN_ALTAR_BOY,
        SCREEN_UNWELCOME,		// the inn, for a wanted party: $URBAN00/01
        SCREEN_STABLES,			// cards 3, 1 and 7
        SCREEN_STABLES_SALE,
        SCREEN_MARKET_GUARDED,	// the market at night: $MARKE01 card 19,
        SCREEN_MARKET_STUMBLE,	// the watch hears you (2),
        SCREEN_MARKET_ALARM,	// the guards come (1),
        SCREEN_MARKET_BRIBED,	// the guards look aside (9),
        SCREEN_MARKET_REFUSED,	// or not (10)
        SCREEN_NIGHT_WATCH,		// the night watch: $NIGHT00 card 0,
        SCREEN_NIGHT_WATCH_MARKET,	// after the market (1),
        SCREEN_NIGHT_WATCH_AGAIN,	// "Not you again" (2),
        SCREEN_NIGHT_WATCH_CAUGHT,	// a member fell behind (4),
        SCREEN_WATCH_ESCAPED,	// you outdistanced them (3),
        SCREEN_WATCH_SCARED,	// they flee from your steel (7),
        SCREEN_WATCH_BEATEN,	// they lie beaten (8),
        SCREEN_WATCH_RETREAT,	// you retreated from the fight (10),
        SCREEN_WATCH_PRISON,	// they took you to the dungeon (11)
        SCREEN_STORE,			// items left with the innkeeper: cards 5
        SCREEN_RECOVER,			// and 6
        SCREEN_FUGGER,			// the market's banks: $FUGGE00.MSG,
        SCREEN_MEDICI,			// $MEDIC00.MSG,
        SCREEN_HANSE,			// and the League: $HANSE00.MSG
        SCREEN_FUGGER_COLD,		// the banks, for a disliked party
        SCREEN_MEDICI_COLD,
        SCREEN_FUGGER_REDEEMED,	// a letter of credit redeemed,
        SCREEN_MEDICI_REDEEMED,
        SCREEN_FUGGER_DEPOSIT,	// or bought
        SCREEN_MEDICI_DEPOSIT,
        SCREEN_PHYSICIAN,		// $PHYSI00.MSG: the physician,
        SCREEN_PHYSICIAN_SHUT,	// his door shut on a wanted party,
        SCREEN_PHYSICIAN_PRICE,	// his price for the wounded,
        SCREEN_PHYSICIAN_SKILL,	// what the party learns of his skill,
        SCREEN_PHYSICIAN_UNSURE,
        SCREEN_PHYSICIAN_IDIOT,
        SCREEN_PHYSICIAN_NO_TRADE,
        SCREEN_PHYSICIAN_TREATED,
        SCREEN_PHYSICIAN_POOR,	// not enough money for the treatment
        SCREEN_PHYSICIAN_TUTOR,	// his answers to would-be students:
        SCREEN_PHYSICIAN_APPRENTICES,	// cards 4, 5, 6 and 11
        SCREEN_PHYSICIAN_NOTHING,
        SCREEN_PHYSICIAN_NO_STUDENTS,
        SCREEN_ALCHEMIST,		// $ALCHE00.MSG: the alchemist (card 0),
        SCREEN_ALCHEMIST_AGAIN,	// the next questions (1),
        SCREEN_ALCHEMIST_UNKNOWN,	// not found (2), at night (3),
        SCREEN_ALCHEMIST_NIGHT,
        SCREEN_ALCHEMIST_ANGRY,	// "...I turn you into toads!" (6),
        SCREEN_STONE_BEYOND,	// the stone: "your abilities are beyond
        SCREEN_STONE_IMPROVED,	// my own" (5), improved (7)
        SCREEN_PHYSICIAN_NIGHT,	// woken at night: card 1,
        SCREEN_PHYSICIAN_CURSES,	// and cursing the party: card 7
        SCREEN_OUTSIDE_CAPITAL,	// before the walls: $CITYE00 card 5 (a
        SCREEN_OUTSIDE_FREE,	// capital), 6 (a free city),
        SCREEN_WAIT_DAWN,		// waiting for dawn (1) or night (2)
        SCREEN_WAIT_NIGHT,
        SCREEN_DAY_GATE,		// the gate by day: $CITYG01 card 0,
        SCREEN_DAY_GATE_GUARDED,	// the guards nervous (18),
        SCREEN_TOLL_PAID,		// the toll paid (1),
        SCREEN_GUARDS_CHARMED,	// the guards befriended (2) or not (3),
        SCREEN_GUARDS_UNMOVED,
        SCREEN_SLIPPED_IN,		// slipped in (4) or not (5)
        SCREEN_SLIP_NOTICED,
        SCREEN_NIGHT_GATE,		// the gate at night: $CITYG00 card 0,
        SCREEN_NIGHT_GATE_ALERTED,	// after the alarm (0, the first
                                // options gone), let in (1), refused (2),
        SCREEN_NIGHT_GATE_OPENED,
        SCREEN_NIGHT_GATE_SHUT,
        SCREEN_NIGHT_GATE_ALARM,	// recognized (3), talked through (4),
        SCREEN_NIGHT_GATE_TALKED,	// bribed (5), falling back (12)
        SCREEN_NIGHT_GATE_BRIBED,
        SCREEN_NIGHT_GATE_RETIRED,
        SCREEN_DAY_WALL,		// the wall by day: $CITYW00 card 0,
        SCREEN_WALL_DAWN,		// having searched until night, the wait
        SCREEN_DAY_WALL_BRIBED,	// for dawn ($CITYE00 3); bribed (1),
        SCREEN_DAY_WALL_ROPE,	// up the rope (3) or fallen (4), all up
        SCREEN_DAY_WALL_FALL,	// (11) or one fallen (12) and the others
        SCREEN_DAY_WALL_CLIMBED,	// back down (13)
        SCREEN_DAY_WALL_SLIP,
        SCREEN_DAY_WALL_HELP,
        SCREEN_DAY_WALL_SLIP_ALONE,	// (12 when nobody is up)
        SCREEN_NIGHT_WALL,		// the wall at night: $CITYW01 card 0,
        SCREEN_WALL_DUSK,		// the wait for the night ($CITYE00 4);
        SCREEN_NIGHT_WALL_ROPE,	// up the rope (1) or fallen (2), all up
        SCREEN_NIGHT_WALL_FALL,	// (3) or one fallen (11) and the others
        SCREEN_NIGHT_WALL_CLIMBED,	// back down (12), through the sewer
        SCREEN_NIGHT_WALL_SLIP,	// (4) or not (5)
        SCREEN_NIGHT_WALL_HELP,
        SCREEN_NIGHT_WALL_SLIP_ALONE,
        SCREEN_SEWER,
        SCREEN_SEWER_STUCK,
        SCREEN_CHALLENGE,		// recognized as wanted: $CHALL00 card 0,
        SCREEN_CHALLENGE_WON,	// the guards defeated (1), fled from (3),
        SCREEN_CHALLENGE_FLED,	// the party arrested (4), talked away
        SCREEN_CHALLENGE_ARRESTED,	// (5, 6, 7) or not (8), the guards
        SCREEN_CHALLENGE_DECOYED,	// bribed (9) or not (10)
        SCREEN_CHALLENGE_BLUFFED,
        SCREEN_CHALLENGE_COWED,
        SCREEN_CHALLENGE_TALK_FAILED,
        SCREEN_CHALLENGE_BRIBED,
        SCREEN_CHALLENGE_REFUSED,
        SCREEN_CELL,			// the dungeon: $DUNGE00 cards 0..3, the
        SCREEN_DARK_CELL,		// cells from the best to the worst and the
        SCREEN_OUBLIETTE,		// dark one lit by Saint Lucy,
        SCREEN_LIT_CELL,
        SCREEN_PICK_CAUGHT,		// the lock: caught (5), picked (6),
        SCREEN_LOCK_PICKED,
        SCREEN_GUARDROOM_WON,	// the guardroom won (8) or lost (9),
        SCREEN_RECAPTURED,
        SCREEN_WINDOW_ESCAPED,	// the window: out (10) or caught (11),
        SCREEN_WINDOW_CAUGHT,
        SCREEN_TUNNEL_DONE,		// the tunnel: out (12), found (13), how
        SCREEN_TUNNEL_FOUND,	// far (22),
        SCREEN_TUNNEL_PROGRESS,
        SCREEN_SEDUCED,			// the turnkey seduced (14) or not (15),
        SCREEN_SCOFFED,
        SCREEN_PRAYED,			// prayers (17), to the magistrate (16)
        SCREEN_TO_MAGISTRATE,
        SCREEN_MAGISTRATE,		// the magistrate: $MAGIS00 card 0, after
        SCREEN_MAGISTRATE_AGAIN,	// the torture (1), no plea (4), the
        SCREEN_UNPLEADED,		// sentences: death (5), flogging (6), a
        SCREEN_SENTENCED,		// fine (7), a fine and a flogging (8),
        SCREEN_FLOGGED,			// acquitted (9)
        SCREEN_FINED,
        SCREEN_FINED_FLOGGED,
        SCREEN_ACQUITTED,
        SCREEN_EXECUTION,		// the execution: $EXECU01 card 0, one
        SCREEN_BEHEADED,		// member beheaded (1), the ropes broken
        SCREEN_ROPES_BROKEN,	// (6) or not (11), the rescues: pardon
        SCREEN_ROPES_HOLD,		// (7), the abbot (8), the bankers (9),
        SCREEN_PARDONED,		// the mob (10); the fight won (12) or
        SCREEN_CLAIMED_BY_ABBOT,	// lost (13)
        SCREEN_BOUGHT_OFF,
        SCREEN_MOB,
        SCREEN_EXECUTION_ESCAPED,
        SCREEN_EXECUTION_RECAPTURED,
        SCREEN_CHASE,			// the chase: $CHASE00 card 0, the guards
        SCREEN_CHASE_WON,		// beaten (1), fled from (3), the party
        SCREEN_CHASE_FLED,		// caught (4), hidden until night (9) or
        SCREEN_CHASE_CAUGHT,	// dawn (10), found (11), the ambush (12),
        SCREEN_HIDDEN_TILL_NIGHT,	// overtaken (14), outrun (15)
        SCREEN_HIDDEN_TILL_DAWN,
        SCREEN_HIDING_FOUND,
        SCREEN_AMBUSH,
        SCREEN_OVERTAKEN,
        SCREEN_OUTRUN,
        SCREEN_PRIEST,			// the priest in the dungeon: $DUNGE01
        SCREEN_PRIEST_CONFESSION,	// card 0, the confession (1), freed by
        SCREEN_PRIEST_RELEASED,	// the Church (2), a tool smuggled in (3),
        SCREEN_PRIEST_SMUGGLED,	// outraged (4), a good word (5), the
        SCREEN_PRIEST_OUTRAGED,	// magistrate (6)
        SCREEN_PRIEST_GOOD_WORD,
        SCREEN_PRIEST_MAGISTRATE,
        SCREEN_BATHILDIS,		// the dungeon's saints: $DUNGE00 cards 7,
        SCREEN_WALL_CRACKED,	// 19 (Dismas, Peter), 20 (Reinold), 21
        SCREEN_REINOLD_CLIMB,	// (Jude), no answer (23)
        SCREEN_EARTHQUAKE,
        SCREEN_NO_ANSWER,
        SCREEN_CHALLENGE_UNANSWERED,	// the guards' saints: $CHALL00 cards
        SCREEN_CHRISTINA_LIFTS,	// 15 (no answer), 16 (Christina), 17,
        SCREEN_GUARDS_AT_PEACE,	// 18 (Reinold)
        SCREEN_REINOLD_WALKS,
        SCREEN_GATE_LIFTED,		// the gates' and walls' saints: $CITYG01
        SCREEN_GATE_UNANSWERED,	// 10, 12; $CITYG00 10, 11; $CITYW00 9,
        SCREEN_NIGHT_GATE_LIFTED,	// 10; $CITYW01 8 (Christina), 9, 10
        SCREEN_NIGHT_GATE_UNANSWERED,
        SCREEN_DAY_WALL_LIFTED,
        SCREEN_DAY_WALL_UNANSWERED,
        SCREEN_NIGHT_WALL_CHRISTINA,
        SCREEN_NIGHT_WALL_LIFTED,
        SCREEN_NIGHT_WALL_UNANSWERED,
        SCREEN_WATCH_SUNLIGHT,	// the watch's saints: $NIGHT00 6, 13
        SCREEN_WATCH_UNANSWERED,
        SCREEN_COURT_SAINT,		// the magistrate's: $MAGIS00 2, 3
        SCREEN_COURT_UNANSWERED,
        SCREEN_EXECUTION_SAINT,	// the execution's: $EXECU01 2, 3
        SCREEN_STORM,			// (Gregory), 4
        SCREEN_EXECUTION_UNANSWERED,
        SCREEN_GATE_SHOUT,		// leaving through the gate: $SELEC00 cards
        SCREEN_GATE_SLIPPED,	// 1 (the guards alerted), 2 (slipped out),
        SCREEN_GATE_SAINT,		// 7, 8 (the saints), 9 (fought through),
        SCREEN_GATE_SAINT_UNANSWERED,	// 10 (beaten back), 11 (beaten
        SCREEN_GATE_DASHED,		// unconscious), 12 (the dungeon)
        SCREEN_GATE_FLED,
        SCREEN_GATE_DUMPED,
        SCREEN_GATE_ARRESTED,
        SCREEN_INNER_WALL,		// the wall from inside: $SELEC01 card 0,
        SCREEN_SEWER_OUT,		// the sewer (1, 2), the sally port (3,
        SCREEN_SEWER_BLOCKED,	// 4), the rope (5), spotted (6), over the
        SCREEN_SALLY_BRIBED,	// wall (7, 8, 9: nobody, one, some
        SCREEN_SALLY_ALARM,		// fallen), the saints (10, 11), waiting
        SCREEN_ROPE_DOWN,		// for the dark (12)
        SCREEN_WALL_SPOTTED,
        SCREEN_OVER_WALL,
        SCREEN_OVER_WALL_ONE_FELL,
        SCREEN_OVER_WALL_FALLS,
        SCREEN_INNER_SAINT,
        SCREEN_INNER_SAINT_UNANSWERED,
        SCREEN_WAIT_FOR_DARK,
        SCREEN_STUMBLING,		// the back alleys at night: $SIDES01 1
        SCREEN_NEWS,			// news and rumors: $CITYN00 card 0; the
        SCREEN_INN_RAID,		// guards at the inn ($URBAN00/01 4); the
        SCREEN_NOTICES_EXPLAINED,	// notices ($OFFIC00 4, 5, 0, 6), news
        SCREEN_NOTICES_TOO_DARK,	// from elsewhere ($AFFAI00 3), gossip
        SCREEN_NOTICE_CURFEW,	// ($SITUA01 0, 5, 6, 7), special jobs
        SCREEN_NOTICE_CURFEW_LORD,	// ($SPECI00 0)
        SCREEN_AFFAIRS_NONE,
        SCREEN_GOSSIP_NOTHING,
        SCREEN_GOSSIP_JOKES,
        SCREEN_GOSSIP_DULL,
        SCREEN_GOSSIP_NOTHING_EVER,
        SCREEN_JOBS,
        SCREEN_NOTICE_PRICES,	// the notices of the city's state: prices
        SCREEN_NOTICE_SIEGE,	// (1), a siege (2), no assemblies (3); news
        SCREEN_NOTICE_ASSEMBLY,	// from elsewhere: the city's ruler
        SCREEN_AFFAIRS_OVERTHROWN,	// overthrown (1), a rebellion put down
        SCREEN_AFFAIRS_CRUSHED,	// (2), unrest elsewhere (4), a dragon (5),
        SCREEN_AFFAIRS_UNREST,	// the mines (17); gossip: prices (1),
        SCREEN_AFFAIRS_DRAGON,	// rats (2), politics (3), traitors (8), a
        SCREEN_AFFAIRS_MINES,	// new government (9)
        SCREEN_GOSSIP_PRICES,
        SCREEN_GOSSIP_RATS,
        SCREEN_GOSSIP_POLITICS,
        SCREEN_GOSSIP_TRAITORS,
        SCREEN_GOSSIP_NEW_RULERS,
        SCREEN_FUGGER_TASK,		// the banks' special tasks: $FUGGE00 and
        SCREEN_MEDICI_TASK,		// $MEDIC00 card 1 (the purse shown), card
        SCREEN_FUGGER_BUSY,		// 10 (too busy); the robber knight's offer:
        SCREEN_MEDICI_BUSY,		// $RAUBI00 card 6 (the Fuggers), 8 (the
        SCREEN_ROBBER_FUGGER,	// Medici), where his castle is (14)
        SCREEN_ROBBER_MEDICI,
        SCREEN_ROBBER_WHEREABOUTS,
        SCREEN_TOWER,			// a castle: the robber knight's tower,
        SCREEN_FORT,			// $RAUBI03 card 0, or his fort (24)
        SCREEN_SIEGE,			// $RAUBI03: the siege camp (1)
        SCREEN_SIEGE_ATTACK,	// the knight attacks (7)
        SCREEN_SIEGE_RETURN,	// his band returns (8)
        SCREEN_SIEGE_SALLY,		// a hungry sally (9)
        SCREEN_TOWER_REFUSED,	// turned away (10)
        SCREEN_TOWER_WELCOME,	// welcomed inside (11)
        SCREEN_TOWER_ATTACK,	// attacked at the door (12)
        SCREEN_DUEL,			// the duel accepted (13)
        SCREEN_DUEL_MEN,		// his men sent instead (14)
        SCREEN_TOWER_SAINT,		// a saint's welcome (15)
        SCREEN_SNEAK_IN,		// in by a window (16)
        SCREEN_SNEAK_HEARD,		// heard (17)
        SCREEN_REINOLD_WINDOW,	// St. Reinold's window (18)
        SCREEN_TOWER_UNANSWERED,	// no saint answers (19)
        SCREEN_KNIGHT_SLAIN,	// the knight slain (20)
        SCREEN_DRIVEN_OFF,		// driven off (21)
        SCREEN_LEFT_FOR_DEAD,	// left for dead (22)
        SCREEN_MEN_BEATEN,		// his men beaten (23)
        SCREEN_SLUM_REST,		// $SLUMD00: an hour's rest (5), a room
        SCREEN_SLUM_ROOM,		// (2) or a shanty (3) to live in, the
        SCREEN_SLUM_SHANTY,		// rest disturbed (7)
        SCREEN_SLUM_DISTURBED,
        SCREEN_THIEVES,			// $CITYT00: the thieves in ambush (1)
        SCREEN_THIEVES_ROBBED,	// the party submits (3)
        SCREEN_THIEVES_TALKED,	// talked into leaving (4)
        SCREEN_THIEVES_UNCONVINCED,	// or not (5)
        SCREEN_THIEVES_SCARED,	// scared away (6)
        SCREEN_THIEVES_UNIMPRESSED,	// or not (7)
        SCREEN_THIEVES_SAINT,	// a saint pacifies them (8)
        SCREEN_THIEVES_UNANSWERED,	// or not (9)
        SCREEN_THIEVES_OUTRIDDEN,	// on horseback (10)
        SCREEN_THIEVES_ELUDED,	// running (11)
        SCREEN_THIEVES_CAUGHT,	// or caught (12)
        SCREEN_THIEVES_SLAIN,	// the thieves slain (15), the neighbors'
        SCREEN_THIEVES_LEFT_FOR_DEAD,	// thanks (17, 18); the party
        SCREEN_THIEVES_THANKED,	// beaten and robbed (16)
        SCREEN_THIEVES_BLESSED,
        SCREEN_SHELL_GAME,		// $SHELL00: the shell game man (0), the
        SCREEN_SHELL_LOST_RIGHT,	// pea under the right-hand (1), middle
        SCREEN_SHELL_LOST_MIDDLE,	// (2) or left-hand shell (3), the
        SCREEN_SHELL_LOST_LEFT,	// shells shuffled: it seems under the
        SCREEN_SHELL_RIGHT,		// right-hand (4), middle (5) or left-hand
        SCREEN_SHELL_MIDDLE,	// shell (6); won (7)
        SCREEN_SHELL_LEFT,
        SCREEN_SHELL_WON,
        SCREEN_GROVE_HOUR,		// $CITYG05: an hour (1), a bell (2),
        SCREEN_GROVE_BELL,		// a nap until nightfall (3), awake
        SCREEN_GROVE_NAP,		// $Number1 hours later (4)
        SCREEN_GROVE_AWAKE,
        SCREEN_GROVE_MOONLIGHT,	// $CITYG06: an hour (1), a bell (2), a
        SCREEN_GROVE_DOZE,		// night under the bushes (3), the
        SCREEN_GROVE_CAMP,		// morning (5)
        SCREEN_GROVE_MORNING,
        SCREEN_MONASTERY_AGAIN,	// $CITYM00: "other requests?" (5), the
        SCREEN_MONASTERY_REFUSED,	// gate slammed (1), the monks' Mass (2,
        SCREEN_MONKS_MASS,		// 3), the monk inquires (4), too busy
        SCREEN_MONKS_PRAYED,	// (7), the library closed (16), the
        SCREEN_MONKS_INQUIRE,	// abbess refuses (10, 11), no sanctuary
        SCREEN_MONKS_BUSY,		// (12)
        SCREEN_LIBRARY_CLOSED,
        SCREEN_ABBESS_REFUSED,
        SCREEN_ABBESS_PHYSICIAN,
        SCREEN_MONASTERY_NO_SANCTUARY,
        SCREEN_MONKS_TUTORS,	// the Master of Novices teaches (8), or
        SCREEN_MONKS_NO_TUTORS,	// the abbot forbids it (6)
        SCREEN_MONASTERY_NIGHT_REFUSED,	// $CITYM01: "come back in the
        SCREEN_MONKS_NIGHT_PRAYED,	// morning" (1), prayers (2) or not
        SCREEN_MONKS_NIGHT_UNMOVED,	// (3), the abbess sent for (5) or not
        SCREEN_MONKS_NIGHT_ABBESS,	// (6), no sanctuary (8)
        SCREEN_MONKS_NIGHT_NO_HELP,
        SCREEN_MONKS_NIGHT_NO_SANCTUARY,
        SCREEN_FUGGER_REWARD,	// $FUGGE00, $MEDIC00 card 7: the reward
        SCREEN_MEDICI_REWARD,	// paid
        SCREEN_ROBBER_AVENGED,	// $RAUBI01: the banker avenged (5), the
        SCREEN_ROBBER_REASON,	// light of reason (8)
        SCREEN_NOT_IMPLEMENTED,
        SCREEN_COUNT
    };

    enum result {
        LEAVE_CITY,				// the party is back on the map
        QUIT,
        PARTY_LOST				// all its members died
    };

    explicit		CityVisit(GameData& data);	// throws if data is missing
                    ~CityVisit();

    // The party in the city and the game's clock (not owned). Set them
    // before Enter() or Run(). At night the streets and the gate show
    // their night cards; some options take time. Trading changes the
    // party's money and items.
    void			SetParty(party* members);
    void			SetClock(GameTime* clock)	{ fClock = clock; }
    // The game's seed global (SaveFile::Seed()), 0 by default
    void			SetSeed(uint16 seed)		{ fSeed = seed; }
    // The information screens (not owned; NULL: none).
    void			SetInfoView(InfoView* info);
    // Called by Run() for Ctrl+S (not set: nothing happens)
    void			SetSaveHandler(
                        const std::function<void(GameWindow&)>& handler)
                        { fSaveHandler = handler; }
    // The party's reputation in each location of DARKLAND.LOC (not
    // owned; NULL: 0 everywhere). Some options change it.
    void			SetReputations(std::vector<int16>* reputations)
                        { fReputations = reputations; }
    // The game's events and the locations' state (+0x14 of their saved
    // records) and arrival (+0x0C), not owned; NULL: none. The news tell
    // of them, the quests change them.
    void			SetWorld(std::vector<world_event>* events,
                        std::vector<uint8>* locationFlags,
                        std::vector<uint16>* enterStates = NULL)
                        { fEvents = events; fLocationFlags = locationFlags;
                          fEnterStates = enterStates; }

    // Runs from `screen` in city `cityIndex` until the party leaves the
    // city or the user quits. Another place of DARKLAND.LOC (a castle...)
    // starts from SCREEN_OUTSIDE, its arrival.
    result			Run(GameWindow& window, int cityIndex, int screen);

    // Step by step: Enter() shows a screen, Choose() takes an option of
    // the current card and returns false when the party left the city.
    void			Enter(int cityIndex, int screen);
    bool			Choose(int option);
    int				Screen() const			{ return fScreen; }
    CardView&		View()					{ return fView; }
    // After Choose(): the merchant to trade with (Run() then shows the
    // trade screen), or -1.
    int				PendingTrade() const	{ return fPendingTrade; }
    TradeView&		Trade()					{ return fTrade; }
    // After Choose(): whether the party takes up residence at the inn
    // (Run() then shows ResidenceView).
    bool			PendingResidence() const	{ return fPendingResidence; }
    // After Choose(): whether the party opens the inn's cache (Run() then
    // shows it on the trade screen), and the items left, by city.
    bool			PendingCache() const		{ return fPendingCache; }
    std::map<int, std::vector<cache_item> >& Caches()	{ return fCaches; }
    ResidenceView&	Residence()				{ return fResidence; }
    // After Choose(): whether the party fights the night watch or the
    // guards (Run() then shows the battle, and ResolveBattle() its
    // outcome)
    bool			PendingBattle() const	{ return fPendingBattle; }
    void			ResolveBattle(int outcome);	// a battle_outcome
    // The inn's price of a meal and a night for the party, in pfennigs
    uint32			InnPrice() const;

    // The card variables of a city: $PlaceName, $PlaceDesc, $Inn,
    // $citySquare...
    // $CityLordTitle: who governs the city for its lord (a Vogt, a
    // Bürgermeister...)
    static std::string	CityLordTitle(const city& c);
    static void		AddCityVariables(GameData& data, int cityIndex,
                        card_variables& variables);
    // The card variables of a party: $LeaderName, $ChosenOneName... (the
    // leader first, then the others in walking order) and the pronouns
    // of the first one ($he, $his...).
    static void		AddPartyVariables(const party& members,
                        card_variables& variables);

private:
    void			_Show(int screen, bool withScene = true);
    // The church's options (DARKLAND.EXE 1838:0214, 03CC, 067C): they
    // change the party and the time, and return the result screen.
    int				_Mass();
    int				_AltarBoy();
    int				_Confession();
    int				_Donation();
    // The inn (DARKLAND.EXE, file 0xA6B5E): the price of a meal and a
    // night, sleeping, the stables

    int				_Sleep();
    int				_Stables();
    // The banks' letters of credit (DARKLAND.EXE, file 0xC4568 and
    // 0xC4634; the Medici's are the same)
    int				_Redeem(int result);
    void			_Deposit();
    // The physician (DARKLAND.EXE, file 0xA2E6A)
    int				_PhysicianSkill();
    int				_Wounded() const;
    uint32			_TreatmentPrice();
    int				_BestHealer() const;
    int				_DiscussTreatments();
    int				_AskAid();
    int				_Components();
    int				_Treatment();
    int				_Students();
    int				_LeavePhysician(bool apologize);
    // The market at night and the night watch (DARKLAND.EXE, file
    // 0xA0C62 and 0xBF0BB); marks are the game's timed events, by kind
    bool			_Marked(int kind) const;
    void			_Mark(int kind, uint32 hours, bool extend = false);
    uint32			_Bribe() const;
    uint32			_Fine() const;
    int				_SneakChance() const;
    int				_NightWalkChance();
    int				_Slowest() const;
    int				_Sneak();
    int				_BribeGuards();
    int				_PayFine();
    int				_RunFromWatch();
    int				_FightWatch();
    int				_GoToGate(bool byDay);
    uint32			_Toll() const;
    int				_TollChance() const;
    int				_CharmChance() const;
    int				_SlipChance() const;
    int				_PayToll();
    int				_CharmGuards();
    int				_SlipIn();
    uint32			_NightBribe() const;
    int				_HailWatch();
    int				_TalkToWatch();
    int				_BribeWatch();
    int				_GoToWall(bool byDay);
    uint32			_WallBribe() const;
    int				_ClimberScore(int member) const;
    int				_BestClimber(int first) const;
    int				_WeakestClimber() const;
    int				_Strongest() const;
    void			_Fall(int member, int amount = 10, int minWounds = 1);
    int				_BribeWall();
    int				_ClimbWithRope(bool byDay);
    int				_ClimbAlone(bool byDay);
    int				_ForceGrate();
    void			_ChangeReputation(int low, int high);
    void			_RunBattle(GameWindow& window);
    // The guards who recognize a wanted party (DARKLAND.EXE, file
    // 0x914FE, state 1)
    int				_Challenge(int from = -1);
    uint32			_ChallengeBribe() const;
    int				_ChallengeTalkChance() const;
    int				_ChallengeBribeChance() const;
    int				_TalkToGuards();
    int				_BribeChallenge();
    void			_FightGuards();
    int				_ResolveGuardBattle(int outcome);
    // The dungeon (DARKLAND.EXE, file 0x988C6, state 0xD), the
    // magistrate (file 0xFB4A0, state 0x8C) and the execution (file
    // 0xFBEA8, state 0x8D)
    int				_EnterPrison();
    void			_Search();
    int				_CellScreen() const;
    void			_WorseCell();
    void			_Beating();
    void			_Flogging();
    void			_GiveEach(int code);
    void			_GiveTo(character& member, int code);
    int				_Picker() const;
    int				_PickChance() const;
    int				_Climber(int* score) const;
    int				_Seductress() const;
    int				_PickLock();
    int				_ClimbWindow();
    int				_Dig();
    int				_Seduce();
    int				_Pray();
    int				_WaitForMagistrate();
    void			_FightJailGuards();
    int				_EnterCourt();
    int				_KeepSilent();
    int				_Plead(bool guilty);
    int				_CourtFine();
    int				_Rescue(int saint = -1);
    int				_BreakRopes();
    void			_FightAtExecution();
    int				_ResolveJailBattle(int outcome);
    int				_ResolveExecutionBattle(int outcome);
    // The chase (DARKLAND.EXE, file 0xF2112, state 0x7A)
    int				_EnterChase();
    int				_ChaseRunChance() const;
    int				_AmbushChance() const;
    int				_HideChance() const;
    int				_OutrunGuards();
    int				_Ambush();
    int				_Hide();
    void			_FightPursuers();
    int				_ResolveChaseBattle(int outcome);
    // The priest in the dungeon (DARKLAND.EXE, file 0xF675C, state 0x83)
    int				_PriestChance() const;
    int				_ConfessToPriest();
    int				_AskPriestForHelp();
    int				_AskGoodWord();
    int				_BackFromPriest();
    // Leaving the city (DARKLAND.EXE: the gate, file 0xBC8C4, state 0x3A;
    // the wall from inside, file 0xBD916, state 0x3B); -1: the party
    // left the city
    int				_ExitWalk();
    int				_ExitHideChance() const;
    int				_ExitHide();
    void			_FightAtGate(bool nervous);
    int				_ResolveGateBattle(int outcome);
    bool			_HasHorses() const;
    void			_LeaveHorses();
    uint32			_InnerWallBribe() const;
    int				_SewerChance(int* member) const;
    int				_WallStealth(int* member) const;
    int				_Sewer(bool horses);
    int				_BribeSally();
    int				_RopeDown(bool afterDark);
    int				_ClimbOver(bool afterDark);
    // News and rumors (DARKLAND.EXE: the inn's option, file 0xA6E40;
    // the menu, file 0xE3A70, state 0x66)
    int				_InnNews();
    int				_Notices();
    int				_Affairs();
    int				_Gossip();
    int				_NextNews();
    // The game's events (0E76:324C, 3180, 32FE, 3470, 360C, 3C28)
    bool			_EventRunning(const world_event& e) const;
    bool			_AnyEvent(int kind) const;
    bool			_EventHere(int kind) const;
    bool			_EventHere(int kind, int subject) const;
    int				_EventLocation(int category, int kind) const;
    uint8			_CityState() const;
    void			_SetPlaceVariables(int place, int from);
    int				_NearestCity(int place) const;
    std::string		_DirectionTo(int from, int place) const;
    // Quests (0E76:2C4E makes an event, 3B62 finds one, 353E, 3404, 392C
    // ask of them)
    int				_AddEvent(int16 unknown1A, int16 location, int16 unknown20,
                        int16 category, int16 subject, int16 unknown1E,
                        int16 unknown26, int hours, int16 unknown24, int16 kind);
    int				_FindEvent(int category, int subject, int location,
                        int unknown1A, int kind, int unknown2A) const;
    bool			_PatronQuest(int kind, int patron) const;
    bool			_RewardDue(int kind, int patron) const;
    bool			_PatronBusy(int patron) const;
    int				_NearestCastle() const;
    int				_BankTaskChance(int patron) const;
    int				_BankTasks(int patron);
    int				_OfferQuest();
    void			_HireAgainstRobber(int patron, int castle, int patronSeed,
                        int reward, int strength, int extra);
    // Saints (DARKLAND.EXE: a card's saints at DS:EE4B, the invocation
    // 0E76:2180, overlay 0x22 at file 0x6B7D0)
    std::vector<int> _SaintsFor(int screen) const;
    bool			_SaintKnown(int screen) const;
    void			_ShowSaints();
    int				_Invoke(int member, int saint);
    int				_SaintAnswered(int screen, int index);
    int				_SaintIgnored(int screen);
    // The alchemist (DARKLAND.EXE, file 0xD9B53)
    int				_AlchemistSkill(bool withBonus) const;
    int				_StoneQuality() const;
    uint32			_StonePrice() const;
    int				_AlchemistChance() const;
    int				_BestSkill(int skill) const;
    void			_SetChosen(int member);
    int				_Stone();
    int				_AlchemistShop();
    int				_Reputation() const;
    // A city's location property 0x21: its number plus the seed global
    uint16			_PeopleSeed() const;
    // A man of the city named by the game (1367:0DB4) for `seed`
    std::string		_PersonName(uint16 seed);
    std::vector<int> _HiddenOptions(int screen) const;
    // The city's record, or an empty one in another place (size 1)
    const city&		_City() const;
    bool			_InCity() const;
    // The robber knight's tower (DARKLAND.EXE, state 0x93, file 0xFF716)
    int				_EnterPlace(int place);
    int				_TowerScreen() const;
    int				_Losses() const;
    void			_AddLosses(int count);
    int				_DuelChance() const;
    int				_TowerSneakChance() const;
    int				_LaySiege();
    int				_AskInside();
    int				_Duel();
    int				_SneakIntoTower();
    int				_StormTower();
    void			_FightKnight();
    void			_FightKnightsMen();
    int				_ResolveKnightBattle(int outcome);
    int				_ResolveMenBattle(int outcome);
    void			_KnightSlain();
    int				_TowerInside(int state);
    void			_ClaimRewards(int kind, int place);
    int				_BankReward(int patron);
    int				_PatronThanks();
    // The slum's lodging, the thieves (state 0x24) and the shell game
    // (state 0xB2)
    int				_SlumLodging();
    int				_AfterSlumCamp();
    int				_MeetThieves();
    int				_ThievesTalkChance() const;
    int				_TalkToThieves();
    int				_ThievesScareChance() const;
    int				_ScareThieves();
    int				_RunFromThieves();
    void			_FightThieves();
    int				_ResolveThievesBattle(int outcome);
    void			_Robbed();
    bool			_FeastNear() const;
    int				_PlayShells(int shell);
    int				_Grove(int option);
    // The city's teachers still here (0E76:3878(0x28, location)), a new
    // one (0E76:2C4E) unless one of its skill is
    bool			_HasTutors() const;
    void			_AddTutor(const city_tutor& tutor);
    int				_TutoringChance();
    int				_AskTutoring();
    // The monastery (states 0x36, 0x38)
    bool			_InMonastery(int screen) const;
    int				_EnterMonastery();
    uint32			_MonksPrice() const;
    int				_BestVirtue() const;
    int				_MonksPrayers();
    int				_NightPrayersChance() const;
    int				_NightPrayers();
    bool			_Wounded(int percent) const;
    int				_AbbessChance(int bonus, int percent) const;
    int				_AskAbbess();
    int				_NightHelp();
    int				_LibraryChance();
    int				_AskLibrary();

    GameData&		fData;
    CardView		fView;
    TradeView		fTrade;
    int				fPendingTrade;
    msg_card		fNotImplementedCard;
    card_variables	fVariables;
    party*			fParty;
    GameTime*		fClock;
    InfoView*		fInfo;
    std::function<void(GameWindow&)> fSaveHandler;
    std::vector<int16>* fReputations;
    bool			fNight;			// the current card is a night card
    int				fCity;
    int				fScreen;
    int				fPreviousScreen;	// where "not implemented" goes back
    std::mt19937	fRandom;
    uint16			fSeed;
    // the physicians met, by city: their skill (the game keeps them as
    // people of the city), and until when they treat no one again
    std::map<int, int>	fPhysicianSkill;
    std::map<int, uint32> fTreatedUntil;	// in hours, see HourStamp()
    std::map<int, uint32> fNoStudentsUntil;
public:
    // The teachers found, by city (see city_tutor): persons of kind 0x28
    const std::map<int, std::vector<city_tutor> >& Tutors() const
                        { return fTutors; }
private:
    std::map<int, std::vector<city_tutor> > fTutors;
    ResidenceView	fResidence;
    bool			fPendingResidence;
    bool			fPendingCache;
    // the items left at the inns; the game keeps them in CACHE.TMP, one
    // cache per location (not read here)
    std::map<int, std::vector<cache_item> > fCaches;
    bool			fTreatmentOffered;
    bool			fStoneOffered;			// once a visit
    std::map<int, uint32> fAlchemistAngryUntil;
    std::map<std::pair<int, int>, uint32> fMarks;	// (kind, city): until
    int				fWatchReturn;	// where paying the fine leads
    bool			fPendingBattle;
    enum battle_kind {
        BATTLE_WITH_WATCH,
        BATTLE_WITH_GATE_GUARDS,
        BATTLE_WITH_JAIL_GUARDS,
        BATTLE_AT_EXECUTION,
        BATTLE_WITH_PURSUERS,
        BATTLE_AT_GATE,
        BATTLE_WITH_KNIGHT,
        BATTLE_WITH_KNIGHTS_MEN,
        BATTLE_WITH_THIEVES
    };
    struct foes {
        int enemy;				// in DARKLAND.ENM
        int variant;
        int count;
    };
    int				fBattleKind;	// a battle_kind
    std::vector<foes> fFoes;
    int				fCell;			// 0..3, as the game's DS:8DEE
    int				fTunnel;		// how far the tunnel is dug, in %
    int				fTortures;
    bool			fMagistrateComing;
    // the saint list shown in place of a card: (member, index in the
    // card's saints)
    bool			fChoosingSaint;
    int				fRescueSaint;	// the saint who answered at the block
    int				fGateReturn;	// where "not leave just yet" goes
    bool			fGateShoutFight;	// card 1 of the gate: the fight next
    int				fAfterDark;		// the wall's option waiting for the dark
    int				fNewsReturn;	// where the news menu goes back to
    std::vector<std::pair<int, int> > fNewsQueue;	// the news' cards
                                    // still to show, and their places
    std::vector<world_event>* fEvents;
    std::vector<uint8>* fLocationFlags;
    std::vector<uint16>* fEnterStates;
    int				fQuestPatron;	// the bank giving a task (8, 6), -1
    int				fQuestPlace;	// the task's place (DS:E896)
    bool			fQuestRobber;	// the task is the robber knight
    int				fAfterCard;		// the tower's results: the next screen,
                                    // -1 the map
    bool			fSlumCamp;		// the pending residence is in the slum
    int				fThievesReturn;	// where the thieves' cards end
    int				fShellReturn;	// the square or the market
    bool			fShellWon;		// DS:8E1C: the man lets a party win once
    int				fGroveHours;	// the nap until nightfall
    int				fMonastery;		// its card shown on arrival
    int				fMonkAnswer;	// the screen after "the monk inquires"
    int				fThanksReturn;	// the patron's screen after $RAUBI01
    std::vector<std::pair<int, int> > fSaintChoices;
    std::unique_ptr<ExeData> fExe;	// the saints' rules
    int				fChallengeReturn;	// where the party came from
    int				fChallengeReputation;	// the reputation then (the
                                            // guards', the chase's)
    bool			fPartyLost;
    bool			fWallFailed;	// this stay at the wall: climbing failed
    std::vector<cache_item> fLoot;
    std::unique_ptr<ExeData> fNames;
};
