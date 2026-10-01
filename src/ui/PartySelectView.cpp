/*
 * PartySelectView.cpp
 */

#include "PartySelectView.h"

#include "Bitmap.h"
#include "CreationView.h"
#include "FileStream.h"
#include "GameData.h"
#include "InfoView.h"
#include "PICImage.h"
#include "Palette.h"
#include "PartyColors.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <SDL.h>

#include <algorithm>
#include <cstring>

static const uint32 kFontIndex		= 2;

// Measured on CRETSCR3.PIC and the overlay's hit tests (x 70..200, y
// 45..165, 15 pixels a button): the buttons, the list, the strips
static const int kButtonLeft		= 68;
static const int kButtonTop			= 44;
static const int kButtonPitch		= 15;
static const int kButtonWidth		= 123;
static const int kButtonHeight		= 13;
static const int kListLeft			= 198;
static const int kListTop			= 20;
static const int kListWidth			= 114;
static const int kRowHeight			= 9;
static const int kListRows			= 17;
static const int kTitleLeft			= 64;
static const int kTitleTop			= 3;
static const int kTitleWidth		= 254;
static const int kPartyLeft			= 5;
static const int kPartyTop			= 6;
static const int kPartyWidth		= 52;

// The buttons' texts (DARKLAND.EXE, 290E:1E8A.. and the far strings
// 290E:1D43 40, 39, 103, 65, 100..102, 42, 58, 43, 44: a 0xFF and the
// letters after the first, which is the key), by action
static const char* kLabels[PartySelectView::ACTION_COUNT] = {
    "Create a Character", "Examine a Character", "Add to the Party",
    "Delete from the Party", "Select Character Image", "Kill a Character",
    "Begin the Adventure", "Return to Main Menu", "Heraldry", "1st color",
    "2nd color", "3rd color"
};
static const char kKeys[PartySelectView::ACTION_COUNT] = {
    'C', 'E', 'A', 'D', 'S', 'K', 'B', 'R', 'H', '1', '2', '3'
};
// The buttons from the top: CRETSCR3.PIC's eight, CRETSCRN.PIC's eleven
static const PartySelectView::action kInnSlots[8] = {
    PartySelectView::ACTION_CREATE, PartySelectView::ACTION_EXAMINE,
    PartySelectView::ACTION_ADD, PartySelectView::ACTION_DELETE,
    PartySelectView::ACTION_IMAGE, PartySelectView::ACTION_KILL,
    PartySelectView::ACTION_BEGIN, PartySelectView::ACTION_RETURN
};
static const PartySelectView::action kSheetSlots[11] = {
    PartySelectView::ACTION_CREATE, PartySelectView::ACTION_ADD,
    PartySelectView::ACTION_HERALDRY, PartySelectView::ACTION_IMAGE,
    PartySelectView::ACTION_COLOR1, PartySelectView::ACTION_COLOR2,
    PartySelectView::ACTION_COLOR3, PartySelectView::ACTION_DELETE,
    PartySelectView::ACTION_KILL, PartySelectView::ACTION_BEGIN,
    PartySelectView::ACTION_RETURN
};
// CRETSCRN.PIC: the buttons, 15 pixels apart from y 24 (the overlay's hit
// test is y 25..190); the party's boxes at the left, 40 pixels each, and
// the highlighted member's picture below them
static const int kSheetButtonTop	= 24;
static const int kBoxHeight			= 40;
static const int kBoxLeft			= 2;
static const int kBoxWidth			= 56;
static const int kPortraitLeft		= 12;
static const int kPortraitTop		= 164;
static const int kFirstFigureColor	= 235;	// the sprites' own 8 colors


static bool
Shown(const retired_member& who, int city)
{
    return city == PartySelectView::kEverywhere || who.city < 0
        || who.city == city;
}


/* static */
std::vector<roster_entry>
PartySelectView::MakeRoster(const party& members,
    const std::vector<retired_member>& retired, int city)
{
    std::vector<roster_entry> roster;
    for (size_t i = 0; i < members.members.size(); i++) {
        roster_entry entry;
        entry.member = members.members[i];
        entry.image = i < members.images.size() ? members.images[i] : "";
        if (i < members.colors.size())
            entry.colors = members.colors[i];
        entry.inParty = true;
        entry.city = -1;
        roster.push_back(entry);
    }
    for (const retired_member& who : retired) {
        if (!Shown(who, city))
            continue;
        roster.push_back(roster_entry{ who.member, who.image, who.colors,
            false, who.city });
    }
    return roster;
}


