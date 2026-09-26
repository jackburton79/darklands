/*
 * MapViewer.h
 * Interactive world map: scrolling, city names, a status bar with the
 * tile under the mouse, an info panel for a city, and the party, which
 * travels across the map and enters cities.
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
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;

class MapViewer {
public:
    static const uint16 kScreenWidth	= 320;
    static const uint16 kScreenHeight	= 200;

    explicit		MapViewer(GameData& data);	// throws if data is missing
                    ~MapViewer();

    // Opens a window and runs until the user quits.
    void			Run();

    // Input, in screen (320x200) coordinates.
    void			ScrollBy(int dx, int dy);
    void			CenterOn(uint16 tileX, uint16 tileY);
    void			CenterOnParty();
    void			MouseMoved(const GFX::point& point);
    // The mouse left the window: hides the cursor.
    void			MouseLeft();
    // Left click: on the map, travel there (entering the city, if it is
    // one); in a city, choose the place under the mouse.
    void			Clicked(const GFX::point& point);
    // Right click: the info panel of the city under the mouse.
    void			RightClicked(const GFX::point& point);
    // In a city: choose place number `number` (1-based: the menu lists
    // them as A, B, C...).
    void			ChoosePlace(int number);
    // Closes the info panel, else leaves the city, else stops the party.
    // Returns false if there was nothing to close (i.e. quit).
    bool			Escape();

    // Advances the party one tile along its path; returns false if it
    // was not traveling. Run() calls it on a timer.
    bool			Tick();

    bool			IsTraveling() const	{ return !fPath.empty(); }
    const map_position& PartyPosition() const	{ return fParty; }
    // Index of the city whose info panel is open, or -1.
    int				SelectedCity() const	{ return fSelectedCity; }
    // Index of the city the party is in, or -1 when on the map.
    int				CurrentCity() const		{ return fCity; }
    // Last message shown in the city menu.
    const std::string& CityMessage() const	{ return fCityMessage; }

    // Draws the current view into the 320x200 buffer and returns it.
    Bitmap*			Draw();

private:
    GFX::point		_ScreenToMap(const GFX::point& point) const;
    int				_CityAt(const GFX::point& mapPoint) const;
    void			_SetOrigin(int x, int y);
    void			_TravelTo(const map_position& destination, int city);
    void			_EnterCity(int city);
    void			_KeepPartyVisible();
    std::vector<int> _CityMenuPlaces() const;
    int				_CityMenuItemAt(const GFX::point& point) const;
    void			_DrawStatusBar();
    void			_DrawCityPanel();
    void			_DrawCityMenu();
    void			_DrawCursor();
    void			_DrawText(const Font& font, const std::string& utf8,
                        int x, int y, uint8 color) const;

    GameData&		fData;
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
    int				fDestinationCity;	// city at the end of fPath, or -1
    int				fCity;				// city the party is in, or -1
    std::string		fCityMessage;

    uint8			fBlack;
    uint8			fWhite;
    uint8			fYellow;
    uint8			fGray;
    uint8			fDarkGray;
};
