#include "FontFile.h"

#include "FileStream.h"
#include "Stream.h"

#include <stdexcept>

// Each font is: glyph width table, 8-byte header, glyph bitmap. The file
// header's offset table points at the bitmap, i.e. just past the header.
static const size_t kFontHeaderSize = 8;


bool
font_data::Pixel(uint8 c, uint16 x, uint16 y) const
{
    const size_t offset = (size_t(y) * CountGlyphs() + (c - firstChar))
        * bytesPerRow + x / 8;
    return (bitmap[offset] >> (7 - x % 8)) & 1;
}


FontFile::FontFile(const std::string& fileName)
{
    Stream* stream = new FileStream(fileName.c_str(), FileStream::READ_ONLY);
    try {
        const size_t size = stream->Size();
        if (size < 2)
            throw std::runtime_error("FontFile: file too small");
        const uint16 count = stream->ReadWordLEAt(0);
        if (count == 0 || size < 2 + 2 * size_t(count))
            throw std::runtime_error("FontFile: invalid font count");

        std::vector<size_t> offsets(count);
        for (uint16 i = 0; i < count; i++)
            offsets[i] = stream->ReadWordLEAt(2 + 2 * i);

        for (uint16 i = 0; i < count; i++) {
            const size_t offset = offsets[i];
            if (offset < 2 + 2 * size_t(count) + kFontHeaderSize || offset > size)
                throw std::runtime_error("FontFile: invalid font offset");

            uint8 header[kFontHeaderSize];
            if (stream->ReadAt(offset - kFontHeaderSize, header, kFontHeaderSize)
                    != (ssize_t)kFontHeaderSize) {
                throw std::runtime_error("FontFile: truncated font header");
            }
            font_data font;
            font.firstChar = header[0];
            font.lastChar = header[1];
            font.bytesPerRow = header[2];
            // rows = byte 4 + byte 6: matches the data size of all 7 fonts
            // in FONTS.FNT/FONTS.UTL (inferred; see docs/formats.md)
            font.height = header[4] + header[6];
            // widths are ink widths; byte 5 (1 in every font) is most
            // likely the gap between glyphs (inferred)
            font.spacing = header[5];
            if (font.lastChar < font.firstChar || font.bytesPerRow == 0
                    || font.height == 0) {
                throw std::runtime_error("FontFile: invalid font header");
            }

            const size_t glyphs = font.CountGlyphs();
            if (offset - kFontHeaderSize < glyphs)
                throw std::runtime_error("FontFile: truncated width table");
            font.widths.resize(glyphs);
            if (stream->ReadAt(offset - kFontHeaderSize - glyphs,
                    font.widths.data(), glyphs) != (ssize_t)glyphs) {
                throw std::runtime_error("FontFile: truncated width table");
            }
            for (size_t g = 0; g < glyphs; g++) {
                if (font.widths[g] > 8 * font.bytesPerRow)
                    throw std::runtime_error("FontFile: glyph wider than its rows");
            }

            font.bitmap.resize(glyphs * font.bytesPerRow * font.height);
            if (offset + font.bitmap.size() > size
                    || stream->ReadAt(offset, font.bitmap.data(), font.bitmap.size())
                        != (ssize_t)font.bitmap.size()) {
                throw std::runtime_error("FontFile: truncated glyph bitmap");
            }
            fFonts.push_back(font);
        }
    } catch (...) {
        delete stream;
        throw;
    }
    delete stream;
}


uint32
FontFile::CountFonts() const
{
    return uint32(fFonts.size());
}


const font_data&
FontFile::FontAt(uint32 index) const
{
    if (index >= fFonts.size())
        throw std::out_of_range("FontFile::FontAt(): invalid index");
    return fFonts[index];
}