/* static */
void
PartySelectView::ApplyRoster(const std::vector<roster_entry>& roster,
    party& members, std::vector<retired_member>& retired, int city)
{
    std::vector<retired_member> kept;
    for (const retired_member& who : retired) {
        if (!Shown(who, city))
            kept.push_back(who);
    }
    const party before = members;
    members.members.clear();
    members.images.clear();
    members.colors.clear();
    members.leader = 0;
    for (const roster_entry& entry : roster) {
        if (entry.inParty) {
            // the leader stays the same one if he is still there
            if (!before.members.empty() && entry.member.fullName
                    == before.members[size_t(before.leader)].fullName)
                members.leader = int(members.members.size());
            members.members.push_back(entry.member);
            members.images.push_back(entry.image);
            members.colors.push_back(entry.colors);
        } else {
            kept.push_back(retired_member{ entry.member, entry.image,
                entry.colors, entry.city < 0 && city >= 0 ? city : entry.city });
        }
    }
    retired = kept;
}


PartySelectView::PartySelectView(GameData& data)
    :
    fData(data),
    fInfo(NULL),
    fBuffer(NULL),
    fInCity(false),
    fSelected(-1),
    fTop(0),
    fHot(-1),
    fPressed(-1),
    fSheet(false),
    fColorStep(0),
    fRandom(std::random_device()())
{
    fPalette = PICImage::EGAPalette();
    fFont.reset(new Font(fData.Fonts(), kFontIndex));
    fBackground = _LoadPicture("CRETSCR3.PIC", &fPalette);
    GFX::Palette sheetPalette = PICImage::EGAPalette();
    fSheetBackground = _LoadPicture("CRETSCRN.PIC", &sheetPalette);
    fButton = _LoadPicture("BUTTNCR1.PIC");
    fButtonLit = _LoadPicture("BUTTNCR2.PIC");
    fBuffer = new Bitmap(kScreenWidth, kScreenHeight, 8);
    fBuffer->SetColors(fPalette.colors, 0, 256);
    fBlack = NearestColor(fPalette, 16, 16, 16);
    fWhite = NearestColor(fPalette, 255, 255, 255);
    fCrimson = NearestColor(fPalette, 200, 16, 40);
    fDim = NearestColor(fPalette, 110, 110, 110);
    fMark = NearestColor(fPalette, 24, 120, 40);
}


PartySelectView::~PartySelectView()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


PartySelectView::raw_picture
PartySelectView::_LoadPicture(const std::string& name,
    GFX::Palette* palette) const
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
PartySelectView::_DrawPicture(const raw_picture& picture, int x, int y)
{
    for (int row = 0; row < picture.height; row++) {
        for (int column = 0; column < picture.width; column++) {
            const int px = x + column;
            const int py = y + row;
            if (px < 0 || py < 0 || px >= kScreenWidth || py >= kScreenHeight)
                continue;
            fBuffer->PutPixel(px, py,
                picture.pixels[size_t(row) * picture.width + column]);
        }
    }
}


void
PartySelectView::_Text(const std::string& utf8, int x, int y, uint8 color,
    int maxWidth)
{
    fFont->RenderString(Font::ToGameCharset(utf8), fBuffer, GFX::point(x, y),
        color, uint32(maxWidth));
}


void
PartySelectView::SetRoster(const std::vector<roster_entry>& roster, bool inCity)
{
    fRoster.clear();
    for (const roster_entry& entry : roster) {
        if (entry.inParty)
            fRoster.push_back(entry);
    }
    for (const roster_entry& entry : roster) {
        if (!entry.inParty)
            fRoster.push_back(entry);
    }
    fInCity = inCity;
    fSelected = fRoster.empty() ? -1 : 0;
    fTop = 0;
}


int
PartySelectView::CountInParty() const
{
    int count = 0;
    for (const roster_entry& entry : fRoster)
        count += entry.inParty ? 1 : 0;
    return count;
}


void
PartySelectView::Select(int index)
{
    fSelected = index >= 0 && index < int(fRoster.size()) ? index : -1;
    if (fSelected >= 0) {
        if (fSelected < fTop)
            fTop = fSelected;
        else if (fSelected >= fTop + kListRows)
            fTop = fSelected - kListRows + 1;
    }
}


