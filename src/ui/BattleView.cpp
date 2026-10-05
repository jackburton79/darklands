#include "BattleView.h"

#include "BattleMap.h"
#include "Bitmap.h"
#include "Catalog.h"
#include "EnemyFile.h"
#include "ExeData.h"
#include "GameData.h"
#include "Character.h"
#include "ImcFile.h"
#include "ImgFile.h"
#include "MenuBar.h"
#include "Palette.h"
#include "ListFile.h"
#include "ScreenSupport.h"
#include "Stream.h"
#include "TextSupport.h"
#include "TradeView.h"

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

// A frame of a walk, in milliseconds: a step takes kFramesPerStep of them
// (the game's speed is not known)
static const Uint32 kFrameTicks			= 60;

// The white damage numbers of BATTLEGR.IMG: picture n shows "-n", 1..42
// (43.. are the same in red: when the game uses which is not known)
static const int kMaxDamagePicture		= 42;

static const uint32 kFontIndex			= 2;	// FONTS.FNT, as the cards


BattleView::BattleView(GameData& data)
    :
    fData(data),
    fExe(new ExeData(data.PathFor("DARKLAND.EXE"))),
    fBuffer(new Bitmap(kScreenWidth, kScreenHeight, 8)),
    fPalette(data.SpritePalette("")),
    fSelected(-1),
    fPictures(new ImgFile(data.PathFor("BATTLEGR.IMG"))),
    fFont(new Font(data.Fonts(), kFontIndex)),
    fRandom(std::random_device()()),
    fMenu(NULL),
    fTicks(0),
    fEnemiesActive(true),
    fOrigin(0, 0),
    fMouse(0, 0),
    fCursorVisible(false),
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
    fSelected = -1;
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


BattleView::figure
BattleView::_MakeFigure(const std::string& image, int weapon, int x, int y,
    int direction)
{
    figure f;
    f.sprites = _LoadSprites(image, "CB", weapon);
    f.walk = _LoadSprites(image, "WK", weapon);
    f.death = _LoadSprites(image, "DY", -1);
    f.frame = 0;
    f.ticks = 0;
    f.x = x;
    f.y = y;
    f.direction = direction;
    f.colors = -1;
    f.member = -1;
    f.enemyType = -1;
    f.orderTarget = -1;
    f.stats = fighter();
    f.target = -1;
    f.strikeFrame = 0;
    f.fallFrame = 0;
    f.damage = 0;
    f.damageTicks = 0;
    f.reload = 0;
    f.shotTicks = 0;
    f.shotX = 0;
    f.shotY = 0;
    return f;
}


