/*
 * MapViewer.h
 * Interactive world map: scrolling, city names, a status bar with the
 * tile under the mouse, and an info panel for the selected city.
 *
 * Everything is drawn into a 320x200 8-bit buffer with the map palette
 * (the game's resolution); Run() shows it scaled up in a window. The
 * input handlers and Draw() work without a window, for testing.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <memory>

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
    void			MouseMoved(const GFX::point& point);
    // The mouse left the window: hides the cursor.
    void			MouseLeft();
    void			Clicked(const GFX::point& point);
    // Closes the city panel; returns false if none was open.
    bool			ClosePanel();

    // -1 if no city is selected.
    int				SelectedCity() const	{ return fSelectedCity; }

    // Draws the current view into the 320x200 buffer and returns it.
    Bitmap*			Draw();

private:
    GFX::point		_ScreenToMap(const GFX::point& point) const;
    int				_CityAt(const GFX::point& mapPoint) const;
    void			_SetOrigin(int x, int y);
    void			_DrawStatusBar();
    void			_DrawCityPanel();
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
    int				fSelectedCity;

    uint8			fBlack;
    uint8			fWhite;
    uint8			fYellow;
    uint8			fGray;
    uint8			fDarkGray;
};
