/*
 * PartySelectView.h
 * The party selection screen (manual pp. 11-12): the characters of the
 * world on a list, and eight buttons to act on the highlighted one: create
 * a character, examine, add to the party, delete from it, select its
 * image, kill, begin the adventure, return. DARKLAND.EXE keeps it in the
 * overlay at file 0x7C5F0 (CRETSCR3.PIC, the buttons' letters are in
 * DS:290E:1E8A..1F12); the inn's "find somebody to join your party" opens
 * it too (0E76:2246).
 *
 * Reproduced here: examining, adding, deleting, killing, beginning and
 * returning, and creating a character (CreationView: a whole life to
 * live through; not at the inn, *inferred*). Selecting an image is not,
 * and shows dim.
 *
 * Everything is drawn into a 320x200 8-bit buffer; the input handlers and
 * Draw() work without a window, for testing.
 */
#pragma once

#include "Character.h"
#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
class GameWindow;
class InfoView;

// A character of the world and whether he is in the party
struct roster_entry {
    character member;
    std::string image;			// the pictures' code, e.g. "F60"
    std::vector<uint8> colors;	// of his battle figure
    bool inParty;
    int city;					// where he waits (-1: anywhere)
};

class PartySelectView {
public:
    enum action {
        ACTION_CREATE = 0,
        ACTION_EXAMINE,
        ACTION_ADD,
        ACTION_DELETE,
        ACTION_IMAGE,
        ACTION_KILL,
        ACTION_BEGIN,
        ACTION_RETURN,
        ACTION_HERALDRY,		// the new game's screen (CRETSCRN.PIC) only:
        ACTION_COLOR1,			// the shield, and the three colors of the
        ACTION_COLOR2,			// character's battle figure
        ACTION_COLOR3,
        ACTION_COUNT,
        ACTION_NONE = -1
    };
    enum result {
        RESULT_BEGIN,			// "Begin the adventure"
        RESULT_RETURN,			// "Return"
        RESULT_QUIT				// the window was closed
    };
    static const int kMaxParty = 4;
    static const int kScreenWidth = 320;
    static const int kScreenHeight = 200;

    // The roster for a party and the members who retired: those of `city`
    // and of nowhere in particular (-1), or all of them for kEverywhere. And
    // the other way: the party in the order the roster has it, the others
    // back to the retired (the ones of other cities kept as they were).
    static const int kEverywhere = -2;
    static std::vector<roster_entry> MakeRoster(const party& members,
                        const std::vector<retired_member>& retired, int city);
    static void		ApplyRoster(const std::vector<roster_entry>& roster,
                        party& members, std::vector<retired_member>& retired,
                        int city);

    explicit		PartySelectView(GameData& data);	// throws if data is missing
                    ~PartySelectView();

    // The characters: the party first, in order, then the others. In a
    // city (`inCity`) the game's own rules about what may be done are not
    // known: only examining, adding, deleting and returning are on.
    void			SetRoster(const std::vector<roster_entry>& roster,
                        bool inCity);
    const std::vector<roster_entry>& Roster() const	{ return fRoster; }
    // The screen: the eight buttons of CRETSCR3.PIC (the default, and the
    // inn's), or the eleven of CRETSCRN.PIC (a new game's: no Examine, but
    // Heraldry, the image and the three colors of a member of the party)
    void			SetSheet(bool sheet);
    bool			IsSheet() const			{ return fSheet; }
    int				ButtonCount() const		{ return fSheet ? 11 : 8; }
    // The action of a button (by its place, from the top)
    action			ButtonAction(int slot) const;
    // For "Examine": the information screens (not owned; NULL: none)
    void			SetInfoView(InfoView* info)	{ fInfo = info; }

    result			Run(GameWindow& window);

    // The highlighted character (index into Roster()), or -1
    int				Selected() const		{ return fSelected; }
    void			Select(int index);
    int				CountInParty() const;
    bool			IsEnabled(action what) const;
    // Does the action on the highlighted character; true if it was done
    // (ACTION_BEGIN and ACTION_RETURN are the caller's: they only say so)
    bool			Do(action what, GameWindow* window = NULL);

    // Input, in screen coordinates
    void			MouseMoved(const GFX::point& point);
    // A click: a name selects it, a button acts; returns the action or
    // ACTION_NONE
    action			Clicked(const GFX::point& point, GameWindow* window = NULL);
    GFX::rect		ButtonRect(int slot) const;
    GFX::rect		NameRect(int row) const;
    const char*		ButtonLabel(int slot) const;
    // The keyboard: a button's letter, Up/Down on the list; Return adds or
    // deletes; Esc returns.
    action			KeyPressed(int key, GameWindow* window = NULL);

    Bitmap*			Draw();

private:
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    raw_picture		_LoadPicture(const std::string& name,
                        GFX::Palette* palette = NULL) const;
    void			_DrawPicture(const raw_picture& picture, int x, int y);
    void			_Text(const std::string& utf8, int x, int y, uint8 color,
                        int maxWidth = 1000);
    int				_NameAt(const GFX::point& point) const;
    int				_ButtonAt(const GFX::point& point) const;
    int				_MemberBoxAt(const GFX::point& point) const;
    void			_DrawParty();
    void			_DrawPortrait();
    const raw_picture* _Picture(const std::string& name);
    void			_Examine(GameWindow* window);
    bool			_Create(GameWindow& window);

    GameData&		fData;
    InfoView*		fInfo;
    Bitmap*			fBuffer;
    std::unique_ptr<Font> fFont;
    raw_picture		fBackground;
    raw_picture		fButton;		// BUTTNCR1.PIC
    raw_picture		fButtonLit;		// BUTTNCR2.PIC
    raw_picture		fSheetBackground;	// CRETSCRN.PIC
    std::map<std::string, raw_picture> fPictures;	// shields, figures
    GFX::Palette	fPalette;
    std::vector<roster_entry> fRoster;
    bool			fInCity;
    int				fSelected;
    int				fTop;			// the first name shown
    int				fHot;			// the button under the mouse, or -1
    int				fPressed;		// the button shown pressed, or -1
    bool			fSheet;
    int				fColorStep;		// the preset the colors keys are at, 0..5
    uint8			fWhite;
    uint8			fCrimson;
    uint8			fDim;
    uint8			fMark;
    uint8			fBlack;
    std::mt19937	fRandom;
};
