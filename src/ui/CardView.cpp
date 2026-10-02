#include "CardView.h"

#include "Bitmap.h"
#include "InfoView.h"
#include "MenuBar.h"
#include "FileStream.h"
#include "GameData.h"
#include "GameSettings.h"
#include "MsgFile.h"
#include "PICImage.h"
#include "Palette.h"
#include "PartySidebar.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <SDL.h>

#include <algorithm>

const uint16 CardView::kScreenWidth;
const uint16 CardView::kScreenHeight;

// Font of the card text: index into FONTS.FNT (verified on the manual's
// screenshot: same glyphs and line widths)
static const int kNoResult			= -100;	// Run() goes on
static const uint32 kTextFontIndex	= 2;

// Screen layout: the party sidebar on the left, the card on the right.
// The frame is 7 + 245 + 8 pixels wide, the width of its pictures.
static const int kCardLeft			= 60;
// Card text position relative to the header values (measured on the
// manual's screenshot, about +-1 pixel)
static const int kTextOffsetX		= -2;
static const int kTextOffsetY		= 1;
static const int kTextWidthMargin	= 10;	// header's third value less this is the width
static const int kLineGap			= 1;	// between lines of a paragraph
static const int kParagraphGap		= 2;	// extra, per 0x14 code
static const int kOptionTextGap		= 1;	// after the "..." of an option

// Illuminated capitals (ILLMCAPS.PIC): A..Z in 20 x 20 cells, 21 pixels
// apart, 15 per row. The first line of text is next to the capital,
// aligned with its bottom.
static const int kCapitalSize		= 20;
static const int kCapitalPitch		= 21;
static const int kCapitalsPerRow	= 15;
static const int kCapitalGap		= 2;

// Colors. Index 255 is the paper: the blank background of the scene
// pictures, (63, 57, 54) in 356 of the 389 palettes that set it, and the
// border dots. The rest is inferred from the manual's screenshots, which
// are almost black and white: text in the darkest brown of the card
// range 128..159, a dark capital box (its lattice, index 5, the EGA
// magenta, prints as dark as its background), white sidebar text and
// divine favor bar, darker endurance and strength bars.
static const uint8 kPaperColor		= 255;
static const GFX::Color kPaperRGB	= { 252, 228, 216, 0 };	// (63, 57, 54)
static const uint8 kTextColor		= 137;
static const uint8 kHighlightColor	= 140;
static const uint8 kDimColor		= 8;	// EGA dark gray: the Extras' dim options
static const uint8 kSidebarColor	= 159;

// Under the text, the scene is faded toward the paper, as on the
// manual's screenshot (p. 17), where it is barely visible behind the
// text: the share of paper in its colors (inferred)
static const int kSceneFadePercent	= 70;


static const char*
kBorderNames[4] = {
    "RPBDRTOP.PIC", "RPBDRBTM.PIC", "RPBDRLFT.PIC", "RPBDRRGT.PIC"
};

enum { BORDER_TOP = 0, BORDER_BOTTOM, BORDER_LEFT, BORDER_RIGHT };


static bool
IsOptionCode(uint8 c)
{
    return c == MSG_CODE_OPTION || c == MSG_CODE_SAINT_OPTION
        || c == MSG_CODE_POTION_OPTION || c == MSG_CODE_BATTLE_OPTION;
}


// Options the game fills in or hides: without the "..." prefix ("1 not
// available", "5"), empty, just a number ("...3"), or saying so
static bool
IsPlaceholderOption(bool hasPrefix, const std::string& text)
{
    if (!hasPrefix || text.empty()
            || text.find("option should be hidden") != std::string::npos)
        return true;
    return text.find_first_not_of("0123456789 ") == std::string::npos;
}


static bool
IsNameCharacter(char c, bool digits)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
        || (digits && c >= '0' && c <= '9');
}


// Replaces the $Name variables that have a value.
static std::string
Substitute(const std::string& text, const card_variables& variables)
{
    std::string result;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '$') {
            result += text[i++];
            continue;
        }
        // a name is letters, then digits ($Money1, $Support3)
        size_t end = i + 1;
        while (end < text.size() && IsNameCharacter(text[end], false))
            end++;
        while (end < text.size() && IsNameCharacter(text[end], true))
            end++;
        const card_variables::const_iterator value
            = variables.find(text.substr(i + 1, end - i - 1));
        if (value != variables.end())
            result += Font::ToGameCharset(value->second);
        else
            result += text.substr(i, end - i);
        i = end;
    }
    return result;
}


