/*
 * Sprite.h
 * The pictures of the battle files (.IMC sprites, BATTLEGR.IMG,
 * COMMONSP.IMG): width, height, then per row the pixel count, the blank
 * pixels on the left and the pixels. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <vector>

// One picture; index 0 is transparent
struct sprite {
    uint16 width;
    uint16 height;
    std::vector<uint8> pixels;	// width * height
};

// Reads the picture at data[position]. Throws std::runtime_error if it
// does not fit in data.
sprite ReadSprite(const std::vector<uint8>& data, size_t position);
