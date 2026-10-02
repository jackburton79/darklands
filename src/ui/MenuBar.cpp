/*
 * MenuBar.cpp
 */

#include "MenuBar.h"

#include "GameData.h"
#include "Bitmap.h"
#include "Palette.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <algorithm>
#include <cstring>


static const uint32 kFontIndex		= 0;	// FONTS.FNT: the small one, with
                                            // the shortcuts' glyphs

static const int kRowHeight			= 9;
static const int kFirstTitle		= 6;	// from the bar's left
static const int kTitleGap			= 10;
static const int kMenuPadding		= 4;
static const int kShortcutGap		= 8;
static const int kTextTop			= 1;

// Colors, measured on the manual's screenshot (p. 17): not decoded
static const GFX::Color kBackgroundRGB	= { 44, 36, 32, 0 };
static const GFX::Color kBorderRGB		= { 12, 8, 8, 0 };
static const GFX::Color kTextRGB		= { 220, 204, 164, 0 };
static const GFX::Color kDimRGB			= { 120, 104, 84, 0 };
static const GFX::Color kHighlightRGB	= { 220, 204, 164, 0 };
static const GFX::Color kMarkedRGB		= { 44, 36, 32, 0 };

// The glyphs of FONTS.FNT (font 0) that the game's texts use: a checkmark
// (01, 02 is the blank of the same width), and the keys' names
#define CHECKED		"\x01"
#define UNCHECKED	"\x02"
#define ALT			"\x0F"
#define F6			"\x0A"
#define SPACE_KEY	"\x13"
#define RETURN_KEY	"\x12"
#define ESCAPE_KEY	"\x03"


struct menu_item_definition {
    menu_command	command;
    const char*		text;			// DARKLAND.EXE's, without the check slot
    const char*		shortcut;
    char			key;			// the letter, or 0
    bool			alt;			// with Alt (else the battle's key)
    bool			subMenu;		// Difficulty
};

struct menu_definition {
    const char*		title;
    const menu_item_definition* items;
    int				count;
    bool			checkSlot;		// each item starts with 01 or 02
};


// DS:0CBC and DS:0D2F: the Game menu
static const menu_item_definition kGameItems[] = {
    { MENU_SAVE_GAME, "Save Game", ALT "S", 'S', true, false },
    { MENU_LOAD_GAME, "Load Saved Game", ALT "L", 'L', true, false },
    { MENU_NONE, "Difficulty", ALT "D", 'D', true, true },
    { MENU_SHOW_CHANGES, "Show Changes", ALT "C", 'C', true, false },
    { MENU_MUSIC, "Music", ALT "M", 'M', true, false },
    { MENU_SOUND_EFFECTS, "Sound FX", ALT "F", 'F', true, false },
    { MENU_EXTRAS, "Extras", ALT "X", 'X', true, false },
    { MENU_PAUSE, "Pause", ALT "P", 'P', true, false },
    { MENU_QUIT, "Quit to DOS", ALT "Q", 'Q', true, false }
};

// DS:0D2F: the Difficulty sub-menu
static const menu_item_definition kDifficultyItems[] = {
    { MENU_DIFFICULTY_BASIC, "Basic", "", 0, false, false },
    { MENU_DIFFICULTY_STANDARD, "Standard", "", 0, false, false },
    { MENU_DIFFICULTY_EXPERT, "Expert", "", 0, false, false }
};

