#include "MapViewer.h"

#include "Bitmap.h"
#include "CityFile.h"
#include "CityLabels.h"
#include "GameData.h"
#include "GraphicsEngine.h"
#include "LocationFile.h"
#include "Palette.h"
#include "TextSupport.h"
#include "WorldMap.h"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

const uint16 MapViewer::kScreenWidth;
const uint16 MapViewer::kScreenHeight;

// The window shows the 320x200 screen scaled by this factor
static const int kWindowScale		= 2;

// Fonts of FONTS.FNT (see docs/formats.md)
static const uint32 kLabelFontIndex	= 2;	// city names on the map
static const uint32 kTextFontIndex	= 0;	// status bar and panel

static const int kStatusBarHeight	= 10;
static const int kScrollStep		= 16;	// pixels per arrow key press
static const int kFastScrollStep	= 64;	// with shift
static const int kDragThreshold		= 3;	// pixels before a click is a drag
static const int kCityHitRadius		= 12;	// map pixels around a city tile
static const int kTickMilliseconds	= 60;	// party travel speed: one tile per tick
static const int kPartyMargin		= 48;	// keep the party this far from the edges
static const int kPartyIconType		= 30;	// blue flag row of MAPICON2.PIC

// The places of a city, in the order the panel and the city menu list them
static const struct {
    int place;
    const char* label;
} kPlaces[] = {
    { CITY_SQUARE, "Square" }, { CITY_TOWN_HALL, "Town hall" },
    { CITY_CASTLE, "Castle" }, { CITY_CATHEDRAL, "Cathedral" },
    { CITY_CHURCH, "Church" }, { CITY_MARKET, "Market" },
    { CITY_MINT_SQUARE, "Mint" }, { CITY_SLUMS, "Slums" },
    { CITY_ARMORY, "Armory" }, { CITY_PAWNSHOP, "Pawnshop" },
    { CITY_MONASTERY, "Monastery" }, { CITY_INN, "Inn" },
    { CITY_UNIVERSITY, "University" }
};

// City menu layout
static const int kMenuLeft			= 20;
static const int kMenuTop			= 12;
static const int kMenuWidth			= 280;
static const int kMenuItemsTop		= 44;	// relative to kMenuTop
static const int kMenuLineHeight	= 10;

// Mouse cursor: libjgame hides the system cursor, so we draw our own
// into the 320x200 screen. '#' = outline, 'o' = fill; hot spot top left.
static const char* kCursor[] = {
    "#",
    "##",
    "#o#",
    "#oo#",
    "#ooo#",
    "#oooo#",
    "#ooooo#",
    "#oooooo#",
    "#ooooo###",
    "#oo#oo#",
    "##  #oo#",
    "#   #oo#",
    "     ##"
};

// Terrain names by tile type, as labeled on the icon sheets
static const char* kTerrainNames[32] = {
    "Plains", "Ocean", "Major river", "Minor river", "Tidal marsh", "Marsh",
    "Geest", "Geest", "Farmland", "Farmland", "Fields/woods", "Fields/woods",
    "Light woods", "Light woods", "Forest", "Forest",
    "Forest", "Forest", "Rocky", "Rocky", "Rocky", "Rocky", "Alps", "Alps",
    "Road", "Ford", "River", "Bridge", "Castle", "City", "Flag", "Flag"
};


