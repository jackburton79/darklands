#include "Travel.h"

#include "WorldMap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <queue>

static const int kTypeOcean = 1;
static const int kTypeMajorRiver = 2;
static const int kTypeRoad = 24;


// Minutes per pixel of movement, by tile type (DARKLAND.EXE, the
// switch at file 0x60568); 9 for the types it does not list
static const uint8 kMinutesPerPixel[32] = {
    9,			// plains
    40,			// ocean
    40, 30,		// major, minor river
    50, 50,		// tidal marsh, marsh
    17, 13,		// geest
    9, 11,		// farmland
    10, 12,		// fields and woods
    14, 16,		// light woods
    18, 27, 40, 60,	// forest, denser and denser
    20, 30, 45, 67,	// rocky, rougher and rougher
    98, 197,	// alps
    6,			// road
    9, 9, 9, 9, 9, 9, 9	// ford, river, bridge, castle, city, flags
};


// Frames to move between two pixels: one a frame horizontally, one
// every other frame vertically (DARKLAND.EXE)
static int
Frames(const GFX::point& a, const GFX::point& b)
{
    return std::max(std::abs(b.x - a.x), 2 * std::abs(b.y - a.y));
}


bool
IsPassable(const WorldMap& map, uint16 x, uint16 y)
{
    return x < map.Width() && y < map.Height()
        && map.TileTypeAt(x, y) != kTypeOcean
        && map.TileTypeAt(x, y) != kTypeMajorRiver;
}


// A lower bound of the minutes from a tile to another: the frames of the
// straight line at the lowest cost, the road's
static float
Estimate(const WorldMap& map, int ax, int ay, int bx, int by)
{
    return float(Frames(map.TileCenter(ax, ay), map.TileCenter(bx, by))
        * kMinutesPerPixel[kTypeRoad]);
}


// The minutes between two neighbors: half the frames on each tile
static float
StepMinutes(const WorldMap& map, int ax, int ay, int bx, int by)
{
    const int frames = Frames(map.TileCenter(ax, ay), map.TileCenter(bx, by));
    return frames * (kMinutesPerPixel[map.TileTypeAt(ax, ay) & 31]
        + kMinutesPerPixel[map.TileTypeAt(bx, by) & 31]) / 2.0f;
}


std::vector<map_position>
FindPath(const WorldMap& map, const map_position& from, const map_position& to,
    bool anyGoal)
{
    std::vector<map_position> path;
    if (from == to || to.x >= map.Width() || to.y >= map.Height()
            || (!anyGoal && !IsPassable(map, to.x, to.y))
            || from.x >= map.Width() || from.y >= map.Height()) {
        return path;
    }

    const int width = map.Width();
    const size_t count = size_t(width) * map.Height();
    std::vector<float> cost(count, INFINITY);
    std::vector<int32> previous(count, -1);
    std::vector<bool> done(count, false);

    typedef std::pair<float, int32> entry;	// estimated total cost, tile
    std::priority_queue<entry, std::vector<entry>, std::greater<entry> > open;
    const int32 start = from.y * width + from.x;
    const int32 goal = to.y * width + to.x;
    cost[start] = 0;
    open.push(entry(Estimate(map, from.x, from.y, to.x, to.y), start));

    while (!open.empty()) {
        const int32 current = open.top().second;
        open.pop();
        if (done[current])
            continue;
        done[current] = true;
        if (current == goal)
            break;

        const int x = current % width;
        const int y = current / width;
        // odd rows are shifted right by half a tile
        const int west = (y & 1) ? x : x - 1;
        const int neighbors[8][2] = {
            { west, y - 1 }, { west + 1, y - 1 },	// NW, NE
            { west, y + 1 }, { west + 1, y + 1 },	// SW, SE
            { x - 1, y }, { x + 1, y },				// W, E
            { x, y - 2 }, { x, y + 2 }				// N, S
        };
        for (const auto& n : neighbors) {
            const int nx = n[0];
            const int ny = n[1];
            if (nx < 0 || ny < 0 || nx >= width || ny >= map.Height())
                continue;
            const int32 next = ny * width + nx;
            if (next != goal && !IsPassable(map, nx, ny))
                continue;
            const float newCost = cost[current] + StepMinutes(map, x, y, nx, ny);
            if (newCost < cost[next]) {
                cost[next] = newCost;
                previous[next] = current;
                open.push(entry(newCost + Estimate(map, nx, ny, to.x, to.y), next));
            }
        }
    }

    if (previous[goal] < 0)
        return path;
    for (int32 tile = goal; tile != start; tile = previous[tile])
        path.push_back(map_position{ uint16(tile % width), uint16(tile / width) });
    std::reverse(path.begin(), path.end());
    return path;
}


uint32
TravelHours(const WorldMap& map, const map_position& from,
    const map_position& to, int& accumulator)
{
    const GFX::point a = map.TileCenter(from.x, from.y);
    const GFX::point b = map.TileCenter(to.x, to.y);
    const int dx = b.x - a.x;
    const int dy = b.y - a.y;
    const int frames = std::max(std::abs(dx), 2 * std::abs(dy));
    uint32 hours = 0;
    for (int frame = 1; frame <= frames; frame++) {
        const GFX::point p(a.x + dx * frame / frames, a.y + dy * frame / frames);
        uint16 x = to.x, y = to.y;
        map.TileAtPixel(p, x, y);
        accumulator += kMinutesPerPixel[map.TileTypeAt(x, y) & 31];
        if (accumulator >= 60) {
            hours++;
            accumulator -= 60;
        }
    }
    return hours;
}
