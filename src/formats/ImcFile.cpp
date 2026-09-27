#include "ImcFile.h"

#include "Lzexe.h"
#include "Stream.h"

#include <stdexcept>

// The frame table follows a header of 60 bytes (the "DY" death
// animations) or 80 bytes (the others). verified: in all 333 files of
// the six catalogs, exactly one of the two sizes makes the table's data
// size match the file.
static const size_t kHeaderSizes[] = { 60, 80 };
static const size_t kParagraph = 16;


static inline uint16
WordAt(const std::vector<uint8>& data, size_t offset)
{
    if (offset + 2 > data.size())
        throw std::runtime_error("ImcFile: truncated data");
    return data[offset] | (data[offset + 1] << 8);
}


ImcFile::ImcFile(Stream* stream)
{
    std::vector<uint8> compressed(stream->Size());
    if (stream->ReadAt(0, compressed.data(), compressed.size())
            != (ssize_t)compressed.size()) {
        throw std::runtime_error("ImcFile: read error");
    }
    const std::vector<uint8> data = LzexeDecompress(compressed);

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
        fSprites[i] = ReadSprite(data,
            start + kParagraph * WordAt(data, table + 4 + 2 * i));
    }
}


int
ImcFile::CountFrames() const
{
    return int(fSprites.size() / kDirectionCount);
}


const sprite&
ImcFile::SpriteAt(int frame, int direction) const
{
    if (frame < 0 || frame >= CountFrames() || direction < 0
            || direction >= kDirectionCount) {
        throw std::out_of_range("ImcFile::SpriteAt(): invalid index");
    }
    return fSprites[size_t(frame) * kDirectionCount + direction];
}
