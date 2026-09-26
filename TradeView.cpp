#include "TradeView.h"

#include "Bitmap.h"
#include "Character.h"
#include "FileStream.h"
#include "GameData.h"
#include "ListFile.h"
#include "PICImage.h"
#include "Palette.h"
#include "PartySidebar.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>

static const uint32 kFontIndex		= 2;	// FONTS.FNT, as the cards

// Layout (the scroll interiors measured on BUYSELL.PIC: the gray areas)
static const int kTextLeft			= 76;
static const int kHeaderTop			= 8;
static const int kLineHeight		= 9;
static const int kActionsLeft		= 86;
static const int kActionsTop		= kHeaderTop + 2 * kLineHeight;
static const int kRowHeight			= 8;
static const int kVisibleRows		= 4;
static const struct {
    int left, right;			// interior
    int top, bottom;
    int labelTop;
} kScrolls[2] = {
    { 80, 267, 84, 116, 67 },	// the merchant's
    { 104, 291, 146, 178, 129 }	// the member's
};
static const int kRodHeight			= 8;	// the rods above and below

// Colors: dark text on the parchment, EGA crimson for the letters to
// type (manual p. 7: "the crimson letter"), dim for unavailable actions
static const uint8 kTextColor		= 137;
static const uint8 kCrimsonColor	= 4;	// EGA red
static const uint8 kDisabledColor	= 8;	// EGA dark gray
static const uint8 kHighlightColor	= 140;

static const char* kMerchantNames[MERCHANT_COUNT] = {
    "Swordsmith", "Blacksmith", "Armorer", "Bowyer"
};

// What each merchant deals in, by item category (inferred)
static const uint32 kMerchantGoods[MERCHANT_COUNT] = {
    ITEM_EDGED,
    ITEM_IMPACT | ITEM_POLEARM | ITEM_FLAIL | ITEM_THROWN,
    ITEM_METAL_ARMOR | ITEM_SHIELD | ITEM_ARMOR,
    ITEM_BOW | ITEM_MISSILE_DEVICE | ITEM_ARROW | ITEM_QUARREL | ITEM_BALL
};
// What each merchant buys at the full price: more than it sells (the
// screenshot shows the swordsmith buying a halberd, a polearm); the
// others' likewise by trade (inferred)
static const uint32 kMeleeWeapons	= ITEM_EDGED | ITEM_IMPACT | ITEM_POLEARM
    | ITEM_FLAIL | ITEM_THROWN;
static const uint32 kMerchantBuys[MERCHANT_COUNT] = {
    kMeleeWeapons,
    kMeleeWeapons,
    kMerchantGoods[MERCHANT_ARMORER],
    kMerchantGoods[MERCHANT_BOWYER]
};
static const uint32 kNeverSold		= ITEM_RELIC | ITEM_SPECIAL;

// The actions and their letters (DARKLAND.EXE: " P|urchase an item"...)
static const char* kActionNames[4] = {
    "Purchase an item", "Sell an item", "Barter for another person", "Leave"
};

static const size_t kMaxItems		= 64;	// per character record


TradeView::TradeView(GameData& data)
    :
    fData(data),
    fBuffer(NULL),
    fParty(NULL),
    fKind(MERCHANT_SWORDSMITH),
    fMember(0),
    fActive(SCROLL_MERCHANT),
    fMouse(0, 0),
    fCursorVisible(false)
{
    fSelected[0] = fSelected[1] = 0;
    fTop[0] = fTop[1] = 0;
    fFont.reset(new Font(fData.Fonts(), kFontIndex));
    fSidebar.reset(new PartySidebar(fData));
    fData.Lists();

    fPalette = PICImage::EGAPalette();
    FileStream stream(fData.PathFor("BUYSELL.PIC").c_str(),
        FileStream::READ_ONLY);
    PICImage image(&stream);
    image.ApplyPalette(fPalette);
    fBackground.width = image.Width();
    fBackground.height = image.Height();
    fBackground.pixels = image.RawBytes();

    fBuffer = new Bitmap(320, 200, 8);
}


TradeView::~TradeView()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


void
TradeView::SetParty(party* members)
{
    fParty = members;
    fSidebar->SetParty(members);
}


