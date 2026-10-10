/*
 * DigitalSound.h
 * Reader for the .DGT files (OPENDARK.DGT, ENDDARK1.DGT, ENDDARK2.DGT): the
 * digitized sound of the opening and the endings. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

class DigitalSound {
public:
    // The sampling rate the files are played at is not known: this is a
    // guess (*inferred*, from the files' spectra: a low-pass at about
    // three quarters of the Nyquist frequency)
    static const uint32 kDefaultRate = 8000;

    explicit		DigitalSound(const std::string& fileName);	// throws on error

    // Unsigned 8-bit samples, mono
    const std::vector<uint8>& Samples() const	{ return fSamples; }
    // In seconds, at a rate
    double			Duration(uint32 rate) const
                        { return double(fSamples.size()) / rate; }

    // Writes a WAV file (PCM, 8 bits, mono) at the given rate; throws on
    // error
    void			WriteWav(const std::string& fileName, uint32 rate) const;

private:
    std::vector<uint8> fSamples;
};