void
PartySelectView::SetSheet(bool sheet)
{
    fSheet = sheet;
    fHot = -1;
    // the new screen's own colors
    if (sheet) {
        GFX::Palette palette = PICImage::EGAPalette();
        FileStream stream(fData.PathFor("CRETSCRN.PIC").c_str(),
            FileStream::READ_ONLY);
        PICImage image(&stream);
        image.ApplyPalette(palette);
        for (int i = 0; i < kFirstFigureColor; i++)
            fPalette.colors[i] = palette.colors[i];
    } else {
        FileStream stream(fData.PathFor("CRETSCR3.PIC").c_str(),
            FileStream::READ_ONLY);
        PICImage image(&stream);
        image.ApplyPalette(fPalette);
    }
    fBuffer->SetColors(fPalette.colors, 0, 256);
    fBlack = NearestColor(fPalette, 16, 16, 16);
    fWhite = NearestColor(fPalette, 255, 255, 255);
    fCrimson = NearestColor(fPalette, 200, 16, 40);
    fDim = NearestColor(fPalette, 110, 110, 110);
    fMark = NearestColor(fPalette, 24, 120, 40);
}


PartySelectView::action
PartySelectView::ButtonAction(int slot) const
{
    if (slot < 0 || slot >= ButtonCount())
        return ACTION_NONE;
    return fSheet ? kSheetSlots[slot] : kInnSlots[slot];
}


bool
PartySelectView::IsEnabled(action what) const
{
    const bool chosen = fSelected >= 0 && fSelected < int(fRoster.size());
    switch (what) {
        case ACTION_CREATE:
            return !fInCity;
        case ACTION_IMAGE:
        case ACTION_HERALDRY:
        case ACTION_COLOR1:
        case ACTION_COLOR2:
        case ACTION_COLOR3:
            // a member of the party (DARKLAND.EXE looks him up among the
            // five slots); the inn's screen has no such buttons
            return chosen && fRoster[size_t(fSelected)].inParty && !fInCity;
        case ACTION_EXAMINE:
            return chosen && fInfo != NULL && !fSheet;
        case ACTION_ADD:
            return chosen && !fRoster[size_t(fSelected)].inParty
                && CountInParty() < kMaxParty;
        case ACTION_DELETE:
            return chosen && !fInCity && fRoster[size_t(fSelected)].inParty;
        case ACTION_KILL:
            return chosen && !fInCity;
        case ACTION_BEGIN:
            return !fInCity && CountInParty() >= 1;
        case ACTION_RETURN:
            return true;
        default:
            return false;
    }
}


bool
PartySelectView::Do(action what, GameWindow* window)
{
    if (!IsEnabled(what))
        return false;
    if (what == ACTION_CREATE)
        return window != NULL && _Create(*window);
    const size_t chosen = size_t(fSelected);
    switch (what) {
        case ACTION_EXAMINE:
            _Examine(window);
            return true;
        case ACTION_ADD: {
            // after the last member of the party
            roster_entry entry = fRoster[chosen];
            entry.inParty = true;
            fRoster.erase(fRoster.begin() + long(chosen));
            const int at = CountInParty();
            fRoster.insert(fRoster.begin() + at, entry);
            Select(at);
            return true;
        }
        case ACTION_DELETE: {
            roster_entry entry = fRoster[chosen];
            entry.inParty = false;
            fRoster.erase(fRoster.begin() + long(chosen));
            const int at = CountInParty();
            fRoster.insert(fRoster.begin() + at, entry);
            Select(at);
            return true;
        }
        case ACTION_KILL:
            fRoster.erase(fRoster.begin() + long(chosen));
            Select(std::min(fSelected, int(fRoster.size()) - 1));
            return true;
        case ACTION_HERALDRY: {
            // the next shield, 'A'..'O' (the byte goes up, from 'P' back to
            // 'A', file 0x73290)
            char& shield = fRoster[chosen].member.heraldry;
            shield = (shield < 'A' || shield >= 'O') ? 'A' : char(shield + 1);
            return true;
        }
        case ACTION_IMAGE: {
            // the next of the game's four pictures
            roster_entry& entry = fRoster[chosen];
            entry.image = ImageCode((ImageIndex(entry.image) + 1) % kImageCount);
            return true;
        }
        case ACTION_COLOR1:
        case ACTION_COLOR2:
        case ACTION_COLOR3: {
            // one of six presets, the step shared by the three keys and
            // the members (DS:E890, 0..5, going up before it is used)
            fColorStep = (fColorStep + 1) % 6;
            roster_entry& entry = fRoster[chosen];
            return ApplyColorPreset(entry.colors, int(what) - ACTION_COLOR1 + 1,
                entry.image, fColorStep);
        }
        case ACTION_BEGIN:
        case ACTION_RETURN:
            return true;
        default:
            return false;
    }
}