void
TradeView::SetMerchant(merchant_kind kind)
{
    fKind = kind;
    fStock.clear();
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (size_t code = 0; code < items.size(); code++) {
        const item_definition& definition = items[code];
        if (!definition.name.empty()
                && (definition.flags & kMerchantGoods[kind]) != 0
                && (definition.flags & kNeverSold) == 0)
            fStock.push_back(uint16(code));
    }
    // most valuable first, as on the manual's screenshot
    std::stable_sort(fStock.begin(), fStock.end(),
        [&items](uint16 a, uint16 b) { return items[a].value > items[b].value; });

    fMember = 0;
    fActive = SCROLL_MERCHANT;
    fSelected[0] = fSelected[1] = 0;
    fTop[0] = fTop[1] = 0;
    fMessage.clear();
}


void
TradeView::Run(GameWindow& window)
{
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
                SDL_PushEvent(&event);	// for the screen below, which quits
                return;
            case SDL_KEYDOWN: {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE || key == SDLK_l)
                    return;
                switch (key) {
                    case SDLK_UP:		MoveSelection(-1); break;
                    case SDLK_DOWN:		MoveSelection(1); break;
                    case SDLK_LEFT:
                    case SDLK_RIGHT:	SwitchScroll(); break;
                    case SDLK_p:		Purchase(); break;
                    case SDLK_s:		Sell(); break;
                    case SDLK_b:		BarterForNextMember(); break;
                    default:
                        break;
                }
                dirty = true;
                break;
            }
            case SDL_MOUSEMOTION:
                MouseMoved(GameWindow::ToScreen(event.motion.x, event.motion.y));
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


uint16
TradeView::StockCode(int index) const
{
    return index >= 0 && index < int(fStock.size()) ? fStock[index] : 0;
}


// Fitted on the manual's screenshot (p. 29), all quality 25: the
// swordsmith sells for 394 pf items worth 125, 190 for 60, 127 for 40,
// and buys for 203 an item worth 200, 330 for 325. The leader's skills
// presumably matter ("changing leaders can change prices"); the quality
// too. (inferred)
uint32
TradeView::BuyingPrice(uint16 code) const
{
    const uint32 value = fData.Lists().Items()[code].value;
    return (value * 3141 + 1400 + 500) / 1000;
}


int
TradeView::SellingPrice(uint16 code) const
{
    // the screenshot shows the swordsmith paying much less, and not in
    // proportion, for potions: the rule for goods outside a merchant's
    // trade is unknown, so these are not sold here
    if (!_Deals(code))
        return -1;
    const uint32 value = fData.Lists().Items()[code].value;
    return int((value * 1015 + 500) / 1000);
}


bool
TradeView::Purchase()
{
    fMessage.clear();
    if (!_Available(ACTION_PURCHASE))
        return false;
    const uint16 code = fStock[fSelected[SCROLL_MERCHANT]];
    const uint32 price = BuyingPrice(code);
    const uint32 purse = TotalPfennigs(fParty->cash);
    if (price > purse) {
        fMessage = "Not enough money";	// DARKLAND.EXE
        return false;
    }
    std::vector<item>& items = _MemberItems();
    const item_definition& definition = fData.Lists().Items()[code];
    for (item& carried : items) {
        if (carried.code == code && carried.quality == definition.quality
                && carried.quantity < 255) {
            carried.quantity++;
            fParty->cash = MoneyFromPfennigs(purse - price);
            return true;
        }
    }
    if (items.size() >= kMaxItems) {
        fMessage = "No room for more items";
        return false;
    }
    items.push_back(item{ code, uint8(definition.type), definition.quality,
        1, definition.weight });
    fParty->cash = MoneyFromPfennigs(purse - price);
    return true;
}


bool
TradeView::Sell()
{
    fMessage.clear();
    if (!_Available(ACTION_SELL))
        return false;
    std::vector<item>& items = _MemberItems();
    const int index = fSelected[SCROLL_MEMBER];
    const int price = SellingPrice(items[index].code);
    if (price < 0) {
        fMessage = std::string("The ") + kMerchantNames[fKind]
            + " does not buy that";
        return false;
    }
    const uint8 type = uint8(items[index].type);
    if (items[index].quantity > 1)
        items[index].quantity--;
    else {
        items.erase(items.begin() + index);
        if (fSelected[SCROLL_MEMBER] >= int(items.size()))
            fSelected[SCROLL_MEMBER] = std::max(0, int(items.size()) - 1);
    }
    // what was in use and is gone is no longer in use
    character& member = fParty->members[fMember];
    for (uint8& slot : member.equipment) {
        if (slot != type)
            continue;
        bool left = false;
        for (const item& carried : items)
            left = left || carried.type == type;
        if (!left)
            slot = kNoEquipment;
    }
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) + price);
    return true;
}


