#include "Sprite.h"

#include <stdexcept>


sprite
ReadSprite(const std::vector<uint8>& data, size_t position)
{
    if (position + 2 > data.size())
        throw std::runtime_error("ReadSprite: invalid offset");
    sprite result;
    result.width = data[position];
    result.height = data[position + 1];
    position += 2;
    result.pixels.assign(size_t(result.width) * result.height, 0);
    for (uint16 y = 0; y < result.height; y++) {
        if (position + 2 > data.size())
            throw std::runtime_error("ReadSprite: truncated row");
        const size_t count = data[position];
        const size_t skip = data[position + 1];
        position += 2;
        if (skip + count > result.width || position + count > data.size())
            throw std::runtime_error("ReadSprite: invalid row");
        for (size_t x = 0; x < count; x++)
            result.pixels[y * result.width + skip + x] = data[position + x];
        position += count;
    }
    return result;
}
