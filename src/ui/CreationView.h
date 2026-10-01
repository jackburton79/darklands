/*
 * CreationView.h
 * The creation of a character as DARKLAND.EXE shows it (the overlay at
 * file 0x76890, 1462:4664 and what it calls; CHARGEN.PIC): the name and
 * sex, the family background, then experience points (EPs) to spend on
 * attributes, then occupations and EPs on skills, as many as the player
 * wants. The rules are CharacterCreation's.
 *
 * Everything is drawn into a 320x200 8-bit buffer; the input handlers and
 * Draw() work without a window, for testing.
 */
#pragma once

#include "CharacterCreation.h"
#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
class GameWindow;

class CreationView {
public:
    enum result {
        RESULT_CREATED,			// "Begin Adventuring": see Created()
        RESULT_CANCELLED,		// "Return to game options"
        RESULT_QUIT				// the window was closed
    };
    static const int kScreenWidth = 320;
    static const int kScreenHeight = 200;

    // `random(n)` is 0..n-1; throws if the data is missing
    CreationView(GameData& data, const std::function<int(int)>& random);
    ~CreationView();

    result			Run(GameWindow& window);

    CharacterCreation&	Creation()			{ return *fCreation; }
    const CharacterCreation& Creation() const	{ return *fCreation; }
    // The character, after RESULT_CREATED
    const character& Created() const		{ return fCreated; }
    bool			IsDone() const			{ return fDone; }
    bool			IsCancelled() const		{ return fCancelled; }

    // Input, in screen coordinates
    void			MouseMoved(const GFX::point& point);
    void			Clicked(const GFX::point& point);
    // SDLK_ codes; the text of a name goes through TypeText()
    void			KeyPressed(int key);
    void			TypeText(const std::string& text);
    // The animation of the potion in the tube
    void			Tick(uint32 milliseconds);

    // A name is being typed (a box with the prompt shows)
    bool			IsTyping() const		{ return fTyping != TYPING_NONE; }
    const std::string& Typed() const		{ return fTypedText; }

    // The boxes the screen reacts to, for tests
    static GFX::rect ButtonRect(int row);			// the middle column
    static GFX::rect AttributeRect(int row);		// 0..6
    static GFX::rect SkillRect(int row);			// 0..20, with gaps
    static GFX::rect GaugeRect();
    // The 21 rows of the skill boards: 7 and 14 are gaps; the others are
    // skills 0..6, 7..12 and 13..18
    static int		SkillOfRow(int row);
    static int		RowOfSkill(int skill);
    static const int kSkillRows = 21;

    int				Highlighted() const		{ return fHighlight; }
    int				SelectedRow() const		{ return fRow; }

    Bitmap*			Draw();

private:
    enum typing {
        TYPING_NONE,
        TYPING_NAME,
        TYPING_NICKNAME
    };
    enum list_kind {
        LIST_NONE,
        LIST_FORMULAE,
        LIST_SAINTS
    };
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    raw_picture		_LoadPicture(const std::string& name,
                        GFX::Palette* palette = NULL) const;
    void			_DrawPicture(const raw_picture& picture, int x, int y);
    void			_DrawPartOf(const raw_picture& picture, int x, int y,
                        int rows);
    void			_Text(const std::string& text, int x, int y, uint8 color);
    void			_ShadowText(const std::string& text, int x, int y);
    void			_Button(int row, const std::string& label, bool hot);
    void			_DrawBoards();
    void			_DrawGauge();
    void			_DrawInput();
    void			_DrawList();
    std::vector<std::string> _ButtonLabels() const;
    int				_ButtonAt(const GFX::point& point) const;
    void			_PressButton(int row);
    void			_MoveRow(int step);
    void			_Plus();
    void			_Minus();
    void			_Finish();
    void			_Select(int row);
    void			_StartTyping(typing what);
    void			_StopTyping(bool accept);

    GameData&		fData;
    std::unique_ptr<CharacterCreation> fCreation;
    std::unique_ptr<Font> fFont;
    Bitmap*			fBuffer;
    GFX::Palette	fPalette;
    raw_picture		fBackground;
    raw_picture		fPlate;			// BUTTONA.PIC
    raw_picture		fBubbles[5];	// BUBBLE01..05.PIC
    raw_picture		fTube;			// BUBLBACK.PIC
    raw_picture		fTextBack;		// TEXTBACK.PIC
    character		fCreated;
    bool			fDone;
    bool			fCancelled;
    int				fHighlight;		// the choice under the mouse, or -1
    int				fRow;			// the selected attribute / skill row
    int				fFrame;			// of the potion
    uint32			fFrameTime;
    typing			fTyping;
    std::string		fTypedText;
    list_kind		fList;
    GFX::point		fMouse;
    uint8			fBlack;
    uint8			fCream;
    uint8			fHover;
    uint8			fSelected;
    uint8			fWhite;
    uint8			fCrimson;
};
