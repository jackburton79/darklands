#include "CardView.h"

#include "Bitmap.h"
#include "Character.h"
#include "FileStream.h"
#include "GameData.h"
#include "MsgFile.h"
#include "PICImage.h"
#include "Palette.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <SDL.h>

#include <algorithm>

const uint16 CardView::kScreenWidth;
const uint16 CardView::kScreenHeight;

// Fonts of FONTS.FNT: the card text (verified on the manual's
// screenshot: same glyphs and line widths), the sidebar numbers
static const uint32 kTextFontIndex	= 2;
static const uint32 kNumberFontIndex = 0;

// Screen layout: the party sidebar on the left, the card on the right.
// The frame is 7 + 245 + 8 pixels wide, the width of its pictures.
static const int kCardLeft			= 60;
static const int kSidebarLeft		= 3;	// SIDEBAR.PIC, 54 x 198
static const int kSidebarTop		= 1;

// Character boxes in the sidebar, one per party member (measured on the
// manual's screenshots, p. 17 and 28, about +-1 pixel): the nickname,
// the character's picture (<image>STAT.PIC, 10 x 19) and three bars with
// their values beneath: endurance, strength, divine favor
static const int kBoxTop			= 1;
static const int kBoxHeight			= 39;
static const int kNameLeft			= 5;
static const int kPictureLeft		= 5;
static const int kPictureTop		= 9;	// relative to the box
static const int kBarsTop			= 10;
static const int kBarHeight			= 16;
static const int kBarWidth			= 2;
static const int kNumbersTop		= 28;
static const int kBarCenters[3]		= { 23, 40, 53 };

// Card text position relative to the header values (measured on the
// manual's screenshot, about +-1 pixel)
static const int kTextOffsetX		= -2;
static const int kTextOffsetY		= 1;
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
static const uint8 kSidebarColor	= 159;
static const uint8 kNameColor		= 15;	// EGA white
static const uint8 kLeaderColor		= 14;	// EGA yellow
static const uint8 kNumberColor		= 15;
static const uint8 kBarTrackColor	= 8;	// EGA dark gray
static const uint8 kBarColors[3]	= { 12, 10, 15 };	// light red, light
                                                        // green, white

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
    fShowingScene(false),
    fParty(NULL),
    fCapital(0),
    fCapitalPosition(0, 0),
    fSelected(-1),
    fMouse(0, 0),
    fCursorVisible(false)
{
    fFont.reset(new Font(fData.Fonts(), kTextFontIndex));
    fNumberFont.reset(new Font(fData.Fonts(), kNumberFontIndex));

    // card palette: the EGA colors (0..15), the capitals' range, paper
    fCardPalette = PICImage::EGAPalette();
    fCapitals = _LoadPicture("ILLMCAPS.PIC", &fCardPalette);
    fCardPalette.colors[kPaperColor] = kPaperRGB;
    fPalette = fCardPalette;

    for (int i = 0; i < 4; i++)
        fBorders[i] = _LoadPicture(kBorderNames[i]);
    fSidebar = _LoadPicture("SIDEBAR.PIC");

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
    const std::vector<int>& hidden)
{
    _Layout(card, Normalize(Substitute(card.text, variables)), hidden);
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
    fParty = members;
    fPictures.clear();
    if (fParty == NULL)
        return;
    for (const std::string& image : fParty->images) {
        if (fPictures.find(image) == fPictures.end())
            fPictures[image] = _LoadPicture(image + "STAT.PIC");
    }
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
        switch (event.type) {
            case SDL_QUIT:
                return -1;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return -1;
                    case SDLK_UP:
                        SelectPrevious();
                        break;
                    case SDLK_DOWN:
                        SelectNext();
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    case SDLK_SPACE:
                        chosen = Choose();
                        break;
                    default:
                        if (fShowingScene || fOptions.empty())
                            chosen = Choose();
                        break;
                }
                dirty = true;
                break;
            case SDL_MOUSEMOTION:
                MouseMoved(GameWindow::ToScreen(event.motion.x,
                    event.motion.y));
                dirty = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    chosen = Clicked(GameWindow::ToScreen(event.button.x,
                        event.button.y));
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
    const std::vector<int>& hidden)
{
    fLines.clear();
    fOptions.clear();
    fCapital = 0;

    const int interiorLeft = kCardLeft + fBorders[BORDER_LEFT].width;
    const int interiorTop = fBorders[BORDER_TOP].height;
    const int left = interiorLeft + card.textLeft + kTextOffsetX;
    const int right = interiorLeft + card.textRight + kTextOffsetX;
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
        if (option) {
            const size_t textStart = line.find(char(MSG_CODE_OPTION_TEXT), i);
            if (textStart != std::string::npos) {
                prefix = line.substr(i + 1, textStart - i - 1);
                i = textStart + 1;
            } else
                i++;
        }
        std::string rest = line.substr(i);
        while (!rest.empty() && rest[rest.size() - 1] == ' ')
            rest.erase(rest.size() - 1);
        if (option) {
            optionNumber++;
            if (std::find(hidden.begin(), hidden.end(), optionNumber)
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
            fLines.push_back(text_line{ left, y, prefix });
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
            fLines.push_back(text_line{ lineX, y, part });
            y += lineHeight;
            first = false;
        } while (!rest.empty());
        if (option)
            fOptions.push_back(option_area{ optionNumber, top - 1, y - kLineGap });
    }
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
    for (const text_line& line : fLines) {
        fFont->RenderString(line.text, fBuffer, GFX::point(line.x, line.y),
            kTextColor);
    }
}