// DS:0D49 and DS:067C..0726 (the names' blanks widen the menu)
static const menu_item_definition kOrdersItems[] = {
    { MENU_RESUME, "Resume", SPACE_KEY, 0, false, false },
    { MENU_ORDERS_FINISHED, "?", RETURN_KEY, 0, false, false },
    { MENU_ENEMY_INFO, "Enemy Info", " E", 'E', false, false },
    { MENU_WALK_TOWARDS, "Walk towards      ", " W", 'W', false, false },
    { MENU_FLEE_TOWARDS, "Flee towards      ", " F", 'F', false, false },
    { MENU_HALT, "Halt", " H", 'H', false, false },
    { MENU_TRAVEL_AS_GROUP, "Travel As Group", " G", 'G', false, false },
    { MENU_TRAVEL_SINGLE_FILE, "Travel Single File", " Q", 'Q', false, false },
    { MENU_USE_DOOR, "Use Door      ", " U", 'U', false, false },
    { MENU_USE_STAIRS, "Use Stairs      ", " U", 'U', false, false },
    { MENU_OPEN_CHEST, "Open Chest      ", " O", 'O', false, false },
    { MENU_PICK_LOCK, "Pick Lock      ", " P", 'P', false, false },
    { MENU_DISSOLVE_LOCK, "Dissolve Lock       ", " D", 'D', false, false },
    { MENU_DISARM_TRAP, "Disarm simple trap", " D", 'D', false, false },
    { MENU_SURRENDER, "Surrender (All)", " S", 'S', false, false },
    { MENU_LOOT_BODIES, "Loot Bodies", " L", 'L', false, false },
    { MENU_EXIT_BATTLEFIELD, "Exit Battlefield      ", " X", 'X', false, false },
    { MENU_CANCEL_ORDERS, "Cancel Giving Order", ESCAPE_KEY, 0, false, false }
};

// DS:0DA9
static const menu_item_definition kAttackItems[] = {
    { MENU_THROW, "Throw", "T>", 'T', false, false },
    { MENU_STD_ATTACK, "Std Attack ", "A", 'A', false, false },
    { MENU_VULNERABLE, "Vulnerable", "V", 'V', false, false },
    { MENU_BERSERK, "Berserk", "B", 'B', false, false },
    { MENU_PARRY, "Parry", "P", 'P', false, false },
    { MENU_USE_MISSILE, "Use Missile", "M", 'M', false, false }
};

// DS:0DEE
static const menu_item_definition kPartyItems[] = {
    { MENU_PARTY_INFO, "Party Info", F6, 0, false, false },
    { MENU_MARCHING_ORDER, "Change Marching Order", ALT "O", 'O', true, false }
};

static const menu_definition kMenus[MenuBar::kMenuCount] = {
    { "Game", kGameItems, 9, true },
    { "Orders", kOrdersItems, 18, true },
    { "Attack", kAttackItems, 6, false },
    { "Party", kPartyItems, 2, false }
};

static const int kDifficultyItem = 2;		// in the Game menu


MenuBar::MenuBar(GameData& data)
    :
    fData(data),
    fFont(new Font(data.Fonts(), kFontIndex)),
    fSettings(&fOwnSettings),
    fMenu(-1),
    fItem(-1),
    fSubOpen(false),
    fSubItem(-1),
    fByMouse(false)
{
    // The Game menu and Party Info are on, the battle's orders off until
    // a battle turns them on
    fStance = MENU_NONE;
    for (int i = 0; i < MENU_COUNT; i++)
        fEnabled[i] = false;
    for (int menu = 0; menu < kMenuCount; menu++) {
        if (menu == 1 || menu == 2)
            continue;
        for (int item = 0; item < kMenus[menu].count; item++)
            fEnabled[kMenus[menu].items[item].command] = true;
    }
    for (int i = 0; i < 3; i++)
        fEnabled[MENU_DIFFICULTY_BASIC + i] = true;
    fEnabled[MENU_NONE] = true;

    int left = kLeft + kFirstTitle;
    for (int menu = 0; menu < kMenuCount; menu++) {
        const int width = fFont->StringWidth(kMenus[menu].title);
        fTitleLeft.push_back(left);
        fTitleWidth.push_back(width);
        left += width + kTitleGap;
    }
}


MenuBar::~MenuBar()
{
}


void
MenuBar::SetSettings(game_settings* settings)
{
    fSettings = settings != NULL ? settings : &fOwnSettings;
}


void
MenuBar::SetEnabled(menu_command command, bool enabled)
{
    if (command > MENU_NONE && command < MENU_COUNT)
        fEnabled[command] = enabled;
}


bool
MenuBar::IsEnabled(menu_command command) const
{
    return command >= MENU_NONE && command < MENU_COUNT && fEnabled[command];
}


