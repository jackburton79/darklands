/*
 * BattleView.h
 * A provisional view of a battlefield, seen from above: the map's cells
 * as colored squares, their walls as lines, their objects as marks. The
 * game's own drawing is not decoded yet (docs/exe.md, "Battles"); this
 * shows what the maps hold, with the battle's colors.
 *
 * The input handlers and Draw() work without a window, for testing.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <memory>
#include <string>

class BattleMap;
class Bitmap;
class GameData;
class GameWindow;
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

    // Runs until Esc; the arrow keys scroll.
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

    uint8			_GroundColor(const battle_cell& cell) const;
    bool			_IsOpen(int x, int y) const;
    void			_DrawCell(int x, int y, int left, int top);

    Bitmap*			fBuffer;
    std::unique_ptr<BattleMap> fMap;
    GFX::point		fOrigin;		// top left, in pixels
    place			fPlace;
};
