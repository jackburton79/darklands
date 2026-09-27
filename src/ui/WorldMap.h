/*
 * WorldMap.h
 * The world map: DARKLAND.MAP tiles drawn with the MAPICONS.PIC /
 * MAPICON2.PIC icon sheets. See docs/formats.md for the geometry and
 * the tile column rule.
 */
#pragma once

#include "GraphicsDefs.h"
#include "MapFile.h"
#include "SupportDefs.h"

#include <string>
#include <vector>

class Bitmap;

class WorldMap {
public:
    // Map tile geometry: icon sheet cells are kTileWidth x kTileHeight
    // pixels; odd rows are shifted right by half a tile and consecutive
    // rows are kRowStep pixels apart, so tiles overlap.
    static const uint16 kTileWidth	= 16;
    static const uint16 kTileHeight	= 12;
    static const uint16 kRowStep	= 4;

    // mapFile: DARKLAND.MAP; sheetFiles: MAPICONS.PIC and MAPICON2.PIC.
    // Throws if the map cannot be read. If the sheets cannot be read the
    // map is drawn with synthetic gray levels instead (HasSheets() false).
    WorldMap(const std::string& mapFile, const std::string sheetFiles[2]);

    uint16			Width() const	{ return fWidth; }		// in tiles
    uint16			Height() const	{ return fHeight; }
    // Size of the whole map in pixels.
    uint32			PixelWidth() const;
    uint32			PixelHeight() const;

    const map_tile&	TileAt(uint16 x, uint16 y) const;
    // 0..15 for MAPICONS.PIC rows, 16..31 for MAPICON2.PIC rows
    int				TileTypeAt(uint16 x, uint16 y) const;
    // Icon sheet column, from the diagonal neighbors (see docs/formats.md).
    uint8			ColumnAt(uint16 x, uint16 y) const;

    // Top-left pixel of tile (x, y)'s cell, and the center of its ground.
    GFX::point		TileOrigin(uint16 x, uint16 y) const;
    GFX::point		TileCenter(uint16 x, uint16 y) const;
    // The tile whose ground contains map pixel `point` (the nearest
    // ground center, in the diamond metric of the staggered grid).
    // Returns false if the point is off the map.
    bool			TileAtPixel(const GFX::point& point, uint16& x,
                        uint16& y) const;

    bool			HasSheets() const	{ return fHaveSheets; }
    const GFX::Palette& Palette() const	{ return fPalette; }

    // Draws the part of the map whose top-left map pixel is `origin` into
    // `bitmap` (8-bit, using Palette()), covering the whole bitmap.
    // Pixels no tile covers are left untouched.
    void			Draw(Bitmap* bitmap, const GFX::point& origin) const;

    // Draws one icon sheet cell (tile type 0..31, column 0..15) as if it
    // were tile (x, y), in the same coordinates as Draw(). Does nothing
    // without sheets.
    void			DrawIcon(Bitmap* bitmap, const GFX::point& origin,
                        int type, uint8 column, uint16 x, uint16 y) const;

    // Returns a new bitmap with the whole map. Release() it when done.
    Bitmap*			Render() const;

private:
    void			_LoadSheets(const std::string sheetFiles[2]);
    void			_DrawTile(Bitmap* bitmap, uint16 x, uint16 y,
                        int destX, int destY) const;
    void			_DrawCell(Bitmap* bitmap, int type, uint8 column,
                        int destX, int destY) const;

    uint16			fWidth;
    uint16			fHeight;
    std::vector<map_tile>	fTiles;		// row-major
    std::vector<uint8>		fColumns;	// row-major, precomputed

    bool			fHaveSheets;
    uint16			fSheetWidth;
    std::vector<uint8>		fSheets[2];
    GFX::Palette	fPalette;
};