void
MenuBar::EnableAll(bool enabled)
{
    for (int i = MENU_NONE + 1; i < MENU_COUNT; i++)
        fEnabled[i] = enabled;
}


bool
MenuBar::IsChecked(menu_command command) const
{
    switch (command) {
        case MENU_DIFFICULTY_BASIC:
            return fSettings->difficulty == DIFFICULTY_BASIC;
        case MENU_DIFFICULTY_STANDARD:
            return fSettings->difficulty == DIFFICULTY_STANDARD;
        case MENU_DIFFICULTY_EXPERT:
            return fSettings->difficulty == DIFFICULTY_EXPERT;
        case MENU_SHOW_CHANGES:
            return fSettings->showChanges;
        case MENU_MUSIC:
            return fSettings->music;
        case MENU_SOUND_EFFECTS:
            return fSettings->soundEffects;
        case MENU_EXTRAS:
            return fSettings->extras;
        case MENU_STD_ATTACK:
        case MENU_VULNERABLE:
        case MENU_BERSERK:
        case MENU_PARRY:
            return command == fStance;
        default:
            return false;
    }
}


int
MenuBar::CountItems(int menu) const
{
    return menu >= 0 && menu < kMenuCount ? kMenus[menu].count : 0;
}


const menu_item_definition&
MenuBar::_Item(int menu, int item) const
{
    return kMenus[menu].items[item];
}


menu_command
MenuBar::ItemCommand(int menu, int item) const
{
    return _Item(menu, item).command;
}


std::string
MenuBar::ItemText(int menu, int item) const
{
    const menu_item_definition& definition = _Item(menu, item);
    std::string text = definition.text;
    if (kMenus[menu].checkSlot)
        text = (IsChecked(definition.command) ? CHECKED : UNCHECKED) + text;
    return text;
}


std::string
MenuBar::ItemShortcut(int menu, int item) const
{
    return _Item(menu, item).shortcut;
}


menu_command
MenuBar::HighlightedCommand() const
{
    if (fMenu < 0)
        return MENU_NONE;
    if (fSubOpen && fSubItem >= 0)
        return kDifficultyItems[fSubItem].command;
    if (fItem >= 0)
        return _Item(fMenu, fItem).command;
    return MENU_NONE;
}


bool
MenuBar::_Usable(int menu, int item) const
{
    const menu_item_definition& definition = _Item(menu, item);
    return definition.subMenu || fEnabled[definition.command];
}


// The width of the item column (check slot and text) of a menu, and
// where its shortcuts' column starts
int
MenuBar::_ColumnWidth(int menu, int* shortcutLeft) const
{
    int names = 0;
    int shortcuts = 0;
    for (int item = 0; item < kMenus[menu].count; item++) {
        names = std::max<int>(names, fFont->StringWidth(ItemText(menu, item)));
        shortcuts = std::max<int>(shortcuts,
            fFont->StringWidth(_Item(menu, item).shortcut));
    }
    if (shortcutLeft != NULL)
        *shortcutLeft = names + kShortcutGap;
    return names + kShortcutGap + shortcuts;
}


GFX::rect
MenuBar::TitleRect(int menu) const
{
    return GFX::rect(fTitleLeft[size_t(menu)] - kMenuPadding / 2, 0,
        fTitleWidth[size_t(menu)] + kMenuPadding, kHeight);
}


GFX::rect
MenuBar::_MenuRect(int menu) const
{
    const int width = _ColumnWidth(menu, NULL) + 2 * kMenuPadding;
    int left = fTitleLeft[size_t(menu)] - kMenuPadding;
    left = std::max(kLeft, std::min(left, kScreenWidth - width));
    return GFX::rect(left, kHeight, width, 2 + kMenus[menu].count * kRowHeight);
}


GFX::rect
MenuBar::ItemRect(int menu, int item) const
{
    const GFX::rect box = _MenuRect(menu);
    return GFX::rect(box.x + 1, box.y + 1 + item * kRowHeight, box.w - 2,
        kRowHeight);
}