// Removes the codes whose meaning is unknown (0x13, 0x01: presumably
// conditional sentences, all shown for now) and starts a new line at
// each paragraph code that is not already at the start of one.
static std::string
Normalize(const std::string& text)
{
    std::string result;
    for (const char c : text) {
        if (c == 0x13 || c == 0x01)
            continue;
        if (c == MSG_CODE_PARAGRAPH && !result.empty()
                && result[result.size() - 1] != MSG_CODE_NEWLINE
                && result[result.size() - 1] != MSG_CODE_PARAGRAPH)
            result += char(MSG_CODE_NEWLINE);
        result += c;
    }
    return result;
}


// #pragma mark - CardView


CardView::CardView(GameData& data)
    :
    fData(data),
    fBuffer(NULL),
    fInfo(NULL),
    fMenu(NULL),
    fSettings(NULL),
    fShowingScene(false),
    fCapital(0),
    fCapitalPosition(0, 0),
    fSelected(-1),
    fMouse(0, 0),
    fCursorVisible(false),
    fTextLeft(0),
    fTextBottom(0),
    fPromptLength(0),
    fPromptDigits(true)
{
    fFont.reset(new Font(fData.Fonts(), kTextFontIndex));
    fSidebar.reset(new PartySidebar(fData));

    // card palette: the EGA colors (0..15), the capitals' range, paper
    fCardPalette = PICImage::EGAPalette();
    fCapitals = _LoadPicture("ILLMCAPS.PIC", &fCardPalette);
    fCardPalette.colors[kPaperColor] = kPaperRGB;
    fPalette = fCardPalette;

    for (int i = 0; i < 4; i++)
        fBorders[i] = _LoadPicture(kBorderNames[i]);

    fScene.width = fScene.height = 0;
    fBuffer = new Bitmap(kScreenWidth, kScreenHeight, 8);
}


CardView::~CardView()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


void
CardView::SetCard(const msg_card& card, const card_variables& variables,
    const std::vector<int>& hidden, const std::vector<int>& disabled)
{
    fPrompt.clear();
    fPromptText.clear();
    _Layout(card, Normalize(Substitute(card.text, variables)), hidden,
        disabled);
    fSelected = fOptions.empty() ? -1 : 0;
    // the option under the mouse, if it is on the window
    if (fCursorVisible)
        MouseMoved(fMouse);
}


void
CardView::SetScene(const std::string& pictureName, bool showFirst)
{
    fScene.width = fScene.height = 0;
    fScene.pixels.clear();
    fPalette = fScenePalette = fCardPalette;
    fShowingScene = false;
    if (pictureName.empty())
        return;
    // the scene sets 16..255; the card range is put back. Under the card
    // its colors are faded.
    GFX::Palette palette = fCardPalette;
    fScene = _LoadPicture(pictureName, &palette);
    for (int i = 128; i < 160; i++)
        palette.colors[i] = fCardPalette.colors[i];
    fScenePalette = palette;
    fShowingScene = showFirst;
    for (int i = 16; i < 256; i++) {
        if (i >= 128 && i < 160)
            continue;
        GFX::Color& color = palette.colors[i];
        color.r = uint8(color.r + (kPaperRGB.r - color.r) * kSceneFadePercent / 100);
        color.g = uint8(color.g + (kPaperRGB.g - color.g) * kSceneFadePercent / 100);
        color.b = uint8(color.b + (kPaperRGB.b - color.b) * kSceneFadePercent / 100);
    }
    fPalette = palette;
}


void
CardView::SetParty(const party* members)
{
    fSidebar->SetParty(members);
}


int
CardView::Run()
{
    GameWindow window("Darklands");
    return Run(window);
}


