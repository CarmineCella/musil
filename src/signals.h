// signals.h — offline signal processing on vectors, C++ half. The Musil half
// is signals.mu. Ported from Musil 1.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// A signal is a vector of samples. A complex spectrum is (list re im): two
// vectors of the same length. A polar spectrum is (list mag phase). A stereo
// buffer is (list left right), as read-wav returns it.
//
// Only what needs an element loop is here: the FFT, the table oscillator,
// the recursive (IIR) filter, fractional delay, windowed-sinc resampling,
// autocorrelation, channel (de)interleaving, peak picking and gather. Windows, wavetables,
// polar conversion, convolution, spectral features, filter design, comb and
// allpass sections, STFT and overlap-add are compositions of vector
// operations and live in signals.mu.

#pragma once
#include "core.h"

namespace musil {

inline size_t next_pow2(size_t n) { size_t p = 1; while (p < n) p <<= 1; return p; }

// In-place iterative radix-2 FFT on interleaved (re, im) pairs; n a power of two; sign -1 forward, +1 inverse (unscaled).
inline void fft_inplace(double* d, size_t n, int sign) {
    for (size_t i = 1, j = 0; i < n; i++) {                       // bit reversal
        size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) { std::swap(d[2 * i], d[2 * j]); std::swap(d[2 * i + 1], d[2 * j + 1]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = sign * 2 * 3.14159265358979323846 / (double)len, wr = std::cos(ang), wi = std::sin(ang);
        for (size_t i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (size_t j = 0; j < len / 2; j++) {
                size_t p = 2 * (i + j), q = 2 * (i + j + len / 2);
                double ur = d[p], ui = d[p + 1], vr = d[q] * cr - d[q + 1] * ci, vi = d[q] * ci + d[q + 1] * cr;
                d[p] = ur + vr; d[p + 1] = ui + vi; d[q] = ur - vr; d[q + 1] = ui - vi;
                double t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}

// (fft signal) => (list re im), each of length next-pow2(length); the signal is zero-padded
inline vptr sig_fft(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); size_t n = next_pow2(std::max<size_t>(2, x.size()));
    std::vector<double> buf(2 * n, 0.0);
    for (size_t k = 0; k < x.size(); k++) buf[2 * k] = x[k];
    fft_inplace(buf.data(), n, -1);
    varr re(n), im(n);
    for (size_t k = 0; k < n; k++) { re[k] = buf[2 * k]; im[k] = buf[2 * k + 1]; }
    return v_list({ v_arr(std::move(re)), v_arr(std::move(im)) });
}
// (ifft (list re im)) => real signal of the same length (a power of two)
inline vptr sig_ifft(vlist& a, Interp& i) {
    vlist& s = i.list(a[0]);
    if (s.size() != 2) i.bad("a spectrum is (list re im)");
    const varr& re = i.num(s[0]); const varr& im = i.num(s[1]);
    size_t n = re.size();
    if (im.size() != n) i.bad("re and im must have the same length");
    if (n < 2 || (n & (n - 1))) i.bad("spectrum length must be a power of two (as fft produces)");
    std::vector<double> buf(2 * n);
    for (size_t k = 0; k < n; k++) { buf[2 * k] = re[k]; buf[2 * k + 1] = im[k]; }
    fft_inplace(buf.data(), n, 1);
    varr out(n); for (size_t k = 0; k < n; k++) out[k] = buf[2 * k] / (double)n;
    return v_arr(std::move(out));
}

// (osc sr freqs table) => one sample per element of freqs, reading the wavetable with linear
// interpolation. The table's last element must equal its first (a guard point; gen makes one).
inline vptr sig_osc(vlist& a, Interp& i) {
    double sr = i.scalar(a[0]); const varr& f = i.num(a[1]); const varr& t = i.num(a[2]);
    if (t.size() < 2) i.bad("table must have at least 2 elements");
    size_t n = t.size() - 1; double fn = sr / (double)n;
    varr out(f.size()); double phi = 0;
    for (size_t k = 0; k < f.size(); k++) {
        size_t ip = (size_t)phi; double fp = phi - (double)ip;
        out[k] = (1 - fp) * t[ip] + fp * t[ip + 1];
        phi += f[k] / fn;
        while (phi >= (double)n) phi -= (double)n;
        while (phi < 0) phi += (double)n;
    }
    return v_arr(std::move(out));
}

// (iir x b a) => y with a0 y[n] = sum b[k] x[n-k] - sum a[k>0] y[n-k]; a[0] need not be 1
inline vptr sig_iir(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); const varr& b = i.num(a[1]); const varr& av = i.num(a[2]);
    if (av.size() == 0 || b.size() == 0) i.bad("b and a must not be empty");
    if (av[0] == 0) i.bad("a[0] must be nonzero");
    size_t n = x.size(), nb = b.size(), na = av.size(); varr y(0.0, n);
    for (size_t k = 0; k < n; k++) {
        double acc = 0;
        for (size_t j = 0; j < nb && j <= k; j++) acc += b[j] * x[k - j];
        for (size_t j = 1; j < na && j <= k; j++) acc -= av[j] * y[k - j];
        y[k] = acc / av[0];
    }
    return v_arr(std::move(y));
}

// (delay x d) => x delayed by d samples (fractional, linear interpolation), same length
inline vptr sig_delay(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); double d = i.scalar(a[1]);
    if (d < 0) i.bad("delay must be >= 0");
    size_t n = x.size(); varr y(0.0, n);
    for (size_t k = 0; k < n; k++) {
        double pos = (double)k - d; if (pos < 0) continue;
        size_t i0 = (size_t)pos; double fr = pos - (double)i0;
        y[k] = i0 + 1 < n ? (1 - fr) * x[i0] + fr * x[i0 + 1] : x[std::min(i0, n - 1)];
    }
    return v_arr(std::move(y));
}

// (resample x factor) => x with its length multiplied by factor, by windowed-sinc interpolation
// (any ratio; when downsampling the sinc is also the anti-aliasing lowpass)
inline vptr sig_resample(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); double factor = i.scalar(a[1]);
    if (factor <= 0) i.bad("factor must be > 0");
    size_t in = x.size(); if (in == 0) return v_arr(varr());
    size_t out_len = std::max<size_t>(1, (size_t)std::floor(in * factor + 0.5));
    const int half = 24; const double pi = 3.14159265358979323846;
    double fc = factor < 1 ? factor : 1.0;                 // cutoff relative to the input Nyquist
    varr out(0.0, out_len);
    for (size_t k = 0; k < out_len; k++) {
        double t = (double)k / factor; long c = (long)std::floor(t);
        double acc = 0, wsum = 0;
        for (long j = c - half + 1; j <= c + half; j++) {
            double d = t - (double)j, win = 0.5 + 0.5 * std::cos(pi * d / half);   // Hann-windowed sinc
            double s = d == 0 ? fc : std::sin(pi * fc * d) / (pi * d);
            double wgt = s * win; wsum += wgt;
            if (j >= 0 && (size_t)j < in) acc += x[j] * wgt;
        }
        out[k] = wsum != 0 ? acc / wsum : 0;         // weights normalised to one: a constant stays a constant
    }
    return v_arr(std::move(out));
}

// (autocorr x) => biased autocorrelation (sum / n) for lags 0 .. n/2 - 1; it decays with lag, which
// makes the first periodicity peak the largest
inline vptr sig_autocorr(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); size_t n = x.size();
    if (n < 4) i.bad("signal too short");
    size_t h = n / 2; varr r(0.0, h);
    for (size_t lag = 0; lag < h; lag++) { double s = 0; for (size_t k = 0; k + lag < n; k++) s += x[k] * x[k + lag]; r[lag] = s / (double)n; }
    return v_arr(std::move(r));
}

