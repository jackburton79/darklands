#include "InfoView.h"

#include "Bitmap.h"
#include "Equipment.h"
#include "Character.h"
#include "CityFile.h"
#include "FileStream.h"
#include "GameData.h"
#include "GameTime.h"
#include "ListFile.h"
#include "PICImage.h"
#include "Palette.h"
#include "PartySidebar.h"
#include "ScreenSupport.h"
#include "TextSupport.h"
#include "WorldMap.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>

// Font of FONTS.FNT for all the text, as the cards
static const uint32 kFontIndex		= 2;
static const int kLineHeight		= 9;

// Colors: dark text on the light plaques, white on the dark boards
static const uint8 kLabelColor		= 137;
static const uint8 kValueColor		= 15;	// EGA white

// The labels are the game's (DARKLAND.EXE, next to "pics\ptystats.pic").
// The boxes are the panels of PTYSTATS.PIC, measured on the picture
// (+-2 pixels): x, y, width, height
static const GFX::rect kTitleBox(62, 1, 257, 11);
static const GFX::rect kFameBox(62, 14, 134, 30);
static const GFX::rect kMapTitleBox(196, 15, 123, 12);
static const GFX::rect kMapInfoBox(196, 28, 123, 45);
static const GFX::rect kSmallMapBox(196, 73, 124, 127);
static const struct {
    const char* label;
    GFX::rect labelBox;
    GFX::rect valueBox;
} kPanels[6] = {
    { "TIME", GFX::rect(62, 44, 67, 13), GFX::rect(62, 58, 67, 31) },
    { "DATE", GFX::rect(129, 44, 67, 13), GFX::rect(129, 58, 67, 31) },
    { "LOCATION", GFX::rect(62, 90, 67, 13), GFX::rect(62, 104, 67, 31) },
    { "LOCAL REP", GFX::rect(129, 90, 67, 13), GFX::rect(129, 104, 67, 31) },
    { "WEALTH", GFX::rect(62, 137, 67, 14), GFX::rect(62, 152, 67, 44) },
    { "NOTES", GFX::rect(129, 137, 67, 14), GFX::rect(129, 152, 67, 44) }
};
enum { PANEL_TIME = 0, PANEL_DATE, PANEL_LOCATION, PANEL_LOCAL_REP,
    PANEL_WEALTH, PANEL_NOTES };

// The small map of PTYSTATS.PIC: fitted on its 92 red city dots, from
// the world map pixel coordinates (mean error 1.8 pixels, max 4.6)
static const double kSmallMapScaleX		= 0.02172;
static const double kSmallMapOffsetX	= 202.93;
static const double kSmallMapScaleY		= 0.02867;
static const double kSmallMapOffsetY	= 72.49;
static const int kLocatorCenter			= 4;	// MAPLOCTR.PIC, 9 x 9
static const int kCityHitDistance		= 4;	// small map pixels

// The character page, ARMBACK.PIC: the figure stands in the archway,
// whose picture (ARMSBACK.PIC) is at (59, 13) (verified: pixel match)
static const int kFigureLeft		= 59;
static const int kFigureTop			= 13;
static const GFX::rect kNameBox(65, 2, 192, 10);
static const GFX::rect kAgeBox(262, 2, 56, 10);
static const GFX::rect kAttributesBox(196, 14, 62, 67);
static const GFX::rect kInUseBox(196, 84, 62, 63);
static const GFX::rect kLoadBox(74, 183, 108, 13);
static const GFX::rect kSkillBoxes[3] = {
    GFX::rect(262, 14, 56, 65), GFX::rect(262, 80, 56, 57),
    GFX::rect(262, 139, 56, 59)
};
static const int kSkillsPerBox[3] = { 7, 6, 6 };

// The Equipment button, and the scroll that unrolls over the figure
// (DARKLAND.EXE's panel table, 290E:3559: record 9)
// and the Formulae and Saints buttons (records 10 and 11), whose scrolls
// unroll 16 and 32 pixels lower
static const GFX::rect kScrollButtons[3] = {
    GFX::rect(196, 149, 61, 13), GFX::rect(196, 165, 61, 13),
    GFX::rect(196, 181, 61, 13)
};
static const int kScrollLeft		= 62;
static const int kScrollTops[3]		= { 30, 46, 62 };
static const int kScrollTextLeft	= 9;
static const int kScrollTextTop		= 9;
static const int kScrollRowHeight	= 8;
static const int kScrollWidth		= 137;
static const uint8 kScrollTextColor	= 8;
static const uint8 kScrollCursorColor = 9;