int
CardView::Run(GameWindow& window)
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
        int chosen = -1;
        int menuResult;
        if (_MenuEvent(window, event, menuResult)) {
            if (menuResult != kNoResult)
                return menuResult;
            dirty = true;
            continue;
        }
        switch (event.type) {
            case SDL_QUIT:
                return -1;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_s
                        && (event.key.keysym.mod & KMOD_CTRL) != 0)
                    return kSaveRequested;
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return -1;
                    case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4:
                    case SDLK_F5:
                        if (fInfo != NULL)
                            fInfo->Run(window, int(event.key.keysym.sym - SDLK_F1));
                        break;
                    case SDLK_F6:
                        if (fInfo != NULL)
                            fInfo->Run(window, InfoView::kPartyPage);
                        break;
                    case SDLK_BACKSPACE:
                        Backspace();
                        break;
                    case SDLK_UP:
                        SelectPrevious();
                        break;
                    case SDLK_DOWN:
                        SelectNext();
                        break;
                    case SDLK_SPACE:
                        if (Prompting() && !fPromptDigits)
                            break;				// typed, as SDL_TEXTINPUT
                        chosen = Choose();
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        chosen = Choose();
                        break;
                    default: {
                        const SDL_Keycode key = event.key.keysym.sym;
                        if (Prompting() && !fPromptDigits) {
                            // the text comes as SDL_TEXTINPUT
                        } else if (Prompting()) {
                            if (key >= SDLK_0 && key <= SDLK_9)
                                TypeCharacter(char('0' + key - SDLK_0));
                            else if (key >= SDLK_KP_1 && key <= SDLK_KP_9)
                                TypeCharacter(char('1' + key - SDLK_KP_1));
                            else if (key == SDLK_KP_0)
                                TypeCharacter('0');
                        } else if (fShowingScene || fOptions.empty())
                            chosen = Choose();
                        break;
                    }
                }
                dirty = true;
                break;
            case SDL_TEXTINPUT:
                if (Prompting() && !fPromptDigits) {
                    TypeText(event.text.text);
                    dirty = true;
                }
                break;
            case SDL_MOUSEMOTION:
                MouseMoved(GameWindow::ToScreen(event.motion.x,
                    event.motion.y));
                dirty = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    const GFX::point point = GameWindow::ToScreen(
                        event.button.x, event.button.y);
                    const int member = fSidebar->MemberAt(point);
                    if (member >= 0 && fInfo != NULL)
                        fInfo->Run(window, member);
                    else
                        chosen = Clicked(point);
                    dirty = true;
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                    MouseLeft();
                dirty = true;
                break;
            default:
                break;
        }
        if (chosen >= 0)
            return chosen;
    }
}


// The menu bar's events (the right button, F10, the shortcuts): true if
// it took the event; `result` is then what Run() returns, or kNoResult
bool
CardView::_MenuEvent(GameWindow& window, const SDL_Event& event, int& result)
{
    result = kNoResult;
    if (fMenu == NULL)
        return false;
    menu_command command;
    if (!fMenu->Handle(window, event, [this]() { return Draw(); },
            !Prompting(), command))
        return false;
    switch (command) {
        case MENU_SAVE_GAME:
            result = kSaveRequested;
            break;
        case MENU_LOAD_GAME:
            result = kLoadRequested;
            break;
        case MENU_MARCHING_ORDER:
            result = kOrderRequested;
            break;
        case MENU_QUIT:
            result = -1;
            break;
        case MENU_PARTY_INFO:
            if (fInfo != NULL)
                fInfo->Run(window, InfoView::kPartyPage);
            break;
        case MENU_PAUSE:
            MenuBar::Pause(window);
            break;
        default:
            break;
    }
    return true;
}


void
CardView::MouseMoved(const GFX::point& point)
{
    fMouse = point;
    fCursorVisible = point.x >= 0 && point.y >= 0 && point.x < kScreenWidth
        && point.y < kScreenHeight;
    const int option = _OptionAt(point);
    if (option >= 0)
        fSelected = option;
}


int
CardView::SelectedOption() const
{
    return fSelected >= 0 ? fOptions[fSelected].number : -1;
}


void
CardView::MouseLeft()
{
    fCursorVisible = false;
}


int
CardView::Clicked(const GFX::point& point)
{
    MouseMoved(point);
    if (fShowingScene) {
        fShowingScene = false;
        return -1;
    }
    if (fOptions.empty())
        return point.x >= kCardLeft ? 0 : -1;
    const int option = _OptionAt(point);
    return option >= 0 ? fOptions[option].number : -1;
}


void
CardView::SelectNext()
{
    if (!fOptions.empty())
        fSelected = (fSelected + 1) % int(fOptions.size());
}


void
CardView::SelectPrevious()
{
    if (!fOptions.empty())
        fSelected = (fSelected + int(fOptions.size()) - 1) % int(fOptions.size());
}


void
CardView::SetPrompt(const std::string& prompt, const std::string& text,
    size_t maxLength, bool digitsOnly)
{
    fPrompt = prompt;
    fPromptLength = maxLength;
    fPromptDigits = digitsOnly;
    fPromptText.clear();
    TypeText(text);
}