void
TradeView::BarterForNextMember()
{
    if (fParty != NULL && !fParty->members.empty())
        SetMember((fMember + 1) % int(fParty->members.size()));
}


void
TradeView::SetMember(int member)
{
    if (fParty == NULL || member < 0 || member >= int(fParty->members.size()))
        return;
    fMember = member;
    fSelected[SCROLL_MEMBER] = 0;
    fTop[SCROLL_MEMBER] = 0;
    fMessage.clear();
}


void
TradeView::Select(scroll which, int index)
{
    const int count = which == SCROLL_MERCHANT ? int(fStock.size())
        : int(_MemberItems().size());
    fActive = which;
    fSelected[which] = std::max(0, std::min(index, count - 1));
    if (fSelected[which] < fTop[which])
        fTop[which] = fSelected[which];
    if (fSelected[which] >= fTop[which] + kVisibleRows)
        fTop[which] = fSelected[which] - kVisibleRows + 1;
}


void
TradeView::MoveSelection(int delta)
{
    Select(scroll(fActive), fSelected[fActive] + delta);
}


void
TradeView::SwitchScroll()
{
    fActive = fActive == SCROLL_MERCHANT ? SCROLL_MEMBER : SCROLL_MERCHANT;
}


void
TradeView::MouseMoved(const GFX::point& point)
{
    fMouse = point;
    fCursorVisible = point.x >= 0 && point.y >= 0 && point.x < 320
        && point.y < 200;
}


bool
TradeView::Clicked(const GFX::point& point)
{
    MouseMoved(point);
    fMessage.clear();
    const int member = fSidebar->MemberAt(point);
    if (member >= 0) {
        SetMember(member);
        return true;
    }
    switch (_ActionAt(point)) {
        case ACTION_PURCHASE:	Purchase(); return true;
        case ACTION_SELL:		Sell(); return true;
        case ACTION_BARTER:		BarterForNextMember(); return true;
        case ACTION_LEAVE:		return false;
        default:
            break;
    }
    // the scrolls: a row selects it, the rods scroll ("left-click at the
    // top or bottom of the scroll", manual p. 29)
    for (int which = 0; which < 2; which++) {
        const auto& box = kScrolls[which];
        if (point.x < box.left || point.x > box.right)
            continue;
        if (point.y >= box.top - kRodHeight && point.y < box.top) {
            Select(scroll(which), fTop[which] - 1);
            fTop[which] = std::max(0, fTop[which] - 1);
        } else if (point.y > box.bottom && point.y <= box.bottom + kRodHeight) {
            Select(scroll(which), fTop[which] + kVisibleRows);
        } else if (point.y >= box.top && point.y <= box.bottom) {
            Select(scroll(which), fTop[which] + (point.y - box.top - 1)
                / kRowHeight);
        }
    }
    return true;
}


Bitmap*
TradeView::Draw()
{
    fBuffer->SetColors(fPalette.colors, 0, 256);
    for (int y = 0; y < fBackground.height; y++) {
        for (int x = 0; x < fBackground.width; x++)
            fBuffer->PutPixel(x, y, fBackground.pixels[size_t(y) * fBackground.width + x]);
    }
    fSidebar->Draw(fBuffer, false);
    if (fParty == NULL || fParty->members.empty())
        return fBuffer;

    // "%s holds the purse of %dfl, %dgr, %dpf (%lupf)."
    const character& leader = fParty->members[fParty->leader];
    const character& member = fParty->members[fMember];
    char text[160];
    snprintf(text, sizeof(text), "%s holds the purse of %dfl, %dgr, %dpf.",
        leader.shortName.c_str(), fParty->cash.florins, fParty->cash.groschen,
        fParty->cash.pfennigs);
    _DrawText(text, kTextLeft, kHeaderTop, kTextColor);
    // "%s barters for %s to the %Fs", or the last message
    if (fMessage.empty()) {
        const std::string forWhom = fMember == fParty->leader
            ? (leader.female ? "herself" : "himself") : member.shortName;
        snprintf(text, sizeof(text), "%s barters for %s to the %s",
            leader.female ? "She" : "He", forWhom.c_str(), kMerchantNames[fKind]);
        _DrawText(text, kTextLeft + 4, kHeaderTop + kLineHeight, kTextColor);
    } else {
        _DrawText(fMessage, kTextLeft + 4, kHeaderTop + kLineHeight,
            kCrimsonColor);
    }

    for (int i = 0; i < ACTION_COUNT; i++) {
        const int y = kActionsTop + i * kLineHeight;
        const std::string name = kActionNames[i];
        if (_Available(i)) {
            _DrawText(name.substr(0, 1), kActionsLeft, y, kCrimsonColor);
            _DrawText(name.substr(1), kActionsLeft
                + fFont->StringWidth(name.substr(0, 1)) + 1, y, kTextColor);
        } else
            _DrawText(name, kActionsLeft, y, kDisabledColor);
    }

    _DrawScroll(SCROLL_MERCHANT);
    _DrawScroll(SCROLL_MEMBER);

    if (fCursorVisible) {
        DrawMouseCursor(fBuffer, fMouse, NearestColor(fPalette, 0, 0, 0),
            NearestColor(fPalette, 255, 255, 255));
    }
    return fBuffer;
}


