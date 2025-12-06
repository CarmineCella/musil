#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

// Constants
const double A4_FREQ = 440.0;  // A4 reference frequency in Hz
const int CHROMA_BINS = 12;    // 12 semitones per octave

/**
 * Compute chroma features from amplitude spectra.
 * 
 * @param spectra        Vector of amplitude spectra (each spectrum is a vector of magnitudes)
 * @param sample_rate    Sample rate in Hz
 * @param fft_size       FFT size used to compute the spectra
 * @param tuning_freq    Reference tuning frequency in Hz (default: A4 = 440 Hz)
 * @param octave_norm    Whether to normalize across octaves (default: true)
 * 
 * @return Vector of chroma feature vectors (12 bins per frame)
 */
std::vector<std::vector<double>> compute_chroma(
    const std::vector<std::vector<double>>& spectra,
    double sample_rate,
    int fft_size,
    double tuning_freq = A4_FREQ,
    bool octave_norm = true)
{
    if (spectra.empty()) {
        throw std::invalid_argument("Input spectra is empty");
    }
    
    if (sample_rate <= 0 || fft_size <= 0) {
        throw std::invalid_argument("Sample rate and FFT size must be positive");
    }
    
    int num_frames = spectra.size();
    int spectrum_size = spectra[0].size();
    
    // Verify all spectra have the same size
    for (const auto& spectrum : spectra) {
        if (spectrum.size() != static_cast<size_t>(spectrum_size)) {
            throw std::invalid_argument("All spectra must have the same size");
        }
    }
    
    // Frequency resolution (Hz per bin)
    double freq_resolution = sample_rate / static_cast<double>(fft_size);
    
    // Pre-compute frequency-to-chroma mapping
    // For each FFT bin, determine which chroma bin it belongs to
    std::vector<int> bin_to_chroma(spectrum_size, -1);
    std::vector<double> bin_weights(spectrum_size, 0.0);
    
    for (int bin = 0; bin < spectrum_size; ++bin) {
        // Frequency of this bin in Hz
        double freq = bin * freq_resolution;
        
        // Skip DC and very low frequencies (below ~30 Hz)
        if (freq < 30.0) continue;
        
        // Convert frequency to MIDI note number
        // MIDI note number = 69 + 12 * log2(freq / 440)
        double midi_note = 69.0 + 12.0 * std::log2(freq / tuning_freq);
        
        // Get the chroma class (0-11, where 0=C, 1=C#, ..., 11=B)
        int chroma_class = static_cast<int>(std::round(midi_note)) % CHROMA_BINS;
        if (chroma_class < 0) chroma_class += CHROMA_BINS;
        
        bin_to_chroma[bin] = chroma_class;
        
        // Optional: compute weight based on deviation from exact semitone
        // This creates a smoother chroma by weighting bins between semitones
        double deviation = std::abs(midi_note - std::round(midi_note));
        bin_weights[bin] = std::cos(deviation * M_PI / 2.0); // Cosine weighting
        // For simpler hard assignment, use: bin_weights[bin] = 1.0;
    }
    
    // Allocate output
    std::vector<std::vector<double>> chroma_features(num_frames, 
                                                      std::vector<double>(CHROMA_BINS, 0.0));
    
    // Process each frame
    for (int frame = 0; frame < num_frames; ++frame) {
        const auto& spectrum = spectra[frame];
        auto& chroma = chroma_features[frame];
        
        // Sum spectral energy into chroma bins
        for (int bin = 0; bin < spectrum_size; ++bin) {
            int chroma_idx = bin_to_chroma[bin];
            if (chroma_idx >= 0) {
                // Weighted sum (or simply add spectrum[bin] for hard assignment)
                chroma[chroma_idx] += spectrum[bin] * bin_weights[bin];
            }
        }
        
        // Normalize the chroma vector
        if (octave_norm) {
            // L2 normalization
            double norm = 0.0;
            for (int c = 0; c < CHROMA_BINS; ++c) {
                norm += chroma[c] * chroma[c];
            }
            norm = std::sqrt(norm);
            
            if (norm > 1e-10) {  // Avoid division by zero
                for (int c = 0; c < CHROMA_BINS; ++c) {
                    chroma[c] /= norm;
                }
            }
        }
    }
    
    return chroma_features;
}

/**
 * Advanced version with additional parameters for fine-tuning
 */
