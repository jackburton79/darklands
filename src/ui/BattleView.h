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
    // The open cell without a figure nearest to (x, y), in rings around
    // it; false if there is none.
    bool			FindFreeCell(int& x, int& y) const;

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

    struct figure {
        std::shared_ptr<ImcFile> sprites;
        int			x;
        int			y;
        int			direction;
        int			colors;		// the first of its 8 colors, or -1
    };

    std::shared_ptr<ImcFile> _LoadSprites(const std::string& image);
    void			_DrawFigure(const figure& f);
    uint8			_GroundColor(const battle_cell& cell) const;
    bool			_IsOpen(int x, int y) const;
    void			_DrawCell(int x, int y, int left, int top);

    GameData&		fData;
    Bitmap*			fBuffer;
    GFX::Palette	fPalette;
    std::vector<figure> fFigures;
    std::unique_ptr<BattleMap> fMap;
    GFX::point		fOrigin;		// top left, in pixels
    place			fPlace;
};
