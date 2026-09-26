#include "MsgFile.h"

#include "FileStream.h"
#include "Stream.h"

#include <cstring>
#include <memory>
#include <stdexcept>

static const size_t kCardHeaderSize = 5;


MsgFile::MsgFile(Stream* stream)
{
    _Load(stream);
}


MsgFile::MsgFile(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(
        new FileStream(fileName.c_str(), FileStream::READ_ONLY));
    _Load(stream.get());
}


uint32
MsgFile::CountCards() const
{
    return uint32(fCards.size());
}


const msg_card&
MsgFile::CardAt(uint32 index) const
{
    if (index >= fCards.size())
        throw std::out_of_range("MsgFile::CardAt(): invalid index");
    return fCards[index];
}


void
MsgFile::_Load(Stream* stream)
{
    const size_t size = stream->Size();
    if (size < 1)
        throw std::runtime_error("MsgFile: file too small");
    std::vector<uint8> data(size);
    if (stream->ReadAt(0, data.data(), size) != (ssize_t)size)
        throw std::runtime_error("MsgFile: read error");

    // byte 0: card count; then per card a 5-byte header and a
    // NUL-terminated text. Trailing bytes, if any, are ignored.
    const uint8 count = data[0];
    size_t offset = 1;
    fCards.reserve(count);
    for (uint8 i = 0; i < count; i++) {
        if (offset + kCardHeaderSize > size)
            throw std::runtime_error("MsgFile: truncated card header");
        msg_card card;
        card.textTop = data[offset];
        card.textLeft = data[offset + 1];
        card.unknown1 = data[offset + 2];
        card.textRight = data[offset + 3];
        card.unknown2 = data[offset + 4];
        offset += kCardHeaderSize;

        const uint8* text = &data[offset];
        const uint8* end = (const uint8*)memchr(text, 0, size - offset);
        if (end == NULL)
            throw std::runtime_error("MsgFile: unterminated card text");
        card.text.assign((const char*)text, end - text);
        offset += card.text.size() + 1;
        fCards.push_back(card);
    }
}
