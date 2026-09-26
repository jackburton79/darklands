/*
 * CityVisit.h
 * The party in a city: which card follows which. The original game
 * keeps this logic in DARKLAND.EXE, so the flow is rebuilt from the card
 * texts: the start at the inn, the arrival from the map, the inn, the
 * main street, the side streets and the city gate. Options that lead to
 * places not implemented yet show a "not implemented" card; options for
 * places the city does not have are hidden.
 *
 * Run() shows the cards in a window; Enter(), Choose() and the view work
 * without one, for testing.
 */
#pragma once

#include "CardView.h"
#include "MsgFile.h"

#include <string>
#include <vector>

class GameData;
class GameTime;
class GameWindow;
struct party;

class CityVisit {
public:
    enum screen_id {
        SCREEN_START = 0,		// the game starts at the inn: $PARTY02.MSG
        SCREEN_OUTSIDE,			// arriving from the map: $OUTSI00.MSG
        SCREEN_INN,				// $URBAN00.MSG
        SCREEN_MAIN_STREET,		// $MAINS01.MSG
        SCREEN_SIDE_STREET,		// $SIDES00.MSG
        SCREEN_GATE,			// leaving through the gate: $SELEC00.MSG
        SCREEN_SLEEP,			// a meal and eight hours of sleep at the inn
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
    // their night cards; some options take time.
    void			SetParty(const party* members);
    void			SetClock(GameTime* clock)	{ fClock = clock; }

    // Runs from `screen` in city `cityIndex` until the party leaves the
    // city or the user quits.
    result			Run(GameWindow& window, int cityIndex, int screen);

    // Step by step: Enter() shows a screen, Choose() takes an option of
    // the current card and returns false when the party left the city.
    void			Enter(int cityIndex, int screen);
    bool			Choose(int option);
    int				Screen() const			{ return fScreen; }
    CardView&		View()					{ return fView; }

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
    std::vector<int> _HiddenOptions(int screen) const;

    GameData&		fData;
    CardView		fView;
    msg_card		fNotImplementedCard;
    card_variables	fVariables;
    const party*	fParty;
    GameTime*		fClock;
    bool			fNight;			// the current card is a night card
    int				fCity;
    int				fScreen;
    int				fPreviousScreen;	// where "not implemented" goes back
};
