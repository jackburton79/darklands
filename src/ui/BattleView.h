/*
 * BattleView.h
 * A provisional view of a battlefield, seen from above: the map's cells
 * as colored squares, their walls as lines, their objects as marks, and
 * the figures of the party and of the enemies (their battle sprites). The
 * game's own drawing is not decoded yet (docs/exe.md, "Battles"); this
 * shows what the maps hold, with the battle's colors.
 *
 * The input handlers and Draw() work without a window, for testing.
 */
#pragma once

#include "BattlePath.h"
#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <memory>
#include <string>
#include <vector>

class BattleMap;
class Bitmap;
class GameData;
class GameWindow;
class ImcFile;
struct battle_cell;

class BattleView {
public:
    static const int	kCellSize = 16;		// pixels per cell

    explicit		BattleView(GameData& data);
                    ~BattleView();

    // The map and its name in IMAPS.CAT ("ICITY.000"), which tells the
    // kind of place
    void			SetMap(std::unique_ptr<BattleMap> map,
                        const std::string& name);

    // Figures stand on open cells; direction 0..7 is the column of their
    // sprites (0 seems to face up, 4 down). Party members wear the colors
    // of party::colors (member: 0..4). They throw if the sprites are
    // missing.
    void			AddPartyMember(int member, const std::string& image,
                        const std::vector<uint8>& colors, int x, int y,
                        int direction);
    void			AddEnemy(const std::string& image, int x, int y,
                        int direction);
    int				CountFigures() const	{ return int(fFigures.size()); }
    battle_position	FigurePosition(int figure) const;

    // Moving the party: select a member (0..4, -1 none), then send it to
    // a cell. Each Tick() shows the next frame of the walking figures'
    // walk; every kFramesPerStep ticks they step to the next cell of
    // their path. MoveSelectedTo() returns false if the cell cannot be
    // reached.
    static const int	kFramesPerStep = 2;
    void			SelectMember(int member);
    int				SelectedMember() const;
    bool			MoveSelectedTo(int x, int y);
    bool			IsMoving() const;
    void			Tick();

    // The enemies walk, a step at a time, toward the nearest party member
    // (along the paths) and stop beside it, facing it. Active at first.
    void			SetEnemiesActive(bool active)	{ fEnemiesActive = active; }
    bool			EnemiesActive() const	{ return fEnemiesActive; }

    // A click on the screen (320x200 coordinates): selects the party
    // member there, or sends the selected one to that cell.
    void			Clicked(const GFX::point& point);
    // The open cell without a figure nearest to (x, y), in rings around
    // it; false if there is none.
    bool			FindFreeCell(int& x, int& y) const;

    // Runs until Esc; the arrow keys scroll, 1..5 select a member, a
    // click selects or moves, the space bar stops or starts the enemies.
    void			Run(GameWindow& window);

    // Moves the view by cells, within the map
    void			Scroll(int dx, int dy);
    GFX::point		Origin() const			{ return fOrigin; }

    Bitmap*			Draw();

private:
    enum place {
        PLACE_WILDERNESS,
        PLACE_TOWN,			// cities, town gates and walls
        PLACE_MINE,
        PLACE_BUILDING		// fortresses, monasteries, tombs
    };

    struct figure {
        std::shared_ptr<ImcFile> sprites;	// standing ("CB")
        std::shared_ptr<ImcFile> walk;		// walking ("WK")
        int			frame;		// of the walk
        int			ticks;		// since the last step
        int			x;
        int			y;
        int			direction;
        int			colors;		// the first of its 8 colors, or -1
        int			member;		// in the party, or -1 (an enemy)
        std::vector<battle_position> path;	// still to walk
    };

    std::shared_ptr<ImcFile> _LoadSprites(const std::string& image,
                        const char* set);
    std::vector<battle_position> _Occupied(const figure* except) const;
    bool			_PlanEnemy(figure& enemy);
    void			_DrawFigure(const figure& f);
    uint8			_GroundColor(const battle_cell& cell) const;
    bool			_IsOpen(int x, int y) const;
    void			_DrawCell(int x, int y, int left, int top);

    GameData&		fData;
    Bitmap*			fBuffer;
    GFX::Palette	fPalette;
    std::vector<figure> fFigures;
    int				fSelected;		// index into fFigures, or -1
    bool			fEnemiesActive;
    std::unique_ptr<BattleMap> fMap;
    GFX::point		fOrigin;		// top left, in pixels
    place			fPlace;
};
