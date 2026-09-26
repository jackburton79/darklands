#include "WorldMap.h"

#include "Bitmap.h"
#include "FileStream.h"
#include "PICImage.h"
#include "Stream.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

const uint16 WorldMap::kTileWidth;
const uint16 WorldMap::kTileHeight;
const uint16 WorldMap::kRowStep;

// Tile types: sheet 0 rows are types 0..15, sheet 1 rows types 16..31.
enum {
    kTileOcean		= 1,
    kTileMajorRiver	= 2,
    kTileMinorRiver	= 3,
    kTileRoad		= 24,
    kTileFord		= 25,
    kTileBridge		= 27,
    kTileCity		= 29
};


static int
TileType(const map_tile& tile)
{
    return (tile.secondPalette ? 16 : 0) + tile.row;
}


// Whether a tile of type `type` joins its diagonal neighbor of type
// `neighbor`: tiles of the same type join; roads also join fords,
// bridges and cities, rivers also join fords, bridges and the sea.
static bool
Connects(int type, int neighbor)
{
    if (neighbor == type)
        return true;
    switch (type) {
        case kTileRoad:
            return neighbor == kTileFord || neighbor == kTileBridge
                || neighbor == kTileCity;
        case kTileMajorRiver:
        case kTileMinorRiver:
            return neighbor == kTileMajorRiver || neighbor == kTileMinorRiver
                || neighbor == kTileFord || neighbor == kTileBridge
                || neighbor == kTileOcean;
        default:
            return false;
    }
}


WorldMap::WorldMap(const std::string& mapFile, const std::string sheetFiles[2])
    :
    fWidth(0),
    fHeight(0),
    fHaveSheets(false),
    fSheetWidth(0)
{
    MapFile map(mapFile);
    fWidth = map.Width();
    fHeight = map.Height();
    fTiles.reserve(size_t(fWidth) * fHeight);
    for (uint16 y = 0; y < fHeight; y++) {
        const std::vector<map_tile> row = map.Row(y);
        fTiles.insert(fTiles.end(), row.begin(), row.end());
    }

    // Sheet column of each tile: a bit mask of the diagonal neighbors it
    // joins -- NW = 1, NE = 2, SW = 4, SE = 8. Odd rows are shifted right
    // by half a tile, so the neighbors' x depends on the row parity.
    // Off-map neighbors count as joined.
    fColumns.resize(fTiles.size());
    for (int y = 0; y < fHeight; y++) {
        const int west = (y & 1) ? 0 : -1;
        for (int x = 0; x < fWidth; x++) {
            const int type = TileTypeAt(x, y);
            const int neighbors[4][2] = {
                { x + west, y - 1 }, { x + west + 1, y - 1 },	// NW, NE
                { x + west, y + 1 }, { x + west + 1, y + 1 }	// SW, SE
            };
            uint8 column = 0;
            for (int i = 0; i < 4; i++) {
                const int nx = neighbors[i][0];
                const int ny = neighbors[i][1];
                if (nx < 0 || nx >= fWidth || ny < 0 || ny >= fHeight
                        || Connects(type, TileTypeAt(nx, ny))) {
                    column |= 1 << i;
                }
            }
            fColumns[size_t(y) * fWidth + x] = column;
        }
    }

    // GFX::Palette's constructor leaves colors uninitialized
    memset(fPalette.colors, 0, sizeof(fPalette.colors));
    _LoadSheets(sheetFiles);
}


uint32
WorldMap::PixelWidth() const
{
    return uint32(fWidth) * kTileWidth + kTileWidth / 2;
}


uint32
WorldMap::PixelHeight() const
{
    return uint32(fHeight - 1) * kRowStep + kTileHeight;
}


const map_tile&
WorldMap::TileAt(uint16 x, uint16 y) const
{
    if (x >= fWidth || y >= fHeight)
        throw std::out_of_range("WorldMap::TileAt(): invalid tile");
    return fTiles[size_t(y) * fWidth + x];
}


int
WorldMap::TileTypeAt(uint16 x, uint16 y) const
{
    return TileType(TileAt(x, y));
}


uint8
WorldMap::ColumnAt(uint16 x, uint16 y) const
{
    if (x >= fWidth || y >= fHeight)
        throw std::out_of_range("WorldMap::ColumnAt(): invalid tile");
    return fColumns[size_t(y) * fWidth + x];
}


GFX::point
WorldMap::TileOrigin(uint16 x, uint16 y) const
{
    return GFX::point(x * kTileWidth + ((y & 1) ? kTileWidth / 2 : 0),
        y * kRowStep);
}


GFX::point
WorldMap::TileCenter(uint16 x, uint16 y) const
{
    // ground tiles occupy the lower two thirds of the cell
    const GFX::point origin = TileOrigin(x, y);
    return GFX::point(origin.x + kTileWidth / 2, origin.y + 8);
}