// Abbreviations as in DARKLAND.EXE
static const char* kAttributeNames[ATTRIBUTE_COUNT] = {
    "End", "Str", "Agl", "Per", "Int", "Chr", "DF"
};
static const char* kSkillNames[kSkillCount] = {
    "wEdg", "wImp", "wFll", "wPol", "wThr", "wBow", "wMsD",
    "Alch", "Relg", "Virt", "SpkC", "SpkL", "R&W",
    "Heal", "Artf", "Stlh", "StrW", "Ride", "WdWs"
};
static const char* kNoEquipmentNames[EQUIPMENT_COUNT] = {
    "No Weapon", "No V:Armor", "No L:Armor", "No Shield", "No Msl Wpn"
};

// Figure pictures by item type, in the order DARKLAND.EXE lists them
// (verified: the same order as the armor items of DARKLAND.LST)
static const uint16 kFirstVitalsType	= 67;	// V:Clothing .. V:Plate Armor
static const uint16 kFirstLimbsType		= 76;	// L:Clothing .. L:Plate Armor
static const uint16 kFirstShieldType	= 95;	// Small .. Large Shield
static const char* kVitalsPictures[9] = {
    "PAD-VIT.PIC", "PAD-VIT.PIC", "LEAT-VIT.PIC", "STUDVIT.PIC",
    "CUIRBVIT.PIC", "SCALEVIT.PIC", "CHAINVIT.PIC", "BRIGVIT.PIC",
    "PLATEVIT.PIC"
};
static const char* kLimbsPictures[9] = {
    "PAD-LIMS.PIC", "PAD-LIMS.PIC", "LEAT-LIM.PIC", "STUDLIM.PIC",
    "CUIRBLIM.PIC", "SCALELIM.PIC", "CHAINLIM.PIC", "BRIGLIM.PIC",
    "PLATELIM.PIC"
};
static const char* kShieldPictures[3] = {
    "SMALLSH.PIC", "MEDIUMSH.PIC", "MEDIUMSH.PIC"
};

// City sizes (3..8) as words (inferred: Kassel, size 5, is
// "Moderate-Sized" on the manual's screenshot)
static const char*
SizeName(int size)
{
    if (size <= 4)
        return "Small-Sized";
    if (size <= 6)
        return "Moderate-Sized";
    return "Large-Sized";
}


static std::string
HourName(int hour)
{
    if (hour == 0)
        return "Midnight";
    if (hour == 12)
        return "Noon";
    return std::to_string(hour % 12 == 0 ? 12 : hour % 12)
        + (hour < 12 ? " AM" : " PM");
}


static std::string
TwoDigits(int value)
{
    char text[8];
    snprintf(text, sizeof(text), "%02d", value);
    return text;
}


// #pragma mark - InfoView


InfoView::InfoView(GameData& data)
    :
    fData(data),
    fBuffer(NULL),
    fParty(NULL),
    fClock(NULL),
    fReputations(NULL),
    fPosition{ 0, 0 },
    fPage(kPartyPage),
    fScrollOpen(false),
    fScrollKind(0),
    fDragging(false),
    fDragItem(-1),
    fDragAll(false),
    fMouse(0, 0),
    fCursorVisible(false)
{
    fFont.reset(new Font(fData.Fonts(), kFontIndex));
    fSidebar.reset(new PartySidebar(fData));
    fData.Lists();
    fData.Cities();

    fPartyPalette = PICImage::EGAPalette();
    fPartyBackground = _LoadPicture("PTYSTATS.PIC", &fPartyPalette);
    fCharacterPalette = PICImage::EGAPalette();
    fCharacterBackground = _LoadPicture("ARMBACK.PIC", &fCharacterPalette);
    fLocator = _LoadPicture("MAPLOCTR.PIC");
    static const char* kScrollNames[6] = { "ARMBRSH8.PIC", "ARMBRSH9.PIC",
        "ARMBRS10.PIC", "ARMBRS11.PIC", "ARMBRS12.PIC", "ARMBRS13.PIC" };
    for (int i = 0; i < 6; i++)
        fScrolls[i] = _LoadPicture(kScrollNames[i], &fCharacterPalette);
    for (int kind = 0; kind < SCROLL_KINDS; kind++) {
        for (int i = 0; i < 5; i++)
            fTop[kind][i] = fCursor[kind][i] = 0;
    }
    fHand = _LoadPicture("HANDICON.PIC", &fCharacterPalette);
    fGrip = _LoadPicture("HANDICN2.PIC", &fCharacterPalette);

    fBuffer = new Bitmap(320, 200, 8);
}


