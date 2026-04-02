// wav_tools.h
//

#ifndef WAV_TOOLS_H
#define WAV_TOOLS_H

#include <cstdint>
#include <fstream>
#include <sstream>
#include <cstring>

// WAV I/O -----------------------------------------------------------------
struct WAVHeader {
    char     riff[4];          // "RIFF"
    uint32_t chunkSize;        // 36 + dataSize
    char     wave[4];          // "WAVE"
    char     fmt[4];           // "fmt "
    uint32_t subchunk1Size;    // 16 for PCM
    uint16_t audioFormat;      // 1=PCM, 3=IEEE float
    uint16_t numChannels;      // number of channels
    uint32_t sampleRate;       // sample rate
    uint32_t byteRate;         // sampleRate * numChannels * bitsPerSample / 8
    uint16_t blockAlign;       // numChannels * bitsPerSample / 8
    uint16_t bitsPerSample;    // bits per sample (16 or 32)
    char     data[4];          // "data"
    uint32_t dataSize;         // size of data section in bytes
};

inline std::vector<std::vector<double>> read_wav_raw(const char* filename, WAVHeader& header) {
    std::stringstream err;
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        err << "cannot open WAV file " << filename;
        throw std::runtime_error(err.str());
    }

    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    if (!file.good()) {
        err << "invalid or truncated WAV header";
        throw std::runtime_error(err.str());
    }

    if (header.audioFormat != 1 && header.audioFormat != 3) { // PCM or IEEE float
        err << "unsupported WAV format (not PCM or IEEE float)";
        throw std::runtime_error(err.str());
    }

    if (header.bitsPerSample != 16 && !(header.bitsPerSample == 32 && header.audioFormat == 3)) {
        err << "unsupported bits per sample: " << header.bitsPerSample;
        throw std::runtime_error(err.str());
    }

    int bytesPerSample = header.bitsPerSample / 8;
    int numSamples = static_cast<int>(header.dataSize / bytesPerSample);
    numSamples /= header.numChannels;

    std::vector<std::vector<double>> channels(
        header.numChannels, std::vector<double>(numSamples));

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < header.numChannels; ++ch) {
            if (header.bitsPerSample == 16) {
                int16_t sample;
                file.read(reinterpret_cast<char*>(&sample), sizeof(int16_t));
                if (!file.good()) {
                    err << "unexpected EOF while reading samples";
                    throw std::runtime_error(err.str());
                }
                channels[ch][i] = sample / 32768.0;  // [-1,1]
            } else if (header.bitsPerSample == 32 && header.audioFormat == 3) {
                float sample;
                file.read(reinterpret_cast<char*>(&sample), sizeof(float));
                if (!file.good()) {
                    err << "unexpected EOF while reading samples";
                    throw std::runtime_error(err.str());
                }
                channels[ch][i] = sample;  // already [-1,1]
            }
        }
    }

    file.close();
    return channels;
}

inline void write_wav_raw(const char* filename,
                          std::vector<std::vector<double>>& channels,
                          WAVHeader& header) {
    std::stringstream err;
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        err << "cannot write WAV file " << filename;
        throw std::runtime_error(err.str());
    }

    if (channels.empty()) {
        err << "no channels to write";
        throw std::runtime_error(err.str());
    }

    std::size_t numSamples = channels[0].size();
    for (std::size_t ch = 1; ch < channels.size(); ++ch) {
        if (channels[ch].size() != numSamples) {
            err << "all channels must have the same length";
            throw std::runtime_error(err.str());
        }
    }

    // compute dataSize from channels
    header.dataSize = 0;
    for (const auto& ch : channels) {
        header.dataSize += static_cast<uint32_t>(ch.size() * (header.bitsPerSample / 8));
    }

    // chunkSize = 36 + dataSize
    header.chunkSize = 36 + header.dataSize;

    file.write(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    int bytesPerSample = header.bitsPerSample / 8;
    (void) bytesPerSample;
    int numSamplesInt  = static_cast<int>(numSamples);

    for (int i = 0; i < numSamplesInt; ++i) {
        for (std::size_t ch = 0; ch < channels.size(); ++ch) {
            double v = channels[ch][i];
            if (header.bitsPerSample == 16) {
                if (v > 1.0)  v = 1.0;
                if (v < -1.0) v = -1.0;
                int16_t intSample = static_cast<int16_t>(v * 32767.0);
                file.write(reinterpret_cast<char*>(&intSample), sizeof(int16_t));
            } else if (header.bitsPerSample == 32 && header.audioFormat == 3) {
                float floatSample = static_cast<float>(v);
                file.write(reinterpret_cast<char*>(&floatSample), sizeof(float));
            } else {
                err << "unsupported bits per sample or audio format";
                throw std::runtime_error(err.str());
            }
        }
    }

    file.close();
}

#endif // WAV_TOOLS_H

// eof
