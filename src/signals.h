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
// autocorrelation, channel (de)interleaving, peak picking, gather, the in-place
// overlap-add, and the comb and allpass delay lines. Windows, wavetables,
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
// --- stateful loops: each is written once, with its state as an argument, so the offline builtin
// (fresh state, the whole vector) and the streaming node in live.h (state kept between blocks)
// run the same code and produce the same samples ---
struct osc_state { double phase = 0; };
inline void osc_run(const double* freq, double* out, size_t n, const double* table, size_t table_len, double sr, osc_state& st) {
    size_t len = table_len - 1; double fn = sr / (double)len;
    for (size_t k = 0; k < n; k++) {
        size_t ip = (size_t)st.phase; double fp = st.phase - (double)ip;
        out[k] = (1 - fp) * table[ip] + fp * table[ip + 1];
        st.phase += freq[k] / fn;
        while (st.phase >= (double)len) st.phase -= (double)len;
        while (st.phase < 0) st.phase += (double)len;
    }
}
inline vptr sig_osc(vlist& a, Interp& i) {
    double sr = i.scalar(a[0]); const varr& f = i.num(a[1]); const varr& t = i.num(a[2]);
    if (t.size() < 2) i.bad("table must have at least 2 elements");
    varr out(f.size()); osc_state st;
    if (f.size()) osc_run(&f[0], &out[0], f.size(), &t[0], t.size(), sr, st);
    return v_arr(std::move(out));
}

// (iir x b a) => y with a0 y[n] = sum b[k] x[n-k] - sum a[k>0] y[n-k]; a[0] need not be 1
struct iir_state { std::vector<double> xs, ys; };   // the last nb inputs and na outputs, newest first
inline void iir_run(const double* x, double* y, size_t n, const double* b, size_t nb, const double* a, size_t na, iir_state& st) {
    if (st.xs.size() != nb) st.xs.assign(nb, 0.0);
    if (st.ys.size() != na) st.ys.assign(na, 0.0);
    for (size_t k = 0; k < n; k++) {
        for (size_t j = nb - 1; j > 0; j--) st.xs[j] = st.xs[j - 1];
        st.xs[0] = x[k];
        double acc = 0;
        for (size_t j = 0; j < nb; j++) acc += b[j] * st.xs[j];
        for (size_t j = 1; j < na; j++) acc -= a[j] * st.ys[j];
        double out = acc / a[0];
        for (size_t j = na - 1; j > 0; j--) st.ys[j] = st.ys[j - 1];
        st.ys[1 < na ? 1 : 0] = out; st.ys[0] = out;
        y[k] = out;
    }
}
inline vptr sig_iir(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); const varr& b = i.num(a[1]); const varr& av = i.num(a[2]);
    if (av.size() == 0 || b.size() == 0) i.bad("b and a must not be empty");
    if (av[0] == 0) i.bad("a[0] must be nonzero");
    varr y(0.0, x.size()); iir_state st;
    if (x.size()) iir_run(&x[0], &y[0], x.size(), &b[0], b.size(), &av[0], av.size(), st);
    return v_arr(std::move(y));
}

// (delay x d) => x delayed by d samples (fractional, linear interpolation), same length
struct delay_state { std::vector<double> ring; size_t w = 0; };   // a ring of past inputs; w is the write index
inline void delay_run(const double* x, double* y, size_t n, const double* d, delay_state& st, size_t max_delay) {
    size_t cap = max_delay + 2;
    if (st.ring.size() != cap) { st.ring.assign(cap, 0.0); st.w = 0; }
    for (size_t k = 0; k < n; k++) {
        st.ring[st.w] = x[k];
        double dd = d[k] < 0 ? 0 : (d[k] > (double)max_delay ? (double)max_delay : d[k]);
        double pos = (double)st.w - dd; while (pos < 0) pos += cap;
        size_t i0 = (size_t)pos, i1 = (i0 + 1) % cap; double fr = pos - (double)i0;
        y[k] = (1 - fr) * st.ring[i0] + fr * st.ring[i1 % cap];
        st.w = (st.w + 1) % cap;
    }
}
inline vptr sig_delay(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); double d = i.scalar(a[1]);
    if (d < 0) i.bad("delay must be >= 0");
    size_t n = x.size(); varr y(0.0, n); delay_state st; varr dd(d, n);
    if (n) delay_run(&x[0], &y[0], n, &dd[0], st, (size_t)std::ceil(d) + 1);
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