InfoView::~InfoView()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


void
InfoView::SetParty(party* members)
{
    fParty = members;
    fSidebar->SetParty(members);
}


void
InfoView::SetPosition(const map_position& position)
{
    fPosition = position;
}


void
InfoView::Run(GameWindow& window, int page)
{
    Show(page);
    bool dirty = true;
    for (;;) {
        if (dirty) {
            window.Show(Draw());
            dirty = false;
        }
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 100) == 0)
            continue;
        switch (event.type) {
            case SDL_QUIT:
                // leave it for the screen below, which quits
                SDL_PushEvent(&event);
                return;
            case SDL_KEYDOWN: {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key >= SDLK_F1 && key <= SDLK_F5) {
                    if (!FunctionKey(int(key - SDLK_F1)))
                        return;
                } else if (key == SDLK_F6) {
                    if (!FunctionKey(kPartyPage))
                        return;
                } else if (!KeyPressed(int(key), (SDL_GetModState() & KMOD_SHIFT) != 0))
                    return;		// "tap any key"
                dirty = true;
                break;
            }
            case SDL_MOUSEMOTION:
                MouseMoved(GameWindow::ToScreen(event.motion.x,
                    event.motion.y));
                dirty = true;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    Pressed(GameWindow::ToScreen(event.button.x,
                        event.button.y), (SDL_GetModState() & KMOD_SHIFT) != 0);
                    dirty = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (!Clicked(GameWindow::ToScreen(event.button.x,
                            event.button.y)))
                        return;
                    dirty = true;
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                    fCursorVisible = false;
                dirty = true;
                break;
            default:
                break;
        }
    }
}


void
InfoView::Show(int page)
{
    if (page != kPartyPage && (fParty == NULL || page < 0
            || page >= int(fParty->members.size())))
        page = kPartyPage;
    if (page != fPage) {
        fScrollOpen = false;
        fDragging = false;
    }
    fPage = page;
}


void
InfoView::MouseMoved(const GFX::point& point)
{
    fMouse = point;
    fCursorVisible = point.x >= 0 && point.y >= 0 && point.x < 320
        && point.y < 200;
}


bool
InfoView::Clicked(const GFX::point& point)
{
    MouseMoved(point);
    if (fPage == kPartyPage)
        return false;
    if (fDragging) {
        _Drop(point);
        return true;
    }
    const int member = fSidebar->MemberAt(point);
    if (member == fPage)
        return false;
    if (member >= 0) {
        Show(member);
        return true;
    }
    for (int kind = 0; kind < SCROLL_KINDS; kind++) {
        const GFX::rect& button = kScrollButtons[kind];
        if (point.x >= button.x && point.y >= button.y
                && point.x < button.x + int(button.w)
                && point.y < button.y + int(button.h)) {
            OpenScroll(kind, !(fScrollOpen && fScrollKind == kind));
            return true;
        }
    }
    if (fScrollOpen && fParty != NULL) {
        const int index = _ScrollRowAt(point);
        if (index >= 0)
            SetScrollCursor(index);
    }
    return true;
}


// The item under a point of the scroll, or -1
int
InfoView::_ScrollRowAt(const GFX::point& point) const
{
    if (!fScrollOpen || fParty == NULL || fPage < 0
            || fPage >= int(fParty->members.size()))
        return -1;
    const int row = (point.y - _ScrollTop() - kScrollTextTop) / kScrollRowHeight;
    if (point.x < kScrollLeft || point.x >= kScrollLeft + kScrollWidth
            || point.y < _ScrollTop() + kScrollTextTop || row < 0
            || row >= kScrollRows)
        return -1;
    const int index = fTop[fScrollKind][fPage] + row;
    return index < ScrollCount() ? index : -1;
}


void
InfoView::Pressed(const GFX::point& point, bool shift)
{
    MouseMoved(point);
    const int index = _ScrollRowAt(point);
    if (index < 0)
        return;
    SetScrollCursor(index);
    if (fScrollKind != SCROLL_EQUIPMENT)
        return;			// the lists are only read
    fDragging = true;
    fDragItem = index;
    fDragAll = shift;
}


