/*
 * PartyColors.h
 * The colors of a party member's battle figure, as the party screen of a
 * new game (CRETSCRN.PIC) changes them: the keys 1, 2 and 3 (1st, 2nd and
 * 3rd color) each set a part of the 24 bytes (8 RGB triplets of 6 bits,
 * the palette indices 235..242 of the sprites) to one of six presets, which
 * depend on the character's picture (F01, F60, C00 or A00).
 * DARKLAND.EXE: overlay file 0x72230, 1462:14D0.. (docs/exe.md, "The party
 * screen").
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

static const int kFigureColorBytes = 24;

// The pictures a character can have, in the game's order
static const int kImageCount = 4;
const char* ImageCode(int index);			// "F01", "F60", "C00", "A00"
int ImageIndex(const std::string& image);	// -1 if unknown

// Puts preset `step` (0..5) of color `key` (1..3) in `colors` for the
// picture `image`; false if the picture is unknown. `colors` is made 24 bytes
// long first.
bool ApplyColorPreset(std::vector<uint8>& colors, int key,
    const std::string& image, int step);
