#include "TradeView.h"

#include "Bitmap.h"
#include "Character.h"
#include "CityFile.h"
#include "FileStream.h"
#include "GameData.h"
#include "InfoView.h"
#include "ListFile.h"
#include "PICImage.h"
#include "Palette.h"
#include "PartySidebar.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <random>

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
    "Swordsmith", "Blacksmith", "Armorer", "Bowyer", "Artificier",
    "Clothmaker",
    "Goods Merchant", "Foreign Trader", "herbalist", "Pawnshop",
    "Stablemaster", "Physician", "Alchemist", "Alchemist"
};

// DARKLAND.EXE, see docs/exe.md. The shop type (its quality in the
// city record; kAnyShop: quality 25, kPawnshop: 10) and the item
// categories it deals in, as the guild shops (the same mask by day and at
// night), the market (1893:06AF, 0B51, 1039, 13FA), the inn's stables
// (file 0xA71B4) and the physician (file 0xA3604) open the trade screen
static const int kAnyShop	= -1;
static const int kPawnshop	= -2;

static const struct {
    int shop;
    uint32 goods;			// item_definition::flags bits
} kMerchants[MERCHANT_COUNT] = {
    { SHOP_SWORDSMITH, 0x040000FF },
    { SHOP_BLACKSMITH, 0x040000FF },
    { SHOP_ARMORER, 0x040000FF },
    { SHOP_BOWYER, 0x083C0030 },
    { SHOP_ARTIFICER, 0x0000800E },
    { SHOP_CLOTHMAKER, 0x04000000 },
    { SHOP_GOODS_MERCHANT, 0x0002C100 },
    { kAnyShop, 0x003F843F },
    { kAnyShop, 0x00000400 },
    { kPawnshop, 0x2C3EC3FF },
    { kAnyShop, ITEM_HORSE },
    { kAnyShop, ITEM_COMPONENT },			// type 9 (file 0xA3604)
    { kAnyShop, ITEM_POTION },				// file 0xDA10B
    { kAnyShop, ITEM_COMPONENT }			// file 0xDA122
};


// The quality of a shop's goods (DARKLAND.EXE, 18E7:1C30)
static int
ShopQuality(GameData& data, int cityIndex, merchant_kind kind)
{
    const int shop = kMerchants[kind].shop;
    if (shop == kPawnshop)
        return 10;
    if (shop == kAnyShop || cityIndex < 0)
        return 25;
    return data.Cities().CityAt(uint32(cityIndex)).shopQuality[shop];
}

// The chance (percent) that a merchant has an item, by city size - 1
// (1 outside cities) and rarity (0..11): the table at 290E:399B
static const uint8 kStockChance[9][12] = {
    { 50, 20, 15, 10, 5, 1, 1, 0, 0, 0, 0, 0 },
    { 75, 50, 35, 25, 15, 10, 5, 1, 1, 0, 0, 0 },
    { 99, 75, 65, 60, 45, 30, 20, 10, 7, 4, 2, 1 },
    { 99, 99, 90, 80, 65, 50, 35, 20, 15, 7, 4, 2 },
    { 100, 99, 99, 99, 85, 70, 50, 35, 25, 15, 7, 3 },
    { 100, 100, 99, 99, 90, 80, 65, 55, 40, 30, 15, 6 },
    { 100, 100, 100, 100, 99, 99, 85, 75, 60, 50, 20, 12 },
    { 100, 100, 100, 100, 100, 100, 100, 100, 85, 75, 30, 20 },
    { 100, 100, 100, 100, 100, 100, 100, 100, 99, 99, 50, 30 }
};

// Item types every merchant keeps and sells at quality 25: arrows,
// quarrels, balls
static bool
IsAmmunition(uint16 type)
{
    return type == 0x40 || type == 0x41 || type == 0x42;
}


static int64
Clamp(int64 value, int64 low, int64 high)
{
    return std::max(low, std::min(value, high));
}


// The actions and their letters (DARKLAND.EXE: " P|urchase an item"...),
// and the cache's (DS:37F6...)
static const char* kActionNames[4] = {
    "Purchase an item", "Sell an item", "Barter for another person", "Leave"
};
static const char* kCacheActionNames[4] = {
    "Get an item from cache", "Put an item into cache",
    "Cache another person's items", "Leave"
};
// and the loot's (DS:4C42...)
static const char* kLootActionNames[4] = {
    "Get an item from pile of loot", "Put an item into the pile of loot",
    "Distribute to a different person", "Leave"
};

static const size_t kMaxItems		= 64;	// per character record