// (interleave (list a b ...)) => one vector a0 b0 a1 b1 ...
inline vptr sig_interleave(vlist& a, Interp& i) {
    vlist& ch = i.list(a[0]); if (ch.empty()) i.bad("no channels");
    size_t n = i.num(ch[0]).size(), c = ch.size();
    for (auto& v : ch) if (i.num(v).size() != n) i.bad("all channels must have the same length");
    varr out(n * c);
    for (size_t k = 0; k < n; k++) for (size_t j = 0; j < c; j++) out[k * c + j] = ch[j]->num[k];
    return v_arr(std::move(out));
}
// (deinterleave v channels) => (list a b ...): the channels of an interleaved vector
inline vptr sig_deinterleave(vlist& a, Interp& i) {
    const varr& v = i.num(a[0]); long c = i.index(a[1]);
    if (c < 1) i.bad("channels must be >= 1");
    if (v.size() % (size_t)c) i.bad("length is not a multiple of the channel count");
    size_t n = v.size() / (size_t)c; vlist out;
    for (long j = 0; j < c; j++) { varr ch(n); for (size_t k = 0; k < n; k++) ch[k] = v[k * c + j]; out.push_back(v_arr(std::move(ch))); }
    return v_list(std::move(out));
}

// (local-maxima v) => indices k with v[k] > v[k-1] and v[k] > v[k+1] (interior points only)
inline vptr sig_local_maxima(vlist& a, Interp& i) {
    const varr& v = i.num(a[0]); std::vector<double> out;
    for (size_t k = 1; k + 1 < v.size(); k++) if (v[k] > v[k - 1] && v[k] > v[k + 1]) out.push_back((double)k);
    return from_vec(out);
}
// (gather v indices) => the elements of v at the given (integer) indices, as a vector
inline vptr sig_gather(vlist& a, Interp& i) {
    const varr& v = i.num(a[0]); const varr& idx = i.num(a[1]); varr out(idx.size());
    for (size_t k = 0; k < idx.size(); k++) {
        long j = (long)idx[k]; if (j < 0) j += (long)v.size();
        if (j < 0 || (size_t)j >= v.size()) i.bad("index " + std::to_string((long)idx[k]) + " out of range (size " + std::to_string(v.size()) + ")");
        out[k] = v[j];
    }
    return v_arr(std::move(out));
}

inline void add_signals(Interp& i) {
    i.def("fft", sig_fft, 1, 1); i.def("ifft", sig_ifft, 1, 1);
    i.def("osc", sig_osc, 3, 3); i.def("iir", sig_iir, 3, 3); i.def("delay", sig_delay, 2, 2);
    i.def("resample", sig_resample, 2, 2); i.def("autocorr", sig_autocorr, 1, 1);
    i.def("interleave", sig_interleave, 1, 1); i.def("deinterleave", sig_deinterleave, 2, 2);
    i.def("local-maxima", sig_local_maxima, 1, 1); i.def("gather", sig_gather, 2, 2);
}

} // namespace musil
