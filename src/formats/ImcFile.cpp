#include "ImcFile.h"

#include "Stream.h"

#include <stdexcept>

// The frame table follows a header of 60 bytes (the "DY" death
// animations) or 80 bytes (the others). verified: in all 333 files of
// the six catalogs, exactly one of the two sizes makes the table's data
// size match the file.
static const size_t kHeaderSizes[] = { 60, 80 };
static const size_t kParagraph = 16;


namespace {

class BitReader {
public:
    BitReader(const std::vector<uint8>& data)
        :
        fData(data),
        fPosition(0),
        fBits(0),
        fCount(0)
    {
        _Reload();
    }

    uint8 Byte()
    {
        if (fPosition >= fData.size())
            throw std::runtime_error("ImcFile: truncated data");
        return fData[fPosition++];
    }

    // The flag bits come in 16-bit words, lowest bit first; the next word
    // is read as soon as the last bit of one is taken.
    int Bit()
    {
        const int bit = fBits & 1;
        if (--fCount == 0)
            _Reload();
        else
            fBits >>= 1;
        return bit;
    }

private:
    void _Reload()
    {
        const uint8 low = Byte();
        fBits = low | (Byte() << 8);
        fCount = 16;
    }

    const std::vector<uint8>& fData;
    size_t			fPosition;
    uint16			fBits;
    int				fCount;
};

}


static inline uint16
WordAt(const std::vector<uint8>& data, size_t offset)
{
    if (offset + 2 > data.size())
        throw std::runtime_error("ImcFile: truncated data");
    return data[offset] | (data[offset + 1] << 8);
}


/* static */
std::vector<uint8>
ImcFile::Decompress(const std::vector<uint8>& data)
{
    BitReader reader(data);
    std::vector<uint8> output;
    for (;;) {
        if (reader.Bit()) {
            output.push_back(reader.Byte());
            continue;
        }
        size_t length;
        size_t distance;
        if (!reader.Bit()) {
            // 00ll: a short copy, from up to 256 bytes back
            length = (reader.Bit() << 1);
            length |= reader.Bit();
            length += 2;
            distance = 256 - reader.Byte();
        } else {
            // 01: 13 bits of distance, 3 of length
            const uint8 low = reader.Byte();
            const uint8 high = reader.Byte();
            distance = 0x2000 - (low | ((high & 0xF8) << 5));
            length = (high & 0x07) + 2;
            if (length == 2) {
                length = reader.Byte();
                if (length == 0)
                    break;		// the end
                if (length == 1)
                    continue;	// a segment change in LZEXE: nothing to do
                length++;
            }
        }
        if (distance > output.size())
            throw std::runtime_error("ImcFile: invalid back reference");
        const size_t start = output.size() - distance;
        for (size_t i = 0; i < length; i++)
            output.push_back(output[start + i]);
    }
    return output;
}


ImcFile::ImcFile(Stream* stream)
{
    std::vector<uint8> compressed(stream->Size());
    if (stream->ReadAt(0, compressed.data(), compressed.size())
            != (ssize_t)compressed.size()) {
        throw std::runtime_error("ImcFile: read error");
    }
    const std::vector<uint8> data = Decompress(compressed);

    // frame table: frame count, data size, 8 offsets per frame (in
    // paragraphs from the end of the table)
    size_t table = 0;
    int frames = 0;
    for (size_t header : kHeaderSizes) {
        if (header + 4 > data.size())
            continue;
        const int count = WordAt(data, header);
        const size_t tableEnd = header + 4 + kParagraph * count;
        if (count > 0 && tableEnd <= data.size()
                && WordAt(data, header + 2) == data.size() - tableEnd) {
            table = header;
            frames = count;
            break;
        }
    }
    if (frames == 0)
        throw std::runtime_error("ImcFile: no frame table");

    const size_t start = table + 4 + kParagraph * frames;
    fSprites.resize(size_t(frames) * kDirectionCount);
    for (size_t i = 0; i < fSprites.size(); i++) {
        size_t position = start + kParagraph * WordAt(data, table + 4 + 2 * i);
        if (position + 2 > data.size())
            throw std::runtime_error("ImcFile: invalid frame offset");
        imc_sprite& sprite = fSprites[i];
        sprite.width = data[position];
        sprite.height = data[position + 1];
        position += 2;
        sprite.pixels.assign(size_t(sprite.width) * sprite.height, 0);
        // each row: pixel count, blank pixels on the left, the pixels
        for (uint16 y = 0; y < sprite.height; y++) {
            if (position + 2 > data.size())
                throw std::runtime_error("ImcFile: truncated row");
            const size_t count = data[position];
            const size_t skip = data[position + 1];
            position += 2;
            if (skip + count > sprite.width || position + count > data.size())
                throw std::runtime_error("ImcFile: invalid row");
            for (size_t x = 0; x < count; x++)
                sprite.pixels[y * sprite.width + skip + x] = data[position + x];
            position += count;
        }
    }
}


int
ImcFile::CountFrames() const
{
    return int(fSprites.size() / kDirectionCount);
}


const imc_sprite&
ImcFile::SpriteAt(int frame, int direction) const
{
    if (frame < 0 || frame >= CountFrames() || direction < 0
            || direction >= kDirectionCount) {
        throw std::out_of_range("ImcFile::SpriteAt(): invalid index");
    }
    return fSprites[size_t(frame) * kDirectionCount + direction];
}
