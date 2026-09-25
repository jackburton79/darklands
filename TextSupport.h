/*
 * TextSupport.h
 * Text rendering with the game's bitmap fonts (FONTS.FNT, FONTS.UTL).
 *
 * Adapted from the TextSupport code of Stefano Ceccherini's Infinity
 * Engine reimplementation (created 03/set/2013): same Font interface,
 * with glyphs loaded from a FontFile instead of BAM resources.
 */
#pragma once

#include "FontFile.h"
#include "SupportDefs.h"

#include <climits>
#include <string>

class Bitmap;

namespace GFX {
    class point;
}


class Font {
public:
    Font(const FontFile& file, uint32 index);	// throws on invalid index

    uint16 Height() const;
    // Strings are in the game's character set; see ToGameCharset().
    uint16 StringWidth(const std::string& string) const;

    // Removes and returns the longest leading part of `string` that fits
    // in maxWidth, breaking at a space when possible.
    std::string TruncateString(std::string& string, uint16 maxWidth,
                    uint16* truncatedWidth = NULL) const;

    // Draws `string` with its top-left corner at `point`, setting glyph
    // pixels to `color` (a palette index for 8-bit bitmaps) and leaving
    // the others untouched. Stops at maxWidth pixels.
    void RenderString(const std::string& string, Bitmap* bitmap,
                    const GFX::point& point, uint32 color,
                    uint32 maxWidth = UINT_MAX) const;

    // Returns a new 8-bit bitmap (index 0 = background, 1 = text) with
    // the string rendered on it. Owned by the caller: Release() it.
    Bitmap* GetRenderedString(const std::string& string) const;

    // Converts UTF-8 text to the game's character set: ASCII, with
    // ä ö ü Ä Ö Ü ß stored as 0x1F { | [ \ ] _ (see docs/formats.md;
    // FONTS.UTL has ë instead of ß at _).
    // Characters the game cannot display become '?'.
    static std::string ToGameCharset(const std::string& utf8);

private:
    font_data fFont;
};