// (add-at! dst pos src) => dst, with src added in place starting at pos; dst must be long enough.
// The overlap-add of an STFT is O(total) with this and O(frames x total) with copies.
inline vptr sig_add_at_inplace(vlist& a, Interp& i) {
    varr& dst = a[0]->t == Value::NUM ? a[0]->num : (i.num(a[0]), a[0]->num);
    long pos = i.index(a[1]); const varr& src = i.num(a[2]);
    if (pos < 0 || (size_t)pos + src.size() > dst.size()) i.bad("does not fit: " + std::to_string(src.size()) + " samples at " + std::to_string(pos) + " into " + std::to_string(dst.size()));
    for (size_t k = 0; k < src.size(); k++) dst[pos + k] += src[k];
    return a[0];
}
// (comb x d g) => feedback comb y[n] = x[n] + g y[n-d]; (allpass x d g) => Schroeder allpass
// y[n] = -g x[n] + x[n-d] + g y[n-d]. Both are a delay line, not a dense IIR: O(n) whatever d is.
struct comb_state { std::vector<double> ring; size_t w = 0; };
// y[n] = x[n] + g y[n-d]
inline void comb_run(const double* x, double* y, size_t n, size_t d, double g, comb_state& st) {
    if (st.ring.size() != d) { st.ring.assign(d, 0.0); st.w = 0; }
    for (size_t k = 0; k < n; k++) { double out = x[k] + g * st.ring[st.w]; st.ring[st.w] = out; st.w = (st.w + 1) % d; y[k] = out; }
}
struct allpass_state { std::vector<double> xin, yout; size_t w = 0; };
// y[n] = -g x[n] + x[n-d] + g y[n-d]
inline void allpass_run(const double* x, double* y, size_t n, size_t d, double g, allpass_state& st) {
    if (st.xin.size() != d) { st.xin.assign(d, 0.0); st.yout.assign(d, 0.0); st.w = 0; }
    for (size_t k = 0; k < n; k++) { double out = -g * x[k] + st.xin[st.w] + g * st.yout[st.w]; st.xin[st.w] = x[k]; st.yout[st.w] = out; st.w = (st.w + 1) % d; y[k] = out; }
}
inline vptr sig_comb(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); long d = i.index(a[1]); double g = i.scalar(a[2]);
    if (d < 1) i.bad("delay must be >= 1");
    varr y(0.0, x.size()); comb_state st;
    if (x.size()) comb_run(&x[0], &y[0], x.size(), (size_t)d, g, st);
    return v_arr(std::move(y));
}
inline vptr sig_allpass(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); long d = i.index(a[1]); double g = i.scalar(a[2]);
    if (d < 1) i.bad("delay must be >= 1");
    varr y(0.0, x.size()); allpass_state st;
    if (x.size()) allpass_run(&x[0], &y[0], x.size(), (size_t)d, g, st);
    return v_arr(std::move(y));
}

// --- the phase vocoder engine ---------------------------------------------------------
// (pvoc x opts) after sparkle: fixed synthesis hop I = window/overlap, analysis hop D = I/stretch
// (so the synthesis overlap never thins out), zero-phase windowing in a zero-padded frame,
// peak-locked phases (Laroche-Dolson), pitch shift and formant move by spectral resampling,
// formant preservation through the cepstral (true) envelope, denoising, three cross-synthesis
// modes and two phase effects. Parameters may ramp: a value given as (list start end).
// It is C++ because a frame is sixty vector operations on 4096 bins, ten thousand times a
// minute: the interpreter's per-operation cost dominated (25x slower than this loop).
struct pvoc_opts { double stretch0 = 1, stretch1 = 1, pitch0 = 1, pitch1 = 1, form0 = 1, form1 = 1, thr0 = 0, thr1 = 0, xa0 = 0, xa1 = 0;
                   int window = 2048, overlap = 8, pad = 1, env = 0, xmode = 0; std::string phase; const varr* other = nullptr; };
