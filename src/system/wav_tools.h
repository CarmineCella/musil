// wav_tools.h — WAV files in and out: 8, 16, 24 and 32-bit integer PCM, 32 and 64-bit IEEE float, any number of
// channels and any sample rate, plain and WAVE_FORMAT_EXTENSIBLE headers, unknown chunks skipped.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.

#ifndef WAV_TOOLS_H
#define WAV_TOOLS_H

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// What is read from a file, or given to write one: the sample rate, the channel count, the bit depth and whether
// samples are floats. (The layout on disk is written and read by the functions below, not by this struct.)
struct WAVHeader {
    uint16_t audioFormat = 1;      // 1 = PCM integers, 3 = IEEE float
    uint16_t numChannels = 1;
    uint32_t sampleRate = 44100;
    uint16_t bitsPerSample = 16;   // 8, 16, 24, 32 (PCM); 32, 64 (float)
    uint32_t dataSize = 0;         // bytes of samples
};

namespace wav_detail {
inline uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t rd32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
inline void wr16(std::ostream& o, uint16_t v) { unsigned char b[2] = { (unsigned char)(v & 255), (unsigned char)(v >> 8) }; o.write((const char*)b, 2); }
inline void wr32(std::ostream& o, uint32_t v) { unsigned char b[4] = { (unsigned char)(v & 255), (unsigned char)((v >> 8) & 255), (unsigned char)((v >> 16) & 255), (unsigned char)((v >> 24) & 255) }; o.write((const char*)b, 4); }
}

// (read) the channels of a WAV file as doubles in [-1, 1], and its header; throws with a message on failure
inline std::vector<std::vector<double>> read_wav_raw(const char* filename, WAVHeader& header) {
    using namespace wav_detail;
    std::ifstream f(filename, std::ios::binary);
    if (!f) throw std::runtime_error(std::string("cannot open WAV file ") + filename);
    unsigned char riff[12]; f.read((char*)riff, 12);
    if (f.gcount() != 12 || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) throw std::runtime_error(std::string("not a WAV file: ") + filename);
    bool have_fmt = false; std::vector<unsigned char> data; header = WAVHeader{};
    while (true) {                                            // the chunks: fmt and data are used, the others (LIST, fact, cue, bext ...) skipped
        unsigned char ch[8]; f.read((char*)ch, 8); if (f.gcount() != 8) break;
        uint32_t size = rd32(ch + 4);
        if (std::memcmp(ch, "fmt ", 4) == 0) {
            std::vector<unsigned char> fmt(size); f.read((char*)fmt.data(), size); if ((uint32_t)f.gcount() != size) throw std::runtime_error("truncated fmt chunk");
            header.audioFormat = rd16(fmt.data()); header.numChannels = rd16(fmt.data() + 2); header.sampleRate = rd32(fmt.data() + 4); header.bitsPerSample = rd16(fmt.data() + 14);
            if (header.audioFormat == 0xFFFE && size >= 40) header.audioFormat = rd16(fmt.data() + 24);   // extensible: the real format is in the sub-format's first two bytes
            have_fmt = true;
        } else if (std::memcmp(ch, "data", 4) == 0) {
            data.resize(size); f.read((char*)data.data(), size); data.resize((size_t)f.gcount());   // a truncated file yields what it has
            header.dataSize = (uint32_t)data.size(); break;
        } else { f.seekg(size + (size & 1), std::ios::cur); }
        if (size & 1 && std::memcmp(ch, "data", 4) != 0) {}
    }
    if (!have_fmt) throw std::runtime_error(std::string("no fmt chunk in ") + filename);
    if (header.numChannels == 0) throw std::runtime_error("a WAV file with no channels");
    int bits = header.bitsPerSample, fmt = header.audioFormat;
    bool ok = (fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && (bits == 32 || bits == 64));
    if (!ok) { std::ostringstream err; err << "unsupported WAV format " << fmt << " at " << bits << " bits"; throw std::runtime_error(err.str()); }
    size_t bps = (size_t)bits / 8, frames = data.size() / (bps * header.numChannels);
    std::vector<std::vector<double>> chans(header.numChannels, std::vector<double>(frames));
    const unsigned char* p = data.data();
    for (size_t n = 0; n < frames; n++) for (int c = 0; c < header.numChannels; c++, p += bps) {
        double v;
        if (fmt == 3 && bits == 32) { float x; std::memcpy(&x, p, 4); v = x; }
        else if (fmt == 3) { double x; std::memcpy(&x, p, 8); v = x; }
        else if (bits == 8) v = ((int)p[0] - 128) / 128.0;                                          // 8-bit PCM is unsigned
        else if (bits == 16) v = (int16_t)rd16(p) / 32768.0;
        else if (bits == 24) { int32_t x = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8; v = x / 8388608.0; }
        else v = (int32_t)rd32(p) / 2147483648.0;
        chans[c][n] = v;
    }
    return chans;
}