// The button went up with an item in hand
void
InfoView::_Drop(const GFX::point& point)
{
    fDragging = false;
    if (fParty == NULL || fDragItem < 0)
        return;
    character& member = fParty->members[size_t(fPage)];
    const size_t item = size_t(fDragItem);
    const int target = fSidebar->MemberAt(point);
    if (target >= 0) {
        if (target < int(fParty->members.size()))
            GiveItem(member, item, fParty->members[size_t(target)], fDragAll);
        _ClampScroll();
        return;
    }
    if (point.x >= kInUseBox.x && point.x < kInUseBox.x + int(kInUseBox.w)
            && point.y >= kInUseBox.y
            && point.y < kInUseBox.y + int(kInUseBox.h)) {
        ReadyItem(member, item, fData.Lists().Items());
        return;
    }
    const int to = _ScrollRowAt(point);
    if (to >= 0)
        MoveItem(member, item, size_t(to));
}


void
InfoView::OpenScroll(bool open)
{
    OpenScroll(SCROLL_EQUIPMENT, open);
}


void
InfoView::OpenScroll(int kind, bool open)
{
    if (kind < 0 || kind >= SCROLL_KINDS)
        return;
    fScrollOpen = open && fPage != kPartyPage;
    if (fScrollOpen) {
        fScrollKind = kind;
        fDragging = false;
        _ClampScroll();
    }
}


// The row of the open scroll the cursor is on, or -1
int
InfoView::ScrollCursor() const
{
    if (fParty == NULL || fPage < 0 || fPage >= int(fParty->members.size())
            || fCursor[fScrollKind][fPage] >= ScrollCount())
        return -1;
    return fCursor[fScrollKind][fPage];
}


void
InfoView::SetScrollCursor(int index)
{
    if (fPage < 0 || fPage >= 5)
        return;
    fCursor[fScrollKind][fPage] = index;
    _ClampScroll();
}


// The rows of the open scroll: the items; the formulae known, a row for
// each version (DARKLAND.EXE 1462:35C8); the saints known (1462:37FE)
int
InfoView::ScrollCount() const
{
    if (fParty == NULL || fPage < 0 || fPage >= int(fParty->members.size()))
        return 0;
    const character& member = fParty->members[size_t(fPage)];
    int count = 0;
    switch (fScrollKind) {
        case SCROLL_EQUIPMENT:
            return int(member.items.size());
        case SCROLL_FORMULAE:
            for (int k = 0; k < kFormulaCount; k++) {
                const int versions = FormulaVersions(member, k);
                for (int bit = 0; bit < 3; bit++)
                    count += (versions >> bit) & 1;
            }
            return count;
        default:
            for (int saint = 0; saint < 136; saint++) {
                if (KnowsSaint(member, saint))
                    count++;
            }
            return count;
    }
}


// The text of a row of the open scroll
std::string
InfoView::ScrollText(int index) const
{
    if (fParty == NULL || fPage < 0 || fPage >= int(fParty->members.size())
            || index < 0)
        return "";
    const character& member = fParty->members[size_t(fPage)];
    int row = 0;
    if (fScrollKind == SCROLL_EQUIPMENT) {
        if (index >= int(member.items.size()))
            return "";
        const std::vector<item_definition>& definitions = fData.Lists().Items();
        const item& carried = member.items[size_t(index)];
        const std::string name = carried.code < definitions.size()
            ? definitions[carried.code].name : "?";
        char text[96];
        snprintf(text, sizeof(text), "%s %02dq (%d)", name.c_str(),
            carried.quality, carried.quantity);
        return text;
    }
    if (fScrollKind == SCROLL_FORMULAE) {
        const std::vector<std::string>& names = fData.Lists().Formulae();
        for (int k = 0; k < kFormulaCount; k++) {
            const int versions = FormulaVersions(member, k);
            for (int bit = 0; bit < 3; bit++) {
                if (((versions >> bit) & 1) == 0)
                    continue;
                if (row++ == index) {
                    const size_t name = size_t(3 * k + bit);
                    return name < names.size() ? names[name] : "?";
                }
            }
        }
        return "";
    }
    const std::vector<std::string>& names = fData.Lists().Saints();
    for (int saint = 0; saint < 136; saint++) {
        if (!KnowsSaint(member, saint))
            continue;
        if (row++ == index)
            return size_t(saint) < names.size() ? names[size_t(saint)] : "?";
    }
    return "";
}


