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
#include "MsgFile.h"
#include "ResidenceView.h"
#include "TradeView.h"

#include <map>
#include <memory>
#include <random>
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
        SCREEN_MONASTERY,		// $MONAS00.MSG, $MONAS01.MSG
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
    // The party's reputation in each location of DARKLAND.LOC (not
    // owned; NULL: 0 everywhere). Some options change it.
    void			SetReputations(std::vector<int16>* reputations)
                        { fReputations = reputations; }

    // Runs from `screen` in city `cityIndex` until the party leaves the
    // city or the user quits.
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
    // After Choose(): whether the party fights the night watch (Run() then
    // shows the battle, and ResolveBattle() its outcome)
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
    void			_Fall(int member);
    int				_BribeWall();
    int				_ClimbWithRope(bool byDay);
    int				_ClimbAlone(bool byDay);
    int				_ForceGrate();
    void			_ChangeReputation(int low, int high);
    void			_RunBattle(GameWindow& window);
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

    GameData&		fData;
    CardView		fView;
    TradeView		fTrade;
    int				fPendingTrade;
    msg_card		fNotImplementedCard;
    card_variables	fVariables;
    party*			fParty;
    GameTime*		fClock;
    InfoView*		fInfo;
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
    // The teachers found, by city (see city_tutor)
    const std::map<int, city_tutor>& Tutors() const	{ return fTutors; }
private:
    std::map<int, city_tutor> fTutors;
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
    bool			fPartyLost;
    bool			fWallFailed;	// this stay at the wall: climbing failed
    std::vector<cache_item> fLoot;
    std::unique_ptr<ExeData> fNames;
};
