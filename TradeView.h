/*
 * TradeView.h
 * Buying and selling with a merchant: the "item exchange scrolls" of the
 * manual (pp. 28-29) on BUYSELL.PIC. The upper scroll lists what the
 * merchant offers, the lower one what a party member carries; the party
 * leader does the bargaining, for any member ("Barter for another
 * person").
 *
 * What a merchant sells and the prices are the game's own rules, in
 * DARKLAND.EXE; they are rebuilt here from the item categories of
 * DARKLAND.LST and the prices on the manual's screenshot (see
 * docs/formats.md). The input handlers and Draw() work without a window,
 * for testing.
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
class PartySidebar;
struct item;
struct party;

enum merchant_kind {
    MERCHANT_SWORDSMITH = 0,	// edged weapons
    MERCHANT_BLACKSMITH,		// impact, polearm, flail, thrown weapons
    MERCHANT_ARMORER,			// armor and shields
    MERCHANT_BOWYER,			// bows, crossbows, guns and their ammunition
    MERCHANT_COUNT
};

class TradeView {
public:
    enum scroll { SCROLL_MERCHANT = 0, SCROLL_MEMBER };

    explicit		TradeView(GameData& data);	// throws if data is missing
                    ~TradeView();

    // The party (not owned): buying and selling change its money and the
    // members' items.
    void			SetParty(party* members);
    // Starts a session with a merchant: its stock, the leader bargaining
    // for the first member.
    void			SetMerchant(merchant_kind kind);

    // Runs until the party leaves.
    void			Run(GameWindow& window);

    // Actions, as the options of the screen. They return false if they
    // are not available now; Message() tells why, when there is a reason.
    bool			Purchase();		// the merchant's highlighted item
    bool			Sell();			// the member's highlighted item
    void			BarterForNextMember();
    void			SetMember(int member);

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

    // Prices in pfennigs; -1 if the merchant does not buy that item.
    uint32			BuyingPrice(uint16 code) const;
    int				SellingPrice(uint16 code) const;

    Bitmap*			Draw();

private:
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    enum action { ACTION_PURCHASE = 0, ACTION_SELL, ACTION_BARTER,
        ACTION_LEAVE, ACTION_COUNT };

    bool			_Deals(uint16 code) const;
    bool			_Available(int which) const;
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
    merchant_kind	fKind;
    std::vector<uint16> fStock;		// item codes, most valuable first
    int				fMember;		// whose items are on the lower scroll
    int				fActive;		// the active scroll
    int				fSelected[2];	// highlighted item of each scroll
    int				fTop[2];		// first item shown of each scroll
    std::string		fMessage;
    GFX::point		fMouse;
    bool			fCursorVisible;
};