// A new person: the life simulation, then he waits with the others
bool
PartySelectView::_Create(GameWindow& window)
{
    CreationView creation(fData, [this](int n) {
        return int(fRandom() % uint32(n));
    });
    if (creation.Run(window) != CreationView::RESULT_CREATED)
        return false;
    roster_entry entry;
    entry.member = creation.Created();
    // the picture is chosen apart (Select Character Image): until then
    // the pictures of the game's own characters, by sex (*inferred*)
    entry.image = entry.member.female ? "F60" : "F01";
    entry.inParty = false;
    entry.city = -1;
    fRoster.push_back(entry);
    Select(int(fRoster.size()) - 1);
    return true;
}


// The character's information screens, with only him in view
void
PartySelectView::_Examine(GameWindow* window)
{
    if (fInfo == NULL || window == NULL)
        return;
    const roster_entry& entry = fRoster[size_t(fSelected)];
    party alone;
    alone.members.push_back(entry.member);
    alone.images.push_back(entry.image);
    alone.colors.push_back(entry.colors);
    alone.leader = 0;
    alone.cash = money{ 0, 0, 0 };
    alone.fame = 0;
    alone.bankNotes = 0;
    alone.philosopherStone = 0;
    party* before = fInfo->Party();
    fInfo->SetParty(&alone);
    fInfo->Run(*window, 0);
    fInfo->SetParty(before);
    fRoster[size_t(fSelected)].member = alone.members[0];
}


GFX::rect
PartySelectView::ButtonRect(int slot) const
{
    return GFX::rect(kButtonLeft,
        (fSheet ? kSheetButtonTop : kButtonTop) + slot * kButtonPitch,
        kButtonWidth, kButtonHeight);
}


GFX::rect
PartySelectView::NameRect(int row) const
{
    return GFX::rect(kListLeft, kListTop + row * kRowHeight, kListWidth,
        kRowHeight);
}


const char*
PartySelectView::ButtonLabel(int slot) const
{
    const action what = ButtonAction(slot);
    if (what == ACTION_NONE)
        return "";
    if (fSheet && what == ACTION_KILL)
        return "Kill character";
    return kLabels[what];
}


int
PartySelectView::_ButtonAt(const GFX::point& point) const
{
    for (int i = 0; i < ButtonCount(); i++) {
        const GFX::rect box = ButtonRect(i);
        if (point.x >= box.x && point.x < box.x + box.w && point.y >= box.y
                && point.y < box.y + box.h)
            return i;
    }
    return -1;
}


// The party's box at a point of the left strip (the new game's screen)
int
PartySelectView::_MemberBoxAt(const GFX::point& point) const
{
    if (!fSheet || point.x < kBoxLeft || point.x >= kBoxLeft + kBoxWidth
            || point.y < 0 || point.y >= kBoxHeight * kMaxParty)
        return -1;
    const int member = point.y / kBoxHeight;
    return member < CountInParty() ? member : -1;
}


int
PartySelectView::_NameAt(const GFX::point& point) const
{
    for (int row = 0; row < kListRows; row++) {
        const GFX::rect box = NameRect(row);
        if (point.x >= box.x && point.x < box.x + box.w && point.y >= box.y
                && point.y < box.y + box.h) {
            const int index = fTop + row;
            return index < int(fRoster.size()) ? index : -1;
        }
    }
    return -1;
}


void
PartySelectView::MouseMoved(const GFX::point& point)
{
    fHot = _ButtonAt(point);
}