TradeView::TradeView(GameData& data)
    :
    fData(data),
    fBuffer(NULL),
    fParty(NULL),
    fInfo(NULL),
    fCity(-1),
    fReputation(0),
    fLocationFlags(0),
    fKind(MERCHANT_SWORDSMITH),
    fCache(NULL),
    fLoot(false),
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
TradeView::SetPlace(int cityIndex, int reputation, uint8 flags)
{
    fCity = cityIndex;
    fReputation = reputation;
    fLocationFlags = flags;
}


/* static */
bool
TradeView::CityHasMerchant(GameData& data, int cityIndex, merchant_kind kind)
{
    if (cityIndex < 0 || cityIndex >= int(data.Cities().CountCities()))
        return false;
    return ShopQuality(data, cityIndex, kind) != 0;
}


void
TradeView::SetMerchant(merchant_kind kind, uint32 seed)
{
    fKind = kind;
    fCache = NULL;
    fLoot = false;
    fStock.clear();

    // DARKLAND.EXE draws each item with random(100) against the chance
    // for its rarity, adjusted by the quality of the goods; the seed here
    // is ours, so that the stock stays the same during a visit
    std::mt19937 random(seed);
    const std::vector<item_definition>& items = fData.Lists().Items();
    int size = 1;
    if (fCity >= 0)
        size = std::min(int(fData.Cities().CityAt(uint32(fCity)).size) - 1, 8);
    for (size_t code = 0; code < items.size(); code++) {
        const item_definition& definition = items[code];
        if (definition.name.empty() || definition.unsellable
                || (definition.flags & kMerchants[kind].goods) == 0)
            continue;
        if (!IsAmmunition(definition.type)) {
            int rarity = definition.rarity;
            const int quality = MerchantQuality(uint16(code));
            if (quality < 23)
                rarity++;
            else if (quality > 29)
                rarity -= 2;
            else if (quality > 26)
                rarity--;
            rarity = std::max(0, std::min(rarity, 11));
            if (int(random() % 100) > kStockChance[size][rarity])
                continue;
        }
        fStock.push_back(uint16(code));		// in item order, as the game
    }

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
                if ((event.key.keysym.mod & KMOD_CTRL) != 0
                        && key >= SDLK_F1 && key <= SDLK_F5) {
                    SetLeader(int(key - SDLK_F1));
                    dirty = true;
                    break;
                }
                if (key >= SDLK_F1 && key <= SDLK_F6 && fInfo != NULL) {
                    fInfo->Run(window, key == SDLK_F6 ? InfoView::kPartyPage
                        : int(key - SDLK_F1));
                    dirty = true;
                    break;
                }
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


// The quality of the merchant's goods: halfway between the item's
// default quality and the quality of the city's shop; 25 for ammunition,
// alchemical components and types 23..25
uint8
TradeView::MerchantQuality(uint16 code) const
{
    const item_definition& definition = fData.Lists().Items()[code];
    if (IsAmmunition(definition.type) || definition.flags == ITEM_COMPONENT
            || (definition.type >= 0x17 && definition.type <= 0x19))
        return 25;
    return uint8((definition.quality + ShopQuality(fData, fCity, fKind)) / 2);
}


// The price the merchant asks (DARKLAND.EXE, 18E7:355E): twice the
// value at quality 25, more or less by rarity and city size, by the
// leader's charisma and, when the party is disliked, by reputation.
// 32-bit integer arithmetic, divisions rounding toward zero.
uint32
TradeView::BuyingPrice(uint16 code) const
{
    const item_definition& definition = fData.Lists().Items()[code];
    const int64 value = int16(definition.value);
    int64 quality = MerchantQuality(code);
    int64 rarity = definition.rarity;
    const int64 size = fCity >= 0
        ? fData.Cities().CityAt(uint32(fCity)).size : 2;
    // with flag 8 the game adds random(10) to the quality at every
    // computation: not reproduced, so that prices stay put
    if (fCity >= 0 && ((fLocationFlags & 0x02) != 0
            || (fLocationFlags & 0x41) != 0))
        rarity += 3;
    int64 price = quality * value * 2 / 25;
    if (fCity >= 0)
        price += price * size * 2 / (rarity > 5 ? 100 : -100);
    price += price * (25 - _LeaderCharisma()) / 100;
    if (fCity >= 0) {
        const int64 bad = Clamp(int16(fReputation * 50), -999, 99) / 100;
        price += bad * price / -100;
    }
    return uint32(std::max<int64>(price, 1));
}


// What the merchant pays (DARKLAND.EXE, 18E7:3756): the same, from a
// quarter of the value, the charisma the other way round. Every merchant
// buys everything but the unsellable items.
uint32
TradeView::SellingPrice(uint16 code, uint8 itemQuality) const
{
    const item_definition& definition = fData.Lists().Items()[code];
    if (definition.unsellable)
        return 0;
    const int64 value = int16(definition.value) / 4;
    int64 quality = itemQuality;
    int64 rarity = definition.rarity;
    const int64 size = fCity >= 0
        ? fData.Cities().CityAt(uint32(fCity)).size : 2;
    if (fCity >= 0 && ((fLocationFlags & 0x02) != 0
            || (fLocationFlags & 0x41) != 0))
        rarity += 3;
    int64 price = quality * value * 2 / 25;
    if (fCity >= 0)
        price += price * size * 2 / (rarity > 5 ? 100 : -100);
    price += price * (25 - _LeaderCharisma()) / -100;
    if (fCity >= 0) {
        const int64 bad = Clamp(int16(fReputation * 50), -999, 75) / 100;
        price += bad * price / 100;
    }
    return uint32(price < 1 ? 0 : price);
}


void
TradeView::SetLoot(std::vector<cache_item>* pile, const money& cash)
{
    SetCache(pile);
    fLoot = true;
    if (fParty == NULL || (cash.florins == 0 && cash.groschen == 0
            && cash.pfennigs == 0)) {
        return;
    }
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
        + TotalPfennigs(cash));
    char text[80];
    snprintf(text, sizeof(text), "The party finds %dfl, %dgr, %dpf in cash.",
        cash.florins, cash.groschen, cash.pfennigs);
    fMessage = text;
}