void
CardView::TypeCharacter(char c)
{
    if (!Prompting() || fPromptText.size() >= fPromptLength)
        return;
    if (fPromptDigits ? (c >= '0' && c <= '9') : uint8(c) >= 0x1F)
        fPromptText += c;
}


// The characters that stand for letters in the game's character set are
// not typed as themselves
void
CardView::TypeText(const std::string& utf8)
{
    std::string text;
    for (char c : utf8) {
        if (std::string("[\\]_{|}~").find(c) == std::string::npos)
            text += c;
    }
    for (char c : Font::ToGameCharset(text))
        TypeCharacter(c);
}


void
CardView::Backspace()
{
    if (!fPromptText.empty())
        fPromptText.erase(fPromptText.size() - 1);
}


int
CardView::Choose()
{
    if (fShowingScene) {
        fShowingScene = false;
        return -1;
    }
    if (fOptions.empty())
        return 0;
    return SelectedOption();
}


Bitmap*
CardView::Draw()
{
    const GFX::Palette& palette = fShowingScene ? fScenePalette : fPalette;
    fBuffer->SetColors(palette.colors, 0, 256);
    fBuffer->Clear(kSidebarColor);
    _DrawFrame();
    if (!fShowingScene)
        _DrawCard();
    _DrawSidebar();
    if (fCursorVisible) {
        DrawMouseCursor(fBuffer, fMouse, NearestColor(palette, 0, 0, 0),
            NearestColor(palette, 255, 255, 255));
    }
    return fBuffer;
}


CardView::raw_picture
CardView::_LoadPicture(const std::string& name, GFX::Palette* palette) const
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
CardView::_DrawPicture(const raw_picture& picture, int x, int y,
    int transparent, int height)
{
    if (height < 0 || height > picture.height)
        height = picture.height;
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < picture.width; column++) {
            const uint8 pixel = picture.pixels[size_t(row) * picture.width
                + column];
            if (pixel != transparent)
                fBuffer->PutPixel(x + column, y + row, pixel);
        }
    }
}


// Splits the text into screen lines, and finds the options.
void
CardView::_Layout(const msg_card& card, const std::string& text,
    const std::vector<int>& hidden, const std::vector<int>& disabled)
{
    fLines.clear();
    fOptions.clear();
    fCapital = 0;

    const int interiorLeft = kCardLeft + fBorders[BORDER_LEFT].width;
    const int interiorTop = fBorders[BORDER_TOP].height;
    const int left = interiorLeft + card.textLeft + kTextOffsetX;
    // the header's third value is the text's width plus 10, not its right
    // edge (DARKLAND.EXE hands the text routine textRight - 10 as the
    // width, textLeft + 5 as the x: file 0x8D110 + 0x587); the standard
    // cards (10, 240) are the same either way
    const int right = left + card.textRight - kTextWidthMargin;
    const int lineHeight = fFont->Height() + kLineGap;
    int y = interiorTop + card.textTop + kTextOffsetY;
    int optionNumber = -1;

    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(char(MSG_CODE_NEWLINE), start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;

        size_t i = 0;
        while (i < line.size() && line[i] == MSG_CODE_PARAGRAPH)
            i++;
        const int gap = int(i) * kParagraphGap;
        const bool option = i < line.size() && IsOptionCode(uint8(line[i]));
        std::string prefix;
        bool hasPrefix = false;
        if (option) {
            const size_t textStart = line.find(char(MSG_CODE_OPTION_TEXT), i);
            if (textStart != std::string::npos) {
                prefix = line.substr(i + 1, textStart - i - 1);
                i = textStart + 1;
                hasPrefix = true;
            } else
                i++;
        }
        std::string rest = line.substr(i);
        while (!rest.empty() && rest[rest.size() - 1] == ' ')
            rest.erase(rest.size() - 1);
        bool disabledOption = false;
        if (option) {
            optionNumber++;
            disabledOption = std::find(disabled.begin(), disabled.end(),
                optionNumber) != disabled.end();
            if (IsPlaceholderOption(hasPrefix, rest)
                    || std::find(hidden.begin(), hidden.end(), optionNumber)
                        != hidden.end())
                continue;
        }
        y += gap;
        if (!option && rest.empty()) {
            // an empty line; the one after the last newline is not a line
            if (start < text.size())
                y += lineHeight;
            continue;
        }

        const int top = y;
        int x = left;
        if (option) {
            fLines.push_back(text_line{ left, y, prefix, disabledOption });
            x = left + fFont->StringWidth(prefix) + kOptionTextGap;
        }
        int firstX = x;
        if (!option && fLines.empty() && rest[0] >= 'A' && rest[0] <= 'Z') {
            fCapital = uint8(rest[0]);
            fCapitalPosition = GFX::point(left, y);
            rest.erase(0, 1);
            firstX = left + kCapitalSize + kCapitalGap;
            y += kCapitalSize - fFont->Height();
        }
        bool first = true;
        do {
            const int lineX = first ? firstX : x;
            const std::string part = fFont->TruncateString(rest,
                uint16(std::max(right - lineX, 1)));
            fLines.push_back(text_line{ lineX, y, part, disabledOption });
            y += lineHeight;
            first = false;
        } while (!rest.empty());
        if (option && !disabledOption)
            fOptions.push_back(option_area{ optionNumber, top - 1, y - kLineGap });
    }
    fTextLeft = left;
    fTextBottom = y;
}


