/*
 * TextSupport.cpp
 * Adapted from the TextSupport code of Stefano Ceccherini's Infinity
 * Engine reimplementation (created 03/set/2013).
 */

#include "TextSupport.h"

#include "Bitmap.h"
#include "GraphicsDefs.h"

#include <algorithm>
#include <cstdint>


Font::Font(const FontFile& file, uint32 index)
    :
    fFont(file.FontAt(index))
{
}


uint16
Font::Height() const
{
    return fFont.height;
}


uint16
Font::StringWidth(const std::string& string) const
{
    uint32 width = 0;
    for (const char c : string) {
        if (!fFont.HasGlyph(uint8(c)))
            continue;
        if (width > 0)
            width += fFont.spacing;
        width += fFont.Width(uint8(c));
    }
    return uint16(std::min<uint32>(width, UINT16_MAX));
}


std::string
Font::TruncateString(std::string& string, uint16 maxWidth,
    uint16* truncatedWidth) const
{
    std::string line = string;
    size_t breakPos = string.length();
    for (;;) {
        line = string.substr(0, breakPos);
        uint16 newWidth = StringWidth(line);
        if (newWidth <= maxWidth) {
            if (truncatedWidth != NULL)
                *truncatedWidth = newWidth;
            break;
        }
        size_t space = breakPos > 0 ? string.rfind(" ", breakPos - 1) : std::string::npos;
        if (space == std::string::npos) {
            // No space left to break at: the first word alone is wider than
            // the line - split it where it fits (at least one character, so
            // the caller always makes progress).
            size_t length = std::max<size_t>(1, std::min(breakPos, string.length()) - 1);
            while (length > 1 && StringWidth(string.substr(0, length)) > maxWidth)
                length--;
            line = string.substr(0, length);
            if (truncatedWidth != NULL)
                *truncatedWidth = StringWidth(line);
            string = string.substr(length);
            return line;
        }
        breakPos = space;
    }
    if (breakPos == string.length())
        string = "";
    else
        string = string.substr(breakPos + 1, string.length());
    return line;
}


void
Font::RenderString(const std::string& string, Bitmap* bitmap,
    const GFX::point& point, uint32 color, uint32 maxWidth) const
{
    int32 x = point.x;
    const int32 right = maxWidth == UINT_MAX ? INT32_MAX : point.x + int32(maxWidth);
    bool first = true;
    for (const char ch : string) {
        const uint8 c = uint8(ch);
        if (!fFont.HasGlyph(c))
            continue;
        if (!first)
            x += fFont.spacing;
        first = false;
        const uint8 width = fFont.Width(c);
        if (x + width > right)
            break;
        for (uint16 y = 0; y < fFont.height; y++) {
            for (uint16 gx = 0; gx < width; gx++) {
                if (fFont.Pixel(c, gx, y))
                    bitmap->PutPixel(x + gx, point.y + y, color);
            }
        }
        x += width;
    }
}


Bitmap*
Font::GetRenderedString(const std::string& string) const
{
    const uint16 width = std::max<uint16>(StringWidth(string), 1);
    Bitmap* bitmap = new Bitmap(width, Height(), 8);
    const GFX::Color colors[2] = { { 0, 0, 0, 0 }, { 255, 255, 255, 0 } };
    bitmap->SetColors(colors, 0, 2);
    for (uint16 y = 0; y < Height(); y++)
        for (uint16 x = 0; x < width; x++)
            bitmap->PutPixel(x, y, 0);
    RenderString(string, bitmap, GFX::point(0, 0), 1);
    return bitmap;
}


/* static */
std::string
Font::ToGameCharset(const std::string& utf8)
{
    std::string result;
    for (size_t i = 0; i < utf8.length(); i++) {
        const uint8 c = uint8(utf8[i]);
        if (c < 0x80) {
            result += char(c);
            continue;
        }
        // two-byte sequences starting with 0xC3 cover the German letters
        if (c == 0xC3 && i + 1 < utf8.length()) {
            const uint8 next = uint8(utf8[++i]);
            switch (next) {
                case 0xA4:	result += '\x1F'; continue;	// ä
                case 0xB6:	result += '{'; continue;	// ö
                case 0xBC:	result += '|'; continue;	// ü
                case 0x84:	result += '['; continue;	// Ä
                case 0x96:	result += '\\'; continue;	// Ö
                case 0x9C:	result += ']'; continue;	// Ü
                case 0x9F:	result += '_'; continue;	// ß
                default:	break;
            }
        } else {
            // skip the continuation bytes of any other sequence
            while (i + 1 < utf8.length() && (uint8(utf8[i + 1]) & 0xC0) == 0x80)
                i++;
        }
        result += '?';
    }
    return result;
}
