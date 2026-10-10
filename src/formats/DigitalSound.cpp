#include "DigitalSound.h"

#include "FileStream.h"
#include "Stream.h"

#include <cstdio>
#include <memory>
#include <stdexcept>


DigitalSound::DigitalSound(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(new FileStream(fileName.c_str(),
        FileStream::READ_ONLY));
    const ssize_t size = stream->Size();
    if (size <= 0)
        throw std::runtime_error("DigitalSound: empty file " + fileName);
    fSamples.resize(size_t(size));
    if (stream->ReadAt(0, &fSamples[0], size_t(size)) != size)
        throw std::runtime_error("DigitalSound: cannot read " + fileName);
}


static void
PutWord(std::vector<uint8>& bytes, uint16 value)
{
    bytes.push_back(uint8(value & 0xFF));
    bytes.push_back(uint8(value >> 8));
}


static void
PutLong(std::vector<uint8>& bytes, uint32 value)
{
    PutWord(bytes, uint16(value & 0xFFFF));
    PutWord(bytes, uint16(value >> 16));
}


static void
PutTag(std::vector<uint8>& bytes, const char* tag)
{
    for (int i = 0; i < 4; i++)
        bytes.push_back(uint8(tag[i]));
}


void
DigitalSound::WriteWav(const std::string& fileName, uint32 rate) const
{
    // the 44 bytes of the canonical header, then the samples
    std::vector<uint8> bytes;
    const uint32 size = uint32(fSamples.size());
    PutTag(bytes, "RIFF");
    PutLong(bytes, 36 + size);
    PutTag(bytes, "WAVE");
    PutTag(bytes, "fmt ");
    PutLong(bytes, 16);
    PutWord(bytes, 1);			// PCM
    PutWord(bytes, 1);			// mono
    PutLong(bytes, rate);
    PutLong(bytes, rate);		// bytes a second
    PutWord(bytes, 1);			// bytes a frame
    PutWord(bytes, 8);			// bits a sample
    PutTag(bytes, "data");
    PutLong(bytes, size);
    bytes.insert(bytes.end(), fSamples.begin(), fSamples.end());

    FILE* file = fopen(fileName.c_str(), "wb");
    if (file == NULL)
        throw std::runtime_error("DigitalSound: cannot create " + fileName);
    const bool written = fwrite(&bytes[0], 1, bytes.size(), file)
        == bytes.size();
    fclose(file);
    if (!written)
        throw std::runtime_error("DigitalSound: cannot write " + fileName);
}
