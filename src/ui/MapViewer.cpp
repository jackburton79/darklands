#include "MapViewer.h"

#include "Bitmap.h"
#include "CityFile.h"
#include "CityLabels.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
#include "MenuBar.h"
#include "LocationFile.h"
#include "Palette.h"
#include "ScreenSupport.h"
#include "TextSupport.h"
#include "WorldMap.h"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>

const uint16 MapViewer::kScreenWidth;
const uint16 MapViewer::kScreenHeight;

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
    fClock(NULL),
    fTravelMinutes(0),
    fInfo(NULL),
    fMenu(NULL),
    fBuffer(NULL),
    fOrigin(0, 0),
    fMouse(0, 0),
    fMouseInside(false),
    fCursorVisible(false),
    fSelectedCity(-1),
    fParty{ 0, 0 },
    fDestinationPlace(-1),
    fPlace(-1)
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


int
MapViewer::Run()
{
    GameWindow window("Darklands - world map");
    return Run(window);
}


int
MapViewer::Run(GameWindow& window)
{
    fPlace = -1;

    bool quitting = false;
    bool loading = false;
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
            if (fPlace >= 0)
                return fPlace;
            continue;
        }
        if (dirty) {
            window.Show(Draw());
            dirty = false;
            if (!SDL_PollEvent(&event))
                continue;
        }
        do {
            if (_MenuEvent(window, event, quitting, loading)) {
                dirty = true;
                continue;
            }
            switch (event.type) {
                case SDL_QUIT:
                    quitting = true;
                    break;
                case SDL_KEYDOWN: {
                    const bool fast = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                    const int step = fast ? kFastScrollStep : kScrollStep;
                    const SDL_Keycode key = event.key.keysym.sym;
                    if (key == SDLK_s && (event.key.keysym.mod & KMOD_CTRL) != 0) {
                        if (fSaveHandler)
                            fSaveHandler(window);
                    } else if (key == SDLK_ESCAPE) {
                        if (!Escape())
                            quitting = true;
                    } else switch (key) {
                        case SDLK_LEFT:		ScrollBy(-step, 0); break;
                        case SDLK_RIGHT:	ScrollBy(step, 0); break;
                        case SDLK_UP:		ScrollBy(0, -step); break;
                        case SDLK_DOWN:		ScrollBy(0, step); break;
                        case SDLK_SPACE:	CenterOnParty(); break;
                        case SDLK_c:
                            if (fCampHandler) {
                                // the party stops to camp
                                fPath.clear();
                                fCampHandler(window, CampDanger());
                            }
                            break;
                        case SDLK_F1: case SDLK_F2: case SDLK_F3:
                        case SDLK_F4: case SDLK_F5: case SDLK_F6:
                            if (fInfo != NULL) {
                                fInfo->SetPosition(fParty);
                                fInfo->Run(window, key == SDLK_F6
                                    ? InfoView::kPartyPage : int(key - SDLK_F1));
                            }
                            break;
                        default:
                            break;
                    }
                    dirty = true;
                    break;
                }
                case SDL_MOUSEMOTION: {
                    const GFX::point point = GameWindow::ToScreen(
                        event.motion.x, event.motion.y);
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
                        pressPoint = GameWindow::ToScreen(event.button.x,
                            event.button.y);
                        lastPoint = pressPoint;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (!dragging) {
                            Clicked(GameWindow::ToScreen(event.button.x,
                                event.button.y));
                        }
                        buttonDown = false;
                        dragging = false;
                        dirty = true;
                    } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                        MiddleClicked(GameWindow::ToScreen(event.button.x,
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
        } while (!quitting && SDL_PollEvent(&event));
        if (loading)
            return kLoadRequested;
        if (fPlace >= 0)
            return fPlace;
    }
    return -1;
}


// The menu bar's events (the right button, F10, the shortcuts): true if
// it took the event
bool
MapViewer::_MenuEvent(GameWindow& window, const SDL_Event& event,
    bool& quitting, bool& loading)
{
    if (fMenu == NULL)
        return false;
    menu_command command;
    if (!fMenu->Handle(window, event, [this]() { return Draw(); }, true,
            command))
        return false;
    switch (command) {
        case MENU_SAVE_GAME:
            if (fSaveHandler)
                fSaveHandler(window);
            break;
        case MENU_LOAD_GAME:
            if (fLoadHandler && fLoadHandler(window))
                loading = true;
            break;
        case MENU_MARCHING_ORDER:
            if (fOrderHandler)
                fOrderHandler(window);
            break;
        case MENU_QUIT:
            quitting = true;
            break;
        case MENU_PARTY_INFO:
            if (fInfo != NULL) {
                fInfo->SetPosition(fParty);
                fInfo->Run(window, InfoView::kPartyPage);
            }
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
MapViewer::SetPartyPosition(const map_position& position)
{
    fParty = position;
    fPath.clear();
    fDestinationPlace = -1;
    fPlace = -1;
    fSelectedCity = -1;
    CenterOnParty();
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
    fSelectedCity = -1;
    if (!fMouseInside)
        return;

    const GFX::point mapPoint = _ScreenToMap(point);
    const int place = _PlaceAt(mapPoint);
    if (place >= 0 && !IsTraveling()) {
        _TravelTo(_PlacePosition(place), place);
        return;
    }
    uint16 x, y;
    if (fData.Map().TileAtPixel(mapPoint, x, y))
        _TravelTo(map_position{ x, y }, -1);
}


void
MapViewer::MiddleClicked(const GFX::point& point)
{
    MouseMoved(point);
    if (fPlace >= 0 || !fMouseInside)
        return;
    fSelectedCity = _CityAt(_ScreenToMap(point));
}


bool
MapViewer::Escape()
{
    if (fSelectedCity >= 0) {
        fSelectedCity = -1;
        return true;
    }
    if (IsTraveling()) {
        fPath.clear();
        fDestinationPlace = -1;
        return true;
    }
    return false;
}


// File 0x6005E. The terrain under the party: marshes, light forest and
// rock take 1 off, thicker ones 2, 3, 5 and 6, a road adds 1, farmland 2.
// A city (tile type 29) in the 10 x 10 tiles around adds |d - 6|, d being
// the smaller of its distances along the two axes (none if 0), the last
// column's city counting: *inferred* from the code, whose window is the
// screen's own buffer.
int
MapViewer::CampDanger() const
{
    const WorldMap& map = fData.Map();
    int danger = 0;
    const int here = map.TileTypeAt(fParty.x, fParty.y);
    switch (here) {
        case 4: case 5: case 15: case 19:
            danger -= 1;
            break;
        case 16: case 20:
            danger -= 2;
            break;
        case 17: case 21:
            danger -= 3;
            break;
        case 22:
            danger -= 5;
            break;
        case 23:
            danger -= 6;
            break;
        case 24:
            danger += 1;
            break;
        case 8: case 9:
            danger += 2;
            break;
        default:
            break;
    }
    int near = 0;
    for (int dx = -5; dx < 5; dx++) {
        for (int dy = -5; dy < 5; dy++) {
            const int x = int(fParty.x) + dx;
            const int y = int(fParty.y) + dy;
            if (x < 0 || y < 0 || x >= map.Width() || y >= map.Height()
                    || map.TileTypeAt(uint16(x), uint16(y)) != 29)
                continue;
            near = std::min(std::abs(dx), std::abs(dy));
            break;
        }
    }
    if (near != 0)
        danger += std::abs(near - 6);
    return danger;
}


bool
MapViewer::Tick()
{
    if (fPath.empty())
        return false;
    const uint32 hours = TravelHours(fData.Map(), fParty, fPath.front(),
        fTravelMinutes);
    if (fClock != NULL)
        fClock->AddHours(hours);
    fParty = fPath.front();
    fPath.erase(fPath.begin());
    _KeepPartyVisible();
    if (fPath.empty() && fDestinationPlace >= 0)
        _EnterPlace(fDestinationPlace);
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
    if (fSelectedCity >= 0)
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


// Index of the place of DARKLAND.LOC (the cities first) whose tile
// center is nearest to mapPoint, or -1 if none is close enough; the
// cities by their DARKLAND.CTY tile
int
MapViewer::_PlaceAt(const GFX::point& mapPoint) const
{
    const WorldMap& map = fData.Map();
    const LocationFile& locations = fData.Locations();
    int best = -1;
    int bestDistance = kCityHitRadius * kCityHitRadius;
    for (uint32 i = 0; i < locations.CountLocations(); i++) {
        const map_position p = _PlacePosition(int(i));
        if (p.x >= map.Width() || p.y >= map.Height())
            continue;
        const GFX::point center = map.TileCenter(p.x, p.y);
        const int dx = center.x - mapPoint.x;
        const int dy = center.y - mapPoint.y;
        if (dx * dx + dy * dy < bestDistance
                || (best < 0 && dx * dx + dy * dy == bestDistance)) {
            best = int(i);
            bestDistance = dx * dx + dy * dy;
        }
    }
    return best;
}


std::string
MapViewer::_PlaceName(int place) const
{
    if (place >= 0 && uint32(place) < fData.Cities().CountCities())
        return fData.Cities().CityAt(uint32(place)).fullName;
    return fData.Locations().LocationAt(uint32(place)).name;
}


map_position
MapViewer::_PlacePosition(int place) const
{
    if (place >= 0 && uint32(place) < fData.Cities().CountCities()) {
        const city& c = fData.Cities().CityAt(uint32(place));
        return map_position{ c.x, c.y };
    }
    const location& l = fData.Locations().LocationAt(uint32(place));
    return map_position{ l.x, l.y };
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


// Starts traveling to `destination`; `place` is the place there, or -1.
void
MapViewer::_TravelTo(const map_position& destination, int place)
{
    if (destination == fParty) {
        fPath.clear();
        fDestinationPlace = -1;
        if (place >= 0)
            _EnterPlace(place);
        return;
    }
    fPath = FindPath(fData.Map(), fParty, destination, place >= 0);
    fDestinationPlace = fPath.empty() ? -1 : place;
}


void
MapViewer::_EnterPlace(int place)
{
    fPlace = place;
    fDestinationPlace = -1;
    fSelectedCity = -1;
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


void
MapViewer::_DrawStatusBar()
{
    const int top = kScreenHeight - kStatusBarHeight;
    const GFX::rect bar = { 0, sint16(top), kScreenWidth, kStatusBarHeight };
    fBuffer->FillRect(bar, fDarkGray);
    fBuffer->StrokeLine(0, top, kScreenWidth - 1, top, fGray);

    // the time on the left, then the tile under the mouse; the traveling
    // destination or the place under the mouse on the right
    int left = 3;
    if (fClock != NULL) {
        const std::string time = fClock->Describe();
        _DrawText(*fTextFont, time, left, top + 2, fYellow);
        left += fTextFont->StringWidth(Font::ToGameCharset(time)) + 10;
    }
    if (fPlace >= 0) {
        _DrawText(*fTextFont, "In " + _PlaceName(fPlace), left, top + 2,
            fWhite);
        return;
    }
    if (IsTraveling()) {
        std::string text = "Traveling";
        if (fDestinationPlace >= 0)
            text += " to " + _PlaceName(fDestinationPlace);
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
        _DrawText(*fTextFont, text.str(), left, top + 2, fWhite);
    }
    const int place = _PlaceAt(mapPoint);
    if (place >= 0) {
        const std::string name = _PlaceName(place);
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
    if (fCursorVisible)
        DrawMouseCursor(fBuffer, fMouse, fBlack, fWhite);
}


void
MapViewer::_DrawText(const Font& font, const std::string& utf8, int x, int y,
    uint8 color) const
{
    font.RenderString(Font::ToGameCharset(utf8), fBuffer, GFX::point(x, y),
        color);
}
