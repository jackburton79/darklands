#include "ImgFile.h"

#include "FileStream.h"
#include "Stream.h"

#include <memory>
#include <stdexcept>

static const size_t kParagraph = 16;


ImgFile::ImgFile(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(new FileStream(fileName.c_str(),
        FileStream::READ_ONLY));
    std::vector<uint8> data(stream->Size());
    if (stream->ReadAt(0, data.data(), data.size()) != (ssize_t)data.size())
        throw std::runtime_error("ImgFile: read error");

    // the table fills the bytes before the data: verified for both files
    if (data.size() < 2)
        throw std::runtime_error("ImgFile: file too small");
    const size_t dataSize = data[0] | (data[1] << 8);
    if (dataSize > data.size() - 2 || (data.size() - dataSize) % 2 != 0)
        throw std::runtime_error("ImgFile: invalid data size");
    const size_t start = data.size() - dataSize;
    const size_t count = (start - 2) / 2;

    fPictures.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const size_t offset = data[2 + 2 * i] | (data[3 + 2 * i] << 8);
        fPictures.push_back(ReadSprite(data, start + kParagraph * offset));
    }
}


uint32
ImgFile::CountPictures() const
{
    return uint32(fPictures.size());
}


const sprite&
ImgFile::PictureAt(uint32 index) const
{
    if (index >= fPictures.size())
        throw std::out_of_range("ImgFile::PictureAt(): invalid index");
    return fPictures[index];
}
