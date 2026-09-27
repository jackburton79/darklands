#include "BattleView.h"

#include "BattleMap.h"
#include "Bitmap.h"
#include "Catalog.h"
#include "GameData.h"
#include "ImcFile.h"
#include "ScreenSupport.h"
#include "Stream.h"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

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

// The party's figures use colors 235..242; the battle puts each member's
// 8 colors at 80 + 8 · member (docs/formats.md, "Battle sprites")
static const uint8 kPartySpriteColors	= 235;
static const uint8 kPartyColors			= 80;
static const int kFigureColors			= 8;


BattleView::BattleView(GameData& data)
    :
    fData(data),
    fBuffer(new Bitmap(kScreenWidth, kScreenHeight, 8)),
    fPalette(data.SpritePalette("")),
    fOrigin(0, 0),
    fPlace(PLACE_WILDERNESS)
{
    fBuffer->SetColors(fPalette.colors, 0, 256);
}


BattleView::~BattleView()
{
    fBuffer->Release();
}


void
BattleView::SetMap(std::unique_ptr<BattleMap> map, const std::string& name)
{
    fMap = std::move(map);
    fFigures.clear();
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
BattleView::AddPartyMember(int member, const std::string& image,
    const std::vector<uint8>& colors, int x, int y, int direction)
{
    figure f = { _LoadSprites(image), x, y, direction,
        kPartyColors + kFigureColors * member };
    for (int i = 0; i < kFigureColors && size_t(3 * i + 2) < colors.size(); i++) {
        GFX::Color& color = fPalette.colors[f.colors + i];
        color.r = uint8((colors[3 * i] << 2) | (colors[3 * i] >> 4));
        color.g = uint8((colors[3 * i + 1] << 2) | (colors[3 * i + 1] >> 4));
        color.b = uint8((colors[3 * i + 2] << 2) | (colors[3 * i + 2] >> 4));
    }
    if (colors.empty()) {
        // unknown (a new party): gray clothes
        for (int i = 0; i < kFigureColors; i++)
            fPalette.colors[f.colors + i] = fPalette.colors[kGray + 2 + i % 6];
    }
    fBuffer->SetColors(fPalette.colors, 0, 256);
    fFigures.push_back(f);
}


void
BattleView::AddEnemy(const std::string& image, int x, int y, int direction)
{
    figure f = { _LoadSprites(image), x, y, direction, -1 };
    // the enemies of different kinds may share palette indices: the last
    // one's colors win
    fData.ApplyEnemyColors(fPalette, image);
    fBuffer->SetColors(fPalette.colors, 0, 256);
    fFigures.push_back(f);
}


bool
BattleView::FindFreeCell(int& x, int& y) const
{
    if (!fMap)
        return false;
    for (int ring = 0; ring < BattleMap::kSize; ring++) {
        for (int dy = -ring; dy <= ring; dy++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring)
                    continue;
                const int cx = x + dx;
                const int cy = y + dy;
                if (!_IsOpen(cx, cy))
                    continue;
                bool taken = false;
                for (const figure& f : fFigures)
                    taken = taken || (f.x == cx && f.y == cy);
                if (!taken) {
                    x = cx;
                    y = cy;
                    return true;
                }
            }
        }
    }
    return false;
}


// The combat animation ("CB") of a sprite set: "E03" in E00C.CAT, "F60"
// in F60C.CAT. The first one: which one goes with a weapon is not decoded.
std::shared_ptr<ImcFile>
BattleView::_LoadSprites(const std::string& image)
{
    const bool enemy = !image.empty() && (image[0] == 'E' || image[0] == 'M');
    const std::string catalogName = enemy
        ? image.substr(0, 1) + "00C.CAT" : image + "C.CAT";
    std::unique_ptr<Catalog> catalog(fData.OpenCatalog(catalogName));
    const std::string prefix = image + "CB";
    for (int32 i = 0; i < catalog->CountEntries(); i++) {
        if (catalog->EntryAt(i).filename.compare(0, prefix.size(), prefix) != 0)
            continue;
        std::unique_ptr<Stream> stream(catalog->GetStreamAt(uint32(i)));
        return std::shared_ptr<ImcFile>(new ImcFile(stream.get()));
    }
    throw std::runtime_error("no battle sprites for " + image);
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

    // from the back to the front
    std::vector<const figure*> order;
    for (const figure& f : fFigures)
        order.push_back(&f);
    std::stable_sort(order.begin(), order.end(),
        [](const figure* a, const figure* b) { return a->y < b->y; });
    for (const figure* f : order)
        _DrawFigure(*f);
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


// Its feet near the bottom of its cell, centered
void
BattleView::_DrawFigure(const figure& f)
{
    const sprite& picture = f.sprites->SpriteAt(0,
        f.direction % ImcFile::kDirectionCount);
    const int left = f.x * kCellSize + kCellSize / 2 - picture.width / 2
        - fOrigin.x;
    const int top = f.y * kCellSize + kCellSize - 2 - picture.height
        - fOrigin.y;
    for (int y = 0; y < picture.height; y++) {
        const int screenY = top + y;
        if (screenY < 0 || screenY >= kScreenHeight)
            continue;
        for (int x = 0; x < picture.width; x++) {
            const int screenX = left + x;
            uint8 pixel = picture.pixels[y * picture.width + x];
            if (pixel == 0 || screenX < 0 || screenX >= kScreenWidth)
                continue;
            if (f.colors >= 0 && pixel >= kPartySpriteColors
                    && pixel < kPartySpriteColors + kFigureColors) {
                pixel = uint8(f.colors + pixel - kPartySpriteColors);
            }
            fBuffer->PutPixel(screenX, screenY, pixel);
        }
    }
}
