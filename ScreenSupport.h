/*
 * ScreenSupport.h
 * Helpers shared by the interactive screens (MapViewer, CardView): they
 * draw into a 320x200 8-bit buffer (the game's resolution), shown scaled
 * up in a window.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"

class Bitmap;

// The window shows the 320x200 screen scaled by this factor
static const int kWindowScale = 2;

class GameWindow {
public:
    // Opens the window; throws if the graphics engine cannot start.
    explicit		GameWindow(const char* caption);
                    ~GameWindow();

    // Shows an 8-bit 320x200 buffer, with its palette.
    void			Show(const Bitmap* buffer);

    // Window (event) coordinates to screen (320x200) coordinates.
    static GFX::point ToScreen(int x, int y);

private:
                    GameWindow(const GameWindow&);
    GameWindow&		operator=(const GameWindow&);

    Bitmap*			fConverted;
};

// libjgame hides the system cursor, so the screens draw their own.
// `point` is the hot spot (top left).
void DrawMouseCursor(Bitmap* bitmap, const GFX::point& point,
    uint32 outlineColor, uint32 fillColor);