void
TradeView::SetCache(std::vector<cache_item>* cache)
{
    fCache = cache;
    fLoot = false;
    fStock.clear();
    fMember = 0;
    fActive = SCROLL_MEMBER;
    fSelected[0] = fSelected[1] = 0;
    fTop[0] = fTop[1] = 0;
    fMessage.clear();
}


bool
TradeView::Purchase()
{
    fMessage.clear();
    if (fCache != NULL)
        return _TakeFromCache();
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
        if (carried.code == code && carried.quality == MerchantQuality(code)
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
    items.push_back(item{ code, uint8(definition.type), MerchantQuality(code),
        1, definition.weight });
    fParty->cash = MoneyFromPfennigs(purse - price);
    return true;
}


bool
TradeView::Sell()
{
    fMessage.clear();
    if (fCache != NULL)
        return _LeaveInCache();
    if (!_Available(ACTION_SELL))
        return false;
    std::vector<item>& items = _MemberItems();
    const int index = fSelected[SCROLL_MEMBER];
    const uint32 price = SellingPrice(items[index].code, items[index].quality);
    if (price == 0) {
        fMessage = std::string("The ") + kMerchantNames[fKind]
            + " does not buy that";
        return false;
    }
    _RemoveOne(index);
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) + price);
    return true;
}


// One of the member's items is gone (sold or left)
void
TradeView::_RemoveOne(int index)
{
    std::vector<item>& items = _MemberItems();
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
}


// Getting one of the cache's items (file 0x6E1D6): it joins the
// member's items of the same code and quality, or makes a new one (the
// type and weight of its definition)
bool
TradeView::_TakeFromCache()
{
    if (!_Available(ACTION_PURCHASE))
        return false;
    const int index = fSelected[SCROLL_MERCHANT];
    cache_item& stored = (*fCache)[index];
    std::vector<item>& items = _MemberItems();
    bool joined = false;
    for (item& carried : items) {
        if (carried.code == stored.code && carried.quality == stored.quality
                && carried.quantity < 255) {
            carried.quantity++;
            joined = true;
            break;
        }
    }
    if (!joined) {
        if (items.size() >= kMaxItems) {
            fMessage = "No room for more items";
            return false;
        }
        const item_definition& definition = fData.Lists().Items()[stored.code];
        items.push_back(item{ stored.code, uint8(definition.type),
            stored.quality, 1, definition.weight });
    }
    // 0x6E6FA: an entry whose count reaches 0 is removed
    if (--stored.count == 0) {
        fCache->erase(fCache->begin() + index);
        if (fSelected[SCROLL_MERCHANT] >= int(fCache->size()))
            fSelected[SCROLL_MERCHANT] = std::max(0, int(fCache->size()) - 1);
    }
    return true;
}


