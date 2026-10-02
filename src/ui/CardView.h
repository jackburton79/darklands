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
class GameWindow;
class InfoView;
class MenuBar;
class PartySidebar;
struct msg_card;
struct party;
union SDL_Event;

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
    // Options are numbered from 0 in card order; the ones in `hidden` are
    // not shown, the others keep their numbers. The ones in `disabled` are
    // shown as the others but cannot be chosen (DARKLAND.EXE: the option
    // word 2 of DS:EE76..: the text is laid out, no area is made for it).
    void			SetCard(const msg_card& card,
                        const card_variables& variables,
                        const std::vector<int>& hidden = std::vector<int>(),
                        const std::vector<int>& disabled = std::vector<int>());
    // A scene picture (e.g. "MAIN-ST.PIC"): the background of the card,
    // faded under the text; empty for none. With `showFirst`, it is shown
    // alone first, in full color, until a click or a key ("you must
    // left-click or tap Return to see the options", manual p. 27).
    // Throws if it cannot be loaded.
    void			SetScene(const std::string& pictureName,
                        bool showFirst = true);
    // A line to type in under the text, e.g. "Deposit how many Florins?"
    // (the game asks for numbers this way): digits only, or any text,
    // at most `maxLength`, starting as `text` (UTF-8). Return, a click or
    // any option ends it; SetCard() removes it.
    void			SetPrompt(const std::string& prompt,
                        const std::string& text, size_t maxLength,
                        bool digitsOnly = true);
    bool			Prompting() const		{ return !fPrompt.empty(); }
    // In the game's character set
    const std::string& PromptText() const	{ return fPromptText; }
    void			TypeCharacter(char c);	// game character set
    void			TypeText(const std::string& utf8);
    void			Backspace();

    // The party shown in the sidebar (not owned; NULL: none). Throws if
    // a character's picture cannot be loaded.
    void			SetParty(const party* members);
    // The information screens that F1..F6 and the character boxes open
    // (not owned; NULL: none).
    void			SetInfoView(InfoView* info)	{ fInfo = info; }
    // The menu bar (right mouse button, F10, the shortcuts): not owned;
    // NULL: none. Quit ends Run() as Esc does.
    void			SetMenuBar(MenuBar* menu)	{ fMenu = menu; }

    // Runs until an option is chosen: returns its number, or -1 if the
    // user quit, or kSaveRequested for Ctrl+S or the menu's Save Game,
    // kLoadRequested for its Load Saved Game, kOrderRequested for
    // Change Marching Order. A card without options is
    // left with a click or a key (as option 0). The first version opens
    // its own window.
    static const int kSaveRequested = -2;
    static const int kLoadRequested = -3;
    static const int kOrderRequested = -4;
    int				Run();
    int				Run(GameWindow& window);

    // Input, in screen (320x200) coordinates. Clicked() and Choose()
    // return the number of the chosen option, or -1.
    void			MouseMoved(const GFX::point& point);
    void			MouseLeft();
    int				Clicked(const GFX::point& point);
    // Moves the highlight to the next/previous option.
    void			SelectNext();
    void			SelectPrevious();
    // Chooses the highlighted option (or leaves the scene picture).
    int				Choose();

    bool			ShowingScene() const	{ return fShowingScene; }

    // Options shown, and the number of the highlighted one (-1: none).
    int				CountOptions() const	{ return int(fOptions.size()); }
    int				SelectedOption() const;

    Bitmap*			Draw();

private:
    struct text_line {
        int x;
        int y;
        std::string text;		// game character set
    };
    struct option_area {
        int number;				// in card order, hidden options included
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

    void			_Layout(const msg_card& card, const std::string& text,
                        const std::vector<int>& hidden,
                        const std::vector<int>& disabled);
    bool			_MenuEvent(GameWindow& window, const SDL_Event& event,
                        int& result);
    int				_OptionAt(const GFX::point& point) const;
    void			_DrawFrame();
    void			_DrawCard();
    void			_DrawSidebar();

    GameData&		fData;
    Bitmap*			fBuffer;
    std::unique_ptr<Font>	fFont;
    std::unique_ptr<PartySidebar> fSidebar;
    InfoView*		fInfo;
    MenuBar*		fMenu;

    // Frame pictures, decoded once. The capitals sheet (ILLMCAPS.PIC)
    // also carries the palette range of the card (128..159).
    raw_picture		fBorders[4];	// top, bottom, left, right
    raw_picture		fCapitals;
    GFX::Palette	fCardPalette;
    GFX::Palette	fPalette;		// with the scene's faded colors, if any
    GFX::Palette	fScenePalette;	// with the scene's own colors

    raw_picture		fScene;			// width 0 if none
    bool			fShowingScene;	// the scene alone, before the card

    uint8			fCapital;		// letter of the capital, 0 if none
    GFX::point		fCapitalPosition;
    std::vector<text_line>	fLines;
    std::vector<option_area> fOptions;	// the options shown
    int				fSelected;		// index into fOptions, or -1

    GFX::point		fMouse;
    bool			fCursorVisible;

    int				fTextLeft;		// the card's text margin
    int				fTextBottom;	// under the last line of the card
    std::string		fPrompt;		// empty: none
    std::string		fPromptText;
    size_t			fPromptLength;
    bool			fPromptDigits;
};