void
CardView::_DrawSidebar()
{
    fBuffer->FillRect(GFX::rect(0, 0, kCardLeft, kScreenHeight), kSidebarColor);
    _DrawPicture(fSidebar, kSidebarLeft, kSidebarTop);
    if (fParty == NULL)
        return;

    static const int kBarAttributes[3] = { ATTRIBUTE_ENDURANCE,
        ATTRIBUTE_STRENGTH, ATTRIBUTE_DIVINE_FAVOR };
    for (size_t i = 0; i < fParty->members.size() && i < size_t(kMaxPartySize);
            i++) {
        const character& member = fParty->members[i];
        const int top = kBoxTop + int(i) * kBoxHeight;
        fFont->RenderString(Font::ToGameCharset(member.shortName), fBuffer,
            GFX::point(kNameLeft, top), int(i) == fParty->leader
                ? kLeaderColor : kNameColor, kCardLeft - kNameLeft);

        const std::map<std::string, raw_picture>::const_iterator picture
            = fPictures.find(fParty->images[i]);
        if (picture != fPictures.end())
            _DrawPicture(picture->second, kPictureLeft, top + kPictureTop, 0);

        // bars: the current value as a share of the maximum
        for (int bar = 0; bar < 3; bar++) {
            const int attribute = kBarAttributes[bar];
            const int value = member.attributes[attribute];
            const int maximum = std::max<int>(member.maxAttributes[attribute], 1);
            const int height = std::min(kBarHeight, value * kBarHeight / maximum);
            const int x = kBarCenters[bar] - kBarWidth / 2;
            const int bottom = top + kBarsTop + kBarHeight;
            fBuffer->FillRect(GFX::rect(x, top + kBarsTop, kBarWidth,
                kBarHeight), kBarTrackColor);
            if (height > 0) {
                fBuffer->FillRect(GFX::rect(x, bottom - height, kBarWidth,
                    height), kBarColors[bar]);
            }
            const std::string number = std::to_string(value);
            const int width = fNumberFont->StringWidth(number);
            fNumberFont->RenderString(number, fBuffer,
                GFX::point(kBarCenters[bar] - width / 2, top + kNumbersTop),
                kNumberColor);
        }
    }
}