PartySelectView::action
PartySelectView::Clicked(const GFX::point& point, GameWindow* window)
{
    const int name = _NameAt(point);
    if (name >= 0) {
        Select(name);
        return ACTION_NONE;
    }
    // a member of the party: his colors are shown (file 0x723FD)
    const int box = _MemberBoxAt(point);
    if (box >= 0) {
        Select(box);
        return ACTION_NONE;
    }
    const int slot = _ButtonAt(point);
    const action what = ButtonAction(slot);
    if (what == ACTION_NONE || !IsEnabled(what))
        return ACTION_NONE;
    Do(what, window);
    return what;
}


PartySelectView::action
PartySelectView::KeyPressed(int key, GameWindow* window)
{
    if (key == SDLK_UP) {
        Select(std::max(0, fSelected - 1));
        return ACTION_NONE;
    }
    if (key == SDLK_DOWN) {
        Select(std::min(int(fRoster.size()) - 1, fSelected + 1));
        return ACTION_NONE;
    }
    action chosen = ACTION_NONE;
    if (key == SDLK_ESCAPE) {
        chosen = ACTION_RETURN;
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (fSelected >= 0 && fSelected < int(fRoster.size()))
            chosen = fRoster[size_t(fSelected)].inParty ? ACTION_DELETE : ACTION_ADD;
    } else {
        char letter = 0;
        if (key >= SDLK_a && key <= SDLK_z)
            letter = char('A' + key - SDLK_a);
        else if (key >= SDLK_1 && key <= SDLK_3)
            letter = char('1' + key - SDLK_1);
        for (int slot = 0; letter != 0 && slot < ButtonCount(); slot++) {
            const action what = ButtonAction(slot);
            if (kKeys[what] == letter)
                chosen = what;
        }
    }
    if (chosen == ACTION_NONE || !IsEnabled(chosen))
        return ACTION_NONE;
    Do(chosen, window);
    return chosen;
}


const PartySelectView::raw_picture*
PartySelectView::_Picture(const std::string& name)
{
    std::map<std::string, raw_picture>::const_iterator found
        = fPictures.find(name);
    if (found == fPictures.end()) {
        raw_picture picture;
        picture.width = picture.height = 0;
        try {
            picture = _LoadPicture(name + ".PIC");
        } catch (const std::exception&) {
        }
        found = fPictures.insert(std::make_pair(name, picture)).first;
    }
    return found->second.width > 0 ? &found->second : NULL;
}


// The party's boxes (the new game's screen): the number and the nickname,
// the figure (<image>STAT.PIC) and the shield (SHIELD<letter>.PIC, 11 x 19)
void
PartySelectView::_DrawParty()
{
    int line = 0;
    for (const roster_entry& entry : fRoster) {
        if (!entry.inParty)
            continue;
        const int top = line * kBoxHeight;
        const bool selected = fSelected >= 0 && fSelected < int(fRoster.size())
            && &fRoster[size_t(fSelected)] == &entry;
        _Text(std::to_string(line + 1) + " " + entry.member.shortName,
            kBoxLeft + 3, top + 3, selected ? fWhite : fBlack, kBoxWidth - 4);
        if (const raw_picture* figure = _Picture(entry.image + "STAT"))
            _DrawPicture(*figure, kBoxLeft + 6, top + 16);
        const std::string shield = std::string("SHIELD")
            + (entry.member.heraldry >= 'A' && entry.member.heraldry <= 'O'
                ? entry.member.heraldry : 'A');
        if (const raw_picture* picture = _Picture(shield))
            _DrawPicture(*picture, kBoxLeft + 30, top + 16);
        line++;
    }
}


// The highlighted member's picture, in his colors: the 8 palette entries
// 235..242 are his (6-bit RGB triplets); a picture's index 0 is clear
void
PartySelectView::_DrawPortrait()
{
    if (fSelected < 0 || fSelected >= int(fRoster.size())
            || !fRoster[size_t(fSelected)].inParty)
        return;
    const roster_entry& entry = fRoster[size_t(fSelected)];
    const raw_picture* picture = _Picture(entry.image + "SMALL");
    if (picture == NULL)
        return;
    for (int i = 0; i < 8; i++) {
        GFX::Color& color = fPalette.colors[kFirstFigureColor + i];
        if (size_t(3 * i + 2) < entry.colors.size()) {
            const uint8* rgb = &entry.colors[size_t(3 * i)];
            color.r = uint8((rgb[0] << 2) | (rgb[0] >> 4));
            color.g = uint8((rgb[1] << 2) | (rgb[1] >> 4));
            color.b = uint8((rgb[2] << 2) | (rgb[2] >> 4));
        } else {
            color.r = color.g = color.b = uint8(64 + 16 * i);
        }
    }
    fBuffer->SetColors(fPalette.colors, 0, 256);
    for (int row = 0; row < picture->height; row++) {
        for (int column = 0; column < picture->width; column++) {
            const uint8 pixel = picture->pixels[size_t(row) * picture->width
                + column];
            if (pixel != 0) {
                fBuffer->PutPixel(kPortraitLeft + column, kPortraitTop + row,
                    pixel);
            }
        }
    }
}


