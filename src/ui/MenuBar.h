/*
 * MenuBar.h
 * The game's universal menu bar (manual pp. 17-19, docs/exe.md, "The menu
 * bar"): hidden, it appears across the top of the screen while the right
 * mouse button is held down, or after F10, and has four pull-down menus,
 * Game, Orders, Attack and Party. With the mouse one moves, button still
 * down, to an item and releases; with the keyboard, the cursor keys move
 * the highlight, Return selects, F10 or Esc leaves. Some items have
 * shortcuts (Alt+letter).
 *
 * The texts, their order, their shortcuts and which are checked at the
 * start are DARKLAND.EXE's (the table at DS:0CA1..0DF0); the look is
 * not decoded, only measured on the manual's screenshot (p. 17).
 *
 * The bar draws over a screen's 320x200 8-bit buffer; the input handlers
 * and Draw() work without a window, for testing. The views run it with
 * Run() and act on the command it returns; the settings of the Game menu
 * (difficulty, show changes, music, sound) it keeps itself.
 */
#pragma once

#include "GameSettings.h"
#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <SDL.h>

#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
class GameWindow;
struct menu_item_definition;

enum menu_command {
    MENU_NONE = 0,
    // Game
    MENU_SAVE_GAME,
    MENU_LOAD_GAME,
    MENU_DIFFICULTY_BASIC,
    MENU_DIFFICULTY_STANDARD,
    MENU_DIFFICULTY_EXPERT,
    MENU_SHOW_CHANGES,
    MENU_MUSIC,
    MENU_SOUND_EFFECTS,
    MENU_EXTRAS,			// not in the original: see game_settings
    MENU_PAUSE,
    MENU_QUIT,
    // Orders
    MENU_RESUME,
    MENU_ORDERS_FINISHED,
    MENU_ENEMY_INFO,
    MENU_WALK_TOWARDS,
    MENU_FLEE_TOWARDS,
    MENU_HALT,
    MENU_TRAVEL_AS_GROUP,
    MENU_TRAVEL_SINGLE_FILE,
    MENU_USE_DOOR,
    MENU_USE_STAIRS,
    MENU_OPEN_CHEST,
    MENU_PICK_LOCK,
    MENU_DISSOLVE_LOCK,
    MENU_DISARM_TRAP,
    MENU_SURRENDER,
    MENU_LOOT_BODIES,
    MENU_EXIT_BATTLEFIELD,
    MENU_CANCEL_ORDERS,
    // Attack
    MENU_THROW,
    MENU_STD_ATTACK,
    MENU_VULNERABLE,
    MENU_BERSERK,
    MENU_PARRY,
    MENU_USE_MISSILE,
    // Party
    MENU_PARTY_INFO,
    MENU_MARCHING_ORDER,
    MENU_COUNT
};

class MenuBar {
public:
    static const int kScreenWidth	= 320;
    static const int kLeft			= 60;	// the bar starts after the sidebar
    static const int kHeight		= 9;
    static const int kMenuCount		= 4;

    explicit		MenuBar(GameData& data);	// throws if data is missing
                    ~MenuBar();

    // The Game menu's settings, changed by the items; not owned (NULL: the
    // bar's own).
    void			SetSettings(game_settings* settings);
    const game_settings& Settings() const	{ return *fSettings; }

    // Whether a command can be chosen (shown in black) or not (dim). The
    // Game menu's commands and Party Info are available at first, the
    // battle's orders are not (the battle sets them).
    void			SetEnabled(menu_command command, bool enabled);
    bool			IsEnabled(menu_command command) const;
    void			EnableAll(bool enabled);

    // The first item of a menu is the one the highlight starts on
    bool			IsOpen() const			{ return fMenu >= 0; }
    // The index of the open menu (0..3), or -1
    int				OpenMenu() const		{ return fMenu; }
    // The highlighted item of the open menu, or MENU_NONE
    menu_command	HighlightedCommand() const;

    // Opening and leaving it. Open(menu) shows that pull-down menu;
    // `byMouse` is the right button held down (releasing it selects).
    void			Open(int menu, bool byMouse);
    void			Close();