// The Difficulty sub-menu: at the right of its item
GFX::rect
MenuBar::_SubRect() const
{
    int width = 0;
    for (int item = 0; item < 3; item++) {
        width = std::max<int>(width, fFont->StringWidth(
            std::string(UNCHECKED) + kDifficultyItems[item].text));
    }
    width += 2 * kMenuPadding;
    const GFX::rect parent = ItemRect(0, kDifficultyItem);
    int left = parent.x + parent.w;
    if (left + width > kScreenWidth)
        left = parent.x - width;
    return GFX::rect(left, parent.y - 1, width, 2 + 3 * kRowHeight);
}


int
MenuBar::_TitleAt(const GFX::point& point) const
{
    if (point.y < 0 || point.y >= kHeight)
        return -1;
    for (int menu = 0; menu < kMenuCount; menu++) {
        const GFX::rect box = TitleRect(menu);
        if (point.x >= box.x && point.x < box.x + box.w)
            return menu;
    }
    return -1;
}


int
MenuBar::_ItemAt(const GFX::point& point) const
{
    if (fMenu < 0)
        return -1;
    const GFX::rect box = _MenuRect(fMenu);
    if (point.x < box.x + 1 || point.x >= box.x + box.w - 1
            || point.y < box.y + 1 || point.y >= box.y + box.h - 1)
        return -1;
    return (point.y - box.y - 1) / kRowHeight;
}


int
MenuBar::_SubItemAt(const GFX::point& point) const
{
    if (!fSubOpen)
        return -1;
    const GFX::rect box = _SubRect();
    if (point.x < box.x + 1 || point.x >= box.x + box.w - 1
            || point.y < box.y + 1 || point.y >= box.y + box.h - 1)
        return -1;
    return (point.y - box.y - 1) / kRowHeight;
}


void
MenuBar::Open(int menu, bool byMouse)
{
    fMenu = std::max(0, std::min(menu, kMenuCount - 1));
    fItem = -1;
    fSubOpen = false;
    fSubItem = -1;
    fByMouse = byMouse;
}


void
MenuBar::Close()
{
    fMenu = -1;
    fItem = -1;
    fSubOpen = false;
    fSubItem = -1;
    fByMouse = false;
}


void
MenuBar::_SetHighlight(int item)
{
    fItem = item;
    fSubOpen = item == kDifficultyItem && fMenu == 0;
    fSubItem = -1;
}


void
MenuBar::MouseMoved(const GFX::point& point)
{
    if (fMenu < 0)
        return;
    const int title = _TitleAt(point);
    if (title >= 0) {
        if (title != fMenu) {
            fMenu = title;
            fItem = -1;
            fSubOpen = false;
            fSubItem = -1;
        }
        return;
    }
    const int sub = _SubItemAt(point);
    if (sub >= 0) {
        fSubItem = sub;
        return;
    }
    const int item = _ItemAt(point);
    if (item >= 0 && _Usable(fMenu, item)) {
        if (item != fItem)
            _SetHighlight(item);
        else if (fSubOpen)
            fSubItem = -1;
    } else if (!(fSubOpen && fItem >= 0 && item >= 0 && item == fItem)) {
        fItem = -1;
        fSubOpen = false;
        fSubItem = -1;
    }
}


// Applies the Game menu's settings
menu_command
MenuBar::_Choose(menu_command command)
{
    switch (command) {
        case MENU_DIFFICULTY_BASIC:
            fSettings->difficulty = DIFFICULTY_BASIC;
            break;
        case MENU_DIFFICULTY_STANDARD:
            fSettings->difficulty = DIFFICULTY_STANDARD;
            break;
        case MENU_DIFFICULTY_EXPERT:
            fSettings->difficulty = DIFFICULTY_EXPERT;
            break;
        case MENU_SHOW_CHANGES:
            fSettings->showChanges = !fSettings->showChanges;
            break;
        case MENU_MUSIC:
            fSettings->music = !fSettings->music;
            break;
        case MENU_SOUND_EFFECTS:
            fSettings->soundEffects = !fSettings->soundEffects;
            break;
        case MENU_EXTRAS:
            fSettings->extras = !fSettings->extras;
            break;
        default:
            break;
    }
    return command;
}


