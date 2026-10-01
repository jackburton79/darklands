/*
 * InfoView.h
 * The information screens (manual pp. 20-22): the party information
 * (F6: fame, time, date, location, wealth, notes and a small map of
 * Germany) and the character information (F1..F5, or a click on a
 * character box: name, age, attributes, skills, the equipment in use
 * and the character's figure). Both keep the party sidebar on the left.
 *
 * Everything is drawn into a 320x200 8-bit buffer; the input handlers
 * and Draw() work without a window, for testing.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"
#include "Travel.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
class GameTime;
class GameWindow;
class PartySidebar;
struct party;

class InfoView {
public:
    static const int kPartyPage = -1;

    explicit		InfoView(GameData& data);	// throws if data is missing
                    ~InfoView();

    // Not owned; the character page changes what the members carry and
    // use. The position is where the party is on the world map.
    void			SetParty(party* members);
    party*			Party() const			{ return fParty; }
    void			SetClock(const GameTime* clock)	{ fClock = clock; }
    void			SetPosition(const map_position& position);
    // The party's reputation by location (not owned; NULL: unknown).
    void			SetReputations(const std::vector<int16>* reputations)
                        { fReputations = reputations; }

    // Shows a page (kPartyPage, or a party member) until a key or a click
    // closes it; F1..F5 and the character boxes switch characters.
    void			Run(GameWindow& window, int page);

    // The page to show: kPartyPage or a party member.
    void			Show(int page);
    int				Page() const			{ return fPage; }

    // Input, in screen (320x200) coordinates. They return false when the
    // screen closes: in the party page a click anywhere, in a character
    // page a click on that character's box ("left-click again in the
    // box", manual p. 22); a click on another box shows that character.
    void			MouseMoved(const GFX::point& point);
    bool			Clicked(const GFX::point& point);
    // The button goes down: on a row of the equipment scroll it takes
    // the item, which a release elsewhere moves (DARKLAND.EXE 1462:1E42):
    // to another row of the scroll (the item goes there), onto the
    // equipment in use (it is readied) or onto a character's box (it is
    // given: all of it with Shift, held at the press, else one).
    void			Pressed(const GFX::point& point, bool shift);
    bool			IsDragging() const		{ return fDragging; }
    // F1..F5 (`key` 0..4) or F6 (kPartyPage): the same page closes the
    // screen, another one is shown.
    bool			FunctionKey(int page);
    // Any other key closes the screen, unless the equipment scroll is
    // open: then A (ready), U (unready), D (drop one; with Shift all),
    // 1..5 (give to a member; with Shift all), the arrows and Page Up and
    // Down, Home and End, and Esc (closes the scroll) are its keys.
    bool			KeyPressed(int key, bool shift);

    // The equipment scroll of the character page (the Equipment button):
    // the items the character carries, 14 rows at a time
    bool			IsScrollOpen() const	{ return fScrollOpen; }
    void			OpenScroll(bool open);
    int				ScrollCursor() const;		// index into the items, or -1
    void			SetScrollCursor(int index);
    static const int kScrollRows = 14;

    // The city shown in the "map information" panel: the one under the
    // mouse on the small map, else the nearest one.
    int				MapCity() const;

    Bitmap*			Draw();

private:
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    raw_picture		_LoadPicture(const std::string& name,
                        GFX::Palette* palette = NULL) const;
    void			_DrawPicture(const raw_picture& picture, int x, int y,
                        int transparent = -1);
    void			_DrawText(const std::string& utf8, const GFX::rect& box,
                        int line, int lines, uint8 color);
    GFX::point		_SmallMapPoint(const map_position& position) const;
    int				_NearestCity(const map_position& position) const;
    void			_DrawPartyPage();
    void			_DrawCharacterPage();
    void			_DrawEquipmentScroll();
    void			_ClampScroll();
    int				_ScrollRowAt(const GFX::point& point) const;
    void			_Drop(const GFX::point& point);
    const raw_picture* _Picture(const std::string& name);

    GameData&		fData;
    Bitmap*			fBuffer;
    std::unique_ptr<Font>	fFont;
    std::unique_ptr<PartySidebar> fSidebar;

    raw_picture		fPartyBackground;		// PTYSTATS.PIC
    raw_picture		fCharacterBackground;	// ARMBACK.PIC
    raw_picture		fLocator;				// MAPLOCTR.PIC
    GFX::Palette	fPartyPalette;
    GFX::Palette	fCharacterPalette;
    // armor, shield and weapon pictures of the figure, by name; an empty
    // picture if there is none
    std::map<std::string, raw_picture> fFigurePictures;

    party*			fParty;
    const GameTime*	fClock;
    const std::vector<int16>* fReputations;
    map_position	fPosition;
    int				fPage;
    bool			fScrollOpen;
    int				fTop[5];		// first item shown, by member
    int				fCursor[5];		// the highlighted item, by member
    raw_picture		fScrolls[6];	// ARMBRSH8.PIC .. ARMBRS13.PIC
    raw_picture		fHand;			// HANDICON.PIC: over the scroll
    raw_picture		fGrip;			// HANDICN2.PIC: dragging an item
    bool			fDragging;
    int				fDragItem;
    bool			fDragAll;
    GFX::point		fMouse;
    bool			fCursorVisible;
};
