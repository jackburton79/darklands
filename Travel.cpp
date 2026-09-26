#include "Travel.h"

#include "WorldMap.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>

static const int kTypeOcean = 1;


bool
IsPassable(const WorldMap& map, uint16 x, uint16 y)
{
    return x < map.Width() && y < map.Height()
        && map.TileTypeAt(x, y) != kTypeOcean;
}


static float
Distance(const WorldMap& map, int ax, int ay, int bx, int by)
{
    const GFX::point a = map.TileCenter(ax, ay);
    const GFX::point b = map.TileCenter(bx, by);
    return std::hypot(float(a.x - b.x), float(a.y - b.y));
}


std::vector<map_position>
FindPath(const WorldMap& map, const map_position& from, const map_position& to)
{
    std::vector<map_position> path;
    if (from == to || !IsPassable(map, to.x, to.y)
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
    open.push(entry(Distance(map, from.x, from.y, to.x, to.y), start));

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
            if (nx < 0 || ny < 0 || nx >= width || ny >= map.Height()
                    || !IsPassable(map, nx, ny)) {
                continue;
            }
            const int32 next = ny * width + nx;
            const float newCost = cost[current] + Distance(map, x, y, nx, ny);
            if (newCost < cost[next]) {
                cost[next] = newCost;
                previous[next] = current;
                open.push(entry(newCost + Distance(map, nx, ny, to.x, to.y), next));
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
