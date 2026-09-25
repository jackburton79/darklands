#include "CityLabels.h"

#include "Bitmap.h"
#include "LocationFile.h"
#include "TextSupport.h"
#include "WorldMap.h"

#include <climits>


// Index of the palette color closest to (r, g, b).
static uint8
NearestColor(const GFX::Palette& palette, int r, int g, int b)
{
    int best = 0;
    int bestDistance = INT_MAX;
    for (int i = 0; i < 256; i++) {
        const GFX::Color& c = palette.colors[i];
        const int distance = (c.r - r) * (c.r - r) + (c.g - g) * (c.g - g)
            + (c.b - b) * (c.b - b);
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return uint8(best);
}


void
DrawCityLabels(Bitmap* bitmap, const GFX::point& origin, const WorldMap& map,
    const LocationFile& locations, const Font& font)
{
    const uint8 white = NearestColor(map.Palette(), 255, 255, 255);
    const uint8 black = NearestColor(map.Palette(), 0, 0, 0);
    for (uint32 i = 0; i < locations.CountLocations(); i++) {
        const location& loc = locations.LocationAt(i);
        if (loc.type != 0 || loc.x >= map.Width() || loc.y >= map.Height())
            continue;
        const std::string name = Font::ToGameCharset(loc.name);
        const GFX::point cell = map.TileOrigin(loc.x, loc.y);
        const int left = cell.x + WorldMap::kTileWidth / 2
            - font.StringWidth(name) / 2 - origin.x;
        const int top = cell.y - font.Height() - origin.y;
        if (left + font.StringWidth(name) < -1 || top + font.Height() < -1
                || left > bitmap->Width() || top > bitmap->Height()) {
            continue;
        }
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx != 0 || dy != 0) {
                    font.RenderString(name, bitmap,
                        GFX::point(left + dx, top + dy), black);
                }
            }
        }
        font.RenderString(name, bitmap, GFX::point(left, top), white);
    }
}