// (write) channels of doubles to a WAV file with the header's rate, bit depth and format (PCM or float)
inline void write_wav_raw(const char* filename, const std::vector<std::vector<double>>& chans, WAVHeader header) {
    using namespace wav_detail;
    if (chans.empty()) throw std::runtime_error("no channels to write");
    for (auto& c : chans) if (c.size() != chans[0].size()) throw std::runtime_error("all channels must have the same length");
    int bits = header.bitsPerSample, fmt = header.audioFormat;
    bool ok = (fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && (bits == 32 || bits == 64));
    if (!ok) { std::ostringstream err; err << "unsupported WAV format " << fmt << " at " << bits << " bits"; throw std::runtime_error(err.str()); }
    std::ofstream f(filename, std::ios::binary);
    if (!f) throw std::runtime_error(std::string("cannot create WAV file ") + filename);
    uint16_t nch = (uint16_t)chans.size(); size_t bps = (size_t)bits / 8, frames = chans[0].size();
    uint32_t data_size = (uint32_t)(frames * bps * nch), fmt_size = 16 + (fmt == 3 ? 2 : 0);     // float files carry cbSize = 0
    f.write("RIFF", 4); wr32(f, 4 + (8 + fmt_size) + (fmt == 3 ? 12 : 0) + (8 + data_size) + (data_size & 1)); f.write("WAVE", 4);
    f.write("fmt ", 4); wr32(f, fmt_size); wr16(f, (uint16_t)fmt); wr16(f, nch); wr32(f, header.sampleRate);
    wr32(f, (uint32_t)(header.sampleRate * nch * bps)); wr16(f, (uint16_t)(nch * bps)); wr16(f, (uint16_t)bits);
    if (fmt == 3) { wr16(f, 0); f.write("fact", 4); wr32(f, 4); wr32(f, (uint32_t)frames); }
    f.write("data", 4); wr32(f, data_size);
    std::vector<unsigned char> buf(frames * bps * nch); unsigned char* p = buf.data();
    auto clamp = [](double v) { return v < -1.0 ? -1.0 : v > 1.0 ? 1.0 : v; };
    for (size_t n = 0; n < frames; n++) for (int c = 0; c < nch; c++, p += bps) {
        double v = chans[c][n];
        if (fmt == 3 && bits == 32) { float x = (float)v; std::memcpy(p, &x, 4); }
        else if (fmt == 3) { std::memcpy(p, &v, 8); }
        else if (bits == 8) { int x = (int)std::lround(clamp(v) * 127.0) + 128; p[0] = (unsigned char)x; }
        else if (bits == 16) { int32_t x = (int32_t)std::lround(clamp(v) * 32767.0); p[0] = (unsigned char)(x & 255); p[1] = (unsigned char)((x >> 8) & 255); }
        else if (bits == 24) { int32_t x = (int32_t)std::lround(clamp(v) * 8388607.0); p[0] = (unsigned char)(x & 255); p[1] = (unsigned char)((x >> 8) & 255); p[2] = (unsigned char)((x >> 16) & 255); }
        else { int32_t x = (int32_t)std::llround(clamp(v) * 2147483647.0); p[0] = (unsigned char)(x & 255); p[1] = (unsigned char)((x >> 8) & 255); p[2] = (unsigned char)((x >> 16) & 255); p[3] = (unsigned char)((x >> 24) & 255); }
    }
    f.write((const char*)buf.data(), buf.size()); if (data_size & 1) f.put(0);
    if (!f) throw std::runtime_error(std::string("error writing ") + filename);
}

#endif // WAV_TOOLS_H
