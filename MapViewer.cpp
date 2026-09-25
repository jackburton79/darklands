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
    fSelectedCity(-1)
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

    // start on the biggest city
    const CityFile& cities = fData.Cities();
    uint32 start = 0;
    for (uint32 i = 1; i < cities.CountCities(); i++) {
        if (cities.CityAt(i).size > cities.CityAt(start).size)
            start = i;
    }
    if (cities.CountCities() > 0)
        CenterOn(cities.CityAt(start).x, cities.CityAt(start).y);
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
        if (!dirty && SDL_WaitEventTimeout(&event, 100) == 0)
            continue;
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
                    switch (event.key.keysym.sym) {
                        case SDLK_LEFT:		ScrollBy(-step, 0); break;
                        case SDLK_RIGHT:	ScrollBy(step, 0); break;
                        case SDLK_UP:		ScrollBy(0, -step); break;
                        case SDLK_DOWN:		ScrollBy(0, step); break;
                        case SDLK_ESCAPE:
                            if (!ClosePanel())
                                quitting = true;
                            break;
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
                        if (dragging) {
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
                    }
                    break;
                case SDL_WINDOWEVENT:
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
    fMouseInside = point.x >= 0 && point.y >= 0 && point.x < kScreenWidth
        && point.y < kScreenHeight - kStatusBarHeight;
}


void
MapViewer::Clicked(const GFX::point& point)
{
    MouseMoved(point);
    if (!fMouseInside)
        return;
    fSelectedCity = _CityAt(_ScreenToMap(point));
}


bool
MapViewer::ClosePanel()
{
    if (fSelectedCity < 0)
        return false;
    fSelectedCity = -1;
    return true;
}


Bitmap*
MapViewer::Draw()
{
    const WorldMap& map = fData.Map();
    fBuffer->Clear(fBlack);
    map.Draw(fBuffer, fOrigin);
    DrawCityLabels(fBuffer, fOrigin, map, fData.Locations(), *fLabelFont);
    if (fSelectedCity >= 0)
        _DrawCityPanel();
    _DrawStatusBar();
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


void
MapViewer::_DrawStatusBar()
{
    const int top = kScreenHeight - kStatusBarHeight;
    const GFX::rect bar = { 0, sint16(top), kScreenWidth, kStatusBarHeight };
    fBuffer->FillRect(bar, fDarkGray);
    fBuffer->StrokeLine(0, top, kScreenWidth - 1, top, fGray);

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
MapViewer::_DrawText(const Font& font, const std::string& utf8, int x, int y,
    uint8 color) const
{
    font.RenderString(Font::ToGameCharset(utf8), fBuffer, GFX::point(x, y),
        color);
}