    // Input, in screen coordinates. Release() is the right button let go
    // and Clicked() a left click: they return the command chosen (or
    // MENU_NONE) and close the bar. `point` over the bar's titles opens a
    // menu, over an enabled item highlights it.
    void			MouseMoved(const GFX::point& point);
    menu_command	Release(const GFX::point& point);
    menu_command	Clicked(const GFX::point& point);
    // A key while the bar is open: true if it was used. `command` is set
    // when it chose an item.
    bool			KeyDown(SDL_Keycode key, uint16 modifiers,
                        menu_command& command);
    // A shortcut pressed while the bar is closed (Alt+letter; in a battle
    // the letters of the orders, too): the command, or MENU_NONE. The
    // Game menu's settings are applied.
    menu_command	Shortcut(SDL_Keycode key, uint16 modifiers);
    // Shows the bar when `event` asks for it (the right button down, F10)
    // and runs until it is left; returns the command chosen, or MENU_NONE.
    // `screen` is the view's buffer as it was just drawn (its palette
    // gives the colors); it is put back as it was.
    menu_command	Run(GameWindow& window, Bitmap* screen,
                        const SDL_Event& event);
    // True for the events that open the bar
    static bool		Opens(const SDL_Event& event);
    // What a view does with an event: if it opens the bar (or is an Alt
    // shortcut, when `shortcuts`), runs it over what `draw` gives (the
    // view's buffer, just drawn), sets `command` and returns true.
    bool			Handle(GameWindow& window, const SDL_Event& event,
                        const std::function<Bitmap*()>& draw, bool shortcuts,
                        menu_command& command);
    // Asks the screens below to quit too (Quit)
    static void		PostQuit();
    // Pause (Alt+P): everything stops until a key or a click
    static void		Pause(GameWindow& window);

    // Draws the bar over `screen` (when open)
    void			Draw(Bitmap* screen) const;

    // Geometry, for tests: the title's and the item's rectangles
    GFX::rect		TitleRect(int menu) const;
    GFX::rect		ItemRect(int menu, int item) const;
    int				CountItems(int menu) const;
    // The command of an item, its text and its shortcut (as the game stores
    // them, with its glyphs), whether it is checked
    menu_command	ItemCommand(int menu, int item) const;
    std::string		ItemText(int menu, int item) const;
    std::string		ItemShortcut(int menu, int item) const;
    bool			IsChecked(menu_command command) const;
    // The Attack menu's checked item (a battle's selected member's way of
    // fighting), or MENU_NONE
    void			SetStance(menu_command stance)	{ fStance = stance; }

private:
    const menu_item_definition& _Item(int menu, int item) const;
    int				_TitleAt(const GFX::point& point) const;
    int				_ItemAt(const GFX::point& point) const;
    int				_SubItemAt(const GFX::point& point) const;
    GFX::rect		_MenuRect(int menu) const;
    GFX::rect		_SubRect() const;
    int				_NextItem(int from, int direction) const;
    int				_NextSubItem(int from, int direction) const;
    bool			_Usable(int menu, int item) const;
    menu_command	_Choose(menu_command command);
    void			_SetHighlight(int item);
    int				_ColumnWidth(int menu, int* shortcutLeft) const;

    GameData&		fData;
    std::unique_ptr<Font> fFont;
    game_settings	fOwnSettings;
    game_settings*	fSettings;
    bool			fEnabled[MENU_COUNT];
    menu_command	fStance;

    int				fMenu;			// open: 0..3, or -1
    int				fItem;			// highlighted in it, or -1
    bool			fSubOpen;		// the Difficulty sub-menu
    int				fSubItem;		// highlighted in it, or -1
    bool			fByMouse;
    std::vector<int> fTitleLeft;
    std::vector<int> fTitleWidth;
};


// Turns commands off for as long as a screen is up (a trade does not let
// the game be saved), and back on after
class MenuLimits {
public:
                    MenuLimits(MenuBar* bar,
                        std::initializer_list<menu_command> commands);
                    ~MenuLimits();

private:
    MenuBar*		fBar;
    std::vector<menu_command> fCommands;
};