void
BattleView::AddPartyMember(int member, const character& who,
    const std::string& image, const std::vector<uint8>& colors, int x, int y,
    int direction)
{
    figure f = _MakeFigure(image, who.equipment[EQUIPMENT_WEAPON], x, y,
        direction);
    f.colors = kPartyColors + kFigureColors * member;
    f.member = member;
    f.name = who.shortName;
    f.stats = FighterFromCharacter(who, *fExe);
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
BattleView::AddEnemy(uint32 index, int x, int y, int direction)
{
    const enemy_type& type = fData.Enemies().TypeAt(index);
    figure f = _MakeFigure(type.image, type.weapon, x, y, direction);
    f.enemyType = int(index);
    f.stats = FighterFromEnemy(type, *fExe);
    // the enemies of different kinds may share palette indices: the last
    // one's colors win
    fData.ApplyEnemyColors(fPalette, type.image);
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


battle_position
BattleView::FigurePosition(int index) const
{
    const figure& f = fFigures.at(size_t(index));
    return battle_position{ f.x, f.y };
}


const fighter&
BattleView::FigureFighter(int index) const
{
    return fFigures.at(size_t(index)).stats;
}


void
BattleView::SelectMember(int member)
{
    fSelected = -1;
    for (size_t i = 0; i < fFigures.size(); i++) {
        if (member >= 0 && fFigures[i].member == member)
            fSelected = int(i);
    }
}


void
BattleView::HaltSelected()
{
    if (fSelected < 0)
        return;
    fFigures[size_t(fSelected)].path.clear();
    fFigures[size_t(fSelected)].orderTarget = -1;
}


int
BattleView::FigureAt(int x, int y) const
{
    for (size_t i = 0; i < fFigures.size(); i++) {
        if (fFigures[i].x == x && fFigures[i].y == y
                && fFigures[i].stats.status == FIGHTER_ACTIVE)
            return int(i);
    }
    return -1;
}


bool
BattleView::AttackFigure(int foe)
{
    if (fSelected < 0 || foe < 0 || size_t(foe) >= fFigures.size())
        return false;
    figure& attacker = fFigures[size_t(fSelected)];
    const figure& target = fFigures[size_t(foe)];
    if (attacker.stats.status != FIGHTER_ACTIVE
            || target.stats.status != FIGHTER_ACTIVE
            || !_Hostile(attacker, target))
        return false;
    attacker.orderTarget = foe;
    attacker.path.clear();
    return true;
}


int
BattleView::SelectedTarget() const
{
    return fSelected >= 0 ? fFigures[size_t(fSelected)].orderTarget : -1;
}


void
BattleView::SetSelectedStance(int stance)
{
    if (fSelected >= 0 && fFigures[size_t(fSelected)].member >= 0)
        fFigures[size_t(fSelected)].stats.orders = stance;
}


int
BattleView::SelectedStance() const
{
    return fSelected >= 0 ? fFigures[size_t(fSelected)].stats.orders
        : STANCE_STANDARD;
}


bool
BattleView::SetSelectedMissile()
{
    if (fSelected < 0 || fFigures[size_t(fSelected)].member < 0
            || fFigures[size_t(fSelected)].stats.status != FIGHTER_ACTIVE
            || !CanShoot(fFigures[size_t(fSelected)].stats))
        return false;
    fFigures[size_t(fSelected)].stats.orders = STANCE_MISSILE;
    return true;
}


void
BattleView::SelectNext()
{
    const int count = int(fFigures.size());
    for (int step = 1; step <= count; step++) {
        const int i = ((fSelected < 0 ? -1 : fSelected) + step) % count;
        if (fFigures[size_t(i)].member >= 0
                && fFigures[size_t(i)].stats.status == FIGHTER_ACTIVE) {
            fSelected = i;
            return;
        }
    }
}


int
BattleView::SelectedMember() const
{
    return fSelected >= 0 ? fFigures[size_t(fSelected)].member : -1;
}


bool
BattleView::MoveSelectedTo(int x, int y)
{
    if (fSelected < 0 || !fMap)
        return false;
    figure& mover = fFigures[size_t(fSelected)];
    if (mover.stats.status != FIGHTER_ACTIVE)
        return false;
    mover.orderTarget = -1;
    std::vector<battle_position> path = FindBattlePath(*fMap,
        battle_position{ mover.x, mover.y }, battle_position{ x, y },
        _Occupied(&mover));
    if (path.empty())
        return false;
    mover.path = path;
    return true;
}


bool
BattleView::IsMoving() const
{
    for (const figure& f : fFigures) {
        if (!f.path.empty())
            return true;
    }
    return false;
}


// The sprites' 8 columns, clockwise from up (inferred from the sheets)
static int
DirectionOf(int dx, int dy)
{
    static const int kDirections[3][3] = {
        { 7, 0, 1 },	// dy = -1
        { 6, 0, 2 },	// dy = 0
        { 5, 4, 3 }		// dy = 1
    };
    return kDirections[dy + 1][dx + 1];
}


void
BattleView::Tick()
{
    for (figure& f : fFigures) {
        if (f.damageTicks > 0)
            f.damageTicks--;
        if (f.shotTicks > 0)
            f.shotTicks--;
        if (f.stats.status != FIGHTER_ACTIVE) {
            if (f.fallFrame < f.death->CountFrames() - 1)
                f.fallFrame++;
            f.path.clear();
            continue;
        }
        if (f.strikeFrame > 0 && ++f.strikeFrame >= f.sprites->CountFrames())
            f.strikeFrame = 0;

        const bool enemy = f.member < 0 && fEnemiesActive;
        if (enemy && f.path.empty() && !_PlanEnemy(f)) {
            f.frame = 0;
            f.ticks = 0;
        }
        // a member sent against a foe: stand beside it, or step toward it
        // (it may have moved: one step at a time)
        bool chasing = false;
        if (f.member >= 0 && f.orderTarget >= 0) {
            const figure& foe = fFigures[size_t(f.orderTarget)];
            if (foe.stats.status != FIGHTER_ACTIVE) {
                f.orderTarget = -1;
            } else if ((std::abs(foe.x - f.x) <= 1
                    && std::abs(foe.y - f.y) <= 1)
                || (f.stats.orders == STANCE_MISSILE && CanShoot(f.stats)
                    && std::max(std::abs(foe.x - f.x), std::abs(foe.y - f.y))
                        <= MissileRange(f.stats, *fExe)
                    && _HasLineOfFire(f, foe))) {
                f.direction = DirectionOf(foe.x - f.x, foe.y - f.y);
                f.path.clear();
                f.frame = 0;
                f.ticks = 0;
            } else {
                chasing = true;
                if (f.path.empty())
                    _StepToward(f, foe);
            }
        }
        if (f.path.empty())
            continue;
        f.frame = (f.frame + 1) % f.walk->CountFrames();
        if (f.ticks++ % kFramesPerStep != 0)
            continue;
        const battle_position next = f.path.front();
        // another figure may have stepped in since the path was found
        bool taken = false;
        for (const figure& other : fFigures) {
            taken = taken || (&other != &f && other.x == next.x
                && other.y == next.y && other.stats.status == FIGHTER_ACTIVE);
        }
        if (taken) {
            f.path.clear();
            f.frame = 0;
            f.ticks = 0;
            continue;
        }
        f.direction = DirectionOf(next.x - f.x, next.y - f.y);
        f.x = next.x;
        f.y = next.y;
        f.path.erase(f.path.begin());
        // an enemy plans its next step at the next tick: keep its pace
        if (f.path.empty() && !enemy && !chasing) {
            f.frame = 0;
            f.ticks = 0;
        }
    }
    if (++fTicks % kCombatTicks == 0)
        _Fight();
}


bool
BattleView::_Hostile(const figure& a, const figure& b) const
{
    return (a.member < 0) != (b.member < 0);
}


// Every standing figure next to a standing foe, and not walking, fights
// it: its current target if still there, else the first one found
void
BattleView::_Fight()
{
    const std::vector<bool> shot = _Shoot();
    for (size_t i = 0; i < fFigures.size(); i++) {
        figure& f = fFigures[i];
        f.target = -1;
        if (shot[i] || f.stats.status != FIGHTER_ACTIVE || !f.path.empty())
            continue;
        // the foe it was sent against, once beside it, and no other
        if (f.orderTarget >= 0) {
            const figure& ordered = fFigures[size_t(f.orderTarget)];
            if (ordered.stats.status == FIGHTER_ACTIVE
                    && std::abs(ordered.x - f.x) <= 1
                    && std::abs(ordered.y - f.y) <= 1) {
                f.target = f.orderTarget;
                continue;
            }
        }
        for (size_t j = 0; j < fFigures.size() && f.target < 0; j++) {
            const figure& foe = fFigures[j];
            if (foe.stats.status == FIGHTER_ACTIVE && _Hostile(f, foe)
                && std::abs(foe.x - f.x) <= 1 && std::abs(foe.y - f.y) <= 1) {
                f.target = int(j);
            }
        }
    }
    for (size_t i = 0; i < fFigures.size(); i++) {
        figure& f = fFigures[i];
        if (f.target < 0 || f.stats.status != FIGHTER_ACTIVE)
            continue;
        figure& foe = fFigures[size_t(f.target)];
        if (foe.stats.status != FIGHTER_ACTIVE)
            continue;
        // as the game counts them: those fighting the defender (the
        // attacker too) and those fighting the attacker
        int helpers = 0;
        int threats = 0;
        for (const figure& other : fFigures) {
            if (other.stats.status != FIGHTER_ACTIVE)
                continue;
            helpers += other.target == f.target;
            threats += other.target == int(i);
        }
        f.direction = DirectionOf(foe.x - f.x, foe.y - f.y);
        const strike blow = Strike(f.stats, foe.stats, *fExe, helpers,
            threats, fRandom);
        if (blow.result == STRIKE_NONE)
            continue;
        f.strikeFrame = 1;
        if (blow.result == STRIKE_HIT || blow.result == STRIKE_WEAK_HIT) {
            TakeStrike(foe.stats, blow);
            foe.damage = blow.endurance;
            foe.damageTicks = 2 * kCombatTicks;
            if (foe.stats.status != FIGHTER_ACTIVE) {
                foe.path.clear();
                foe.fallFrame = 0;
            }
        }
    }
}


bool
BattleView::_HasLineOfFire(const figure& from, const figure& to) const
{
    if (!fMap)
        return true;
    std::vector<battle_position> others;
    for (const figure& f : fFigures) {
        if (&f != &from && &f != &to && f.stats.status == FIGHTER_ACTIVE)
            others.push_back(battle_position{ f.x, f.y });
    }
    return HasLineOfFire(*fMap, battle_position{ from.x, from.y },
        battle_position{ to.x, to.y }, others);
}


// The foe a figure in the Use Missile order shoots at: the one it was
// sent against if it is in range, else the nearest standing one in
// range; -1 if none
int
BattleView::_ShotTarget(const figure& shooter) const
{
    const int range = MissileRange(shooter.stats, *fExe);
    int best = -1;
    int bestDistance = range + 1;
    for (size_t j = 0; j < fFigures.size(); j++) {
        const figure& foe = fFigures[j];
        if (foe.stats.status != FIGHTER_ACTIVE || !_Hostile(shooter, foe))
            continue;
        const int distance = std::max(std::abs(foe.x - shooter.x),
            std::abs(foe.y - shooter.y));
        if (distance <= range && !_HasLineOfFire(shooter, foe))
            continue;
        if (int(j) == shooter.orderTarget && distance <= range)
            return int(j);
        if (distance < bestDistance) {
            best = int(j);
            bestDistance = distance;
        }
    }
    return best;
}


// Each standing member in the Use Missile order, not walking, with a
// piece to shoot and a foe in range, shoots when it has reloaded. Provisional:
// a bow or a thrown weapon every second combat step, a crossbow every
// third, a gun every fourth (the game's pace is not decoded).
std::vector<bool>
BattleView::_Shoot()
{
    std::vector<bool> shot(fFigures.size(), false);
    for (size_t i = 0; i < fFigures.size(); i++) {
        figure& f = fFigures[i];
        if (f.member < 0 || f.stats.orders != STANCE_MISSILE
                || f.stats.status != FIGHTER_ACTIVE || !f.path.empty()
                || !CanShoot(f.stats))
            continue;
        const int target = _ShotTarget(f);
        if (target < 0)
            continue;
        shot[i] = true;
        if (f.reload > 0) {
            f.reload--;
            continue;
        }
        figure& foe = fFigures[size_t(target)];
        const exe_weapon& weapon
            = fExe->Weapons()[size_t(f.stats.missileType)];
        f.reload = weapon.category == WEAPON_MISSILE_DEVICE
            ? (weapon.range >= 180 ? 3 : 2)
            : 1;
        f.direction = DirectionOf(foe.x - f.x, foe.y - f.y);
        const int distance = std::max(std::abs(foe.x - f.x),
            std::abs(foe.y - f.y));
        const strike blow = Shoot(f.stats, foe.stats, *fExe, distance,
            fRandom);
        f.strikeFrame = 1;
        f.shotTicks = 3;
        f.shotX = foe.x * kCellSize + kCellSize / 2;
        f.shotY = foe.y * kCellSize + kCellSize / 2;
        if (blow.result == STRIKE_HIT || blow.result == STRIKE_WEAK_HIT) {
            TakeStrike(foe.stats, blow);
            foe.damage = blow.endurance;
            foe.damageTicks = 2 * kCombatTicks;
            if (foe.stats.status != FIGHTER_ACTIVE) {
                foe.path.clear();
                foe.fallFrame = 0;
            }
        }
        // the last piece: back to the standard attack
        if (!CanShoot(f.stats))
            f.stats.orders = STANCE_STANDARD;
    }
    return shot;
}


std::vector<battle_position>
BattleView::_Occupied(const figure* except) const
{
    // the fallen do not block
    std::vector<battle_position> occupied;
    for (const figure& f : fFigures) {
        if (&f != except && f.stats.status == FIGHTER_ACTIVE)
            occupied.push_back(battle_position{ f.x, f.y });
    }
    return occupied;
}


// One step toward the nearest party member, along the paths; false when
// the enemy is beside one (it turns to face it) or cannot reach any
bool
BattleView::_PlanEnemy(figure& enemy)
{
    if (!fMap)
        return false;
    const figure* nearest = NULL;
    std::vector<battle_position> best;
    for (const figure& target : fFigures) {
        if (target.member < 0 || target.stats.status != FIGHTER_ACTIVE)
            continue;
        const int dx = target.x - enemy.x;
        const int dy = target.y - enemy.y;
        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) {
            enemy.direction = DirectionOf(dx, dy);
            return false;
        }
        // to the target's cell, as if it were free: the last step is
        // not taken
        std::vector<battle_position> occupied = _Occupied(&enemy);
        occupied.erase(std::remove(occupied.begin(), occupied.end(),
            battle_position{ target.x, target.y }), occupied.end());
        const std::vector<battle_position> path = FindBattlePath(*fMap,
            battle_position{ enemy.x, enemy.y },
            battle_position{ target.x, target.y }, occupied);
        if (!path.empty() && (nearest == NULL || path.size() < best.size())) {
            nearest = &target;
            best = path;
        }
    }
    if (nearest == NULL || best.size() < 2)
        return false;
    enemy.path.assign(1, best.front());
    return true;
}


// One step toward a figure along the paths, to its cell as if it were
// free (the last step is not taken); false if there is no way
bool
BattleView::_StepToward(figure& mover, const figure& goal)
{
    if (!fMap)
        return false;
    std::vector<battle_position> occupied = _Occupied(&mover);
    occupied.erase(std::remove(occupied.begin(), occupied.end(),
        battle_position{ goal.x, goal.y }), occupied.end());
    const std::vector<battle_position> path = FindBattlePath(*fMap,
        battle_position{ mover.x, mover.y },
        battle_position{ goal.x, goal.y }, occupied);
    if (path.size() < 2)
        return false;
    mover.path.assign(1, path.front());
    return true;
}


// A click: on a member selects it; on a standing foe, with a member
// selected, sends the member against it; elsewhere sends the member there
void
BattleView::Clicked(const GFX::point& point)
{
    const int x = (point.x + fOrigin.x) / kCellSize;
    const int y = (point.y + fOrigin.y) / kCellSize;
    for (size_t i = 0; i < fFigures.size(); i++) {
        const figure& f = fFigures[i];
        if (f.x != x || f.y != y)
            continue;
        if (f.member >= 0) {
            SelectMember(f.member);
            return;
        }
        if (f.stats.status == FIGHTER_ACTIVE) {
            AttackFigure(int(i));
            return;
        }
    }
    MoveSelectedTo(x, y);
}


// An animation of a sprite set, "CB" (combat) or "WK" (walking), with a
// weapon type: "E02" and 1 (a long sword, "SW") are E02CBSW.IMC in
// E00C.CAT. Without that weapon's, the set's first.
std::shared_ptr<ImcFile>
BattleView::_LoadSprites(const std::string& image, const char* set,
    int weapon)
{
    const bool enemy = !image.empty() && (image[0] == 'E' || image[0] == 'M');
    const std::string catalogName = enemy
        ? image.substr(0, 1) + "00C.CAT" : image + "C.CAT";
    std::unique_ptr<Catalog> catalog(fData.OpenCatalog(catalogName));
    const std::string prefix = image + set;
    const std::vector<exe_weapon>& weapons = fExe->Weapons();
    if (weapon >= 0 && size_t(weapon) < weapons.size()) {
        const std::string name = prefix + weapons[size_t(weapon)].code + ".IMC";
        std::unique_ptr<Stream> stream(catalog->GetStream(name));
        if (stream)
            return std::shared_ptr<ImcFile>(new ImcFile(stream.get()));
    }
    for (int32 i = 0; i < catalog->CountEntries(); i++) {
        if (catalog->EntryAt(i).filename.compare(0, prefix.size(), prefix) != 0)
            continue;
        std::unique_ptr<Stream> stream(catalog->GetStreamAt(uint32(i)));
        return std::shared_ptr<ImcFile>(new ImcFile(stream.get()));
    }
    throw std::runtime_error("no battle sprites " + prefix);
}


// The item code of the first item of that type in DARKLAND.LST, or -1
static int
ItemOfType(const ListFile& lists, int type)
{
    const std::vector<item_definition>& items = lists.Items();
    for (size_t code = 0; code < items.size(); code++) {
        if (!items[code].name.empty() && items[code].type == type)
            return int(code);
    }
    return -1;
}


// The game (file 0x18DDB) adds, for each foe, a die of the type's sides
// (none under 1; a one-sided die gives 1) plus its bonus, when over 0
money
BattleView::LootCash()
{
    uint32 coins[3] = { 0, 0, 0 };
    for (const figure& f : fFigures) {
        if (f.enemyType < 0)
            continue;
        const enemy_type& type = fData.Enemies().TypeAt(uint32(f.enemyType));
        for (int k = 0; k < 3; k++) {
            int amount = type.cashDice[k] >= 1
                ? int(fRandom() % type.cashDice[k]) + 1 : 0;
            amount += type.cashPlus[k];
            if (amount > 0)
                coins[k] += uint32(amount);
        }
    }
    return MoneyFromPfennigs(coins[0] * 240 + coins[1] * 12 + coins[2]);
}


std::vector<cache_item>
BattleView::Loot() const
{
    // real items only: not the monsters' natural weapons (35..) and hides
    static const int kFirstNaturalWeapon = 35;
    static const int kFirstArmor = 67;
    static const int kLastArmor = 84;
    static const int kFirstShield = 95;
    static const int kLastShield = 97;

    std::vector<cache_item> pile;
    const ListFile& lists = fData.Lists();
    const auto add = [&](int type, int quality) {
        const int code = ItemOfType(lists, type);
        if (code < 0)
            return;
        for (cache_item& stored : pile) {
            if (stored.code == code && stored.quality == quality
                    && stored.count < 255) {
                stored.count++;
                return;
            }
        }
        pile.push_back(cache_item{ uint16(code), uint8(quality), 1 });
    };
    for (const figure& f : fFigures) {
        if (f.enemyType < 0 || f.stats.status == FIGHTER_ACTIVE)
            continue;
        const enemy_type& type = fData.Enemies().TypeAt(uint32(f.enemyType));
        if (type.weapon < kFirstNaturalWeapon)
            add(type.weapon, type.armorQuality);
        for (int l = 0; l < 2; l++) {
            if (type.armor[l] >= kFirstArmor && type.armor[l] <= kLastArmor)
                add(type.armor[l], type.armorQuality);
        }
        if (type.shield >= kFirstShield && type.shield <= kLastShield)
            add(type.shield, type.shieldQuality);
    }
    return pile;
}


battle_outcome
BattleView::Outcome() const
{
    bool partyStanding = false;
    bool enemyStanding = false;
    bool enemies = false;
    for (const figure& f : fFigures) {
        const bool standing = f.stats.status == FIGHTER_ACTIVE;
        if (f.member < 0) {
            enemies = true;
            enemyStanding = enemyStanding || standing;
        } else
            partyStanding = partyStanding || standing;
    }
    if (!partyStanding)
        return BATTLE_LOST;
    if (enemies && !enemyStanding)
        return BATTLE_WON;
    return BATTLE_GOING_ON;
}


namespace {

// The menu bar as a battle needs it, put back when the battle is left
struct BattleMenu {
    explicit BattleMenu(MenuBar* menu)
        :
        bar(menu)
    {
        if (bar == NULL)
            return;
        bar->SetEnabled(MENU_SAVE_GAME, false);
        bar->SetEnabled(MENU_LOAD_GAME, false);
        bar->SetEnabled(MENU_PARTY_INFO, false);
        bar->SetEnabled(MENU_MARCHING_ORDER, false);
    }

    ~BattleMenu()
    {
        if (bar == NULL)
            return;
        bar->SetEnabled(MENU_SAVE_GAME, true);
        bar->SetEnabled(MENU_LOAD_GAME, true);
        bar->SetEnabled(MENU_PARTY_INFO, true);
        bar->SetEnabled(MENU_MARCHING_ORDER, true);
        bar->SetEnabled(MENU_RESUME, false);
        bar->SetEnabled(MENU_HALT, false);
        bar->SetEnabled(MENU_STD_ATTACK, false);
        bar->SetEnabled(MENU_VULNERABLE, false);
        bar->SetEnabled(MENU_BERSERK, false);
        bar->SetEnabled(MENU_PARRY, false);
        bar->SetStance(MENU_NONE);
    }

    MenuBar*	bar;
};

}	// namespace


battle_outcome
BattleView::Run(GameWindow& window)
{
    BattleMenu menuState(fMenu);
    bool dirty = true;
    Uint32 lastStep = 0;
    for (;;) {
        if (fMenu != NULL) {
            // Resume starts the enemies; Halt needs a member selected
            fMenu->SetEnabled(MENU_RESUME, !fEnemiesActive);
            fMenu->SetEnabled(MENU_HALT, fSelected >= 0);
            // the way the selected member fights, checked in the menu
            const bool member = fSelected >= 0
                && fFigures[size_t(fSelected)].member >= 0
                && fFigures[size_t(fSelected)].stats.status == FIGHTER_ACTIVE;
            static const menu_command kStances[4] = { MENU_STD_ATTACK,
                MENU_VULNERABLE, MENU_BERSERK, MENU_PARRY };
            for (menu_command stance : kStances)
                fMenu->SetEnabled(stance, member);
            fMenu->SetEnabled(MENU_USE_MISSILE, member
                && CanShoot(fFigures[size_t(fSelected)].stats));
            const int stance = SelectedStance();
            fMenu->SetStance(!member ? MENU_NONE
                : stance == STANCE_VULNERABLE ? MENU_VULNERABLE
                : stance == STANCE_BERSERK ? MENU_BERSERK
                : stance == STANCE_PARRY ? MENU_PARRY
                : stance == STANCE_MISSILE ? MENU_USE_MISSILE
                : MENU_STD_ATTACK);
        }
        const battle_outcome outcome = Outcome();
        if (dirty) {
            Draw();
            if (outcome == BATTLE_WON)
                _DrawMessage("Victory! The enemies are down.");
            else if (outcome == BATTLE_LOST)
                _DrawMessage("The party has fallen.");
            window.Show(fBuffer);
            dirty = false;
        }
        const Uint32 now = SDL_GetTicks();
        if (now - lastStep >= kFrameTicks) {
            Tick();
            lastStep = now;
            dirty = true;
        }
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 20) == 0)
            continue;
        if (fMenu != NULL && outcome == BATTLE_GOING_ON) {
            menu_command command = MENU_NONE;
            if (MenuBar::Opens(event)) {
                command = fMenu->Run(window, Draw(), event);
                dirty = true;
            } else if (event.type == SDL_KEYDOWN) {
                command = fMenu->Shortcut(event.key.keysym.sym,
                    event.key.keysym.mod);
            }
            if (command == MENU_QUIT) {
                MenuBar::PostQuit();	// for the screen below, which quits
                return BATTLE_LEFT;
            }
            if (command != MENU_NONE) {
                if (command == MENU_RESUME)
                    SetEnemiesActive(true);
                else if (command == MENU_HALT)
                    HaltSelected();
                else if (command == MENU_STD_ATTACK)
                    SetSelectedStance(STANCE_STANDARD);
                else if (command == MENU_VULNERABLE)
                    SetSelectedStance(STANCE_VULNERABLE);
                else if (command == MENU_BERSERK)
                    SetSelectedStance(STANCE_BERSERK);
                else if (command == MENU_PARRY)
                    SetSelectedStance(STANCE_PARRY);
                else if (command == MENU_USE_MISSILE)
                    SetSelectedMissile();
                else if (command == MENU_PAUSE)
                    MenuBar::Pause(window);
                dirty = true;
                lastStep = SDL_GetTicks();
                continue;
            }
            if (event.type == SDL_MOUSEBUTTONUP
                    && event.button.button == SDL_BUTTON_RIGHT)
                continue;
        }
        switch (event.type) {
            case SDL_QUIT:
                SDL_PushEvent(&event);	// for the screen below, which quits
                return outcome != BATTLE_GOING_ON ? outcome : BATTLE_LEFT;
            case SDL_MOUSEMOTION:
                fMouse = GameWindow::ToScreen(event.motion.x, event.motion.y);
                fCursorVisible = true;
                dirty = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (outcome != BATTLE_GOING_ON)
                    return outcome;
                if (event.button.button == SDL_BUTTON_LEFT) {
                    Clicked(GameWindow::ToScreen(event.button.x,
                        event.button.y));
                    lastStep = SDL_GetTicks();
                    dirty = true;
                }
                break;
            case SDL_KEYDOWN:
                if (outcome != BATTLE_GOING_ON)
                    return outcome;
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return BATTLE_LEFT;
                    case SDLK_SPACE:
                        SetEnemiesActive(!fEnemiesActive);
                        break;
                    case SDLK_TAB:
                        SelectNext();
                        break;
                    case SDLK_1:
                    case SDLK_2:
                    case SDLK_3:
                    case SDLK_4:
                    case SDLK_5:
                        SelectMember(int(event.key.keysym.sym - SDLK_1));
                        break;
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
                if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                    fCursorVisible = false;
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
    // the fallen lie under the others
    std::stable_sort(order.begin(), order.end(),
        [](const figure* a, const figure* b) {
            const bool aDown = a->stats.status != FIGHTER_ACTIVE;
            const bool bDown = b->stats.status != FIGHTER_ACTIVE;
            if (aDown != bDown)
                return aDown;
            return a->y < b->y;
        });
    for (const figure* f : order) {
        if (fSelected >= 0 && f == &fFigures[size_t(fSelected)]) {
            // the selected member: a frame around its cell
            fBuffer->StrokeRect(GFX::rect(f->x * kCellSize - fOrigin.x,
                f->y * kCellSize - fOrigin.y, kCellSize, kCellSize),
                kYellow + 7);
        }
        _DrawFigure(*f);
    }
    // the shots in flight: a line to where they went
    for (const figure& f : fFigures) {
        if (f.shotTicks > 0) {
            fBuffer->StrokeLine(f.x * kCellSize + kCellSize / 2 - fOrigin.x,
                f.y * kCellSize + kCellSize / 2 - fOrigin.y,
                f.shotX - fOrigin.x, f.shotY - fOrigin.y, kYellow + 7);
        }
    }
    // the foe the selected member was sent against: a red frame
    const int target = SelectedTarget();
    if (target >= 0 && fFigures[size_t(target)].stats.status == FIGHTER_ACTIVE) {
        const figure& foe = fFigures[size_t(target)];
        fBuffer->StrokeRect(GFX::rect(foe.x * kCellSize - fOrigin.x,
            foe.y * kCellSize - fOrigin.y, kCellSize, kCellSize), kRed + 7);
    }
    _DrawStatus();
    if (fCursorVisible) {
        // libjgame hides the system cursor
        DrawMouseCursor(fBuffer, fMouse, NearestColor(fPalette, 0, 0, 0),
            NearestColor(fPalette, 255, 255, 255));
    }
    return fBuffer;
}


// The party along the bottom: the number that selects a member, his name,
// his Endurance and the letter of his stance (S, V, B, P); the selected
// one is lit, the fallen are dim. Provisional: the game's own panel is
// not decoded
void
BattleView::_DrawStatus()
{
    const int height = fFont->Height() + 3;
    const int top = kScreenHeight - height;
    fBuffer->FillRect(GFX::rect(0, top, kScreenWidth, height), kGray + 1);
    int slots = 0;
    for (const figure& f : fFigures)
        slots += f.member >= 0;
    if (slots == 0)
        return;
    const int width = kScreenWidth / slots;
    int slot = 0;
    for (size_t i = 0; i < fFigures.size(); i++) {
        const figure& f = fFigures[i];
        if (f.member < 0)
            continue;
        const char letter = f.stats.orders == STANCE_VULNERABLE ? 'V'
            : f.stats.orders == STANCE_BERSERK ? 'B'
            : f.stats.orders == STANCE_PARRY ? 'P'
            : f.stats.orders == STANCE_MISSILE ? 'M' : 'S';
        std::string name = f.name;
        std::string text;
        for (;;) {
            text = std::to_string(f.member + 1) + " " + name + " "
                + std::to_string(std::max(f.stats.endurance, 0)) + " "
                + letter;
            if (name.size() <= 3 || fFont->StringWidth(
                    Font::ToGameCharset(text)) <= width - 4)
                break;
            name.erase(name.size() - 1);
        }
        const bool selected = int(i) == fSelected;
        const bool down = f.stats.status != FIGHTER_ACTIVE;
        if (selected) {
            fBuffer->FillRect(GFX::rect(slot * width, top, width, height),
                kGray + 4);
        }
        fFont->RenderString(Font::ToGameCharset(text), fBuffer,
            GFX::point(slot * width + 2, top + 2),
            down ? kGray + 3 : selected ? kYellow + 7 : kGray + 7);
        slot++;
    }
}


// A framed line in the middle of the screen, and how to go on
void
BattleView::_DrawMessage(const std::string& text)
{
    const std::string lines[2] = { text, "(press a key)" };
    int width = 0;
    for (const std::string& line : lines) {
        width = std::max(width,
            int(fFont->StringWidth(Font::ToGameCharset(line))));
    }
    const int height = 2 * fFont->Height() + 12;
    const int left = (kScreenWidth - width) / 2 - 8;
    const int top = (kScreenHeight - height) / 2;
    fBuffer->FillRect(GFX::rect(left, top, width + 16, height), kGray + 1);
    fBuffer->StrokeRect(GFX::rect(left, top, width + 16, height),
        kYellow + 7);
    for (int i = 0; i < 2; i++) {
        const std::string line = Font::ToGameCharset(lines[i]);
        const int x = (kScreenWidth - fFont->StringWidth(line)) / 2;
        fFont->RenderString(line, fBuffer,
            GFX::point(x, top + 5 + i * (fFont->Height() + 2)),
            i == 0 ? kYellow + 7 : kGray + 7);
    }
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
    const ImcFile* sprites = f.sprites.get();
    int frame = f.strikeFrame;
    if (f.stats.status != FIGHTER_ACTIVE) {
        sprites = f.death.get();
        frame = std::min(f.fallFrame, sprites->CountFrames() - 1);
    } else if (!f.path.empty()) {
        sprites = f.walk.get();
        frame = f.frame;
    }
    const sprite& picture = sprites->SpriteAt(frame,
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

    // the last loss of Endurance: BATTLEGR.IMG's picture n is "-n"
    if (f.damageTicks > 0 && f.damage > 0) {
        const sprite& number = fPictures->PictureAt(uint32(std::min(f.damage,
            kMaxDamagePicture)));
        const int numberLeft = f.x * kCellSize + kCellSize / 2
            - number.width / 2 - fOrigin.x;
        const int numberTop = top - number.height - 1;
        for (int y = 0; y < number.height; y++) {
            for (int x = 0; x < number.width; x++) {
                const uint8 pixel = number.pixels[y * number.width + x];
                const int screenX = numberLeft + x;
                const int screenY = numberTop + y;
                if (pixel != 0 && screenX >= 0 && screenX < kScreenWidth
                    && screenY >= 0 && screenY < kScreenHeight) {
                    fBuffer->PutPixel(screenX, screenY, pixel);
                }
            }
        }
    }
}
