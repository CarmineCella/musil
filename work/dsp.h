// dsp.h
//

#ifndef DSP_H
#define DSP_H

#include <cmath>
#include <complex>
#include <vector>

extern const double PI = 3.14159265358979323846;

// Fourier
int next_power_of_two (int n) {
    return pow (2, ceil (log2 (n)));
}
void fft (std::complex<double>* signal, int N) {
    if (N <= 1) return;

    std::vector<std::complex<double>> even (N / 2);
    std::vector<std::complex<double>> odd (N / 2);
    for (int i = 0; i < N / 2; ++i) {
        even[i] = signal[i * 2];
        odd[i] = signal[i * 2 + 1];
    }

    fft (even.data (), N / 2);
    fft (odd.data (), N / 2);

    for (int k = 0; k < N / 2; ++k) {
        std::complex<double> t = std::polar (1.0, -2 * PI * k / N) * odd[k];
        signal[k] = even[k] + t;
        signal[k + N / 2] = even[k] - t;
    }
}
void ifft (std::complex<double>* signal, int N) {
    for (int i = 0; i < N; ++i) {
        signal[i] = std::conj (signal[i]);
    }

    fft (signal, N);

    for (int i = 0; i < N; ++i) {
        signal[i] = std::conj (signal[i]) / static_cast<double> (N);
    }
}
double max_val_cplx (std::complex<double>* arr, int N, int& max_pos) {
    double max_magnitude = 0.0;
    max_pos = 0;  // Default position to 0

    for (int i = 0; i < N; ++i) {
        double magnitude = std::abs(arr[i]);
        if (magnitude > max_magnitude) {
            max_magnitude = magnitude;
            max_pos = i;
        }
    }

    return max_magnitude;
}
std::vector<double> hann_window(int size) {
    std::vector<double> window(size);
    for (int i = 0; i < size; ++i) {
        window[i] = 0.5 * (1 - cos(2 * PI * i / (size - 1)));
    }
    return window;
}
void apply_window(std::vector<std::complex<double>>& buffer, const std::vector<double>& window) {
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] *= window[i];
    }
}

// features
double compute_energy(const std::vector<double>& signal) {
    double energy = 0.0;
    for (double sample : signal) {
        energy += sample * sample;
    }
    return energy / signal.size();
}
double compute_spectral_centroid(const std::vector<double>& spectrum, double sampleRate, int fftSize) {
    double weighted_sum = 0.0, sum = 0.0;
    for (int i = 0; i < fftSize / 2; ++i) {
        weighted_sum += i * spectrum[i];
        sum += spectrum[i];
    }
    return (sum != 0) ? (weighted_sum / sum) * (sampleRate / fftSize) : 0.0;
}
double compute_spectral_spread(const std::vector<double>& spectrum, double centroid, double sampleRate, int fftSize) {
    double variance = 0.0, sum = 0.0;
    for (int i = 0; i < fftSize / 2; ++i) {
        double frequency = i * (sampleRate / fftSize);
        variance += spectrum[i] * pow(frequency - centroid, 2);
        sum += spectrum[i];
    }
    return (sum != 0) ? sqrt(variance / sum) : 0.0;
}
double compute_spectral_flux(const std::vector<double>& spectrum, const std::vector<double>& prev_spectrum) {
    double flux = 0.0;
    for (size_t i = 0; i < spectrum.size(); ++i) {
        flux += pow(spectrum[i] - prev_spectrum[i], 2);
    }
    return sqrt(flux);
}

#endif // DSP_H