int
InfoView::_ScrollTop() const
{
    return kScrollTops[fScrollKind];
}


// The cursor on a row, and the rows shown holding it
void
InfoView::_ClampScroll()
{
    if (fParty == NULL || fPage < 0 || fPage >= int(fParty->members.size()))
        return;
    const int count = ScrollCount();
    int& cursor = fCursor[fScrollKind][fPage];
    int& top = fTop[fScrollKind][fPage];
    cursor = std::max(0, std::min(cursor, count - 1));
    top = std::max(0, std::min(top, std::max(0, count - kScrollRows)));
    if (cursor < top)
        top = cursor;
    else if (cursor >= top + kScrollRows)
        top = cursor - kScrollRows + 1;
}


bool
InfoView::KeyPressed(int key, bool shift)
{
    if (!fScrollOpen || fParty == NULL || fPage < 0
            || fPage >= int(fParty->members.size()))
        return false;
    character& member = fParty->members[size_t(fPage)];
    const int count = ScrollCount();
    const int cursor = fCursor[fScrollKind][fPage];
    const bool items = fScrollKind == SCROLL_EQUIPMENT;
    switch (key) {
        case SDLK_ESCAPE:
            fScrollOpen = false;
            break;
        case SDLK_UP:
            SetScrollCursor(cursor - 1);
            break;
        case SDLK_DOWN:
            SetScrollCursor(cursor + 1);
            break;
        case SDLK_PAGEUP:
            SetScrollCursor(cursor - kScrollRows);
            break;
        case SDLK_PAGEDOWN:
            SetScrollCursor(cursor + kScrollRows);
            break;
        case SDLK_HOME:
            SetScrollCursor(0);
            break;
        case SDLK_END:
            SetScrollCursor(count - 1);
            break;
        case SDLK_a:
            if (items)
                ReadyItem(member, size_t(cursor), fData.Lists().Items());
            break;
        case SDLK_u:
            if (items)
                UnreadyItem(member, size_t(cursor));
            break;
        case SDLK_d:
            if (items) {
                DropItem(member, size_t(cursor), shift);
                _ClampScroll();
            }
            break;
        default:
            if (items && key >= SDLK_1 && key <= SDLK_5) {
                const int target = key - SDLK_1;
                if (target < int(fParty->members.size())) {
                    GiveItem(member, size_t(cursor),
                        fParty->members[size_t(target)], shift);
                    _ClampScroll();
                }
            }
            break;
    }
    return true;
}


bool
InfoView::FunctionKey(int page)
{
    if (page == fPage)
        return false;
    if (page == kPartyPage
            || (fParty != NULL && page >= 0 && page < int(fParty->members.size())))
        Show(page);
    return true;
}


int
InfoView::MapCity() const
{
    const CityFile& cities = fData.Cities();
    if (fCursorVisible && fMouse.x >= kSmallMapBox.x
            && fMouse.y >= kSmallMapBox.y
            && fMouse.x < kSmallMapBox.x + int(kSmallMapBox.w)
            && fMouse.y < kSmallMapBox.y + int(kSmallMapBox.h)) {
        int best = -1;
        int bestDistance = kCityHitDistance * kCityHitDistance + 1;
        for (uint32 i = 0; i < cities.CountCities(); i++) {
            const city& c = cities.CityAt(i);
            const GFX::point p = _SmallMapPoint(map_position{ c.x, c.y });
            const int dx = p.x - fMouse.x;
            const int dy = p.y - fMouse.y;
            if (dx * dx + dy * dy < bestDistance) {
                best = int(i);
                bestDistance = dx * dx + dy * dy;
            }
        }
        if (best >= 0)
            return best;
    }
    return _NearestCity(fPosition);
}


