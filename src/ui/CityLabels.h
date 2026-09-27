/*
 * CityLabels.h
 * City names drawn over the world map.
 */
#pragma once

#include "GraphicsDefs.h"

class Bitmap;
class Font;
class LocationFile;
class WorldMap;

// Writes the name of every city (location type 0) centered above its
// tile, in white with a black outline. `origin` is the map pixel at the
// bitmap's top-left corner, as in WorldMap::Draw().
void DrawCityLabels(Bitmap* bitmap, const GFX::point& origin,
    const WorldMap& map, const LocationFile& locations, const Font& font);
