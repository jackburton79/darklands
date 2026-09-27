#include "InfoView.h"

#include "Bitmap.h"
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


// The carried item in use in a slot, or NULL.
static const item*
ItemInUse(const character& member, int slot)
{
    if (member.equipment[slot] == kNoEquipment)
        return NULL;
    for (const item& carried : member.items) {
        if (carried.type == member.equipment[slot])
            return &carried;
    }
    return NULL;
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

    fBuffer = new Bitmap(320, 200, 8);
}


InfoView::~InfoView()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


void
InfoView::SetParty(const party* members)
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
                } else
                    return;		// "tap any key"
                dirty = true;
                break;
            }
            case SDL_MOUSEMOTION:
                MouseMoved(GameWindow::ToScreen(event.motion.x,
                    event.motion.y));
                dirty = true;
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
    const int member = fSidebar->MemberAt(point);
    if (member == fPage)
        return false;
    if (member >= 0)
        Show(member);
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
            if (pixel != transparent)
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
    _DrawText(std::to_string(weight) + " lbs in use", kLoadBox, 0, 1,
        kValueColor);
}
