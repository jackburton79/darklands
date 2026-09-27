#include "BattlePath.h"

#include "BattleMap.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <utility>

static const int kSize = BattleMap::kSize;


static bool
IsOpen(const BattleMap& map, int x, int y)
{
    return x >= 0 && x < kSize && y >= 0 && y < kSize
        && !map.CellAt(x, y).Closed();
}


// A straight step: the side of each cell toward the other
static bool
CanStepStraight(const BattleMap& map, int x, int y, int dx, int dy)
{
    if (!IsOpen(map, x, y) || !IsOpen(map, x + dx, y + dy))
        return false;
    battle_side from;
    battle_side to;
    if (dx > 0) {
        from = SIDE_NEXT_COLUMN;
        to = SIDE_PREVIOUS_COLUMN;
    } else if (dx < 0) {
        from = SIDE_PREVIOUS_COLUMN;
        to = SIDE_NEXT_COLUMN;
    } else if (dy > 0) {
        from = SIDE_NEXT_ROW;
        to = SIDE_PREVIOUS_ROW;
    } else {
        from = SIDE_PREVIOUS_ROW;
        to = SIDE_NEXT_ROW;
    }
    return map.CellAt(x, y).Wall(from) == 0
        && map.CellAt(x + dx, y + dy).Wall(to) == 0;
}


bool
CanStep(const BattleMap& map, int x, int y, int dx, int dy)
{
    if ((dx == 0 && dy == 0) || dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return false;
    if (dx == 0 || dy == 0)
        return CanStepStraight(map, x, y, dx, dy);
    return CanStepStraight(map, x, y, dx, 0)
        && CanStepStraight(map, x + dx, y, 0, dy)
        && CanStepStraight(map, x, y, 0, dy)
        && CanStepStraight(map, x, y + dy, dx, 0);
}


std::vector<battle_position>
FindBattlePath(const BattleMap& map, const battle_position& from,
    const battle_position& to, const std::vector<battle_position>& occupied)
{
    std::vector<battle_position> path;
    if (from == to || !IsOpen(map, to.x, to.y)
        || std::find(occupied.begin(), occupied.end(), to) != occupied.end()) {
        return path;
    }

    const int kUnreached = -1;
    std::vector<int> cost(kSize * kSize, kUnreached);
    std::vector<int> previous(kSize * kSize, -1);
    std::vector<bool> blocked(kSize * kSize, false);
    for (const battle_position& p : occupied) {
        if (p.x >= 0 && p.x < kSize && p.y >= 0 && p.y < kSize)
            blocked[p.y * kSize + p.x] = true;
    }

    typedef std::pair<int, int> entry;		// cost, cell
    std::priority_queue<entry, std::vector<entry>, std::greater<entry>> open;
    const int start = from.y * kSize + from.x;
    const int goal = to.y * kSize + to.x;
    cost[start] = 0;
    open.push(entry(0, start));
    while (!open.empty()) {
        const entry current = open.top();
        open.pop();
        const int cell = current.second;
        if (current.first != cost[cell])
            continue;
        if (cell == goal)
            break;
        const int x = cell % kSize;
        const int y = cell / kSize;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                const int next = (y + dy) * kSize + x + dx;
                if (!CanStep(map, x, y, dx, dy) || blocked[next])
                    continue;
                const int nextCost = current.first
                    + (dx != 0 && dy != 0 ? 3 : 2);
                if (cost[next] == kUnreached || nextCost < cost[next]) {
                    cost[next] = nextCost;
                    previous[next] = cell;
                    open.push(entry(nextCost, next));
                }
            }
        }
    }
    if (cost[goal] == kUnreached)
        return path;
    for (int cell = goal; cell != start; cell = previous[cell])
        path.push_back(battle_position{ cell % kSize, cell / kSize });
    std::reverse(path.begin(), path.end());
    return path;
}
