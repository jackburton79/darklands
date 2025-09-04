/*
 * PicDecoder.cpp
 *
 *  Created on: 27 apr 2023
 *      Author: Stefano Ceccherini
 */

#include "PICImage.h"

#include "Bitmap.h"
#include "Stream.h"

#include <cassert>
#include <iostream>

// https://github.com/ogamespec/PicDecoder/tree/master/PicDecode
//
// PIC file:
// first 11 bytes are always the same, except for byte at 0x02 and 0x03
// example E05SWST.PIC:
// decimal: 88  48 169   0  29   0  30   0  11   0  32
// E03CLST.PIC:
// decimal: 88  48 148   0  29   0  30   0  11   0  32

// 0x00 word, always "X0"
// 0x02 unsigned, word, size of compressed image
// 0x04 unsigned word, image width
// 0x06 unsigned word, image height
// 0x07 ??? always 0
// 0x08 ??? always 11
// 0x09 ??? always 0
// 0x10 ??? always 32


static GFX::Palette* sEGADefaultPalette;

class DecodingContext {
public:
	DecodingContext(Stream* stream);
	~DecodingContext();

	uint16 GetLUTIndex(int id);
	uint8 GetLUTValue(int id);
	void SetLUTIndex(int id, int newId);
	void SetLUTValue(int id, uint8 value);

	void DecodeNextBytes(uint8* line, uint16 length);
	uint8 NextRun();

	void PushValue(uint8 value);
	uint8 PopValue();

private:
	void _SetupBuffer();

	Stream* fStream;
	bool fBCDPacked;
	int fRepeatCount;
	uint8 fRepeatByte;
	uint8* fBuffer;
	int fBufferOffset;

	uint16 fMagicWord;
	uint8 fMagicByte;
	int fBitPointer;
	uint8* fLUT;

	int fOnesCounter;
	uint32 fBitMask;
	int32 fDWordUnk;

    int fSavedIndex;
    uint8 fSavedByte;

};


// PicDecoder
PICImage::PICImage(Stream* stream)
	:
	fStream(stream)
{
	// EGA palette taken from here:
	// https://moddingwiki.shikadi.net/wiki/EGA_Palette
	if (sEGADefaultPalette == NULL) {
		sEGADefaultPalette = new GFX::Palette;
		sEGADefaultPalette->colors[0] = GFX::Color{0, 0, 0, 0}; // black
		sEGADefaultPalette->colors[1] = GFX::Color{0, 0, 170, 0}; // blue
		sEGADefaultPalette->colors[2] = GFX::Color{0, 170, 0, 0}; // green
		sEGADefaultPalette->colors[3] = GFX::Color{0, 170, 170, 0}; // cyan
		sEGADefaultPalette->colors[4] = GFX::Color{170, 0, 0, 0}; // red
		sEGADefaultPalette->colors[5] = GFX::Color{170, 0, 170, 0}; // magenta
		sEGADefaultPalette->colors[6] = GFX::Color{170, 85, 0, 0}; // yellow / brown
		sEGADefaultPalette->colors[7] = GFX::Color{170, 170, 170, 0}; // white / light gray
		sEGADefaultPalette->colors[8] = GFX::Color{85, 85, 85, 0}; // dark gray / bright black
		sEGADefaultPalette->colors[9] = GFX::Color{85, 85, 255, 0}; // bright blue
		sEGADefaultPalette->colors[10] = GFX::Color{85, 255, 85, 0}; // bright green
		sEGADefaultPalette->colors[11] = GFX::Color{85, 255, 255, 0}; // bright cyan
		sEGADefaultPalette->colors[12] = GFX::Color{255, 85, 85, 0}; // bright red
		sEGADefaultPalette->colors[13] = GFX::Color{255, 85, 255, 0}; // bright magenta
		sEGADefaultPalette->colors[14] = GFX::Color{255, 255, 85, 0}; // bright yellow
		sEGADefaultPalette->colors[15] = GFX::Color{255, 255, 255, 0}; // bright white
	}
}


PICImage::~PICImage()
{
}


uint16
PICImage::Width() const
{
	uint16 width;
	fStream->ReadAt(0x06, &width, sizeof(width)); // 0x06
	return width;

}


uint16
PICImage::Height() const
{
	// 0x08
	uint16 height;
	fStream->ReadAt(0x08, &height, sizeof(height));
	return height;
}


Bitmap*
PICImage::Image()
{
	off_t initialPos = fStream->Position();
	fStream->Seek(0, SEEK_SET);

	// 0x00
	uint16 header = fStream->ReadWordLEAt(0x00);
	if ((header & 0xFF) != 'X') {
		std::cerr << "GetImage: wrong format!" << std::endl;
		return nullptr;
	}

	bool bcdPacked = (header >> 8) & 1;

	uint16 compressedSize = fStream->ReadWordLEAt(0x02);
	uint16 width = fStream->ReadWordLEAt(0x04);
	uint16 height = fStream->ReadWordLEAt(0x06);

	std::cout << "width: " << width << ", height: " << height << std::endl;
	std::cout << "size: " << compressedSize << std::endl;
	std::cout << "BCD packed: " << (bcdPacked ? "true" : "false") << std::endl;

	// Context
	DecodingContext* context = new DecodingContext(fStream);

	// TODO: Check if there is an embedded palette,
	// otherwise use the default
	Bitmap* bitmap = new Bitmap(width, height, 8);
	uint8* line = new uint8[width];
	if (true)
		bitmap->SetColors(sEGADefaultPalette->colors, 0, 15);

	for (auto y = 0; y < height; y++) {
		context->DecodeNextBytes(line, width);
		for (auto x = 0; x < width; x++) {
			uint8 value = line[x];
			value = value % 16;
			if (value > 15)
				std::cout << "value:" << (int)value << std::endl;
			bitmap->PutPixel(x, y, value);
		}
	}

	delete[] line;

	delete context;

	// Return to previous position
	fStream->Seek(initialPos, SEEK_SET);

	return bitmap;
}


