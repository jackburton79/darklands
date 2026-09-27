/*
 * DescriptionFile.h
 * Reader for DARKLAND.DSC, the one-line city descriptions ("a small
 * North Sea port controlled by Dutch nobles"): the $PlaceDesc of the
 * cards. Record i describes city i of DARKLAND.CTY. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

class DescriptionFile {
public:
    explicit		DescriptionFile(const std::string& fileName);	// throws on error

    uint32			CountDescriptions() const;
    // UTF-8.
    const std::string& DescriptionAt(uint32 index) const;

private:
    std::vector<std::string>	fDescriptions;
};