MapViewer::MapViewer(GameData& data)
    :
    fData(data),
    fBuffer(NULL),
    fOrigin(0, 0),
    fMouse(0, 0),
    fMouseInside(false),
    fCursorVisible(false),
    fSelectedCity(-1),
    fParty{ 0, 0 },
    fDestinationCity(-1),
    fCity(-1)
{
    // load everything up front, so missing files are reported right away
    const WorldMap& map = fData.Map();
    fData.Locations();
    fData.Cities();
    fLabelFont.reset(new Font(fData.Fonts(), kLabelFontIndex));
    fTextFont.reset(new Font(fData.Fonts(), kTextFontIndex));

    fBuffer = new Bitmap(kScreenWidth, kScreenHeight, 8);
    fBuffer->SetColors(map.Palette().colors, 0, 256);

    fBlack = NearestColor(map.Palette(), 0, 0, 0);
    fWhite = NearestColor(map.Palette(), 255, 255, 255);
    fYellow = NearestColor(map.Palette(), 255, 255, 85);
    fGray = NearestColor(map.Palette(), 170, 170, 170);
    fDarkGray = NearestColor(map.Palette(), 40, 40, 40);

    // the party starts in front of the biggest city
    const CityFile& cities = fData.Cities();
    uint32 start = 0;
    for (uint32 i = 1; i < cities.CountCities(); i++) {
        if (cities.CityAt(i).size > cities.CityAt(start).size)
            start = i;
    }
    if (cities.CountCities() > 0)
        fParty = map_position{ cities.CityAt(start).x, cities.CityAt(start).y };
    CenterOnParty();
}


MapViewer::~MapViewer()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