bool
TradeView::_Deals(uint16 code) const
{
    const uint32 flags = fData.Lists().Items()[code].flags;
    return (flags & kMerchantBuys[fKind]) != 0 && (flags & kNeverSold) == 0;
}


bool
TradeView::_Available(int which) const
{
    if (fParty == NULL || fParty->members.empty())
        return which == ACTION_LEAVE;
    switch (which) {
        case ACTION_PURCHASE:
            return fActive == SCROLL_MERCHANT && !fStock.empty();
        case ACTION_SELL:
            return fActive == SCROLL_MEMBER
                && !fParty->members[fMember].items.empty();
        case ACTION_BARTER:
            return fParty->members.size() > 1;
        default:
            return true;
    }
}


int
TradeView::_ActionAt(const GFX::point& point) const
{
    if (point.x < kActionsLeft || point.x > kActionsLeft + 150
            || point.y < kActionsTop)
        return -1;
    const int action = (point.y - kActionsTop) / kLineHeight;
    return action < ACTION_COUNT ? action : -1;
}


std::vector<item>&
TradeView::_MemberItems()
{
    return fParty->members[fMember].items;
}


void
TradeView::_DrawText(const std::string& utf8, int x, int y, uint8 color,
    int maxWidth)
{
    fFont->RenderString(Font::ToGameCharset(utf8), fBuffer, GFX::point(x, y),
        color, maxWidth);
}


void
TradeView::_DrawScroll(int which)
{
    const auto& box = kScrolls[which];
    const std::vector<item_definition>& definitions = fData.Lists().Items();
    const character& member = fParty->members[fMember];

    // the label: "The %Fs offers...", "%s has..."
    const std::string label = which == SCROLL_MERCHANT
        ? std::string("The ") + kMerchantNames[fKind] + " offers..."
        : member.shortName + " has...";
    const int width = fFont->StringWidth(Font::ToGameCharset(label));
    _DrawText(label, (box.left + box.right - width) / 2, box.labelTop,
        kTextColor);

    const int count = which == SCROLL_MERCHANT ? int(fStock.size())
        : int(member.items.size());
    for (int row = 0; row < kVisibleRows; row++) {
        const int index = fTop[which] + row;
        if (index >= count)
            break;
        const int y = box.top + 1 + row * kRowHeight;
        if (which == fActive && index == fSelected[which]) {
            fBuffer->FillRect(GFX::rect(box.left, y - 1, box.right - box.left + 1,
                kRowHeight), kHighlightColor);
        }
        char text[80];
        if (which == SCROLL_MERCHANT) {
            // "%5upf  %Fs  %3dq  %3dlbs"
            const item_definition& definition = definitions[fStock[index]];
            snprintf(text, sizeof(text), "%5upf  %s  %3dq  %3dlbs",
                BuyingPrice(fStock[index]), definition.name.c_str(),
                definition.quality, definition.weight);
        } else {
            // "%5upf  %Fs  (%3d) %2dq"
            const item& carried = member.items[index];
            const int price = carried.code < definitions.size()
                ? SellingPrice(carried.code) : -1;
            const std::string name = carried.code < definitions.size()
                ? definitions[carried.code].name : "?";
            if (price >= 0) {
                snprintf(text, sizeof(text), "%5dpf  %s  (%3d) %2dq", price,
                    name.c_str(), carried.quantity, carried.quality);
            } else {
                snprintf(text, sizeof(text), "   ---   %s  (%3d) %2dq",
                    name.c_str(), carried.quantity, carried.quality);
            }
        }
        _DrawText(text, box.left + 2, y, kTextColor, box.right - box.left - 2);
    }
}
