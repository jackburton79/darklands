/*
 * PICImage.cpp
 * Decoder for Darklands .PIC images.
 * Compression algorithm reference:
 *   https://github.com/ogamespec/PicDecoder
 */

#include "PICImage.h"

#include "Bitmap.h"
#include "Stream.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

static const size_t kHeaderSize	= 0x0A;
static const size_t kStackSize	= 10000;

static GFX::Palette
EGAPalette()
{
    // https://moddingwiki.shikadi.net/wiki/EGA_Palette
    static const uint8 kEGA[16][3] = {
        {   0,   0,   0 }, {   0,   0, 170 }, {   0, 170,   0 }, {   0, 170, 170 },
        { 170,   0,   0 }, { 170,   0, 170 }, { 170,  85,   0 }, { 170, 170, 170 },
        {  85,  85,  85 }, {  85,  85, 255 }, {  85, 255,  85 }, {  85, 255, 255 },
        { 255,  85,  85 }, { 255,  85, 255 }, { 255, 255,  85 }, { 255, 255, 255 }
    };
    GFX::Palette palette;
    for (int i = 0; i < 16; i++)
        palette.colors[i] = GFX::Color{ kEGA[i][0], kEGA[i][1], kEGA[i][2], 0 };
    return palette;
}


class DecodingContext {
public:
    DecodingContext(const uint8* data, size_t size, bool bcdPacked,
        uint16 magicWord);
    ~DecodingContext();

    void			DecodeNextBytes(uint8* line, uint16 length);

private:
    uint16			_NextWord();
    uint8			NextRun();
    void			_SetupBuffer();
    void			PushValue(uint8 value);
    uint8			PopValue();
    uint16			GetLUTIndex(int id) const;
    uint8			GetLUTValue(int id) const;
    void			SetLUTIndex(int id, int newId);
    void			SetLUTValue(int id, uint8 value);

    const uint8*	fData;
    size_t			fSize;
    size_t			fDataOffset;
    bool			fBCDPacked;
    int				fRepeatCount;
    uint8			fRepeatByte;
    uint8*			fBuffer;
    int				fBufferOffset;
    uint16			fMagicWord;
    uint8			fMagicByte;
    int				fBitPointer;
    uint8*			fLUT;
    int				fOnesCounter;
    uint32			fBitMask;
    int32			fDWordUnk;
    int				fSavedIndex;
    uint8			fSavedByte;
};


// #pragma mark - PICImage


PICImage::PICImage(Stream* stream)
    :
    fStream(stream),
    fCompressedSize(0),
    fWidth(0),
    fHeight(0),
    fMagicWord(0),
    fBCDPacked(false)
{
    if (stream == NULL)
        throw std::runtime_error("PICImage: NULL stream");

    uint16 magic = stream->ReadWordLEAt(0x00);
    if ((magic & 0xFF) != 'X')
        throw std::runtime_error("PICImage: not a PIC file");
    fBCDPacked = (magic >> 8) & 1;

    fCompressedSize = stream->ReadWordLEAt(0x02);
    fWidth			= stream->ReadWordLEAt(0x04);
    fHeight			= stream->ReadWordLEAt(0x06);
    fMagicWord		= stream->ReadWordLEAt(0x08);

    if (fWidth == 0 || fHeight == 0 || fCompressedSize == 0)
        throw std::runtime_error("PICImage: invalid header");
    if (stream->Size() < kHeaderSize)
        throw std::runtime_error("PICImage: stream too small for header");

    // Field 0x02 counts all bytes from 0x04 to the end of the image data:
    // stream size == fCompressedSize + 4 (verified across EINFO.CAT).
    if (size_t(fCompressedSize) + 4 > stream->Size())
        throw std::runtime_error("PICImage: declared size exceeds stream size");
}


PICImage::~PICImage()
{
}