void
MapViewer::Run()
{
    if (!GraphicsEngine::Initialize())
        throw std::runtime_error("cannot initialize the graphics engine");
    GraphicsEngine* engine = GraphicsEngine::Get();
    engine->SetVideoMode(kScreenWidth * kWindowScale,
        kScreenHeight * kWindowScale, 32, GraphicsEngine::VIDEOMODE_WINDOWED);
    engine->SetWindowCaption("Darklands - world map");

    // 8-bit buffer -> 32-bit copy (palette conversion) -> scaled to the
    // screen (SDL_SoftStretch needs matching formats; it keeps pixels sharp)
    Bitmap* converted = new Bitmap(kScreenWidth, kScreenHeight, 32);

    bool quitting = false;
    bool dirty = true;
    bool buttonDown = false;
    bool dragging = false;
    GFX::point pressPoint(0, 0);
    GFX::point lastPoint(0, 0);
    while (!quitting) {
        SDL_Event event;
        if (!dirty && SDL_WaitEventTimeout(&event,
                IsTraveling() ? kTickMilliseconds : 100) == 0) {
            // no event before the timeout: move the party on
            dirty = Tick();
            continue;
        }
        if (dirty) {
            GFX::rect source = fBuffer->Frame();
            GraphicsEngine::BlitBitmap(Draw(), &source, converted, &source);
            GFX::rect screen = engine->ScreenFrame();
            GraphicsEngine::BlitBitmapScaled(converted, &source,
                engine->ScreenBitmap(), &screen);
            engine->Update();
            dirty = false;
            if (!SDL_PollEvent(&event))
                continue;
        }
        do {
            switch (event.type) {
                case SDL_QUIT:
                    quitting = true;
                    break;
                case SDL_KEYDOWN: {
                    const bool fast = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                    const int step = fast ? kFastScrollStep : kScrollStep;
                    const SDL_Keycode key = event.key.keysym.sym;
                    if (key == SDLK_ESCAPE) {
                        if (!Escape())
                            quitting = true;
                    } else if (fCity >= 0) {
                        // in a city, letters choose a place from the menu
                        if (key >= SDLK_a && key <= SDLK_z)
                            ChoosePlace(key - SDLK_a + 1);
                    } else switch (key) {
                        case SDLK_LEFT:		ScrollBy(-step, 0); break;
                        case SDLK_RIGHT:	ScrollBy(step, 0); break;
                        case SDLK_UP:		ScrollBy(0, -step); break;
                        case SDLK_DOWN:		ScrollBy(0, step); break;
                        case SDLK_SPACE:	CenterOnParty(); break;
                        default:
                            break;
                    }
                    dirty = true;
                    break;
                }
                case SDL_MOUSEMOTION: {
                    // window coordinates: the screen is logical size, so
                    // SDL already maps them to the 640x400 screen
                    const GFX::point point(event.motion.x / kWindowScale,
                        event.motion.y / kWindowScale);
                    if (buttonDown) {
                        if (std::abs(point.x - pressPoint.x) > kDragThreshold
                                || std::abs(point.y - pressPoint.y) > kDragThreshold) {
                            dragging = true;
                        }
                        if (dragging && fCity < 0) {
                            ScrollBy(lastPoint.x - point.x, lastPoint.y - point.y);
                            lastPoint = point;
                        }
                    }
                    MouseMoved(point);
                    dirty = true;
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        buttonDown = true;
                        dragging = false;
                        pressPoint = GFX::point(event.button.x / kWindowScale,
                            event.button.y / kWindowScale);
                        lastPoint = pressPoint;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (!dragging) {
                            Clicked(GFX::point(event.button.x / kWindowScale,
                                event.button.y / kWindowScale));
                        }
                        buttonDown = false;
                        dragging = false;
                        dirty = true;
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        RightClicked(GFX::point(event.button.x / kWindowScale,
                            event.button.y / kWindowScale));
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
        } while (!quitting && SDL_PollEvent(&event));
    }
    converted->Release();
    GraphicsEngine::Destroy();
}


void
MapViewer::ScrollBy(int dx, int dy)
{
    _SetOrigin(fOrigin.x + dx, fOrigin.y + dy);
}


void
MapViewer::CenterOn(uint16 tileX, uint16 tileY)
{
    const WorldMap& map = fData.Map();
    if (tileX >= map.Width() || tileY >= map.Height())
        return;
    const GFX::point center = map.TileCenter(tileX, tileY);
    _SetOrigin(center.x - kScreenWidth / 2,
        center.y - (kScreenHeight - kStatusBarHeight) / 2);
}


void
MapViewer::MouseMoved(const GFX::point& point)
{
    fMouse = point;
    fCursorVisible = point.x >= 0 && point.y >= 0 && point.x < kScreenWidth
        && point.y < kScreenHeight;
    fMouseInside = fCursorVisible && point.y < kScreenHeight - kStatusBarHeight;
}


void
MapViewer::MouseLeft()
{
    fCursorVisible = false;
    fMouseInside = false;
}


void
MapViewer::CenterOnParty()
{
    CenterOn(fParty.x, fParty.y);
}


void
MapViewer::Clicked(const GFX::point& point)
{
    MouseMoved(point);
    if (fCity >= 0) {
        const int item = _CityMenuItemAt(point);
        if (item >= 0)
            ChoosePlace(item + 1);
        return;
    }
    fSelectedCity = -1;
    if (!fMouseInside)
        return;

    const GFX::point mapPoint = _ScreenToMap(point);
    const int cityIndex = _CityAt(mapPoint);
    if (cityIndex >= 0 && !IsTraveling()) {
        const city& c = fData.Cities().CityAt(cityIndex);
        _TravelTo(map_position{ c.x, c.y }, cityIndex);
        return;
    }
    uint16 x, y;
    if (fData.Map().TileAtPixel(mapPoint, x, y))
        _TravelTo(map_position{ x, y }, -1);
}


void
MapViewer::RightClicked(const GFX::point& point)
{
    MouseMoved(point);
    if (fCity >= 0 || !fMouseInside)
        return;
    fSelectedCity = _CityAt(_ScreenToMap(point));
}


void
MapViewer::ChoosePlace(int number)
{
    if (fCity < 0)
        return;
    const std::vector<int> places = _CityMenuPlaces();
    if (number < 1 || number > int(places.size()))
        return;
    const city& c = fData.Cities().CityAt(fCity);
    fCityMessage = c.places[places[number - 1]] + ": not implemented yet.";
}


bool
MapViewer::Escape()
{
    if (fSelectedCity >= 0) {
        fSelectedCity = -1;
        return true;
    }
    if (fCity >= 0) {
        fCity = -1;
        fCityMessage.clear();
        return true;
    }
    if (IsTraveling()) {
        fPath.clear();
        fDestinationCity = -1;
        return true;
    }
    return false;
}


bool
MapViewer::Tick()
{
    if (fPath.empty())
        return false;
    fParty = fPath.front();
    fPath.erase(fPath.begin());
    _KeepPartyVisible();
    if (fPath.empty() && fDestinationCity >= 0)
        _EnterCity(fDestinationCity);
    return true;
}


Bitmap*
MapViewer::Draw()
{
    const WorldMap& map = fData.Map();
    fBuffer->Clear(fBlack);
    map.Draw(fBuffer, fOrigin);
    map.DrawIcon(fBuffer, fOrigin, kPartyIconType, 0, fParty.x, fParty.y);
    DrawCityLabels(fBuffer, fOrigin, map, fData.Locations(), *fLabelFont);
    if (fCity >= 0)
        _DrawCityMenu();
    else if (fSelectedCity >= 0)
        _DrawCityPanel();
    _DrawStatusBar();
    _DrawCursor();
    return fBuffer;
}


GFX::point
MapViewer::_ScreenToMap(const GFX::point& point) const
{
    return GFX::point(point.x + fOrigin.x, point.y + fOrigin.y);
}


// Index of the city (DARKLAND.CTY record, = DARKLAND.LOC location) whose
// tile center is nearest to mapPoint, or -1 if none is close enough.
int
MapViewer::_CityAt(const GFX::point& mapPoint) const
{
    const WorldMap& map = fData.Map();
    const CityFile& cities = fData.Cities();
    int best = -1;
    int bestDistance = kCityHitRadius * kCityHitRadius;
    for (uint32 i = 0; i < cities.CountCities(); i++) {
        const city& c = cities.CityAt(i);
        if (c.x >= map.Width() || c.y >= map.Height())
            continue;
        const GFX::point center = map.TileCenter(c.x, c.y);
        const int dx = center.x - mapPoint.x;
        const int dy = center.y - mapPoint.y;
        if (dx * dx + dy * dy <= bestDistance) {
            best = int(i);
            bestDistance = dx * dx + dy * dy;
        }
    }
    return best;
}


// Clamps in int before storing: GFX::point coordinates are 16-bit.
void
MapViewer::_SetOrigin(int x, int y)
{
    const WorldMap& map = fData.Map();
    const int maxX = int(map.PixelWidth()) - kScreenWidth;
    const int maxY = int(map.PixelHeight()) - (kScreenHeight - kStatusBarHeight);
    fOrigin.x = std::max(0, std::min(x, maxX));
    fOrigin.y = std::max(0, std::min(y, maxY));
}


// Starts traveling to `destination`; `city` is the city there, or -1.
void
MapViewer::_TravelTo(const map_position& destination, int city)
{
    if (destination == fParty) {
        fPath.clear();
        fDestinationCity = -1;
        if (city >= 0)
            _EnterCity(city);
        return;
    }
    fPath = FindPath(fData.Map(), fParty, destination);
    fDestinationCity = fPath.empty() ? -1 : city;
}


void
MapViewer::_EnterCity(int city)
{
    fCity = city;
    fDestinationCity = -1;
    fSelectedCity = -1;
    fCityMessage.clear();
}


// Scrolls when the party gets close to the edges of the view.
void
MapViewer::_KeepPartyVisible()
{
    const GFX::point center = fData.Map().TileCenter(fParty.x, fParty.y);
    const int x = center.x - fOrigin.x;
    const int y = center.y - fOrigin.y;
    if (x < kPartyMargin || x >= kScreenWidth - kPartyMargin
            || y < kPartyMargin
            || y >= kScreenHeight - kStatusBarHeight - kPartyMargin) {
        CenterOnParty();
    }
}


// Place slots of the current city, in menu order.
std::vector<int>
MapViewer::_CityMenuPlaces() const
{
    std::vector<int> places;
    if (fCity < 0)
        return places;
    const city& c = fData.Cities().CityAt(fCity);
    for (const auto& place : kPlaces) {
        if (!c.places[place.place].empty())
            places.push_back(place.place);
    }
    return places;
}


// Menu item (0-based) under a screen point, or -1.
int
MapViewer::_CityMenuItemAt(const GFX::point& point) const
{
    const int count = int(_CityMenuPlaces().size());
    const int top = kMenuTop + kMenuItemsTop;
    if (point.x < kMenuLeft || point.x >= kMenuLeft + kMenuWidth
            || point.y < top || point.y >= top + count * kMenuLineHeight) {
        return -1;
    }
    return (point.y - top) / kMenuLineHeight;
}


void
MapViewer::_DrawCityMenu()
{
    const CityFile& cities = fData.Cities();
    const city& c = cities.CityAt(fCity);
    const std::vector<int> places = _CityMenuPlaces();

    const int height = kMenuItemsTop + int(places.size()) * kMenuLineHeight + 34;
    const GFX::rect panel = { sint16(kMenuLeft), sint16(kMenuTop),
        uint16(kMenuWidth), uint16(height) };
    fBuffer->FillRect(panel, fDarkGray);
    fBuffer->StrokeRect(panel, fGray);

    const int textLeft = kMenuLeft + 8;
    _DrawText(*fLabelFont, c.fullName, textLeft, kMenuTop + 6, fYellow);
    _DrawText(*fTextFont, "Ruled by " + c.places[CITY_RULER], textLeft,
        kMenuTop + 20, fGray);
    _DrawText(*fTextFont, "Where do you want to go?", textLeft,
        kMenuTop + 32, fWhite);

    // highlight the item under the mouse
    const int hovered = fCursorVisible ? _CityMenuItemAt(fMouse) : -1;
    int y = kMenuTop + kMenuItemsTop;
    for (size_t i = 0; i < places.size(); i++) {
        if (int(i) == hovered) {
            const GFX::rect highlight = { sint16(kMenuLeft + 2), sint16(y - 1),
                uint16(kMenuWidth - 4), uint16(kMenuLineHeight) };
            fBuffer->FillRect(highlight, fBlack);
        }
        const std::string label(1, char('A' + i));
        _DrawText(*fTextFont, label + ".", textLeft, y, fGray);
        _DrawText(*fTextFont, c.places[places[i]], textLeft + 16, y, fWhite);
        for (const auto& place : kPlaces) {
            if (place.place == places[i]) {
                _DrawText(*fTextFont, place.label, textLeft + 170, y, fGray);
                break;
            }
        }
        y += kMenuLineHeight;
    }
    y += 6;
    if (!fCityMessage.empty())
        _DrawText(*fTextFont, fCityMessage, textLeft, y, fYellow);
    _DrawText(*fTextFont, "Esc: leave the city", textLeft, y + 12, fGray);
}


void
MapViewer::_DrawStatusBar()
{
    const int top = kScreenHeight - kStatusBarHeight;
    const GFX::rect bar = { 0, sint16(top), kScreenWidth, kStatusBarHeight };
    fBuffer->FillRect(bar, fDarkGray);
    fBuffer->StrokeLine(0, top, kScreenWidth - 1, top, fGray);

    if (fCity >= 0) {
        _DrawText(*fTextFont, "In " + fData.Cities().CityAt(fCity).fullName,
            3, top + 2, fWhite);
        return;
    }
    if (IsTraveling()) {
        std::string text = "Traveling";
        if (fDestinationCity >= 0)
            text += " to " + fData.Cities().CityAt(fDestinationCity).fullName;
        const int width = fTextFont->StringWidth(Font::ToGameCharset(text));
        _DrawText(*fTextFont, text, kScreenWidth - 3 - width, top + 2, fYellow);
    }
    if (!fMouseInside)
        return;
    const WorldMap& map = fData.Map();
    const GFX::point mapPoint = _ScreenToMap(fMouse);
    uint16 x, y;
    if (map.TileAtPixel(mapPoint, x, y)) {
        std::ostringstream text;
        text << x << "," << y << "  " << kTerrainNames[map.TileTypeAt(x, y) & 31];
        _DrawText(*fTextFont, text.str(), 3, top + 2, fWhite);
    }
    const int cityIndex = _CityAt(mapPoint);
    if (cityIndex >= 0) {
        const std::string name = fData.Cities().CityAt(cityIndex).fullName;
        const int width = fTextFont->StringWidth(Font::ToGameCharset(name));
        _DrawText(*fTextFont, name, kScreenWidth - 3 - width, top + 2, fYellow);
    }
}


void
MapViewer::_DrawCityPanel()
{
    const CityFile& cities = fData.Cities();
    const city& c = cities.CityAt(fSelectedCity);

    const int lineHeight = fTextFont->Height() + 2;
    const int width = 196;
    const int left = kScreenWidth - width - 4;
    const int top = 4;
    const int textLeft = left + 5;

    // collect the lines first, to size the panel
    struct line {
        std::string label;	// empty: the value spans the whole panel
        std::string value;
    };
    std::vector<line> lines;
    std::ostringstream size;
    size << "Size " << int(c.size);
    if (c.harbor == CITY_HARBOR_NORTH_SEA)
        size << ", port on the North Sea";
    else if (c.harbor == CITY_HARBOR_BALTIC)
        size << ", port on the Baltic";
    lines.push_back({ "", size.str() });
    lines.push_back({ "Ruler", c.places[CITY_RULER] });
    if (!c.places[CITY_SECOND_POWER].empty()
            && c.places[CITY_SECOND_POWER] != c.places[CITY_RULER]) {
        lines.push_back({ "Also", c.places[CITY_SECOND_POWER] });
    }
    for (const auto& place : kPlaces) {
        if (!c.places[place.place].empty())
            lines.push_back({ place.label, c.places[place.place] });
    }
    std::string neighbors;
    for (uint16 n : c.neighbors) {
        if (n >= cities.CountCities())
            continue;
        if (!neighbors.empty())
            neighbors += ", ";
        neighbors += cities.CityAt(n).shortName;
    }
    if (!neighbors.empty())
        lines.push_back({ "Roads to", neighbors });

    // values start after the widest label
    int labelWidth = 0;
    for (const line& l : lines) {
        labelWidth = std::max<int>(labelWidth,
            fTextFont->StringWidth(Font::ToGameCharset(l.label)));
    }
    const int valueLeft = textLeft + labelWidth + 6;

    // wrap long values; continuation lines keep their value column
    struct row {
        std::string label;
        std::string value;		// in the game's character set
        int x;
    };
    std::vector<row> wrapped;
    for (const line& l : lines) {
        const int x = l.label.empty() ? textLeft : valueLeft;
        std::string rest = Font::ToGameCharset(l.value);
        bool first = true;
        do {
            const std::string part = fTextFont->TruncateString(rest,
                left + width - 5 - x);
            wrapped.push_back({ first ? l.label : "", part, x });
            first = false;
        } while (!rest.empty());
    }

    const int height = 5 + (fLabelFont->Height() + 4) + int(wrapped.size()) * lineHeight + 3;
    const GFX::rect panel = { sint16(left), sint16(top), uint16(width), uint16(height) };
    fBuffer->FillRect(panel, fDarkGray);
    fBuffer->StrokeRect(panel, fGray);

    _DrawText(*fLabelFont, c.fullName, textLeft, top + 4, fYellow);
    int y = top + 4 + fLabelFont->Height() + 4;
    for (const row& r : wrapped) {
        if (!r.label.empty())
            _DrawText(*fTextFont, r.label, textLeft, y, fGray);
        fTextFont->RenderString(r.value, fBuffer, GFX::point(r.x, y), fWhite);
        y += lineHeight;
    }
}


void
MapViewer::_DrawCursor()
{
    if (!fCursorVisible)
        return;
    for (size_t row = 0; row < sizeof(kCursor) / sizeof(kCursor[0]); row++) {
        for (int column = 0; kCursor[row][column] != '\0'; column++) {
            const char c = kCursor[row][column];
            if (c == '#' || c == 'o') {
                fBuffer->PutPixel(fMouse.x + column, fMouse.y + int(row),
                    c == '#' ? fBlack : fWhite);
            }
        }
    }
}


void
MapViewer::_DrawText(const Font& font, const std::string& utf8, int x, int y,
    uint8 color) const
{
    font.RenderString(Font::ToGameCharset(utf8), fBuffer, GFX::point(x, y),
        color);
}