// Leaving one of the member's items (file 0x6E086, 0x6E682): the entry
// of the same code and quality counts one more, or a new one is made
bool
TradeView::_LeaveInCache()
{
    if (!_Available(ACTION_SELL))
        return false;
    const item& carried = _MemberItems()[fSelected[SCROLL_MEMBER]];
    bool joined = false;
    for (cache_item& stored : *fCache) {
        if (stored.code == carried.code && stored.quality == carried.quality
                && stored.count < 255) {
            stored.count++;
            joined = true;
            break;
        }
    }
    if (!joined)
        fCache->push_back(cache_item{ carried.code, carried.quality, 1 });
    _RemoveOne(fSelected[SCROLL_MEMBER]);
    return true;
}


int
TradeView::_CountUpper() const
{
    return fCache != NULL ? int(fCache->size()) : int(fStock.size());
}


void
TradeView::BarterForNextMember()
{
    if (fParty != NULL && !fParty->members.empty())
        SetMember((fMember + 1) % int(fParty->members.size()));
}


void
TradeView::SetLeader(int member)
{
    if (fParty != NULL && member >= 0 && member < int(fParty->members.size()))
        fParty->leader = member;
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
    const int count = which == SCROLL_MERCHANT ? _CountUpper()
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
    if (fMessage.empty() && fCache == NULL) {
        const std::string forWhom = fMember == fParty->leader
            ? (leader.female ? "herself" : "himself") : member.shortName;
        snprintf(text, sizeof(text), "%s barters for %s to the %s",
            leader.female ? "She" : "He", forWhom.c_str(), kMerchantNames[fKind]);
        _DrawText(text, kTextLeft + 4, kHeaderTop + kLineHeight, kTextColor);
    } else if (!fMessage.empty()) {
        _DrawText(fMessage, kTextLeft + 4, kHeaderTop + kLineHeight,
            kCrimsonColor);
    }

    for (int i = 0; i < ACTION_COUNT; i++) {
        const int y = kActionsTop + i * kLineHeight;
        const std::string name = fLoot ? kLootActionNames[i]
            : fCache != NULL ? kCacheActionNames[i] : kActionNames[i];
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
TradeView::_Available(int which) const
{
    if (fParty == NULL || fParty->members.empty())
        return which == ACTION_LEAVE;
    switch (which) {
        case ACTION_PURCHASE:
            return fActive == SCROLL_MERCHANT && _CountUpper() > 0;
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
    // (the cache's: "The cache contains...", "%s currently has...")
    std::string label = which == SCROLL_MERCHANT
        ? std::string("The ") + kMerchantNames[fKind] + " offers..."
        : member.shortName + " has...";
    if (fCache != NULL) {
        label = which == SCROLL_MERCHANT ? std::string(fLoot
            ? "The loot contains..." : "The cache contains...")
            : member.shortName + " currently has...";
    }
    const int width = fFont->StringWidth(Font::ToGameCharset(label));
    _DrawText(label, (box.left + box.right - width) / 2, box.labelTop,
        kTextColor);

    const int count = which == SCROLL_MERCHANT ? _CountUpper()
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
        if (fCache != NULL) {
            // "%Fs  (%3d) %2d-Qual" (DS:38B6)
            const uint16 code = which == SCROLL_MERCHANT
                ? (*fCache)[index].code : member.items[index].code;
            const int count = which == SCROLL_MERCHANT
                ? (*fCache)[index].count : member.items[index].quantity;
            const int quality = which == SCROLL_MERCHANT
                ? (*fCache)[index].quality : member.items[index].quality;
            snprintf(text, sizeof(text), "%s  (%3d) %2d-Qual",
                code < definitions.size() ? definitions[code].name.c_str() : "?",
                count, quality);
        } else if (which == SCROLL_MERCHANT) {
            // "%5upf  %Fs  %3dq  %3dlbs"
            const item_definition& definition = definitions[fStock[index]];
            snprintf(text, sizeof(text), "%5upf  %s  %3dq  %3dlbs",
                BuyingPrice(fStock[index]), definition.name.c_str(),
                MerchantQuality(fStock[index]), definition.weight);
        } else {
            // "%5upf  %Fs  (%3d) %2dq"
            const item& carried = member.items[index];
            const int price = carried.code < definitions.size()
                ? int(SellingPrice(carried.code, carried.quality)) : 0;
            const std::string name = carried.code < definitions.size()
                ? definitions[carried.code].name : "?";
            if (price > 0) {
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


int
TradeView::_LeaderCharisma() const
{
    if (fParty == NULL || fParty->members.empty())
        return 25;
    return fParty->members[fParty->leader].attributes[ATTRIBUTE_CHARISMA];
}