menu_command
MenuBar::Release(const GFX::point& point)
{
    MouseMoved(point);
    menu_command command = MENU_NONE;
    if (fSubOpen && fSubItem >= 0
            && fEnabled[kDifficultyItems[fSubItem].command])
        command = kDifficultyItems[fSubItem].command;
    else if (fItem >= 0 && !_Item(fMenu, fItem).subMenu
            && _Usable(fMenu, fItem))
        command = _Item(fMenu, fItem).command;
    Close();
    return _Choose(command);
}


menu_command
MenuBar::Clicked(const GFX::point& point)
{
    if (fMenu < 0)
        return MENU_NONE;
    const int title = _TitleAt(point);
    if (title >= 0) {
        MouseMoved(point);
        return MENU_NONE;
    }
    if (_SubItemAt(point) < 0 && _ItemAt(point) < 0) {
        Close();
        return MENU_NONE;
    }
    MouseMoved(point);
    if (fItem >= 0 && _Item(fMenu, fItem).subMenu && fSubItem < 0)
        return MENU_NONE;		// the click opened the sub-menu
    return Release(point);
}


// The next usable item from `from` (-1: before the first), or `from`
int
MenuBar::_NextItem(int from, int direction) const
{
    const int count = kMenus[fMenu].count;
    for (int step = 1; step <= count; step++) {
        const int item = ((from + direction * step) % count + count) % count;
        if (_Usable(fMenu, item))
            return item;
    }
    return from;
}


int
MenuBar::_NextSubItem(int from, int direction) const
{
    for (int step = 1; step <= 3; step++) {
        const int item = ((from + direction * step) % 3 + 3) % 3;
        if (fEnabled[kDifficultyItems[item].command])
            return item;
    }
    return from;
}


bool
MenuBar::KeyDown(SDL_Keycode key, uint16 modifiers, menu_command& command)
{
    (void)modifiers;
    command = MENU_NONE;
    if (fMenu < 0)
        return false;
    switch (key) {
        case SDLK_F10:
        case SDLK_ESCAPE:
            Close();
            break;
        case SDLK_LEFT:
            if (fSubOpen && fSubItem >= 0) {
                fSubItem = -1;
            } else if (fSubOpen) {
                fSubOpen = false;
            } else {
                Open((fMenu + kMenuCount - 1) % kMenuCount, fByMouse);
            }
            break;
        case SDLK_RIGHT:
            if (fSubOpen && fSubItem < 0) {
                fSubItem = IsChecked(MENU_DIFFICULTY_BASIC) ? 0
                    : IsChecked(MENU_DIFFICULTY_STANDARD) ? 1 : 2;
            } else if (!fSubOpen) {
                Open((fMenu + 1) % kMenuCount, fByMouse);
            }
            break;
        case SDLK_UP:
        case SDLK_DOWN: {
            const int direction = key == SDLK_UP ? -1 : 1;
            if (fSubOpen && fSubItem >= 0)
                fSubItem = _NextSubItem(fSubItem, direction);
            else
                _SetHighlight(_NextItem(fItem, direction));
            break;
        }
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (fSubOpen && fSubItem >= 0) {
                command = kDifficultyItems[fSubItem].command;
                Close();
            } else if (fItem >= 0 && _Item(fMenu, fItem).subMenu) {
                fSubOpen = true;
                fSubItem = IsChecked(MENU_DIFFICULTY_BASIC) ? 0
                    : IsChecked(MENU_DIFFICULTY_STANDARD) ? 1 : 2;
            } else if (fItem >= 0) {
                command = _Item(fMenu, fItem).command;
                Close();
            }
            break;
        default:
            break;
    }
    command = _Choose(command);
    return true;
}


menu_command
MenuBar::Shortcut(SDL_Keycode key, uint16 modifiers)
{
    if (fMenu >= 0 || key < SDLK_a || key > SDLK_z)
        return MENU_NONE;
    const bool alt = (modifiers & KMOD_ALT) != 0;
    const bool other = (modifiers & (KMOD_CTRL | KMOD_GUI)) != 0;
    if (other)
        return MENU_NONE;
    const char letter = char('A' + (key - SDLK_a));
    for (int menu = 0; menu < kMenuCount; menu++) {
        for (int item = 0; item < kMenus[menu].count; item++) {
            const menu_item_definition& definition = _Item(menu, item);
            if (definition.key != letter || definition.alt != alt
                    || definition.subMenu || !_Usable(menu, item))
                continue;
            return _Choose(definition.command);
        }
    }
    return MENU_NONE;
}


