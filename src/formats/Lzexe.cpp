#include "Lzexe.h"

#include <stdexcept>


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
            throw std::runtime_error("LzexeDecompress: truncated data");
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


std::vector<uint8>
LzexeDecompress(const std::vector<uint8>& data)
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
            throw std::runtime_error("LzexeDecompress: invalid back reference");
        const size_t start = output.size() - distance;
        for (size_t i = 0; i < length; i++)
            output.push_back(output[start + i]);
    }
    return output;
}
