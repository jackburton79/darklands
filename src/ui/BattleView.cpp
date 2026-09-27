#include "BattleView.h"

#include "BattleMap.h"
#include "Bitmap.h"
#include "GameData.h"
#include "ScreenSupport.h"

#include <SDL.h>

#include <algorithm>

static const int kScreenWidth	= 320;
static const int kScreenHeight	= 200;
static const int kMapPixels		= BattleMap::kSize * BattleView::kCellSize;

// Ramps of 8 colors of the battle's palette (BKGNDPAL.DAT at 164..234,
// docs/formats.md), darkest first
static const uint8 kYellow		= 176;
static const uint8 kGreen		= 184;
static const uint8 kRed			= 192;
static const uint8 kBlueGray	= 200;
static const uint8 kGray		= 208;
static const uint8 kBrown		= 216;

// Wall types (docs/formats.md, "Battlefield maps"; inferred)
static const int kRockWall		= 1;
static const int kHouseWall		= 2;
static const int kMasonryWall	= 6;


BattleView::BattleView(GameData& data)
    :
    fBuffer(new Bitmap(kScreenWidth, kScreenHeight, 8)),
    fOrigin(0, 0),
    fPlace(PLACE_WILDERNESS)
{
    const GFX::Palette palette = data.SpritePalette("");
    fBuffer->SetColors(palette.colors, 0, 256);
}


BattleView::~BattleView()
{
    fBuffer->Release();
}


void
BattleView::SetMap(std::unique_ptr<BattleMap> map, const std::string& name)
{
    fMap = std::move(map);
    fOrigin = GFX::point(0, 0);
    if (name.compare(0, 5, "ICITY") == 0 || name.compare(0, 8, "IWILDGAT") == 0
            || name.compare(0, 8, "IWILDWAL") == 0) {
        fPlace = PLACE_TOWN;
    } else if (name.compare(0, 5, "IMINE") == 0)
        fPlace = PLACE_MINE;
    else if (name.compare(0, 5, "IFORT") == 0 || name.compare(0, 5, "IMISC") == 0)
        fPlace = PLACE_BUILDING;
    else
        fPlace = PLACE_WILDERNESS;
}


void
BattleView::Run(GameWindow& window)
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
        switch (event.type) {
            case SDL_QUIT:
                return;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return;
                    case SDLK_LEFT:
                        Scroll(-1, 0);
                        break;
                    case SDLK_RIGHT:
                        Scroll(1, 0);
                        break;
                    case SDLK_UP:
                        Scroll(0, -1);
                        break;
                    case SDLK_DOWN:
                        Scroll(0, 1);
                        break;
                }
                dirty = true;
                break;
            case SDL_WINDOWEVENT:
                dirty = true;
                break;
        }
    }
}


void
BattleView::Scroll(int dx, int dy)
{
    const int x = std::max(0, std::min(kMapPixels - kScreenWidth,
        int(fOrigin.x) + dx * kCellSize));
    const int y = std::max(0, std::min(kMapPixels - kScreenHeight,
        int(fOrigin.y) + dy * kCellSize));
    fOrigin = GFX::point(x, y);
}


Bitmap*
BattleView::Draw()
{
    fBuffer->Clear(0);
    if (!fMap)
        return fBuffer;
    const int firstX = fOrigin.x / kCellSize;
    const int firstY = fOrigin.y / kCellSize;
    for (int y = firstY; y < BattleMap::kSize; y++) {
        const int top = y * kCellSize - fOrigin.y;
        if (top >= kScreenHeight)
            break;
        for (int x = firstX; x < BattleMap::kSize; x++) {
            const int left = x * kCellSize - fOrigin.x;
            if (left >= kScreenWidth)
                break;
            _DrawCell(x, y, left, top);
        }
    }
    return fBuffer;
}


uint8
BattleView::_GroundColor(const battle_cell& cell) const
{
    if (cell.Closed()) {
        switch (fPlace) {
            case PLACE_TOWN:
                return kBrown + 2;		// houses
            case PLACE_BUILDING:
                return kBlueGray + 2;
            default:
                return kGray + 1;		// rock
        }
    }
    const int kind = cell.ground >> 4;
    const int variant = cell.ground & 0x0F;
    if (kind == 4)
        return kBrown + 5;				// inside the fortresses
    if (kind != 1)
        return kYellow + 4;
    switch (fPlace) {
        case PLACE_TOWN:
            return uint8(kGray + 4 + variant % 2);
        case PLACE_MINE:
            return kBrown + 3;
        case PLACE_BUILDING:
            return kGray + 5;
        default:
            return uint8(kGreen + 3 + variant % 3);
    }
}


bool
BattleView::_IsOpen(int x, int y) const
{
    return x >= 0 && x < BattleMap::kSize && y >= 0 && y < BattleMap::kSize
        && !fMap->CellAt(x, y).Closed();
}


void
BattleView::_DrawCell(int x, int y, int left, int top)
{
    const battle_cell& cell = fMap->CellAt(x, y);
    fBuffer->FillRect(GFX::rect(left, top, kCellSize, kCellSize),
        _GroundColor(cell));

    const int object = cell.Object();
    const int center = kCellSize / 2;
    if (fPlace == PLACE_WILDERNESS && object >= 1 && object <= 4) {
        // they line up as paths (inferred)
        fBuffer->FillRect(GFX::rect(left + 2, top + 2, kCellSize - 4,
            kCellSize - 4), kBrown + 6);
    } else if (fPlace == PLACE_WILDERNESS && object >= 11 && object <= 13) {
        // trees (inferred)
        fBuffer->FillCircle(int16(left + center), int16(top + center),
            uint32(center - 2), kGreen);
    } else if (object == 14) {
        // reeds, in the marshes (inferred)
        fBuffer->FillRect(GFX::rect(left + 4, top + 4, 2, 8), kYellow + 2);
        fBuffer->FillRect(GFX::rect(left + 9, top + 5, 2, 7), kYellow + 2);
    } else if (object != 0) {
        fBuffer->FillRect(GFX::rect(left + 5, top + 5, 6, 6), kYellow + 6);
    }

    // the walls of the open cells: those of the closed ones face nowhere
    static const int kDx[SIDE_COUNT] = { 0, 0, -1, 1 };
    static const int kDy[SIDE_COUNT] = { 1, -1, 0, 0 };
    for (int side = 0; side < SIDE_COUNT; side++) {
        const int wall = cell.Wall(battle_side(side));
        if (wall == 0 || (cell.Closed()
                && !_IsOpen(x + kDx[side], y + kDy[side]))) {
            continue;
        }
        uint8 color;
        switch (wall) {
            case kRockWall:
                color = kGray + 6;
                break;
            case kHouseWall:
                color = kBrown;
                break;
            case kMasonryWall:
                color = kBlueGray + 6;
                break;
            default:
                color = kRed + 5;		// doors, gates? (not decoded)
                break;
        }
        GFX::rect edge;
        switch (side) {
            case SIDE_NEXT_ROW:
                edge = GFX::rect(left, top + kCellSize - 2, kCellSize, 2);
                break;
            case SIDE_PREVIOUS_ROW:
                edge = GFX::rect(left, top, kCellSize, 2);
                break;
            case SIDE_PREVIOUS_COLUMN:
                edge = GFX::rect(left, top, 2, kCellSize);
                break;
            default:
                edge = GFX::rect(left + kCellSize - 2, top, 2, kCellSize);
                break;
        }
        fBuffer->FillRect(edge, color);
    }
}