bool
WorldMap::TileAtPixel(const GFX::point& point, uint16& tileX,
    uint16& tileY) const
{
    // Ground lenses are 16 x 8 diamonds: a point belongs to the tile with
    // the smallest |dx| / 8 + |dy| / 4 (scaled by 8 to stay in integers).
    int bestDistance = -1;
    const int centerRow = (point.y - 8) / kRowStep;
    for (int y = centerRow - 2; y <= centerRow + 2; y++) {
        if (y < 0 || y >= fHeight)
            continue;
        const int shift = (y & 1) ? kTileWidth / 2 : 0;
        const int centerColumn = (point.x - shift) / kTileWidth;
        for (int x = centerColumn - 1; x <= centerColumn + 1; x++) {
            if (x < 0 || x >= fWidth)
                continue;
            const GFX::point center = TileCenter(x, y);
            const int distance = std::abs(point.x - center.x)
                + 2 * std::abs(point.y - center.y);
            if (bestDistance < 0 || distance < bestDistance) {
                bestDistance = distance;
                tileX = x;
                tileY = y;
            }
        }
    }
    // farther than half a tile from any center: off the map
    return bestDistance >= 0 && bestDistance <= kTileWidth;
}


void
WorldMap::Draw(Bitmap* bitmap, const GFX::point& origin) const
{
    // Rows whose cells [y * kRowStep, y * kRowStep + kTileHeight) intersect
    // the visible band, drawn top to bottom: lower rows cover upper ones.
    const int top = origin.y;
    const int bottom = origin.y + bitmap->Height();
    const int firstRow = std::max(0, (top - kTileHeight) / kRowStep);
    const int lastRow = std::min(int(fHeight) - 1, bottom / kRowStep);
    const int left = origin.x;
    const int right = origin.x + bitmap->Width();
    const int firstColumn = std::max(0, left / kTileWidth - 1);
    const int lastColumn = std::min(int(fWidth) - 1, right / kTileWidth);

    for (int y = firstRow; y <= lastRow; y++) {
        for (int x = firstColumn; x <= lastColumn; x++) {
            const GFX::point cell = TileOrigin(x, y);
            _DrawTile(bitmap, x, y, cell.x - origin.x, cell.y - origin.y);
        }
    }
}


void
WorldMap::DrawIcon(Bitmap* bitmap, const GFX::point& origin, int type,
    uint8 column, uint16 x, uint16 y) const
{
    if (!fHaveSheets || x >= fWidth || y >= fHeight)
        return;
    const GFX::point cell = TileOrigin(x, y);
    _DrawCell(bitmap, type, column, cell.x - origin.x, cell.y - origin.y);
}


Bitmap*
WorldMap::Render() const
{
    Bitmap* bitmap = new Bitmap(PixelWidth(), PixelHeight(), 8);
    try {
        bitmap->SetColors(fPalette.colors, 0, 256);
        Draw(bitmap, GFX::point(0, 0));
    } catch (...) {
        bitmap->Release();
        throw;
    }
    return bitmap;
}


void
WorldMap::_LoadSheets(const std::string sheetFiles[2])
{
    uint16 sheetHeight = 0;
    fHaveSheets = true;
    for (int i = 0; i < 2 && fHaveSheets; i++) {
        Stream* stream = NULL;
        try {
            stream = new FileStream(sheetFiles[i].c_str(), FileStream::READ_ONLY);
            PICImage image(stream);
            if (fSheetWidth == 0) {
                fSheetWidth = image.Width();
                sheetHeight = image.Height();
            } else if (image.Width() != fSheetWidth || image.Height() != sheetHeight) {
                throw std::runtime_error("sheet size mismatch");
            }
            if (fSheetWidth < 16 * kTileWidth || sheetHeight < 16 * kTileHeight)
                throw std::runtime_error("sheet too small");
            // the map palette is embedded in the sheets ("M0" chunk)
            if (i == 0)
                image.ApplyPalette(fPalette);
            fSheets[i] = image.RawBytes();
        } catch (const std::exception& e) {
            std::cerr << "note: cannot load " << sheetFiles[i] << " ("
                << e.what() << ") -- drawing the map with synthetic colors"
                << std::endl;
            fHaveSheets = false;
        }
        delete stream;
    }
    if (!fHaveSheets) {
        // grayscale ramp, so the synthetic map is visible
        for (int i = 0; i < 256; i++)
            fPalette.colors[i] = GFX::Color{ uint8(i), uint8(i), uint8(i), 0 };
    }
}


void
WorldMap::_DrawTile(Bitmap* bitmap, uint16 x, uint16 y,
    int destX, int destY) const
{
    const map_tile& tile = TileAt(x, y);
    if (!fHaveSheets) {
        // color by palette set + tile row, so geometry and column
        // artifacts are still visible
        const uint8 value = tile.secondPalette
            ? uint8(128 + tile.row * 8) : uint8(16 + tile.row * 8);
        for (uint16 py = 0; py < kTileHeight; py++)
            for (uint16 px = 0; px < kTileWidth; px++)
                bitmap->PutPixel(destX + px, destY + py, value);
        return;
    }

    _DrawCell(bitmap, TileType(tile), ColumnAt(x, y), destX, destY);
}


void
WorldMap::_DrawCell(Bitmap* bitmap, int type, uint8 column,
    int destX, int destY) const
{
    const std::vector<uint8>& sheet = fSheets[(type >> 4) & 1];
    const int sx = (column & 15) * kTileWidth;
    const int sy = (type & 15) * kTileHeight;
    for (uint16 py = 0; py < kTileHeight; py++) {
        const uint8* src = &sheet[size_t(sy + py) * fSheetWidth + sx];
        for (uint16 px = 0; px < kTileWidth; px++) {
            // index 0 is transparent
            if (src[px] != 0)
                bitmap->PutPixel(destX + px, destY + py, src[px]);
        }
    }
}
