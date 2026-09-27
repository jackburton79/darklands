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
#include "TradeView.h"

#include <random>
#include <string>
#include <vector>

class GameData;
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
        SCREEN_UNWELCOME,		// the inn, for a wanted party: $URBAN00/01
        SCREEN_STABLES,			// cards 3, 1 and 7
        SCREEN_STABLES_SALE,
        SCREEN_FUGGER,			// the market's banks: $FUGGE00.MSG,
        SCREEN_MEDICI,			// $MEDIC00.MSG,
        SCREEN_HANSE,			// and the League: $HANSE00.MSG
        SCREEN_FUGGER_COLD,		// the banks, for a disliked party
        SCREEN_MEDICI_COLD,
        SCREEN_FUGGER_REDEEMED,	// a letter of credit redeemed,
        SCREEN_MEDICI_REDEEMED,
        SCREEN_FUGGER_DEPOSIT,	// or bought
        SCREEN_MEDICI_DEPOSIT,
        SCREEN_NOT_IMPLEMENTED,
        SCREEN_COUNT
    };

    enum result {
        LEAVE_CITY,				// the party is back on the map
        QUIT
    };

    explicit		CityVisit(GameData& data);	// throws if data is missing

    // The party in the city and the game's clock (not owned). Set them
    // before Enter() or Run(). At night the streets and the gate show
    // their night cards; some options take time. Trading changes the
    // party's money and items.
    void			SetParty(party* members);
    void			SetClock(GameTime* clock)	{ fClock = clock; }
    // The information screens (not owned; NULL: none).
    void			SetInfoView(InfoView* info);
    // The party's reputation in each location of DARKLAND.LOC (not
    // owned; NULL: 0 everywhere).
    void			SetReputations(const std::vector<int16>* reputations)
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

    // The card variables of a city: $PlaceName, $PlaceDesc, $Inn,
    // $citySquare...
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
    int				_Confession();
    int				_Donation();
    // The inn (DARKLAND.EXE, file 0xA6B5E): the price of a meal and a
    // night, sleeping, the stables
    uint32			_InnPrice() const;
    int				_Sleep();
    int				_Stables();
    // The banks' letters of credit (DARKLAND.EXE, file 0xC4568 and
    // 0xC4634; the Medici's are the same)
    int				_Redeem(int result);
    void			_Deposit();
    int				_Reputation() const;
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
    const std::vector<int16>* fReputations;
    bool			fNight;			// the current card is a night card
    int				fCity;
    int				fScreen;
    int				fPreviousScreen;	// where "not implemented" goes back
    std::mt19937	fRandom;
};