/* static */
Bitmap*
PICImage::Decode(Stream* stream, const GFX::Palette* palette)
{
    PICImage image(stream);
    return image.Image(palette);
}


Bitmap*
PICImage::Image(const GFX::Palette* palette) const
{
    const size_t streamSize = fStream->Size();
    if (streamSize < kHeaderSize)
        throw std::runtime_error("PICImage: stream too small for header");

    const size_t dataSize = streamSize - kHeaderSize;

    std::vector<uint8> compressed(dataSize);
    if (fStream->ReadAt(kHeaderSize, compressed.data(), dataSize)
            != (ssize_t)dataSize) {
        throw std::runtime_error("PICImage: truncated image data");
    }

    DecodingContext context(compressed.data(), dataSize, fBCDPacked, fMagicWord);

    Bitmap* bitmap = new Bitmap(fWidth, fHeight, 8);
    try {
        // Fallback for images without an external palette (EGA-era UI art?):
        // the standard 16-color EGA palette.
        GFX::Palette fallback;
        if (palette == NULL) {
            fallback = EGAPalette();
            palette = &fallback;
        }
        bitmap->SetColors(palette->colors, 0, 256);

        std::vector<uint8> line(fWidth);
        for (uint16 y = 0; y < fHeight; y++) {
            context.DecodeNextBytes(line.data(), fWidth);
            for (uint16 x = 0; x < fWidth; x++)
                bitmap->PutPixel(x, y, line[x]);	// NO % 16 — indices are
                                                    // true palette indices!
        }
    } catch (...) {
        bitmap->Release();
        throw;
    }
    return bitmap;
}


// #pragma mark - DecodingContext
// LZW-like adaptive decoder as reverse-engineered from the original
// executable. The LUT/bit logic is delicate: change it only with reference
// images to compare against.


DecodingContext::DecodingContext(const uint8* data, size_t size,
        bool bcdPacked, uint16 magicWord)
    :
    fData(data),
    fSize(size),
    fDataOffset(0),
    fBCDPacked(bcdPacked),
    fRepeatCount(0),
    fRepeatByte(0),
    fBuffer(new uint8[kStackSize]),
    fBufferOffset(-1),			// "stack empty"; was 0: the first NextRun()
                                // popped uninitialized memory
    fMagicWord(magicWord),
    fMagicByte(0),
    fBitPointer(8),
    fLUT(NULL),
    fOnesCounter(9),
    fBitMask(0x1FF),
    fDWordUnk(0x100),
    fSavedIndex(0),
    fSavedByte(0)
{
    fMagicByte = std::min(uint8(fMagicWord & 0xFF), uint8(11));
    fMagicWord = (fMagicWord & 0xff00) | fMagicByte;
    fLUT = new uint8[size_t(1 << fMagicByte) * 3];
    _SetupBuffer();
}


DecodingContext::~DecodingContext()
{
    delete[] fLUT;
    delete[] fBuffer;
}


uint16
DecodingContext::_NextWord()
{
    if (fDataOffset + 2 > fSize) {
        if (fDataOffset + 1 != fSize)
            throw std::runtime_error("PICImage: unexpected end of compressed data");
        // Odd-sized stream: the final word is a single byte.
        // Zero-fill the missing byte. (Variant A: existing byte = low byte)
        uint16 result = uint16(fData[fDataOffset]);
        fDataOffset = fSize;	// consume it; any further read still throws
        return result;
    }

    uint16 result = uint16(fData[fDataOffset] | (fData[fDataOffset + 1] << 8));
    fDataOffset += 2;
    return result;
}


void
DecodingContext::_SetupBuffer()
{
    fOnesCounter = 9;
    fBitMask = 0x1FF;
    fDWordUnk = 0x100;

    // Fill FF FF 00 pattern
    for (int i = 0; i < (1 << fMagicByte); i++)
        SetLUTIndex(i, 0xffff);

    // Fix first 256 entries: FF FF xx (xx = 00 ... FF)
    for (int i = 0; i < 0x100; i++)
        SetLUTValue(i, uint8(i));
}


