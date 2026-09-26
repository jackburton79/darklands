#include "CardView.h"

#include "Bitmap.h"
#include "CityFile.h"
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

// Font of the card text: index into FONTS.FNT (verified on the manual's
// screenshot: same glyphs and line widths)
static const uint32 kTextFontIndex	= 2;

// Screen layout: the party sidebar on the left, the card on the right.
// The frame is 7 + 245 + 8 pixels wide, the width of its pictures.
static const int kCardLeft			= 60;
static const int kSidebarLeft		= 3;	// SIDEBAR.PIC, 54 x 198
static const int kSidebarTop		= 1;

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

// Colors (inferred from the manual's screenshot, which is almost black
// and white): white paper and border dots (index 255), text in the
// darkest brown of the card range 128..159, a dark capital box: its
// lattice (index 5, the EGA magenta) is as dark as its background
static const uint8 kWhite			= 255;
static const uint8 kPaperColor		= kWhite;
static const uint8 kTextColor		= 137;
static const uint8 kHighlightColor	= 140;
static const uint8 kSidebarColor	= 159;


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
    fCapital(0),
    fCapitalPosition(0, 0),
    fSelected(-1),
    fMouse(0, 0),
    fCursorVisible(false)
{
    fFont.reset(new Font(fData.Fonts(), kTextFontIndex));

    // card palette: the EGA colors (0..15), the capitals' range, white
    fCardPalette = PICImage::EGAPalette();
    fCapitals = _LoadPicture("ILLMCAPS.PIC", &fCardPalette);
    fCardPalette.colors[kWhite] = GFX::Color{ 255, 255, 255, 0 };
    fScenePalette = fCardPalette;

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
CardView::SetCard(const msg_card& card, const card_variables& variables)
{
    _Layout(card, Normalize(Substitute(card.text, variables)));
    fSelected = fOptions.empty() ? -1 : 0;
}


void
CardView::SetScene(const std::string& pictureName)
{
    fScene.width = fScene.height = 0;
    fScene.pixels.clear();
    fShowingScene = false;
    if (pictureName.empty())
        return;
    // the scene sets 16..255; the card range is put back for the sidebar
    GFX::Palette palette = fCardPalette;
    fScene = _LoadPicture(pictureName, &palette);
    for (int i = 128; i < 160; i++)
        palette.colors[i] = fCardPalette.colors[i];
    fScenePalette = palette;
    fShowingScene = true;
}


int
CardView::Run()
{
    GameWindow window("Darklands");
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
                        if (fShowingScene)
                            Choose();
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
    return _OptionAt(point);
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
    return fSelected;
}


Bitmap*
CardView::Draw()
{
    const GFX::Palette& palette = fShowingScene ? fScenePalette : fCardPalette;
    fBuffer->SetColors(palette.colors, 0, 256);
    fBuffer->Clear(kSidebarColor);
    if (fShowingScene)
        _DrawPicture(fScene, 0, 0);
    else {
        _DrawFrame();
        _DrawCard();
    }
    // the sidebar will show the party; the picture alone for now
    fBuffer->FillRect(GFX::rect(0, 0, kCardLeft, kScreenHeight), kSidebarColor);
    _DrawPicture(fSidebar, kSidebarLeft, kSidebarTop);

    if (fCursorVisible) {
        DrawMouseCursor(fBuffer, fMouse, NearestColor(palette, 0, 0, 0),
            NearestColor(palette, 255, 255, 255));
    }
    return fBuffer;
}


/* static */
void
CardView::AddCityVariables(card_variables& variables, const city& c)
{
    // place variables by DARKLAND.CTY slot (inferred from the names and
    // the texts; see docs/formats.md)
    static const struct {
        const char* name;
        int place;
    } kPlaceVariables[] = {
        { "citySquare", CITY_SQUARE }, { "councilHall", CITY_TOWN_HALL },
        { "fortress", CITY_CASTLE }, { "cathedral", CITY_CATHEDRAL },
        { "cityChurch", CITY_CHURCH }, { "marketplace", CITY_MARKET },
        { "slum", CITY_SLUMS }, { "pawnshop", CITY_PAWNSHOP },
        { "monastery", CITY_MONASTERY }, { "Inn", CITY_INN },
        { "inn", CITY_INN }, { "university", CITY_UNIVERSITY }
    };
    variables["PlaceName"] = c.shortName;
    for (const auto& variable : kPlaceVariables) {
        if (!c.places[variable.place].empty())
            variables[variable.name] = c.places[variable.place];
    }
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
CardView::_Layout(const msg_card& card, const std::string& text)
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

    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(char(MSG_CODE_NEWLINE), start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;

        size_t i = 0;
        while (i < line.size() && line[i] == MSG_CODE_PARAGRAPH) {
            y += kParagraphGap;
            i++;
        }
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
            fOptions.push_back(option_area{ top - 1, y - kLineGap });
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
    fBuffer->FillRect(GFX::rect(interiorLeft, top.height, top.width,
        kScreenHeight - top.height - bottom.height), kPaperColor);
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