int
CardView::_OptionAt(const GFX::point& point) const
{
    if (fShowingScene || point.x < kCardLeft)
        return -1;
    for (size_t i = 0; i < fOptions.size(); i++) {
        if (point.y >= fOptions[i].top && point.y < fOptions[i].bottom)
            return int(i);
    }
    return -1;
}


void
CardView::_DrawFrame()
{
    const raw_picture& top = fBorders[BORDER_TOP];
    const raw_picture& bottom = fBorders[BORDER_BOTTOM];
    const raw_picture& leftBorder = fBorders[BORDER_LEFT];
    const int interiorLeft = kCardLeft + leftBorder.width;
    const int interiorBottom = kScreenHeight - bottom.height;
    fBuffer->FillRect(GFX::rect(interiorLeft, top.height, top.width,
        interiorBottom - top.height), kPaperColor);
    // the scene is painted on the paper, under the text: its pictures
    // are blank (paper) outside the card's interior
    if (fScene.width > 0) {
        const int right = std::min<int>(interiorLeft + top.width, fScene.width);
        const int bottomRow = std::min<int>(interiorBottom, fScene.height);
        for (int y = top.height; y < bottomRow; y++) {
            for (int x = interiorLeft; x < right; x++)
                fBuffer->PutPixel(x, y, fScene.pixels[size_t(y) * fScene.width + x]);
        }
    }
    _DrawPicture(top, interiorLeft, 0);
    _DrawPicture(bottom, interiorLeft, kScreenHeight - bottom.height);
    _DrawPicture(leftBorder, kCardLeft, 0);
    _DrawPicture(fBorders[BORDER_RIGHT], interiorLeft + top.width, 0);
}


void
CardView::_DrawCard()
{
    const int interiorLeft = kCardLeft + fBorders[BORDER_LEFT].width;
    if (fSelected >= 0 && fSelected < int(fOptions.size())) {
        const option_area& area = fOptions[fSelected];
        fBuffer->FillRect(GFX::rect(interiorLeft + 1, area.top,
            fBorders[BORDER_TOP].width - 2, area.bottom - area.top),
            kHighlightColor);
    }
    if (fCapital != 0) {
        const int index = fCapital - 'A';
        const int cellX = (index % kCapitalsPerRow) * kCapitalPitch;
        const int cellY = (index / kCapitalsPerRow) * kCapitalPitch;
        for (int y = 0; y < kCapitalSize; y++) {
            for (int x = 0; x < kCapitalSize; x++) {
                fBuffer->PutPixel(fCapitalPosition.x + x,
                    fCapitalPosition.y + y, fCapitals.pixels[
                        size_t(cellY + y) * fCapitals.width + cellX + x]);
            }
        }
    }
    const bool dim = fSettings != NULL && fSettings->extras;
    for (const text_line& line : fLines) {
        fFont->RenderString(line.text, fBuffer, GFX::point(line.x, line.y),
            dim && line.disabled ? kDimColor : kTextColor);
    }
    if (Prompting()) {
        // under the text, with a cursor (the game's look is not known)
        // ('_' is a letter in the game's character set: draw the cursor)
        const std::string line = Font::ToGameCharset(fPrompt) + " "
            + fPromptText;
        const int y = fTextBottom + fFont->Height();
        fFont->RenderString(line, fBuffer, GFX::point(fTextLeft, y),
            kTextColor);
        const int cursorX = fTextLeft + fFont->StringWidth(line) + 1;
        fBuffer->FillRect(GFX::rect(cursorX, y + fFont->Height() - 1, 5, 1),
            kTextColor);
    }
}


void
CardView::_DrawSidebar()
{
    fSidebar->Draw(fBuffer, true);
}
