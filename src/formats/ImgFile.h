/*
 * ImgFile.h
 * Reader for the battle's picture files: BATTLEGR.IMG (damage numbers,
 * cursors, missiles, spell clouds...) and COMMONSP.IMG (frame pieces).
 *
 * A word with the size of the data, one word per picture (its offset in
 * paragraphs from the start of the data), then the pictures (Sprite.h).
 * See docs/formats.md.
 */
#pragma once

#include "Sprite.h"
#include "SupportDefs.h"

#include <string>
#include <vector>

class ImgFile {
public:
    explicit		ImgFile(const std::string& fileName);	// throws on error

    uint32			CountPictures() const;
    const sprite&	PictureAt(uint32 index) const;	// throws on bad index

private:
    std::vector<sprite> fPictures;
};