Bitmap*
InfoView::Draw()
{
    const bool partyPage = fPage == kPartyPage;
    const GFX::Palette& palette = partyPage ? fPartyPalette : fCharacterPalette;
    fBuffer->SetColors(palette.colors, 0, 256);
    _DrawPicture(partyPage ? fPartyBackground : fCharacterBackground, 0, 0);
    fSidebar->Draw(fBuffer, false);
    if (partyPage)
        _DrawPartyPage();
    else
        _DrawCharacterPage();
    if (fCursorVisible) {
        // over the scroll the cursor is a hand, closed on an item in hand
        if (!partyPage && fDragging)
            _DrawPicture(fGrip, fMouse.x - 3, fMouse.y - 3, 0);
        else if (!partyPage && _ScrollRowAt(fMouse) >= 0)
            _DrawPicture(fHand, fMouse.x - 3, fMouse.y - 3, 0);
        else
            DrawMouseCursor(fBuffer, fMouse, NearestColor(palette, 0, 0, 0),
                NearestColor(palette, 255, 255, 255));
    }
    return fBuffer;
}


InfoView::raw_picture
InfoView::_LoadPicture(const std::string& name, GFX::Palette* palette) const
{
    FileStream stream(fData.PathFor(name).c_str(), FileStream::READ_ONLY);
    PICImage image(&stream);
    if (palette != NULL)
        image.ApplyPalette(*palette);
    raw_picture picture;
    picture.width = image.Width();
    picture.height = image.Height();
    picture.pixels = image.RawBytes();
    return picture;
}


void
InfoView::_DrawPicture(const raw_picture& picture, int x, int y,
    int transparent)
{
    for (int row = 0; row < picture.height; row++) {
        for (int column = 0; column < picture.width; column++) {
            const uint8 pixel = picture.pixels[size_t(row) * picture.width
                + column];
            if (pixel != transparent && x + column >= 0 && y + row >= 0
                    && x + column < 320 && y + row < 200)
                fBuffer->PutPixel(x + column, y + row, pixel);
        }
    }
}


// Line `line` of `lines`, centered in the box.
void
InfoView::_DrawText(const std::string& utf8, const GFX::rect& box, int line,
    int lines, uint8 color)
{
    const std::string text = Font::ToGameCharset(utf8);
    const int width = fFont->StringWidth(text);
    const int x = box.x + (int(box.w) - width) / 2;
    const int y = box.y + (int(box.h) - lines * kLineHeight) / 2
        + line * kLineHeight + 1;
    fFont->RenderString(text, fBuffer, GFX::point(std::max<int>(x, box.x), y),
        color, box.w);
}


GFX::point
InfoView::_SmallMapPoint(const map_position& position) const
{
    const GFX::point p = fData.Map().TileCenter(position.x, position.y);
    return GFX::point(int(kSmallMapScaleX * p.x + kSmallMapOffsetX + 0.5),
        int(kSmallMapScaleY * p.y + kSmallMapOffsetY + 0.5));
}


int
InfoView::_NearestCity(const map_position& position) const
{
    const CityFile& cities = fData.Cities();
    int best = -1;
    long bestDistance = 0;
    for (uint32 i = 0; i < cities.CountCities(); i++) {
        const city& c = cities.CityAt(i);
        const long dx = long(c.x) - position.x;
        const long dy = (long(c.y) - position.y) / 4;	// rows are 4 px apart
        const long distance = dx * dx + dy * dy;
        if (best < 0 || distance < bestDistance) {
            best = int(i);
            bestDistance = distance;
        }
    }
    return best;
}


