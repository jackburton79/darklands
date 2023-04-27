/*
 * PicDecoder.cpp
 *
 *  Created on: 27 apr 2023
 *      Author: Stefano Ceccherini
 */

#include "PicDecoder.h"

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


PicDecoder::PicDecoder()
{

}


PicDecoder::~PicDecoder()
{
}


Bitmap*
PicDecoder::GetImage(Stream* stream)
{

}
