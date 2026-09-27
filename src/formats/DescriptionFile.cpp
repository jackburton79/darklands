#include "DescriptionFile.h"

#include "FileStream.h"
#include "LocationFile.h"
#include "Stream.h"

#include <cstring>
#include <memory>
#include <stdexcept>

// verified: 1 + 92 * 80 == file size. Byte 0 (0x5E) is not the count.
static const size_t kHeaderSize		= 1;
static const size_t kRecordSize		= 80;	// NUL-terminated, NUL-padded


DescriptionFile::DescriptionFile(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(
        new FileStream(fileName.c_str(), FileStream::READ_ONLY));
    const size_t size = stream->Size();
    if (size < kHeaderSize || (size - kHeaderSize) % kRecordSize != 0)
        throw std::runtime_error("DescriptionFile: unexpected file size");

    const size_t count = (size - kHeaderSize) / kRecordSize;
    for (size_t i = 0; i < count; i++) {
        char record[kRecordSize];
        if (stream->ReadAt(kHeaderSize + i * kRecordSize, record, kRecordSize)
                != (ssize_t)kRecordSize) {
            throw std::runtime_error("DescriptionFile: truncated record");
        }
        fDescriptions.push_back(LocationFile::DecodeName(record,
            strnlen(record, kRecordSize)));
    }
}


uint32
DescriptionFile::CountDescriptions() const
{
    return uint32(fDescriptions.size());
}


const std::string&
DescriptionFile::DescriptionAt(uint32 index) const
{
    if (index >= fDescriptions.size())
        throw std::out_of_range("DescriptionFile::DescriptionAt(): invalid index");
    return fDescriptions[index];
}