// DecodingContext
DecodingContext::DecodingContext(Stream* stream)
	:
	fStream(stream),
	fBCDPacked(false),
	fRepeatCount(0),
	fRepeatByte(0),
	fBuffer(nullptr),
	fBufferOffset(0),
	fMagicWord(0),
	fMagicByte(0),
	fBitPointer(8),
	fLUT(nullptr),
	fSavedIndex(0),
	fSavedByte(0)
{
	fBCDPacked = fStream->ReadByteAt(0x01) & 1;
	fMagicWord = fStream->ReadWordLEAt(0x08); // 0x08
	fMagicByte = std::min(uint8(fMagicWord & 0xFF), uint8(11));

	fBuffer = new uint8[10000];

	fMagicWord &= 0xff00;
    fMagicWord |= fMagicByte;
	
	fLUT = new uint8[(1 << fMagicByte) * 3];

	_SetupBuffer();

	fStream->Seek(0x0A, SEEK_SET);
}


DecodingContext::~DecodingContext()
{
	delete[] fBuffer;
	delete[] fLUT;
}


void
DecodingContext::_SetupBuffer()
{
	fOnesCounter = 9;
	fBitMask = 0x1FF;
	fDWordUnk = 0x100;

	// Fill FF FF 00 pattern
	for (auto i = 0; i < (1 << fMagicByte); i++) {
		SetLUTIndex(i, 0xffff);
	}

	// Fix first 256 entries by FF FF xx pattern (where xx = 00 ... FF)
	for (auto i = 0; i < 0x100; i++) {
		SetLUTValue(i, uint8(i));
	}
}


uint16
DecodingContext::GetLUTIndex(int id)
{
    uint16 offset = id * 3;
    uint16 word = (uint16)((fLUT[offset + 1] << 8) |
        fLUT[offset]);
    return word;
}


uint8
DecodingContext::GetLUTValue(int id)
{
	return fLUT[id * 3 + 2];
}


void
DecodingContext::SetLUTIndex(int id, int newId)
{
    uint16 offset = id * 3;
    uint16 word = uint16(newId);
    fLUT[offset + 1] = uint8(word >> 8);
    fLUT[offset] = uint8(word & 0xff);
}


void
DecodingContext::SetLUTValue(int id, uint8 value)
{
	fLUT[id * 3 + 2] = value;
}


void
DecodingContext::DecodeNextBytes(uint8* line, uint16 length)
{
	bool debug = false;
	int opcount;
	if (fBCDPacked) {
		opcount = (length + 1) / 2;
	} else {
		opcount = length;
	}

	for (auto i = 0; i < opcount; i++) {
		uint8 value;
		/// Fetch next byte
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
			/// Unpack BCD as uint16
			uint8 hiPart = uint8(value >> 4);
			uint8 lowPart = uint8(value & 0xf);
			line[2 * i + 1] = hiPart;
			line[2 * i] = lowPart;
			if (debug) {
				std::cout << std::hex;
				std::cout << lowPart << " ";
				std::cout << hiPart << " ";
				std::flush(std::cout);
			}
		} else {
			line[i] = value;
			if (debug) {
				std::cout << value << " ";
				std::flush(std::cout);
			}
		}
	}
	if (debug)
		std::cout << std::endl;
}


uint8
DecodingContext::NextRun()
{
	if (fBufferOffset == -1) {
		int b = fMagicWord >> (16 - fBitPointer);
		int c = fBitPointer;
		// Loop 1
		while (c < fOnesCounter) {
			fMagicWord = fStream->ReadWordLE();
			b |= (fMagicWord << c);
			c += 16;
		}

		// After Loop 1
		fBitPointer = c - fOnesCounter;
		int oldIndex = int(b & fBitMask);
		int newIndex = oldIndex;
		if (oldIndex >= fDWordUnk) {
			newIndex = fDWordUnk;
			oldIndex = fSavedIndex;
			PushValue(fSavedByte);
		}

		// Loop 2
		while (true) {
			int index = GetLUTIndex(oldIndex) + 1;
			if (index != 0x10000) {
				PushValue(GetLUTValue(oldIndex));
				oldIndex = index - 1;
			} else
				break;
		}
		// After Loop 2
		fSavedByte = GetLUTValue(oldIndex);
		PushValue(fSavedByte);
		SetLUTValue(fDWordUnk, fSavedByte);
		SetLUTIndex(fDWordUnk, fSavedIndex);
		fDWordUnk++;
		if (fDWordUnk > (int)fBitMask) {
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
	fBufferOffset++;
	fBuffer[fBufferOffset] = value;
}


uint8
DecodingContext::PopValue()
{
	assert(fBufferOffset >= 0);
	return fBuffer[fBufferOffset--];
}