/* static */
bool
MenuBar::Opens(const SDL_Event& event)
{
    return (event.type == SDL_MOUSEBUTTONDOWN
            && event.button.button == SDL_BUTTON_RIGHT)
        || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F10);
}


bool
MenuBar::Handle(GameWindow& window, const SDL_Event& event,
    const std::function<Bitmap*()>& draw, bool shortcuts, menu_command& command)
{
    command = MENU_NONE;
    if (Opens(event)) {
        command = Run(window, draw(), event);
        return true;
    }
    if (shortcuts && event.type == SDL_KEYDOWN
            && (event.key.keysym.mod & KMOD_ALT) != 0) {
        command = Shortcut(event.key.keysym.sym, event.key.keysym.mod);
        return command != MENU_NONE;
    }
    return false;
}


/* static */
void
MenuBar::PostQuit()
{
    SDL_Event quit;
    SDL_zero(quit);
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);
}


/* static */
void
MenuBar::Pause(GameWindow& window)
{
    (void)window;
    for (;;) {
        SDL_Event event;
        if (SDL_WaitEvent(&event) == 0)
            return;
        if (event.type == SDL_QUIT) {
            SDL_PushEvent(&event);		// the view quits too
            return;
        }
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN)
            return;
    }
}


menu_command
MenuBar::Run(GameWindow& window, Bitmap* screen, const SDL_Event& event)
{
    if (!Opens(event))
        return MENU_NONE;
    const size_t size = size_t(screen->Pitch()) * screen->Height();
    const std::vector<uint8> saved(static_cast<uint8*>(screen->Pixels()),
        static_cast<uint8*>(screen->Pixels()) + size);
    GFX::Palette palette;
    screen->GetPalette(palette);
    const uint8 black = NearestColor(palette, 0, 0, 0);
    const uint8 white = NearestColor(palette, 255, 255, 255);

    GFX::point mouse(0, 0);
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        mouse = GameWindow::ToScreen(event.button.x, event.button.y);
        const int title = _TitleAt(mouse);
        Open(title >= 0 ? title : 0, true);
        MouseMoved(mouse);
    } else {
        Open(0, false);
    }

    menu_command command = MENU_NONE;
    while (IsOpen()) {
        std::memcpy(screen->Pixels(), saved.data(), size);
        Draw(screen);
        DrawMouseCursor(screen, mouse, black, white);
        window.Show(screen);
        SDL_Event next;
        if (SDL_WaitEventTimeout(&next, 50) == 0)
            continue;
        switch (next.type) {
            case SDL_QUIT:
                SDL_PushEvent(&next);	// the view quits too
                Close();
                break;
            case SDL_MOUSEMOTION:
                mouse = GameWindow::ToScreen(next.motion.x, next.motion.y);
                MouseMoved(mouse);
                break;
            case SDL_MOUSEBUTTONUP:
                mouse = GameWindow::ToScreen(next.button.x, next.button.y);
                if (next.button.button == SDL_BUTTON_RIGHT)
                    command = Release(mouse);
                else if (next.button.button == SDL_BUTTON_LEFT)
                    command = Clicked(mouse);
                break;
            case SDL_KEYDOWN:
                KeyDown(next.key.keysym.sym, next.key.keysym.mod, command);
                break;
            default:
                break;
        }
    }
    std::memcpy(screen->Pixels(), saved.data(), size);
    return command;
}