void
InfoView::_DrawPartyPage()
{
    _DrawText("PARTY INFORMATION", kTitleBox, 0, 1, kLabelColor);
    _DrawText("MAP INFORMATION", kMapTitleBox, 0, 1, kLabelColor);
    for (const auto& panel : kPanels)
        _DrawText(panel.label, panel.labelBox, 0, 1, kLabelColor);

    // fame: the words the game uses for it are not known
    _DrawText("PARTY FAME", kFameBox, 0, 2, kLabelColor);
    if (fParty != NULL)
        _DrawText(std::to_string(fParty->fame), kFameBox, 1, 2, kValueColor);

    if (fClock != NULL) {
        _DrawText(fClock->BellName(), kPanels[PANEL_TIME].valueBox, 0, 2,
            kValueColor);
        _DrawText(HourName(fClock->Hour()), kPanels[PANEL_TIME].valueBox, 1, 2,
            kValueColor);
        _DrawText(fClock->MonthName(), kPanels[PANEL_DATE].valueBox, 0, 2,
            kValueColor);
        _DrawText(std::to_string(fClock->Day()), kPanels[PANEL_DATE].valueBox,
            1, 2, kValueColor);
    }

    const CityFile& cities = fData.Cities();
    const int nearest = _NearestCity(fPosition);
    if (nearest >= 0) {
        _DrawText(cities.CityAt(nearest).shortName,
            kPanels[PANEL_LOCATION].valueBox, 0, 1, kValueColor);
    }
    if (nearest >= 0 && fReputations != NULL
            && nearest < int(fReputations->size())) {
        const int reputation = (*fReputations)[nearest];
        const GFX::rect& box = kPanels[PANEL_LOCAL_REP].valueBox;
        _DrawText(ReputationWord(reputation), box, 0, 2, kValueColor);
        _DrawText("(" + std::to_string(reputation) + ")", box, 1, 2,
            kValueColor);
    }

    if (fParty != NULL) {
        const GFX::rect& wealth = kPanels[PANEL_WEALTH].valueBox;
        _DrawText(std::to_string(fParty->cash.florins) + " Florins", wealth,
            0, 3, kValueColor);
        _DrawText(std::to_string(fParty->cash.groschen) + " Groschen", wealth,
            1, 3, kValueColor);
        _DrawText(std::to_string(fParty->cash.pfennigs) + " Pfenniges", wealth,
            2, 3, kValueColor);

        std::vector<std::string> notes;
        if (fParty->bankNotes > 0) {
            notes.push_back(std::to_string(fParty->bankNotes) + " Florins");
            notes.push_back("Ltr Credit");
        }
        if (fParty->philosopherStone > 0)
            notes.push_back(std::to_string(fParty->philosopherStone) + " PhStone");
        for (size_t i = 0; i < notes.size(); i++) {
            _DrawText(notes[i], kPanels[PANEL_NOTES].valueBox, int(i),
                int(notes.size()), kValueColor);
        }
    }

    const int mapCity = MapCity();
    if (mapCity >= 0) {
        const city& c = cities.CityAt(uint32(mapCity));
        const bool known = fReputations != NULL
            && mapCity < int(fReputations->size());
        const int lines = known ? 4 : 2;
        _DrawText(c.shortName, kMapInfoBox, 0, lines, kValueColor);
        _DrawText(SizeName(c.size), kMapInfoBox, 1, lines, kValueColor);
        if (known) {
            const int reputation = (*fReputations)[mapCity];
            _DrawText(std::string("Rep: ") + ReputationWord(reputation),
                kMapInfoBox, 2, lines, kValueColor);
            _DrawText("(" + std::to_string(reputation) + ")", kMapInfoBox, 3,
                lines, kValueColor);
        }
    }

    const GFX::point locator = _SmallMapPoint(fPosition);
    _DrawPicture(fLocator, locator.x - kLocatorCenter,
        locator.y - kLocatorCenter, 0);
}


const InfoView::raw_picture*
InfoView::_Picture(const std::string& name)
{
    std::map<std::string, raw_picture>::iterator found
        = fFigurePictures.find(name);
    if (found == fFigurePictures.end()) {
        raw_picture picture;
        picture.width = picture.height = 0;
        struct stat st;
        const std::string path = fData.PathFor(name);
        if (::stat(path.c_str(), &st) == 0)
            picture = _LoadPicture(name);
        found = fFigurePictures.insert(std::make_pair(name, picture)).first;
    }
    return found->second.width > 0 ? &found->second : NULL;
}


