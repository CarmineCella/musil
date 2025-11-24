// utils.h
//

#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <vector>
#include <sstream>
#include <fstream>

// WAV I/O
struct WAVHeader {
    char riff[4];                // RIFF Header
    uint32_t chunkSize;           // RIFF Chunk Size
    char wave[4];                 // WAVE Header
    char fmt[4];                  // fmt subchunk
    uint32_t subchunk1Size;       // Size of the fmt chunk
    uint16_t audioFormat;         // Audio format (1=PCM, 3=IEEE float)
    uint16_t numChannels;         // Number of channels
    uint32_t sampleRate;          // Sampling rate
    uint32_t byteRate;            // (sampleRate * numChannels * bitsPerSample) / 8
    uint16_t blockAlign;          // numChannels * bitsPerSample / 8
    uint16_t bitsPerSample;       // Bits per sample
    char data[4];                 // "data" string
    uint32_t dataSize;            // Size of the data section
};
std::vector<std::vector<double>> read_wav(const char* filename, WAVHeader& header) {
    std::stringstream err;
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        err << "cannot open WAV file " << filename;
        throw std::runtime_error(err.str());
    }

    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    if (header.audioFormat != 1 && header.audioFormat != 3) { // Only PCM (1) and IEEE float (3) supported
        err << "unsupported WAV format (not PCM or IEEE float)";
        throw std::runtime_error(err.str());
    }

    int numSamples = header.dataSize / (header.bitsPerSample / 8);
    numSamples /= header.numChannels;
    std::vector<std::vector<double>> channels(header.numChannels, std::vector<double>(numSamples));

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < header.numChannels; ++ch) {
            if (header.bitsPerSample == 16) {
                int16_t sample;
                file.read(reinterpret_cast<char*>(&sample), sizeof(int16_t));
                channels[ch][i] = sample / 32768.0;  // Normalize to [-1.0, 1.0]
            } else if (header.bitsPerSample == 32 && header.audioFormat == 3) {
                float sample;
                file.read(reinterpret_cast<char*>(&sample), sizeof(float));
                channels[ch][i] = sample;  // No need to normalize, as it's already in [-1.0, 1.0]
            } else {
                err << "unsupported bits per sample: " << header.bitsPerSample;
                throw std::runtime_error(err.str());
            }
        }
    }

    file.close();
    return channels;
}
void write_wav(const char* filename, std::vector<std::vector<double>>& channels, WAVHeader& header) {
    std::stringstream err;
    std::ofstream file(filename, std::ios::binary);

    if (!file) {
        err << "cannot write WAV file " << filename;
        throw std::runtime_error(err.str());
    }

    header.dataSize = 0;
    for (const auto& channel : channels) {
        header.dataSize += channel.size() * (header.bitsPerSample / 8);
    }
    // header.dataSize /= header.numChannels;
    file.write(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    int numSamples = channels[0].size();
    for (int i = 0; i < numSamples; ++i) {
        for (size_t ch = 0; ch < channels.size(); ++ch) {
            if (header.bitsPerSample == 16) {
                int16_t intSample = static_cast<int16_t>(channels[ch][i] * 32767);
                file.write(reinterpret_cast<char*>(&intSample), sizeof(int16_t));
            } else if (header.bitsPerSample == 32 && header.audioFormat == 3) {
                float floatSample = static_cast<float>(channels[ch][i]);
                file.write(reinterpret_cast<char*>(&floatSample), sizeof(float));
            } else {
                err << "unsupported bits per sample or audio format";
                throw std::runtime_error(err.str());
            }
        }
    }

    file.close();
}
std::string remove_extension(const std::string& filename) {
    size_t last_dot = filename.find_last_of(".");
    if (last_dot != std::string::npos) {
        return filename.substr(0, last_dot);
    }
    return filename; // No extension found, return the original filename
}

#endif // UTILS_H