void
MenuBar::Draw(Bitmap* screen) const
{
    if (fMenu < 0)
        return;
    GFX::Palette palette;
    screen->GetPalette(palette);
    const uint8 background = NearestColor(palette, kBackgroundRGB.r,
        kBackgroundRGB.g, kBackgroundRGB.b);
    const uint8 border = NearestColor(palette, kBorderRGB.r, kBorderRGB.g,
        kBorderRGB.b);
    const uint8 text = NearestColor(palette, kTextRGB.r, kTextRGB.g,
        kTextRGB.b);
    const uint8 dim = NearestColor(palette, kDimRGB.r, kDimRGB.g, kDimRGB.b);
    const uint8 highlight = NearestColor(palette, kHighlightRGB.r,
        kHighlightRGB.g, kHighlightRGB.b);
    const uint8 marked = NearestColor(palette, kMarkedRGB.r, kMarkedRGB.g,
        kMarkedRGB.b);

    // the bar and its titles
    screen->FillRect(GFX::rect(kLeft, 0, kScreenWidth - kLeft, kHeight),
        background);
    screen->FillRect(GFX::rect(kLeft, kHeight - 1, kScreenWidth - kLeft, 1),
        border);
    for (int menu = 0; menu < kMenuCount; menu++) {
        const GFX::rect box = TitleRect(menu);
        bool usable = false;
        for (int item = 0; item < kMenus[menu].count; item++)
            usable = usable || _Usable(menu, item);
        uint8 color = usable ? text : dim;
        if (menu == fMenu) {
            screen->FillRect(GFX::rect(box.x, box.y, box.w, box.h - 1),
                highlight);
            color = marked;
        }
        fFont->RenderString(kMenus[menu].title, screen,
            GFX::point(fTitleLeft[size_t(menu)], kTextTop), color);
    }

    // the open pull-down menu
    int shortcutLeft = 0;
    _ColumnWidth(fMenu, &shortcutLeft);
    const GFX::rect box = _MenuRect(fMenu);
    screen->FillRect(box, border);
    screen->FillRect(GFX::rect(box.x + 1, box.y + 1, box.w - 2, box.h - 2),
        background);
    for (int item = 0; item < kMenus[fMenu].count; item++) {
        const GFX::rect row = ItemRect(fMenu, item);
        const bool selected = item == fItem && !(fSubOpen && fSubItem >= 0);
        uint8 color = _Usable(fMenu, item) ? text : dim;
        if (selected) {
            screen->FillRect(row, highlight);
            color = marked;
        }
        fFont->RenderString(ItemText(fMenu, item), screen,
            GFX::point(row.x + 1, row.y + kTextTop), color);
        fFont->RenderString(ItemShortcut(fMenu, item), screen,
            GFX::point(row.x + 1 + shortcutLeft, row.y + kTextTop), color);
        if (_Item(fMenu, item).subMenu) {
            fFont->RenderString(">", screen,
                GFX::point(row.x + row.w - 7, row.y + kTextTop), color);
        }
    }

    // the Difficulty sub-menu
    if (fSubOpen) {
        const GFX::rect sub = _SubRect();
        screen->FillRect(sub, border);
        screen->FillRect(GFX::rect(sub.x + 1, sub.y + 1, sub.w - 2, sub.h - 2),
            background);
        for (int item = 0; item < 3; item++) {
            const menu_command command = kDifficultyItems[item].command;
            const GFX::rect row(sub.x + 1, sub.y + 1 + item * kRowHeight,
                sub.w - 2, kRowHeight);
            uint8 color = fEnabled[command] ? text : dim;
            if (item == fSubItem) {
                screen->FillRect(row, highlight);
                color = marked;
            }
            fFont->RenderString(std::string(IsChecked(command) ? CHECKED
                : UNCHECKED) + kDifficultyItems[item].text, screen,
                GFX::point(row.x + 1, row.y + kTextTop), color);
        }
    }
}


MenuLimits::MenuLimits(MenuBar* bar, std::initializer_list<menu_command> commands)
    :
    fBar(bar),
    fCommands(commands)
{
    if (fBar == NULL)
        return;
    for (size_t i = 0; i < fCommands.size(); i++)
        fBar->SetEnabled(fCommands[i], false);
}


MenuLimits::~MenuLimits()
{
    if (fBar == NULL)
        return;
    for (size_t i = 0; i < fCommands.size(); i++)
        fBar->SetEnabled(fCommands[i], true);
}