void
InfoView::_DrawCharacterPage()
{
    if (fParty == NULL || fPage < 0 || fPage >= int(fParty->members.size()))
        return;
    const character& member = fParty->members[fPage];
    const std::vector<item_definition>& definitions = fData.Lists().Items();

    // the figure: limb armor, vitals armor, shield, weapon
    std::vector<std::string> layers;
    const uint8* equipment = member.equipment;
    const int limbs = equipment[EQUIPMENT_LIMBS] - kFirstLimbsType;
    if (limbs >= 0 && limbs < 9)
        layers.push_back(kLimbsPictures[limbs]);
    const int vitals = equipment[EQUIPMENT_VITALS] - kFirstVitalsType;
    if (vitals >= 0 && vitals < 9)
        layers.push_back(kVitalsPictures[vitals]);
    const int shield = equipment[EQUIPMENT_SHIELD] - kFirstShieldType;
    if (shield >= 0 && shield < 3)
        layers.push_back(kShieldPictures[shield]);
    // weapon pictures are "pics\weapon<n>.pic" in DARKLAND.EXE; the
    // number is the weapon's item type (inferred)
    int weapon = equipment[EQUIPMENT_WEAPON];
    if (weapon == kNoEquipment)
        weapon = equipment[EQUIPMENT_MISSILE];
    if (weapon != kNoEquipment)
        layers.push_back("WEAPON" + std::to_string(weapon) + ".PIC");
    for (const std::string& layer : layers) {
        const raw_picture* picture = _Picture(layer);
        if (picture != NULL)
            _DrawPicture(*picture, kFigureLeft, kFigureTop, 0);
    }

    // name, leadership, age and sex
    const std::string leader = fPage == fParty->leader ? "Leader" : "Not Leader";
    const std::string name = Font::ToGameCharset(member.fullName);
    fFont->RenderString(name, fBuffer, GFX::point(kNameBox.x + 2, kNameBox.y + 1),
        kValueColor, kNameBox.w);
    const std::string leaderText = Font::ToGameCharset(leader);
    fFont->RenderString(leaderText, fBuffer, GFX::point(kNameBox.x + kNameBox.w
        - fFont->StringWidth(leaderText) - 2, kNameBox.y + 1), kValueColor);
    _DrawText("Age " + std::to_string(member.age)
        + (member.female ? " (F)" : " (M)"), kAgeBox, 0, 1, kValueColor);

    // attributes: current / maximum
    for (int i = 0; i < ATTRIBUTE_COUNT; i++) {
        const std::string text = TwoDigits(member.attributes[i]) + "/"
            + TwoDigits(member.maxAttributes[i]) + " " + kAttributeNames[i];
        fFont->RenderString(text, fBuffer, GFX::point(kAttributesBox.x + 3,
            kAttributesBox.y + 2 + i * kLineHeight), kValueColor,
            kAttributesBox.w - 3);
    }

    // skills
    int skill = 0;
    for (int box = 0; box < 3; box++) {
        for (int i = 0; i < kSkillsPerBox[box]; i++, skill++) {
            const std::string text = TwoDigits(member.skills[skill]) + " "
                + kSkillNames[skill];
            fFont->RenderString(Font::ToGameCharset(text), fBuffer,
                GFX::point(kSkillBoxes[box].x + 3,
                    kSkillBoxes[box].y + 2 + i * kLineHeight),
                kValueColor, kSkillBoxes[box].w - 3);
        }
    }

    // equipment in use and its weight
    int weight = 0;
    for (int slot = 0; slot < EQUIPMENT_COUNT; slot++) {
        const item* inUse = ItemInUse(member, slot);
        std::string text = kNoEquipmentNames[slot];
        if (inUse != NULL && inUse->code < definitions.size()) {
            text = definitions[inUse->code].shortName;
            weight += inUse->weight;
        }
        fFont->RenderString(Font::ToGameCharset(text), fBuffer,
            GFX::point(kInUseBox.x + 3, kInUseBox.y + 5 + slot * kLineHeight
                + slot * 3), kValueColor, kInUseBox.w - 3);
    }
    // the load category needs the carrying capacity, which is unknown
    _DrawText(std::to_string(std::min(weight, kMaxWeightInUse))
        + " lbs in use", kLoadBox, 0, 1, kValueColor);
    if (fScrollOpen)
        _DrawScroll();
}


// The scroll that fits the rows (ARMBRSH8..13), over the figure: the items
// carried ("Short Sword 25q (1)"), the formulae or the saints known; the
// row of the cursor in blue
void
InfoView::_DrawScroll()
{
    const int count = ScrollCount();
    const int rows = std::min(count, kScrollRows);
    int which = 0;
    while (which < 5 && fScrolls[which].height < kScrollTextTop + 12
            + rows * kScrollRowHeight)
        which++;
    _DrawPicture(fScrolls[which], kScrollLeft - 1, _ScrollTop(), 0);
    const int top = fTop[fScrollKind][fPage];
    for (int row = 0; row < rows; row++) {
        const int index = top + row;
        if (index >= count)
            break;
        fFont->RenderString(Font::ToGameCharset(ScrollText(index)), fBuffer,
            GFX::point(kScrollLeft + kScrollTextLeft, _ScrollTop()
                + kScrollTextTop + row * kScrollRowHeight),
            index == fCursor[fScrollKind][fPage] ? kScrollCursorColor
                : kScrollTextColor,
            kScrollWidth - 2 * kScrollTextLeft);
    }
}
