/*
 * MapViewer.h
 * Interactive world map: scrolling, city names, a status bar with the
 * tile under the mouse, an info panel for a city, and the party, which
 * travels across the map and reaches cities (Run() then returns, and the
 * caller shows the city).
 *
 * Everything is drawn into a 320x200 8-bit buffer with the map palette
 * (the game's resolution); Run() shows it scaled up in a window. The
 * input handlers, Tick() and Draw() work without a window, for testing.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"
#include "Travel.h"

#include <memory>
#include <functional>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
class GameTime;
class GameWindow;
class InfoView;
class MenuBar;
union SDL_Event;

class MapViewer {
public:
    static const uint16 kScreenWidth	= 320;
    static const uint16 kScreenHeight	= 200;

    explicit		MapViewer(GameData& data);	// throws if data is missing
                    ~MapViewer();

    // Runs until the party reaches a place of DARKLAND.LOC (a city, a
    // castle...): returns its index (the party is then in front of it,
    // CurrentPlace()), or -1 if the user quit, or kLoadRequested for the
    // menu's Load Saved Game.
    // The first version opens its own window.
    static const int kLoadRequested = -2;
    int				Run();
    int				Run(GameWindow& window);

    // The game's clock (not owned; NULL: none): traveling advances it,
    // the status bar shows it.
    void			SetClock(GameTime* clock)	{ fClock = clock; }
    // The information screens that F1..F6 open (not owned; NULL: none).
    void			SetInfoView(InfoView* info)	{ fInfo = info; }
    // The menu bar (right button, F10, shortcuts; not owned; NULL: none)
    void			SetMenuBar(MenuBar* menu)	{ fMenu = menu; }
    // Called by Run() for the menu's Load Saved Game: true if a game was
    // loaded (Run() then returns kLoadRequested); not set: nothing happens
    void			SetLoadHandler(
                        const std::function<bool(GameWindow&)>& handler)
                        { fLoadHandler = handler; }
    // Called by Run() for the menu's Change Marching Order (not set:
    // nothing happens)
    void			SetOrderHandler(
                        const std::function<void(GameWindow&)>& handler)
                        { fOrderHandler = handler; }
    // Called by Run() for the C key (make camp), the party stopped: with
    // the terrain's part of the camp's danger (CampDanger()); it returns 0
    // to go on, else the value Run() then returns (-1 quit, kLoadRequested);
    // not set: nothing happens
    void			SetCampHandler(
                        const std::function<int(GameWindow&, int)>& handler)
                        { fCampHandler = handler; }
    // Called by Run() when the party meets someone on the way (the hazard
    // of DARKLAND.EXE's map step, file 0x5EDA0, see EncounterChance()):
    // with the place nearest to the party, the tile type under it and the
    // state the chooser gave; it returns 0 to go on, else the value Run()
    // then returns (-1 quit, kLoadRequested). `choose(terrain)` gives the
    // state of the meeting, or a negative number for none (the hazard then
    // goes on); `random(n)` is 0..n-1. Not set: no encounters.
    typedef std::function<int(GameWindow&, int, int, int)> encounter_handler;
    void			SetEncounterHandler(const encounter_handler& handler,
                        const std::function<int(int)>& choose,
                        const std::function<int(int)>& random)
                        {
                            fEncounterHandler = handler;
                            fEncounterChooser = choose;
                            fRandom = random;
                        }
    // The chance in a thousand, per step, of the base chance of the place
    // (file 0x607E4: 3, 2 or 1 by the terrain, + the nearest place's weight,
    // DARKLAND.LOC +0x0A), times the party's weariness + 1
    int				EncounterChance(int terrain, int place) const;
    // The place of DARKLAND.LOC nearest to the party (the bigger of the two
    // distances plus half the smaller), or -1
    int				NearestPlace() const;
    // The city nearest to the party (09C0:1FA9 = 1462:271A of overlay 0x1E,
    // file 0x6018A): the larger of the two distances, the rows' a third,
    // plus half the other; the reputation it changes is the one of a meeting
    int				NearestCity() const;
    // The weariness (DS:E488, 0..10) grows slowly (10 in 500 per step,
    // file 0x5F0CC); a meeting resets it (*inferred*)
    int				Weariness() const		{ return fWeariness; }
    // The place of a meeting waiting for Run() to play it, or -1
    int				PendingEncounter() const	{ return fEncounterPlace; }

    // What the place adds to the danger of a camp (DARKLAND.EXE, file
    // 0x6005E; the party's size comes on top, 3 each): a city within 5
    // tiles, and the terrain under the party
    int				CampDanger() const;

    // Called by Run() for Ctrl+S (not set: nothing happens)
    void			SetSaveHandler(
                        const std::function<void(GameWindow&)>& handler)
                        { fSaveHandler = handler; }

    // Puts the party on a tile (e.g. a city it leaves) and centers the view
    // on it; the party stops and is no longer in a city.
    void			SetPartyPosition(const map_position& position);

    // Input, in screen (320x200) coordinates.
    void			ScrollBy(int dx, int dy);
    void			CenterOn(uint16 tileX, uint16 tileY);
    void			CenterOnParty();
    void			MouseMoved(const GFX::point& point);
    // The mouse left the window: hides the cursor.
    void			MouseLeft();
    // Left click: travel there (reaching the place, if there is one).
    void			Clicked(const GFX::point& point);
    // Middle click: the info panel of the city under the mouse (the right
    // button is the menu bar's).
    void			MiddleClicked(const GFX::point& point);
    // Closes the info panel, else stops the party.
    // Returns false if there was nothing to close (i.e. quit).
    bool			Escape();

    // Advances the party one tile along its path, and the clock by the
    // time it takes; returns false if it was not traveling. Run() calls
    // it on a timer.
    bool			Tick();

    bool			IsTraveling() const	{ return !fPath.empty(); }
    const map_position& PartyPosition() const	{ return fParty; }
    // Index of the city whose info panel is open, or -1.
    int				SelectedCity() const	{ return fSelectedCity; }
    // Index of the place the party has reached, or -1 when on the map.
    int				CurrentPlace() const		{ return fPlace; }

    // Draws the current view into the 320x200 buffer and returns it.
    Bitmap*			Draw();

private:
    GFX::point		_ScreenToMap(const GFX::point& point) const;
    int				_CityAt(const GFX::point& mapPoint) const;
    int				_PlaceAt(const GFX::point& mapPoint) const;
    std::string		_PlaceName(int place) const;
    map_position	_PlacePosition(int place) const;
    void			_SetOrigin(int x, int y);
    void			_TravelTo(const map_position& destination, int place);
    void			_EnterPlace(int place);
    void			_KeepPartyVisible();
    bool			_MenuEvent(GameWindow& window, const SDL_Event& event,
                        bool& quitting, bool& loading);
    void			_DrawStatusBar();
    void			_DrawCityPanel();
    void			_DrawCursor();
    void			_DrawText(const Font& font, const std::string& utf8,
                        int x, int y, uint8 color) const;

    GameData&		fData;
    GameTime*		fClock;
    int				fTravelMinutes;	// toward the next hour (DARKLAND.EXE)
    InfoView*		fInfo;
    MenuBar*		fMenu;
    std::function<void(GameWindow&)> fSaveHandler;
    std::function<bool(GameWindow&)> fLoadHandler;
    std::function<void(GameWindow&)> fOrderHandler;
    std::function<int(GameWindow&, int)> fCampHandler;
    encounter_handler fEncounterHandler;
    std::function<int(int)> fEncounterChooser;
    std::function<int(int)> fRandom;
    Bitmap*			fBuffer;
    std::unique_ptr<Font>	fLabelFont;
    std::unique_ptr<Font>	fTextFont;

    GFX::point		fOrigin;		// map pixel at the top-left corner
    GFX::point		fMouse;			// screen coordinates
    bool			fMouseInside;	// over the map area
    bool			fCursorVisible;	// over the window
    int				fSelectedCity;	// info panel

    map_position	fParty;
    std::vector<map_position> fPath;	// tiles still to walk, in order
    int				fDestinationPlace;	// place at the end of fPath, or -1
    int				fPlace;				// place the party reached, or -1
    int				fWeariness;
    int				fEncounterPlace;	// to meet at the next Run() step, or -1
    int				fEncounterState;

    uint8			fBlack;
    uint8			fWhite;
    uint8			fYellow;
    uint8			fGray;
    uint8			fDarkGray;
};
