/*
 * CardView.h
 * A menu card (.MSG) on screen, as in the original game: the party
 * sidebar on the left, the card on the right in its frame, with an
 * illuminated capital, the text and the options to choose from. A card
 * can come with a scene picture, shown first in the same place.
 *
 * Layout measured on the screenshot in the manual (p. 28) and on the
 * card frame pictures; see docs/formats.md. Everything is drawn into a
 * 320x200 8-bit buffer; the input handlers and Draw() work without a
 * window, for testing.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
struct city;
struct msg_card;

// Values of the card variables, without the '$': "PlaceName" -> "Köln".
// UTF-8.
typedef std::map<std::string, std::string> card_variables;

class CardView {
public:
    static const uint16 kScreenWidth	= 320;
    static const uint16 kScreenHeight	= 200;

    explicit		CardView(GameData& data);	// throws if data is missing
                    ~CardView();

    // The card to show. Variables not in `variables` stay as they are.
    void			SetCard(const msg_card& card,
                        const card_variables& variables);
    // A scene picture (e.g. "MAIN-ST.PIC") shown before the card, until
    // a click or a key; empty for none. Throws if it cannot be loaded.
    void			SetScene(const std::string& pictureName);

    // Opens a window and runs until an option is chosen: returns its
    // index, or -1 if the user quit.
    int				Run();

    // Input, in screen (320x200) coordinates. Clicked() and Choose()
    // return the index of the chosen option, or -1.
    void			MouseMoved(const GFX::point& point);
    void			MouseLeft();
    int				Clicked(const GFX::point& point);
    // Moves the highlight to the next/previous option.
    void			SelectNext();
    void			SelectPrevious();
    // Chooses the highlighted option (or leaves the scene picture).
    int				Choose();

    bool			ShowingScene() const	{ return fShowingScene; }
    int				CountOptions() const	{ return int(fOptions.size()); }
    int				SelectedOption() const	{ return fSelected; }

    Bitmap*			Draw();

    // The variables the game takes from a city: $PlaceName, $Inn,
    // $citySquare...
    static void		AddCityVariables(card_variables& variables,
                        const city& c);

private:
    struct text_line {
        int x;
        int y;
        std::string text;		// game character set
    };
    struct option_area {
        int top;
        int bottom;				// exclusive
    };
    // A decoded picture: palette indices, row-major.
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    raw_picture		_LoadPicture(const std::string& name,
                        GFX::Palette* palette = NULL) const;
    // Copies a picture's pixels to the buffer; index `transparent`
    // (if >= 0) is skipped.
    void			_DrawPicture(const raw_picture& picture, int x, int y,
                        int transparent = -1, int height = -1);

    void			_Layout(const msg_card& card, const std::string& text);
    int				_OptionAt(const GFX::point& point) const;
    void			_DrawFrame();
    void			_DrawCard();

    GameData&		fData;
    Bitmap*			fBuffer;
    std::unique_ptr<Font>	fFont;

    // Frame pictures, decoded once. The capitals sheet (ILLMCAPS.PIC)
    // also carries the palette range of the card (128..159).
    raw_picture		fBorders[4];	// top, bottom, left, right
    raw_picture		fSidebar;
    raw_picture		fCapitals;
    GFX::Palette	fCardPalette;
    GFX::Palette	fScenePalette;

    raw_picture		fScene;			// width 0 if none
    bool			fShowingScene;

    uint8			fCapital;		// letter of the capital, 0 if none
    GFX::point		fCapitalPosition;
    std::vector<text_line>	fLines;
    std::vector<option_area> fOptions;
    int				fSelected;		// highlighted option, or -1

    GFX::point		fMouse;
    bool			fCursorVisible;
};