inline void pvoc_ramp(Interp& i, const vptr& v, double& a, double& b) {
    if (v->t == Value::LIST) { if (v->l.size() != 2) i.bad("a ramp is (list start end)"); a = i.scalar(v->l[0]); b = i.scalar(v->l[1]); }
    else a = b = i.scalar(v);
}
// cepstral smoothing of a full N-bin log spectrum: keep the first `order` cepstral coefficients
// (one pass, two FFTs, as sparkle's cepstralEnvelope; the iterated "true envelope" of signals.mu is
// better for analysis but twenty times the work, too much for ten thousand frames)
inline void pvoc_lifter(std::vector<double>& logspec, int order, std::vector<double>& buf) {
    size_t n = logspec.size();
    buf.assign(2 * n, 0.0); for (size_t k = 0; k < n; k++) buf[2 * k] = logspec[k];
    fft_inplace(buf.data(), n, 1);
    for (size_t k = 0; k < n; k++) { bool keep = k <= (size_t)order || k >= n - (size_t)order; buf[2 * k] = keep ? buf[2 * k] / n : 0; buf[2 * k + 1] = 0; }
    fft_inplace(buf.data(), n, -1);
    for (size_t k = 0; k < n; k++) logspec[k] = buf[2 * k];
}
inline vptr sig_pvoc(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); vlist& opts = i.list(a[1]);
    pvoc_opts o;
    for (auto& e : opts) {
        vlist& kv = i.list(e); if (kv.size() != 2 || kv[0]->t != Value::STR) i.bad("an option is (list \"key\" value)");
        const std::string& k = kv[0]->s; const vptr& v = kv[1];
        if (k == "stretch") pvoc_ramp(i, v, o.stretch0, o.stretch1);
        else if (k == "pitch") pvoc_ramp(i, v, o.pitch0, o.pitch1);
        else if (k == "formants") pvoc_ramp(i, v, o.form0, o.form1);
        else if (k == "threshold") pvoc_ramp(i, v, o.thr0, o.thr1);
        else if (k == "window") o.window = (int)i.scalar(v);
        else if (k == "overlap") o.overlap = (int)i.scalar(v);
        else if (k == "pad") o.pad = (int)i.scalar(v);
        else if (k == "envelope") o.env = (int)i.scalar(v);
        else if (k == "phase") o.phase = i.str(v);
        else if (k == "cross") {
            vlist& c = i.list(v); if (c.size() != 3 && c.size() != 4) i.bad("cross is (list mode amount other) or (list mode start end other)");
            o.xmode = (int)i.scalar(c[0]); o.xa0 = i.scalar(c[1]); o.xa1 = c.size() == 4 ? i.scalar(c[2]) : o.xa0; o.other = &i.num(c.back());
            if (o.xmode < 1 || o.xmode > 3) i.bad("cross mode is 1, 2 or 3");
        } else i.bad("unknown option " + k);
    }
    if (o.window < 8 || o.overlap < 1 || o.pad < 0 || o.env < 0 || o.stretch0 <= 0 || o.stretch1 <= 0 || o.pitch0 <= 0 || o.pitch1 <= 0 || o.form0 <= 0 || o.form1 <= 0) i.bad("invalid parameters");
    if (o.xmode == 2 && o.env == 0) i.bad("cross mode 2 needs an envelope order");
    size_t tot = x.size(); if (tot == 0) return v_arr(varr());
    size_t Wn = (size_t)o.window, N = next_pow2(Wn) << o.pad, N2 = N / 2;
    size_t I = std::max<size_t>(1, Wn / (size_t)o.overlap);
    double D0 = (double)I / o.stretch0, D1 = (double)I / o.stretch1;
    size_t frames = (size_t)std::ceil((double)tot / ((D0 + D1) / 2)), offset = (N - Wn) / 2;
    std::vector<double> w(Wn); double gain = 0;
    for (size_t k = 0; k < Wn; k++) { w[k] = 0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * k / Wn); gain += w[k] * w[k]; }
    gain /= I;
    double peak = 0; for (double v : x) peak = std::max(peak, std::fabs(v));
    double wsum = 0; for (double v : w) wsum += v;
    double peak_mag = peak * wsum / 2;          // the magnitude a full-scale sine at the signal's peak would have: the threshold's unit
    std::vector<double> out(I * frames + N, 0.0), buf(2 * N), buf2(2 * N), mags(N2), phase(N2), mags2(N2), phase2(N2), psi(N2, 0.0),
                        oldan(N2, 0.0), oldsyn(N2, 0.0), inc(N2), env1, env2, tmp, newm(N2), newp(N2);
    std::vector<size_t> peaks; peaks.reserve(N2); std::vector<size_t> owner(N2);
    const double tau = 2 * 3.14159265358979323846;
    auto analyse = [&](const varr& sig, double read, bool wrap, std::vector<double>& m, std::vector<double>& ph) {
        std::fill(buf.begin(), buf.end(), 0.0);
        long p = (long)std::floor(read); if (wrap && sig.size()) p %= (long)sig.size();
        for (size_t k = 0; k < Wn; k++) { long idx = p + (long)k; double v = (idx >= 0 && (size_t)idx < sig.size()) ? sig[idx] : 0.0; buf[2 * ((offset + k + N2) % N)] = v * w[k]; }   // centred, then fftshift
        fft_inplace(buf.data(), N, -1);
        for (size_t k = 0; k < N2; k++) { double re = buf[2 * k], im = buf[2 * k + 1]; m[k] = std::sqrt(re * re + im * im); ph[k] = std::atan2(im, re); }
    };
    // smoothed log envelope (half spectrum in, half out), on the symmetric full spectrum
    // The envelope must ride the harmonic peaks, not average peaks and valleys: the log magnitudes
    // are first replaced by their peak-to-peak interpolation (a one-pass stand-in for the iterated
    // "true envelope"), then smoothed once by the lifter.
    auto log_envelope = [&](const std::vector<double>& half, std::vector<double>& env) {
        double mx = 1e-12; for (double v : half) mx = std::max(mx, v);
        double fl = 1e-4 * mx;
        std::vector<double>& lg = env; lg.resize(N2); for (size_t k = 0; k < N2; k++) lg[k] = std::log(std::max(half[k], fl));
        size_t last = 0; bool have = false;
        for (size_t k = 1; k + 1 < N2; k++) if (lg[k] > lg[k - 1] && lg[k] > lg[k + 1]) {
            if (!have) { for (size_t j = 0; j < k; j++) lg[j] = lg[k]; have = true; }
            else for (size_t j = last + 1; j < k; j++) lg[j] = lg[last] + (lg[k] - lg[last]) * (double)(j - last) / (k - last);
            last = k;
        }
        if (have) for (size_t j = last + 1; j < N2; j++) lg[j] = lg[last];
        tmp.assign(N, 0.0); for (size_t k = 0; k < N2; k++) { tmp[k] = lg[k]; if (k) tmp[N - k] = lg[k]; } tmp[N2] = lg[N2 - 1];
        pvoc_lifter(tmp, o.env, buf2); env.assign(tmp.begin(), tmp.begin() + N2);
    };
    auto princarg = [&](double v) { return v - tau * std::round(v / tau); };
    double read = 0;
    for (size_t f = 0; f < frames; f++) {
        double t = frames > 1 ? (double)f / (frames - 1) : 0;
        double D = D0 + (D1 - D0) * t, P = o.pitch0 + (o.pitch1 - o.pitch0) * t, K = o.form0 + (o.form1 - o.form0) * t;
        double T = o.thr0 + (o.thr1 - o.thr0) * t, A = o.xa0 + (o.xa1 - o.xa0) * t;
        analyse(x, read, false, mags, phase);
        if (T > 0) for (size_t k = 0; k < N2; k++) if (mags[k] < T * peak_mag) mags[k] = 0;
        if (o.xmode) {
            analyse(*o.other, read, true, mags2, phase2);
            if (o.xmode == 1) for (size_t k = 0; k < N2; k++) { mags[k] = std::sqrt(mags[k] * mags2[k]); phase[k] = (1 - A) * phase[k] + A * phase2[k]; }
            else if (o.xmode == 2) { log_envelope(mags, env1); log_envelope(mags2, env2); for (size_t k = 0; k < N2; k++) mags[k] *= (1 - A) + A * std::exp(env2[k] - env1[k]); }
            else for (size_t k = 0; k < N2; k++) { mags[k] += (mags2[k] - mags[k]) * A; if (A >= 0.5) phase[k] = phase2[k]; }
        }
        // phase sync
        for (size_t k = 0; k < N2; k++) { double omega = tau * k * D / N; inc[k] = P * (omega + princarg(phase[k] - oldan[k] - omega)) / D; }
        peaks.clear(); for (size_t k = 1; k + 1 < N2; k++) if (mags[k] > mags[k - 1] && mags[k] > mags[k + 1]) peaks.push_back(k);
        if (peaks.empty()) peaks.push_back(0);
        { size_t r = 0, next_start = peaks.size() > 1 ? (peaks[0] + peaks[1]) / 2 + 1 : N2;
          for (size_t k = 0; k < N2; k++) { while (k >= next_start && r + 1 < peaks.size()) { r++; next_start = r + 1 < peaks.size() ? (peaks[r] + peaks[r + 1]) / 2 + 1 : N2; } owner[k] = peaks[r]; } }
        for (size_t k = 0; k < N2; k++) { size_t pk = owner[k]; psi[k] = oldsyn[pk] + I * inc[pk] + (phase[k] - phase[pk]); }
        oldan = phase; oldsyn = psi;
        // formant preservation
        if (o.env > 0 && (P != 1 || K != 1)) {
            double r = K / P;
            // the envelope of the spectrum moved by K/P is the moved envelope: one smoothing per frame,
            // and the correction is the log difference between the two
            log_envelope(mags, env1);
            for (size_t k = 0; k < N2; k++) { double src = k / r; double e = src < N2 ? env1[(size_t)src] : env1[N2 - 1]; mags[k] *= std::exp(e - env1[k]); }
        }
        // pitch shift by spectral resampling
        if (P != 1) {
            for (size_t k = 0; k < N2; k++) { double src = k / P; bool keep = src < N2; size_t idx = keep ? (size_t)src : 0; newm[k] = keep ? mags[idx] : 0; newp[k] = keep ? psi[idx] : 0; }
            mags.swap(newm); psi.swap(newp);
        }
        if (o.phase == "robot") std::fill(psi.begin(), psi.end(), 0.0);
        else if (o.phase == "whisper") { std::uniform_real_distribution<double> d(0, tau); for (auto& p : psi) p = d(i.rng); }
        // synthesis
        std::fill(buf.begin(), buf.end(), 0.0);
        for (size_t k = 0; k < N2; k++) { double re = mags[k] * std::cos(psi[k]), im = mags[k] * std::sin(psi[k]); buf[2 * k] = re; buf[2 * k + 1] = im; if (k) { buf[2 * (N - k)] = re; buf[2 * (N - k) + 1] = -im; } }
        fft_inplace(buf.data(), N, 1);
        for (size_t k = 0; k < Wn; k++) out[f * I + k] += buf[2 * ((offset + k + N2) % N)] / N * w[k];   // unshift, centre, window, overlap-add
        read += D;
    }
    varr y(I * frames); for (size_t k = 0; k < y.size(); k++) y[k] = out[k] / gain;
    return v_arr(std::move(y));
}

