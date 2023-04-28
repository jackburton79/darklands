/*
 * PicDecoder.cpp
 *
 *  Created on: 27 apr 2023
 *      Author: Stefano Ceccherini
 */

#include "PicDecoder.h"

#include "Bitmap.h"
#include "Stream.h"
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


#include <iostream>

class DecodingContext {
public:
	DecodingContext(uint16 magic);

	uint16 GetLUTIndex(int id);
	uint8 GetLUTValue(int id);
	void SetLUTIndex(int id, int newId);
	void SetLUTValue(int id, uint8 value);

	void DecodeNextBytes(uint8* line);
private:
	void _SetupBuffer();

	uint16 fRepeatCount = 0;
	uint8 fRepeatByte = 0;
	uint8* fBuffer;
	uint16 fBufferOffset;

	uint8 fMagicByte;
	uint16 fBitPointer;
	uint8* fLUT;
};


// PicDecoder
PicDecoder::PicDecoder()
{

}


PicDecoder::~PicDecoder()
{
}


Bitmap*
PicDecoder::GetImage(Stream* stream)
{
	uint16 header = stream->ReadWordLE();
	if ((header & 0xFF) != 'X') {
		std::cerr << "GetImage: wrong format!" << std::endl;
		return nullptr;
	}

	bool bcdPacked = (header >> 8) & 1;

	uint16 compressedSize = stream->ReadWordLE(); // 0x04
	uint16 width = stream->ReadWordLE(); // 0x06
	uint16 height = stream->ReadWordLE(); // 0x08

	std::cout << "width: " << width << ", height: " << height << std::endl;
	std::cout << "size: " << compressedSize << std::endl;
	std::cout << "BCD packed: " << (bcdPacked ? "true" : "false") << std::endl;

	// Context
	uint16 magicWord = stream->ReadWordLE(); // 0x0A

	fContext = new DecodingContext(magicWord);

	Bitmap* bitmap = new Bitmap(width, height, 4);

	uint8* line = new uint8[width];

	for (auto y = 0; y < height; y++) {
		fContext->DecodeNextBytes(line);
		for (auto x = 0; x < width; x++) {
			uint8 value = line[x];
			bitmap->PutPixel(x, y, value/*palette[value]*/);
		}
	}

	delete line;

	return bitmap;
}


// DecodingContext
DecodingContext::DecodingContext(uint16 magic)
{
	fRepeatCount = 0;
	fRepeatByte = 0;
	fBuffer = new uint8[10000];
	fBufferOffset = 0;

	fMagicByte = std::min(magic & 0xFF, 11);
	fBitPointer = 8;
	fLUT = new uint8[(1 << fMagicByte) * 3];

	_SetupBuffer();
}


void
DecodingContext::_SetupBuffer()
{
	uint16 onesCounter = 0;
	uint32 bitMask = 0x1FF;
	uint32 dWordUnk = 0x100;

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
DecodingContext::DecodeNextBytes(uint8* line)
{

}
