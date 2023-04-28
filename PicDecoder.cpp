/*
 * PicDecoder.cpp
 *
 *  Created on: 27 apr 2023
 *      Author: Stefano Ceccherini
 */

#include "PicDecoder.h"

#include "Bitmap.h"
#include "Stream.h"

#include <cassert>
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
	DecodingContext(Stream* stream, uint16 magic);

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

	int fRepeatCount = 0;
	uint8 fRepeatByte = 0;
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
PicDecoder::PicDecoder()
	:
	fContext(NULL)
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

	fContext = new DecodingContext(stream, magicWord);

	Bitmap* bitmap = new Bitmap(width, height, 8);
	uint8* line = new uint8[width];

	GFX::Palette palette;
	for (auto c = 0; c < 50; c++) {
		palette.colors[c].a = 0;
		palette.colors[c].r = 0;
		palette.colors[c].g = 30;
		palette.colors[c].b = 30;
	}
	for (auto c = 51; c < 100; c++) {
		palette.colors[c].a = 0;
		palette.colors[c].r = 30;
		palette.colors[c].g = 30;
		palette.colors[c].b = 0;
	}
	for (auto c = 101; c < 200; c++) {
		palette.colors[c].a = 0;
		palette.colors[c].r = 90;
		palette.colors[c].g = 0;
		palette.colors[c].b = 140;
	}
	for (auto c = 201; c < 256; c++) {
		palette.colors[c].a = 0;
		palette.colors[c].r = 10;
		palette.colors[c].g = 200;
		palette.colors[c].b = 10;
	}
	bitmap->SetPalette(palette);
	for (auto y = 0; y < height; y++) {
		fContext->DecodeNextBytes(line, width);
		for (auto x = 0; x < width; x++) {
			uint8 value = line[x];
			//uint32 color = bitmap->MapColor(palette.colors[value].r,
			//		palette.colors[value].g,
			//		palette.colors[value].b);
			bitmap->PutPixel(x, y, value);
		}
	}

	delete line;

	delete fContext;

	return bitmap;
}


// DecodingContext
DecodingContext::DecodingContext(Stream* stream, uint16 magic)
{
	fStream = stream;

	fMagicWord = magic;
	fRepeatCount = 0;
	fRepeatByte = 0;
	fBuffer = new uint8[10000];
	fBufferOffset = 0;

	fMagicByte = uint8(magic & 0xFF);
	if (fMagicByte > 11)
		fMagicByte = 11;

	fMagicWord &= 0xff00;
    fMagicWord |= fMagicByte;

	fBitPointer = 8;
	fLUT = new uint8[(1 << fMagicByte) * 3];

	fSavedIndex = 0;
	fSavedByte = 0;

	_SetupBuffer();
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
	bool debug = true;
	bool bcdPacked = false;
	int opcount;
	if (bcdPacked) {
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
		if (bcdPacked) {
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
		/// Loop 1
		while (c < fOnesCounter) {
			fMagicWord = fStream->ReadWordLE();
			b |= (fMagicWord << c);
			c += 16;
		}

		/// After Loop 1
		fBitPointer = c - fOnesCounter;
		int oldIndex = int(b & fBitMask);
		int newIndex = oldIndex;
		if (oldIndex >= fDWordUnk) {
			newIndex = fDWordUnk;
			oldIndex = fSavedIndex;
			PushValue(fSavedByte);
		}

		/// Loop 2
		while (true) {
			int index = GetLUTIndex(oldIndex) + 1;
			if (index != 0x10000) {
				PushValue(GetLUTValue(oldIndex));
				oldIndex = index - 1;
			} else
				break;
		}
		/// After Loop 2
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