// (adsr sr gate a d s r) => an envelope following a gate signal (1 = on, 0 = off): attack a and
// decay d seconds towards the sustain level s while the gate is on, release r seconds when it goes off;
// exponential segments, so it sounds even; the same code runs in the live engine's adsr node.
struct adsr_state { double level = 0; int stage = 0; };   // stages: 0 idle, 1 attack, 2 decay/sustain, 3 release
inline void adsr_run(const double* gate, double* out, size_t n, double a, double d, double s, double r, double sr, adsr_state& st) {
    auto coef = [&](double secs) { return secs <= 0 ? 0.0 : std::exp(-1.0 / (std::max(secs, 1e-4) * sr)); };
    double ca = coef(a), cd = coef(d), cr = coef(r);
    for (size_t k = 0; k < n; k++) {
        bool on = gate[k] > 0.5;
        if (on && (st.stage == 0 || st.stage == 3)) st.stage = 1;
        if (!on && (st.stage == 1 || st.stage == 2)) st.stage = 3;
        if (st.stage == 1) { st.level = 1.2 - (1.2 - st.level) * ca; if (st.level >= 1) { st.level = 1; st.stage = 2; } }
        else if (st.stage == 2) st.level = s + (st.level - s) * cd;
        else if (st.stage == 3) { st.level *= cr; if (st.level < 1e-5) { st.level = 0; st.stage = 0; } }
        out[k] = st.level;
    }
}
inline vptr sig_adsr(vlist& a, Interp& i) {
    double sr = i.scalar(a[0]); const varr& g = i.num(a[1]);
    double at = i.scalar(a[2]), de = i.scalar(a[3]), su = i.scalar(a[4]), re = i.scalar(a[5]);
    if (sr <= 0 || at < 0 || de < 0 || re < 0 || su < 0 || su > 1) i.bad("times must be >= 0 and sustain in [0, 1]");
    varr out(0.0, g.size()); adsr_state st;
    if (g.size()) adsr_run(&g[0], &out[0], g.size(), at, de, su, re, sr, st);
    return v_arr(std::move(out));
}
// (lag x sr seconds) => x smoothed by a one-pole lowpass with the given time constant (parameter
// smoothing, envelope following); the live engine's lag node
struct lag_state { double y = 0; };
inline void lag_run(const double* x, double* out, size_t n, double coef, lag_state& st) {
    for (size_t k = 0; k < n; k++) { st.y += (x[k] - st.y) * (1 - coef); out[k] = st.y; }
}
inline vptr sig_lag(vlist& a, Interp& i) {
    const varr& x = i.num(a[0]); double sr = i.scalar(a[1]), secs = i.scalar(a[2]);
    if (sr <= 0 || secs < 0) i.bad("sr must be > 0 and seconds >= 0");
    double coef = secs <= 0 ? 0.0 : std::exp(-1.0 / (secs * sr));
    varr out(0.0, x.size()); lag_state st;
    if (x.size()) lag_run(&x[0], &out[0], x.size(), coef, st);
    return v_arr(std::move(out));
}

inline void add_signals(Interp& i) {
    i.def("fft", sig_fft, 1, 1); i.def("ifft", sig_ifft, 1, 1);
    i.def("osc", sig_osc, 3, 3); i.def("iir", sig_iir, 3, 3); i.def("delay", sig_delay, 2, 2);
    i.def("resample", sig_resample, 2, 2); i.def("autocorr", sig_autocorr, 1, 1);
    i.def("interleave", sig_interleave, 1, 1); i.def("deinterleave", sig_deinterleave, 2, 2);
    i.def("local-maxima", sig_local_maxima, 1, 1); i.def("gather", sig_gather, 2, 2);
    i.def("add-at!", sig_add_at_inplace, 3, 3); i.def("comb", sig_comb, 3, 3); i.def("allpass", sig_allpass, 3, 3);
    i.def("pvoc", sig_pvoc, 2, 2);
    i.def("adsr", sig_adsr, 6, 6); i.def("lag", sig_lag, 3, 3);
}

} // namespace musil
