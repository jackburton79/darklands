/*
 * FontFile.h
 * Reader for the game's bitmap font files (FONTS.FNT, FONTS.UTL).
 * See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

// One font of a font file: 1-bit glyphs, one row of `bytesPerRow` bytes
// per glyph per scanline, most significant bit = leftmost pixel.
struct font_data {
    uint8 firstChar;
    uint8 lastChar;
    uint8 bytesPerRow;
    uint8 height;					// rows per glyph
    uint8 spacing;					// pixels between glyphs (inferred)
    std::vector<uint8> widths;		// ink width of each glyph, in pixels
    std::vector<uint8> bitmap;		// row-major: row, then glyph, then byte

    uint32 CountGlyphs() const { return uint32(lastChar) - firstChar + 1; }
    bool HasGlyph(uint8 c) const { return c >= firstChar && c <= lastChar; }
    uint8 Width(uint8 c) const { return widths[c - firstChar]; }
    // Whether pixel (x, y) of glyph `c` is set. The glyph must exist.
    bool Pixel(uint8 c, uint16 x, uint16 y) const;
};

class FontFile {
public:
    explicit		FontFile(const std::string& fileName);	// throws on error

    uint32			CountFonts() const;
    const font_data& FontAt(uint32 index) const;

private:
    std::vector<font_data>	fFonts;
};