uint16
DecodingContext::GetLUTIndex(int id) const
{
    size_t offset = size_t(id) * 3;
    return uint16((fLUT[offset + 1] << 8) | fLUT[offset]);
}


uint8
DecodingContext::GetLUTValue(int id) const
{
    return fLUT[size_t(id) * 3 + 2];
}


void
DecodingContext::SetLUTIndex(int id, int newId)
{
    size_t offset = size_t(id) * 3;
    uint16 word = uint16(newId);
    fLUT[offset + 1] = uint8(word >> 8);
    fLUT[offset] = uint8(word & 0xff);
}


void
DecodingContext::SetLUTValue(int id, uint8 value)
{
    fLUT[size_t(id) * 3 + 2] = value;
}


void
DecodingContext::DecodeNextBytes(uint8* line, uint16 length)
{
    int opCount = fBCDPacked ? (length + 1) / 2 : length;

    for (int i = 0; i < opCount; i++) {
        uint8 value;
        // Fetch next byte (RLE layer on top of the LZW-like decoder)
        if (fRepeatCount != 0) {
            value = fRepeatByte;
            fRepeatCount--;
        } else {
            value = NextRun();
            if (value == 0x90) {
                value = NextRun();
                if (value != 0) {
                    fRepeatCount = value - 1;
                    value = fRepeatByte;
                    fRepeatCount--;
                } else {
                    fRepeatByte = value = 0x90;
                }
            } else {
                fRepeatByte = value;
            }
        }

        // Output byte
        if (fBCDPacked) {
            line[2 * i] = value & 0xf;
            if (2 * i + 1 < length)		// guard against odd widths
                line[2 * i + 1] = value >> 4;
        } else {
            line[i] = value;
        }
    }
}


uint8
DecodingContext::NextRun()
{
    if (fBufferOffset == -1) {
        int b = fMagicWord >> (16 - fBitPointer);
        int c = fBitPointer;

        // Loop 1: refill the bit buffer
        while (c < fOnesCounter) {
            fMagicWord = _NextWord();
            b |= (fMagicWord << c);
            c += 16;
        }

        // After loop 1
        fBitPointer = c - fOnesCounter;
        int oldIndex = int(b & fBitMask);
        int newIndex = oldIndex;
        if (oldIndex >= fDWordUnk) {
            // KwKwK case
            newIndex = fDWordUnk;
            oldIndex = fSavedIndex;
            PushValue(fSavedByte);
        }

        // Loop 2: walk the LUT chain
        while (true) {
            int index = GetLUTIndex(oldIndex) + 1;
            if (index != 0x10000) {
                PushValue(GetLUTValue(oldIndex));
                oldIndex = index - 1;
            } else
                break;
        }

        // After loop 2
        fSavedByte = GetLUTValue(oldIndex);
        PushValue(fSavedByte);
        SetLUTValue(fDWordUnk, fSavedByte);
        SetLUTIndex(fDWordUnk, fSavedIndex);
        fDWordUnk++;
        if (fDWordUnk > int32(fBitMask)) {
            fOnesCounter++;
            fBitMask = (fBitMask << 1) | 1;
        }
        if (fOnesCounter > fMagicByte) {
            _SetupBuffer();
            newIndex = 0;
        }
        fSavedIndex = newIndex;
    }

    return PopValue();
}


void
DecodingContext::PushValue(uint8 value)
{
    if (fBufferOffset + 1 >= int(kStackSize))
        throw std::runtime_error("PICImage: decoder stack overflow");
    fBuffer[++fBufferOffset] = value;
}


uint8
DecodingContext::PopValue()
{
    if (fBufferOffset < 0)
        throw std::runtime_error("PICImage: decoder stack underflow");
    return fBuffer[fBufferOffset--];
}
