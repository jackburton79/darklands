#include "CharacterFile.h"

#include "FileStream.h"
#include "Stream.h"

#include <memory>
#include <stdexcept>

// verified: 2 + 10 + 20 + 4 * 554 == file size
static const size_t kIndicesOffset	= 0x02;
static const size_t kImagesOffset	= 0x0C;
static const size_t kHeaderSize		= 0x20;


CharacterFile::CharacterFile(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(
        new FileStream(fileName.c_str(), FileStream::READ_ONLY));
    const size_t size = stream->Size();
    std::vector<uint8> data(size);
    if (size < kHeaderSize
            || stream->ReadAt(0, data.data(), size) != (ssize_t)size)
        throw std::runtime_error("CharacterFile: file too small");

    const size_t count = data[0] | (data[1] << 8);
    if (size != kHeaderSize + count * kCharacterRecordSize)
        throw std::runtime_error("CharacterFile: unexpected file size");
    for (size_t i = 0; i < count; i++) {
        fCharacters.push_back(ReadCharacter(
            &data[kHeaderSize + i * kCharacterRecordSize]));
    }
    // no leader in the file: the first member
    fParty = MakeParty(fCharacters, &data[kIndicesOffset],
        &data[kImagesOffset], 0);
}
