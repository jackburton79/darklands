/*
 * Lzexe.h
 * The LZ77 compression of LZEXE, used by the battle files (.IMC sprites,
 * the IMAPS.CAT maps). See docs/formats.md, "Battle sprites".
 */
#pragma once

#include "SupportDefs.h"

#include <vector>

// Throws std::runtime_error on invalid or truncated data.
std::vector<uint8> LzexeDecompress(const std::vector<uint8>& data);