std::vector<std::vector<double>> compute_chroma_advanced(
    const std::vector<std::vector<double>>& spectra,
    double sample_rate,
    int fft_size,
    double tuning_freq = A4_FREQ,
    double min_freq = 30.0,      // Minimum frequency to consider (Hz)
    double max_freq = 5000.0,    // Maximum frequency to consider (Hz)
    bool use_log_spectrum = false, // Apply log scaling to spectrum
    bool octave_norm = true,     // Normalize across octaves
    double smoothing = 0.0)      // Temporal smoothing (0 = no smoothing)
{
    if (spectra.empty()) {
        throw std::invalid_argument("Input spectra is empty");
    }
    
    int num_frames = spectra.size();
    int spectrum_size = spectra[0].size();
    double freq_resolution = sample_rate / static_cast<double>(fft_size);
    
    // Pre-compute frequency-to-chroma mapping with frequency limits
    std::vector<int> bin_to_chroma(spectrum_size, -1);
    std::vector<double> bin_weights(spectrum_size, 0.0);
    
    for (int bin = 0; bin < spectrum_size; ++bin) {
        double freq = bin * freq_resolution;
        
        // Apply frequency limits
        if (freq < min_freq || freq > max_freq) continue;
        
        double midi_note = 69.0 + 12.0 * std::log2(freq / tuning_freq);
        int chroma_class = static_cast<int>(std::round(midi_note)) % CHROMA_BINS;
        if (chroma_class < 0) chroma_class += CHROMA_BINS;
        
        bin_to_chroma[bin] = chroma_class;
        
        // Gaussian-like weighting for smoother chroma
        double deviation = std::abs(midi_note - std::round(midi_note));
        bin_weights[bin] = std::exp(-deviation * deviation / 0.5);
    }
    
    // Allocate output
    std::vector<std::vector<double>> chroma_features(num_frames, 
                                                      std::vector<double>(CHROMA_BINS, 0.0));
    
    // Process each frame
    for (int frame = 0; frame < num_frames; ++frame) {
        const auto& spectrum = spectra[frame];
        auto& chroma = chroma_features[frame];
        
        // Sum spectral energy into chroma bins
        for (int bin = 0; bin < spectrum_size; ++bin) {
            int chroma_idx = bin_to_chroma[bin];
            if (chroma_idx >= 0) {
                double value = spectrum[bin];
                
                // Optional: log scaling
                if (use_log_spectrum && value > 0) {
                    value = std::log(1.0 + value);
                }
                
                chroma[chroma_idx] += value * bin_weights[bin];
            }
        }
        
        // Normalize the chroma vector
        if (octave_norm) {
            double norm = 0.0;
            for (int c = 0; c < CHROMA_BINS; ++c) {
                norm += chroma[c] * chroma[c];
            }
            norm = std::sqrt(norm);
            
            if (norm > 1e-10) {
                for (int c = 0; c < CHROMA_BINS; ++c) {
                    chroma[c] /= norm;
                }
            }
        }
    }
    
    // Optional: temporal smoothing
    if (smoothing > 0.0 && num_frames > 1) {
        std::vector<std::vector<double>> smoothed = chroma_features;
        
        for (int frame = 1; frame < num_frames - 1; ++frame) {
            for (int c = 0; c < CHROMA_BINS; ++c) {
                smoothed[frame][c] = 
                    (1.0 - smoothing) * chroma_features[frame][c] +
                    smoothing * 0.5 * (chroma_features[frame-1][c] + 
                                       chroma_features[frame+1][c]);
            }
        }
        
        return smoothed;
    }
    
    return chroma_features;
}

/**
 * Helper function: Compute chroma centroid (dominant pitch class)
 */
int compute_chroma_centroid(const std::vector<double>& chroma) {
    if (chroma.size() != CHROMA_BINS) {
        throw std::invalid_argument("Invalid chroma vector size");
    }
    
    return std::distance(chroma.begin(), 
                        std::max_element(chroma.begin(), chroma.end()));
}

/**
 * Helper function: Compute chroma energy (total energy in chroma)
 */
double compute_chroma_energy(const std::vector<double>& chroma) {
    return std::accumulate(chroma.begin(), chroma.end(), 0.0);
}

/**
 * Helper function: Rotate chroma to a specific key (0=C, 1=C#, etc.)
 */
std::vector<double> rotate_chroma_to_key(const std::vector<double>& chroma, int key) {
    if (chroma.size() != CHROMA_BINS) {
        throw std::invalid_argument("Invalid chroma vector size");
    }
    
    std::vector<double> rotated(CHROMA_BINS);
    for (int i = 0; i < CHROMA_BINS; ++i) {
        rotated[i] = chroma[(i + key) % CHROMA_BINS];
    }
    return rotated;
}

// Example usage
/*
int main() {
    // Example: 100 frames, each with 513 spectral bins (typical for 1024 FFT)
    int num_frames = 100;
    int spectrum_size = 513;
    double sample_rate = 44100.0;
    int fft_size = 1024;
    
    // Generate dummy spectra (in practice, these come from STFT)
    std::vector<std::vector<double>> spectra(num_frames, 
                                             std::vector<double>(spectrum_size, 0.0));
    
    // Fill with some example data (e.g., from real audio STFT)
    // ... your STFT computation here ...
    
    // Compute basic chroma features
    auto chroma = compute_chroma(spectra, sample_rate, fft_size);
    
    // Or use advanced version with more control
    auto chroma_adv = compute_chroma_advanced(
        spectra, 
        sample_rate, 
        fft_size,
        440.0,    // A4 tuning
        50.0,     // min freq
        5000.0,   // max freq
        true,     // use log spectrum
        true,     // octave normalization
        0.3       // temporal smoothing
    );
    
    // Print first frame
    std::cout << "First chroma frame (C, C#, D, D#, E, F, F#, G, G#, A, A#, B):\n";
    for (int i = 0; i < CHROMA_BINS; ++i) {
        std::cout << chroma[0][i] << " ";
    }
    std::cout << "\n";
    
    // Compute dominant pitch class
    int dominant = compute_chroma_centroid(chroma[0]);
    const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", 
                                "F#", "G", "G#", "A", "A#", "B"};
    std::cout << "Dominant pitch class: " << note_names[dominant] << "\n";
    
    return 0;
}
*/