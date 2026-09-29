/*
 * TradeView.h
 * Buying and selling with a merchant: the "item exchange scrolls" of the
 * manual (pp. 28-29) on BUYSELL.PIC. The upper scroll lists what the
 * merchant offers, the lower one what a party member carries; the party
 * leader does the bargaining, for any member ("Barter for another
 * person").
 *
 * What a merchant sells and the prices follow the game's own rules,
 * decoded from DARKLAND.EXE (see docs/exe.md): the shop's goods by item
 * category, a stock drawn by rarity and city size, the quality of the
 * city's shop, the leader's charisma and the party's local reputation.
 * The input handlers and Draw() work without a window, for testing.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <memory>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
class GameWindow;
class InfoView;
class PartySidebar;
struct item;
struct money;
struct party;

enum merchant_kind {
    MERCHANT_SWORDSMITH = 0,
    MERCHANT_BLACKSMITH,
    MERCHANT_ARMORER,
    MERCHANT_BOWYER,
    MERCHANT_ARTIFICER,		// the tinkers' guild
    MERCHANT_CLOTHMAKER,
    MERCHANT_GOODS,			// the market: everyday items,
    MERCHANT_FOREIGN,		// the foreign traders,
    MERCHANT_HERBALIST,		// the pharmacists,
    MERCHANT_PAWNSHOP,		// the Leihhaus
    MERCHANT_STABLES,		// the inn's stablemaster: horses and mules
    MERCHANT_PHYSICIAN,		// alchemical components
    MERCHANT_ALCHEMIST,		// potions,
    MERCHANT_ALCHEMIST_COMPONENTS,	// or components from a lesser one
    MERCHANT_ARMS_OUTFITTER,	// Soldier's Road
    MERCHANT_COUNT
};

// An item left with an innkeeper: one entry per item and quality, with
// a count (DARKLAND.EXE, 4 bytes in CACHE.TMP: code, quality, count)
struct cache_item {
    uint16 code;
    uint8 quality;
    uint8 count;
};

class TradeView {
public:
    enum scroll { SCROLL_MERCHANT = 0, SCROLL_MEMBER };

    explicit		TradeView(GameData& data);	// throws if data is missing
                    ~TradeView();

    // The party (not owned): buying and selling change its money and the
    // members' items.
    void			SetParty(party* members);
    // The information screens that F1..F6 open (not owned; NULL: none)
    void			SetInfoView(InfoView* info)	{ fInfo = info; }
    // Where the trade happens: a city (index into DARKLAND.CTY, or -1),
    // the party's reputation there and the location's flags.
    void			SetPlace(int cityIndex, int reputation, uint8 flags = 0);
    // Starts a session with a merchant: its stock (drawn with `seed`),
    // the leader bargaining for the first member.
    void			SetMerchant(merchant_kind kind, uint32 seed = 0);
    // Or the items left at an inn (not owned): the upper scroll shows
    // them, Purchase() takes one, Sell() leaves one, without money
    // (DARKLAND.EXE, file 0x6DFF6). SetMerchant() ends it.
    void			SetCache(std::vector<cache_item>* cache);
    bool			IsCache() const			{ return fCache != NULL; }
    // Or the loot of a battle (DARKLAND.EXE's overlay at file 0x8A4E0): a
    // pile to take from and leave in, like the cache; the cash goes to
    // the purse, and the screen says so.
    void			SetLoot(std::vector<cache_item>* pile, const money& cash);
    // Whether the city has that merchant (the quality of its guild shop
    // is 0 if not; the market's merchants are in every city).
    static bool		CityHasMerchant(GameData& data, int cityIndex,
                        merchant_kind kind);

    // Runs until the party leaves.
    void			Run(GameWindow& window);

    // Actions, as the options of the screen. They return false if they
    // are not available now; Message() tells why, when there is a reason.
    bool			Purchase();		// the merchant's highlighted item
    bool			Sell();			// the member's highlighted item
    void			BarterForNextMember();
    void			SetMember(int member);
    // Ctrl+F1..F5: another member leads, and bargains (file 0x68E44)
    void			SetLeader(int member);

    // The highlighted item of a scroll, which becomes the active one.
    void			Select(scroll which, int index);
    void			MoveSelection(int delta);	// in the active scroll
    void			SwitchScroll();

    // Input, in screen (320x200) coordinates; Clicked() returns false
    // when the click chose "Leave".
    void			MouseMoved(const GFX::point& point);
    bool			Clicked(const GFX::point& point);

    int				Member() const			{ return fMember; }
    int				ActiveScroll() const	{ return fActive; }
    const std::string& Message() const		{ return fMessage; }
    int				CountStock() const		{ return int(fStock.size()); }
    uint16			StockCode(int index) const;

    // Prices in pfennigs (DARKLAND.EXE): what the merchant asks for one
    // of its items, and pays for an item of the party (0: it does not
    // buy it).
    uint32			BuyingPrice(uint16 code) const;
    uint32			SellingPrice(uint16 code, uint8 quality) const;
    // The quality of the merchant's goods.
    uint8			MerchantQuality(uint16 code) const;

    Bitmap*			Draw();

private:
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    enum action { ACTION_PURCHASE = 0, ACTION_SELL, ACTION_BARTER,
        ACTION_LEAVE, ACTION_COUNT };

    bool			_Available(int which) const;
    int				_CountUpper() const;
    void			_RemoveOne(int index);
    bool			_TakeFromCache();
    bool			_LeaveInCache();
    int				_LeaderCharisma() const;
    int				_ActionAt(const GFX::point& point) const;
    std::vector<item>& _MemberItems();
    void			_DrawText(const std::string& utf8, int x, int y,
                        uint8 color, int maxWidth = 250);
    void			_DrawScroll(int which);

    GameData&		fData;
    Bitmap*			fBuffer;
    std::unique_ptr<Font>	fFont;
    std::unique_ptr<PartySidebar> fSidebar;
    raw_picture		fBackground;
    GFX::Palette	fPalette;

    party*			fParty;
    InfoView*		fInfo;
    int				fCity;
    int				fReputation;
    uint8			fLocationFlags;
    merchant_kind	fKind;
    std::vector<uint16> fStock;		// item codes, most valuable first
    std::vector<cache_item>* fCache;	// NULL: a merchant
    bool			fLoot;			// fCache is a battle's loot
    int				fMember;		// whose items are on the lower scroll
    int				fActive;		// the active scroll
    int				fSelected[2];	// highlighted item of each scroll
    int				fTop[2];		// first item shown of each scroll
    std::string		fMessage;
    GFX::point		fMouse;
    bool			fCursorVisible;
};