Bitmap*
PartySelectView::Draw()
{
    _DrawPicture(fSheet ? fSheetBackground : fBackground, 0, 0);

    // the strip: the highlighted character's name
    if (fSelected >= 0 && fSelected < int(fRoster.size())) {
        const character& who = fRoster[size_t(fSelected)].member;
        const int width = fFont->StringWidth(Font::ToGameCharset(who.fullName));
        _Text(who.fullName, kTitleLeft + (kTitleWidth - width) / 2, kTitleTop,
            fBlack, kTitleWidth);
    }

    // the party, in order
    if (fSheet) {
        _DrawParty();
    } else {
        int line = 0;
        for (const roster_entry& entry : fRoster) {
            if (!entry.inParty)
                continue;
            _Text(std::to_string(line + 1) + " " + entry.member.shortName,
                kPartyLeft, kPartyTop + line * (kRowHeight + 1), fBlack,
                kPartyWidth);
            line++;
        }
    }

    // the buttons
    for (int slot = 0; slot < ButtonCount(); slot++) {
        const GFX::rect box = ButtonRect(slot);
        const bool enabled = IsEnabled(ButtonAction(slot));
        _DrawPicture(fHot == slot && enabled ? fButtonLit : fButton, box.x,
            box.y);
        const std::string label = ButtonLabel(slot);
        const int width = fFont->StringWidth(Font::ToGameCharset(label));
        const int x = box.x + (box.w - width) / 2;
        const uint8 color = enabled ? fBlack : fDim;
        // the key, in crimson
        _Text(label.substr(0, 1), x, box.y + 2, enabled ? fCrimson : fDim);
        _Text(label.substr(1), x + fFont->StringWidth(
            Font::ToGameCharset(label.substr(0, 1))), box.y + 2, color);
    }

    // the characters of the world
    for (int row = 0; row < kListRows; row++) {
        const int index = fTop + row;
        if (index >= int(fRoster.size()))
            break;
        const roster_entry& entry = fRoster[size_t(index)];
        const GFX::rect box = NameRect(row);
        const bool selected = index == fSelected;
        if (selected)
            fBuffer->FillRect(box, fBlack);
        // a mark on those in the party
        if (entry.inParty)
            fBuffer->FillRect(GFX::rect(box.x + 2, box.y + 2, 4, 4), fMark);
        _Text(entry.member.fullName, box.x + 9, box.y + 1,
            selected ? fWhite : fBlack, box.w - 10);
    }
    if (fSheet)
        _DrawPortrait();
    return fBuffer;
}


PartySelectView::result
PartySelectView::Run(GameWindow& window)
{
    GFX::point mouse(0, 0);
    bool dirty = true;
    for (;;) {
        if (dirty) {
            Draw();
            DrawMouseCursor(fBuffer, mouse, fBlack, fWhite);
            window.Show(fBuffer);
            dirty = false;
        }
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 100) == 0)
            continue;
        action done = ACTION_NONE;
        switch (event.type) {
            case SDL_QUIT:
                SDL_PushEvent(&event);		// the screens below quit too
                return RESULT_QUIT;
            case SDL_KEYDOWN:
                done = KeyPressed(event.key.keysym.sym, &window);
                dirty = true;
                break;
            case SDL_MOUSEMOTION:
                mouse = GameWindow::ToScreen(event.motion.x, event.motion.y);
                MouseMoved(mouse);
                dirty = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse = GameWindow::ToScreen(event.button.x, event.button.y);
                    done = Clicked(mouse, &window);
                    dirty = true;
                }
                break;
            case SDL_WINDOWEVENT:
                dirty = true;
                break;
            default:
                break;
        }
        if (done == ACTION_BEGIN)
            return RESULT_BEGIN;
        if (done == ACTION_RETURN)
            return RESULT_RETURN;
    }
}
