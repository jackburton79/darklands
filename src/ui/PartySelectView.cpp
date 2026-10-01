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

// The buttons' texts (DARKLAND.EXE, 290E:1E8A.., a 0xFF and the letters
// after the first, which is the key) in the manual's order
static const char* kLabels[PartySelectView::ACTION_COUNT] = {
    "Create a Character", "Examine a Character", "Add to the Party",
    "Delete from the Party", "Select Character Image", "Kill a Character",
    "Begin the Adventure", "Return to Main Menu"
};
static const char kKeys[PartySelectView::ACTION_COUNT] = {
    'C', 'E', 'A', 'D', 'S', 'K', 'B', 'R'
};


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
    fRandom(std::random_device()())
{
    memset(fPalette.colors, 0, sizeof(fPalette.colors));
    for (int i = 0; i < 256; i++)
        fPalette.colors[i].r = fPalette.colors[i].g = fPalette.colors[i].b = uint8(i);
    fFont.reset(new Font(fData.Fonts(), kFontIndex));
    fBackground = _LoadPicture("CRETSCR3.PIC", &fPalette);
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


bool
PartySelectView::IsEnabled(action what) const
{
    const bool chosen = fSelected >= 0 && fSelected < int(fRoster.size());
    switch (what) {
        case ACTION_CREATE:
            return !fInCity;
        case ACTION_IMAGE:
            return false;				// not reproduced
        case ACTION_EXAMINE:
            return chosen && fInfo != NULL;
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
    const party* before = fInfo->Party();
    fInfo->SetParty(&alone);
    fInfo->Run(*window, 0);
    fInfo->SetParty(before);
}


GFX::rect
PartySelectView::ButtonRect(int index) const
{
    return GFX::rect(kButtonLeft, kButtonTop + index * kButtonPitch,
        kButtonWidth, kButtonHeight);
}


GFX::rect
PartySelectView::NameRect(int row) const
{
    return GFX::rect(kListLeft, kListTop + row * kRowHeight, kListWidth,
        kRowHeight);
}


const char*
PartySelectView::ButtonLabel(int index) const
{
    return index >= 0 && index < ACTION_COUNT ? kLabels[index] : "";
}


int
PartySelectView::_ButtonAt(const GFX::point& point) const
{
    for (int i = 0; i < ACTION_COUNT; i++) {
        const GFX::rect box = ButtonRect(i);
        if (point.x >= box.x && point.x < box.x + box.w && point.y >= box.y
                && point.y < box.y + box.h)
            return i;
    }
    return -1;
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
    const int button = _ButtonAt(point);
    if (button < 0 || !IsEnabled(action(button)))
        return ACTION_NONE;
    Do(action(button), window);
    return action(button);
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
    } else if (key >= SDLK_a && key <= SDLK_z) {
        for (int i = 0; i < ACTION_COUNT; i++) {
            if (kKeys[i] == char('A' + key - SDLK_a))
                chosen = action(i);
        }
    }
    if (chosen == ACTION_NONE || !IsEnabled(chosen))
        return ACTION_NONE;
    Do(chosen, window);
    return chosen;
}


Bitmap*
PartySelectView::Draw()
{
    _DrawPicture(fBackground, 0, 0);

    // the strip: the highlighted character's name
    if (fSelected >= 0 && fSelected < int(fRoster.size())) {
        const character& who = fRoster[size_t(fSelected)].member;
        const int width = fFont->StringWidth(Font::ToGameCharset(who.fullName));
        _Text(who.fullName, kTitleLeft + (kTitleWidth - width) / 2, kTitleTop,
            fBlack, kTitleWidth);
    }

    // the party, in order
    int line = 0;
    for (const roster_entry& entry : fRoster) {
        if (!entry.inParty)
            continue;
        _Text(std::to_string(line + 1) + " " + entry.member.shortName,
            kPartyLeft, kPartyTop + line * (kRowHeight + 1), fBlack,
            kPartyWidth);
        line++;
    }

    // the buttons
    for (int i = 0; i < ACTION_COUNT; i++) {
        const GFX::rect box = ButtonRect(i);
        const bool enabled = IsEnabled(action(i));
        _DrawPicture(fHot == i && enabled ? fButtonLit : fButton, box.x, box.y);
        const std::string label = kLabels[i];
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
