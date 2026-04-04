#ifndef SIGNALS_H
#define SIGNALS_H

#include "core.h"
#include "signals/FFT.h"
#include "signals/features.h"

#include <vector>
#include <valarray>
#include <cmath>
#include <algorithm>

using Real = double;


// ── Local helpers ─────────────────────────────────────────────────────────────

static double sig_scalar(const Value& v, const std::string& fn) {
    if (!std::holds_alternative<NumVal>(v))
        throw Error{"signals", -1, fn + ": expected number"};
    return std::get<NumVal>(v)[0];
}

static const NumVal& sig_nvec(const Value& v, const std::string& fn) {
    if (!std::holds_alternative<NumVal>(v))
        throw Error{"signals", -1, fn + ": expected numeric vector"};
    return std::get<NumVal>(v);
}

// Packed pair helpers: represent two equal-length vectors as one NumVal
// [a0 a1 ... aN-1  b0 b1 ... bN-1]
static std::size_t packed_half_size(const NumVal& v, const std::string& fn) {
    if (v.size() % 2 != 0)
        throw Error{"signals", -1, fn + ": packed vector length must be even"};
    return v.size() / 2;
}

static NumVal pack_pair(const NumVal& a, const NumVal& b, const std::string& fn) {
    if (a.size() != b.size())
        throw Error{"signals", -1, fn + ": both vectors must have the same length"};
    std::valarray<Real> out(Real(0), a.size() + b.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = a[i];
    for (std::size_t i = 0; i < b.size(); ++i) out[a.size() + i] = b[i];
    return NumVal(out);
}

static NumVal packed_first(const NumVal& v, const std::string& fn) {
    std::size_t N = packed_half_size(v, fn);
    std::valarray<Real> out(Real(0), N);
    for (std::size_t i = 0; i < N; ++i) out[i] = v[i];
    return NumVal(out);
}

static NumVal packed_second(const NumVal& v, const std::string& fn) {
    std::size_t N = packed_half_size(v, fn);
    std::valarray<Real> out(Real(0), N);
    for (std::size_t i = 0; i < N; ++i) out[i] = v[N + i];
    return NumVal(out);
}

// Extract b/a from packed filtdesign coefficients [b0 b1 b2 1 a1 a2]
static void unpack_biquad_coeffs(const NumVal& coeffs, std::valarray<Real>& b, std::valarray<Real>& a,
                                 const std::string& fn) {
    if (coeffs.size() != 6)
        throw Error{"signals", -1, fn + ": packed coeffs must have length 6 [b0 b1 b2 1 a1 a2]"};
    b.resize(3);
    a.resize(3);
    b[0] = coeffs[0];
    b[1] = coeffs[1];
    b[2] = coeffs[2];
    a[0] = coeffs[3];
    a[1] = coeffs[4];
    a[2] = coeffs[5];
}

// ── C++ support functions (unchanged from original) ──────────────────────────

static void gen10(const std::valarray<Real>& coeff, std::valarray<Real>& values) {
    Real norm = 0;
    for (unsigned j = 0; j < coeff.size(); ++j) norm += coeff[j];
    if (norm == 0) norm = 1;
    for (unsigned i = 0; i < values.size() - 1; ++i) {
        Real acc = 0;
        for (unsigned j = 0; j < coeff.size(); ++j)
            acc += coeff[j] * sin(2.0 * M_PI * (j + 1) * (Real)i / values.size());
        values[i] = acc / norm;
    }
    values[values.size() - 1] = values[0];
}

static int next_pow2(int n) {
    if (n == 0 || ceil(log2(n)) == floor(log2(n))) return n;
    int count = 0;
    while (n != 0) {
        n >>= 1;
        count++;
    }
    return (1 << count);
}

static std::valarray<Real> conv_one_channel(const std::valarray<Real>& x, const std::valarray<Real>& y) {
    int x_sz = (int)x.size(), y_sz = (int)y.size();
    if (x_sz == 0 || y_sz == 0) return std::valarray<Real>();
    int conv_len = x_sz + y_sz - 1;
    int N = next_pow2(conv_len), len = 2 * N;
    std::valarray<Real> X(Real(0), len), Y(Real(0), len), R(Real(0), len);
    for (int i = 0; i < N; ++i) {
        X[2*i] = (i < x_sz ? x[i] : Real(0));
        Y[2*i] = (i < y_sz ? y[i] : Real(0));
    }
    fft<Real>(&X[0], N, -1);
    fft<Real>(&Y[0], N, -1);
    for (int i = 0; i < N; ++i) {
        Real xr=X[2*i], xi=X[2*i+1], yr=Y[2*i], yi=Y[2*i+1];
        R[2*i]   = xr*yr - xi*yi;
        R[2*i+1] = xr*yi + xi*yr;
    }
    fft<Real>(&R[0], N, 1);
    std::valarray<Real> out(Real(0), conv_len);
    for (int s = 0; s < conv_len; ++s) out[s] = R[2*s] / N;
    return out;
}

static std::valarray<Real> fd_resample(const std::valarray<Real>& x, Real factor) {
    int in_len = (int)x.size();
    if (in_len == 0 || factor <= 0) return std::valarray<Real>();
    int out_len = std::max(1, (int)std::floor((Real)in_len * factor + 0.5));
    int N1 = next_pow2(in_len), N2 = next_pow2(out_len);
    std::valarray<Real> X(Real(0), 2*N1);
    for (int i = 0; i < N1; ++i) X[2*i] = (i < in_len ? x[i] : Real(0));
    fft<Real>(&X[0], N1, -1);
    std::valarray<Real> Y(Real(0), 2*N2);
    int N1h = N1/2, N2h = N2/2, Nc = std::min(N1h, N2h);
    Y[0] = X[0];
    Y[1] = X[1];
    for (int k = 1; k < Nc; ++k) {
        Y[2*k]   = X[2*k];
        Y[2*k+1]   = X[2*k+1];
        Y[2*(N2-k)]   = X[2*(N1-k)];
        Y[2*(N2-k)+1] = X[2*(N1-k)+1];
    }
    if ((N1 % 2 == 0) && (N2 % 2 == 0)) {
        Y[2*N2h]   = X[2*N1h];
        Y[2*N2h+1] = X[2*N1h+1];
    }
    fft<Real>(&Y[0], N2, +1);
    std::valarray<Real> out(Real(0), out_len);
    for (int n = 0; n < out_len; ++n) out[n] = Y[2*n] / (Real)N2;
    return out;
}

// ── Synthesis ─────────────────────────────────────────────────────────────────
static Value fn_mix(std::vector<Value>& args, Interpreter& I) {
    if (args.size() % 2 != 0)
        throw Error{I.filename, I.cur_line(), "mix: arguments must be (pos, sig) pairs"};
    std::vector<Real> out;
    for (std::size_t i = 0; i < args.size() / 2; ++i) {
        sig_yield(I);
        int p = (int)sig_scalar(args[i*2], "mix");
        if (p < 0) throw Error{I.filename, I.cur_line(), "mix: negative position"};
        const NumVal& sig = sig_nvec(args[i*2+1], "mix");
        std::size_t needed = p + sig.size();
        if (needed > out.size()) out.resize(needed, 0.0);
        for (std::size_t t = 0; t < sig.size(); ++t) out[p + t] += sig[t];
    }
    return NumVal(out.data(), out.size());
}

static Value fn_gen(std::vector<Value>& args, Interpreter& I) {
    if (args.size() < 2)
        throw Error{I.filename, I.cur_line(), "gen: need len and at least one harmonic coefficient"};
    int len = (int)sig_scalar(args[0], "gen");
    if (len <= 0) throw Error{I.filename, I.cur_line(), "gen: length must be positive"};
    std::valarray<Real> coeffs(args.size() - 1);
    for (std::size_t i = 1; i < args.size(); ++i) coeffs[i-1] = sig_scalar(args[i], "gen");
    std::valarray<Real> table(Real(0), len + 1);
    gen10(coeffs, table);
    return NumVal(table);
}

static Value fn_osc(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 3)
        throw Error{I.filename, I.cur_line(), "osc: 3 args required (sr, freqs, table)"};
    Real sr = sig_scalar(args[0], "osc");
    const NumVal& freqs = sig_nvec(args[1], "osc");
    const NumVal& table = sig_nvec(args[2], "osc");
    int N = (int)table.size() - 1;
    if (N <= 0) throw Error{I.filename, I.cur_line(), "osc: table too short"};
    Real fn = (Real)sr / N;
    std::valarray<Real> out(Real(0), freqs.size());
    Real phi = 0;
    for (std::size_t i = 0; i < freqs.size(); ++i) {
        if ((i & 1023u) == 0) sig_yield(I);
        int ip = (int)phi;
        Real fp = phi - ip;
        out[i] = (1 - fp) * table[ip] + fp * table[ip + 1];
        phi += freqs[i] / fn;
        if (phi >= N) phi -= N;
    }
    return NumVal(out);
}

// ── Frequency domain ──────────────────────────────────────────────────────────
static Value fn_fft(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1)
        throw Error{I.filename, I.cur_line(), "fft: 1 argument required"};
    const NumVal& sig = sig_nvec(args[0], "fft");
    int d = (int)sig.size(), N = next_pow2(d);
    std::valarray<Real> buf(Real(0), 2 * N);
    for (int i = 0; i < N; ++i) {
        if ((i & 1023) == 0) sig_yield(I);
        buf[2*i] = (i < d ? sig[i] : Real(0));
        buf[2*i+1] = Real(0);
    }
    fft<Real>(&buf[0], N, -1);
    return NumVal(buf);
}

static Value fn_ifft(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1)
        throw Error{I.filename, I.cur_line(), "ifft: 1 argument required"};
    const NumVal& spec = sig_nvec(args[0], "ifft");
    int len = (int)spec.size();
    if (len % 2 != 0) throw Error{I.filename, I.cur_line(), "ifft: spectrum length must be even"};
    int N = len / 2;
    std::valarray<Real> buf(spec);
    fft<Real>(&buf[0], N, +1);
    std::valarray<Real> out(Real(0), N);
    for (int i = 0; i < N; ++i) {
        if ((i & 1023) == 0) sig_yield(I);
        out[i] = buf[2*i] / N;
    }
    return NumVal(out);
}

static Value fn_car2pol(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1)
        throw Error{I.filename, I.cur_line(), "car2pol: 1 argument required"};
    NumVal spec = std::get<NumVal>(args[0]);
    if (spec.size() % 2 != 0)
        throw Error{I.filename, I.cur_line(), "car2pol: spectrum length must be even"};
    rect2pol(&spec[0], (int)spec.size() / 2);
    return spec;
}

static Value fn_pol2car(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1)
        throw Error{I.filename, I.cur_line(), "pol2car: 1 argument required"};
    NumVal spec = std::get<NumVal>(args[0]);
    if (spec.size() % 2 != 0)
        throw Error{I.filename, I.cur_line(), "pol2car: spectrum length must be even"};
    pol2rect(&spec[0], (int)spec.size() / 2);
    return spec;
}

static Value fn_window(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 4)
        throw Error{I.filename, I.cur_line(), "window: 4 args required (N, a0, a1, a2)"};
    int N = (int)sig_scalar(args[0], "window");
    Real a0 = sig_scalar(args[1], "window");
    Real a1 = sig_scalar(args[2], "window");
    Real a2 = sig_scalar(args[3], "window");
    if (N <= 0) throw Error{I.filename, I.cur_line(), "window: N must be positive"};
    std::valarray<Real> win(N);
    make_window(&win[0], N, a0, a1, a2);
    return NumVal(win);
}

// Packed helpers replacing ArrayPtr traffic while keeping builtin names
// deinterleave(sig) -> packed NumVal [first_half, second_half]
static Value fn_deinterleave(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1)
        throw Error{I.filename, I.cur_line(), "deinterleave: 1 argument required"};
    const NumVal& in = sig_nvec(args[0], "deinterleave");
    int L = (int)in.size();
    if (L % 2 != 0)
        throw Error{I.filename, I.cur_line(), "deinterleave: signal length must be even"};
    int N = L / 2;
    std::valarray<Real> a(Real(0), N), b(Real(0), N);
    for (int i = 0; i < N; ++i) {
        if ((i & 1023) == 0) sig_yield(I);
        a[i] = in[2*i];
        b[i] = in[2*i+1];
    }
    return pack_pair(NumVal(a), NumVal(b), "deinterleave");
}

// interleave(packed) -> NumVal, where packed = [a..., b...]
static Value fn_interleave(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1)
        throw Error{I.filename, I.cur_line(), "interleave: 1 argument required (packed pair or legacy array)"};

    NumVal a, b;
    if (std::holds_alternative<NumVal>(args[0])) {
        const NumVal& packed = std::get<NumVal>(args[0]);
        a = packed_first(packed, "interleave");
        b = packed_second(packed, "interleave");
    } else if (std::holds_alternative<ArrayPtr>(args[0])) {
        const auto& lst = std::get<ArrayPtr>(args[0])->elems;
        if (lst.size() != 2)
            throw Error{I.filename, I.cur_line(), "interleave: legacy array must contain exactly 2 vectors"};
        a = std::get<NumVal>(lst[0]);
        b = std::get<NumVal>(lst[1]);
    } else {
        throw Error{I.filename, I.cur_line(), "interleave: argument must be packed vector or array of 2 vectors"};
    }

    if (a.size() != b.size())
        throw Error{I.filename, I.cur_line(), "interleave: both channels must have the same length"};
    int N = (int)a.size();
    std::valarray<Real> out(Real(0), 2 * N);
    for (int i = 0; i < N; ++i) {
        if ((i & 1023) == 0) sig_yield(I);
        out[2*i] = a[i];
        out[2*i+1] = b[i];
    }
    return NumVal(out);
}

// Accessors for packed pairs [a..., b...]
static Value fn_head(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1) throw Error{I.filename, I.cur_line(), "head: 1 packed argument required"};
    return packed_first(sig_nvec(args[0], "head"), "head");
}

static Value fn_tail(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1) throw Error{I.filename, I.cur_line(), "tail: 1 packed argument required"};
    return packed_second(sig_nvec(args[0], "tail"), "tail");
}

// Spectral features
static Value fn_speccent(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2) throw Error{I.filename, I.cur_line(), "speccent: 2 args required (amps, freqs)"};
    const NumVal& amps = sig_nvec(args[0], "speccent");
    const NumVal& freqs = sig_nvec(args[1], "speccent");
    if (amps.size() != freqs.size())
        throw Error{I.filename, I.cur_line(), "speccent: amps and freqs must have same length"};
    return NumVal{speccentr<Real>(&amps[0], &freqs[0], (int)amps.size())};
}
static Value fn_specspread(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 3) throw Error{I.filename, I.cur_line(), "specspread: 3 args required"};
    const NumVal& amps = sig_nvec(args[0], "specspread");
    const NumVal& freqs = sig_nvec(args[1], "specspread");
    Real centroid = sig_scalar(args[2], "specspread");
    if (amps.size() != freqs.size())
        throw Error{I.filename, I.cur_line(), "specspread: amps and freqs must have same length"};
    return NumVal{specspread<Real>(&amps[0], &freqs[0], centroid, (int)amps.size())};
}
static Value fn_specskew(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 4) throw Error{I.filename, I.cur_line(), "specskew: 4 args required"};
    const NumVal& amps = sig_nvec(args[0], "specskew");
    const NumVal& freqs = sig_nvec(args[1], "specskew");
    Real centroid = sig_scalar(args[2], "specskew");
    Real spread = sig_scalar(args[3], "specskew");
    if (amps.size() != freqs.size())
        throw Error{I.filename, I.cur_line(), "specskew: amps and freqs must have same length"};
    return NumVal{specskew<Real>(&amps[0], &freqs[0], centroid, spread, (int)amps.size())};
}
static Value fn_speckurt(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 4) throw Error{I.filename, I.cur_line(), "speckurt: 4 args required"};
    const NumVal& amps = sig_nvec(args[0], "speckurt");
    const NumVal& freqs = sig_nvec(args[1], "speckurt");
    Real centroid = sig_scalar(args[2], "speckurt");
    Real spread = sig_scalar(args[3], "speckurt");
    if (amps.size() != freqs.size())
        throw Error{I.filename, I.cur_line(), "speckurt: amps and freqs must have same length"};
    return NumVal{speckurt<Real>(&amps[0], &freqs[0], centroid, spread, (int)amps.size())};
}
static Value fn_specflux(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2) throw Error{I.filename, I.cur_line(), "specflux: 2 args required (amps, prev_amps)"};
    NumVal amps = std::get<NumVal>(args[0]);
    NumVal oamps = std::get<NumVal>(args[1]);
    if (amps.size() != oamps.size()) throw Error{I.filename, I.cur_line(), "specflux: amps and prev_amps must be same length"};
    return NumVal{specflux<Real>(&amps[0], &oamps[0], (int)amps.size())};
}
static Value fn_specirr(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1) throw Error{I.filename, I.cur_line(), "specirr: 1 argument required"};
    NumVal amps = std::get<NumVal>(args[0]);
    return NumVal{specirr<Real>(&amps[0], (int)amps.size())};
}
static Value fn_specdecr(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1) throw Error{I.filename, I.cur_line(), "specdecr: 1 argument required"};
    const NumVal& amps = sig_nvec(args[0], "specdecr");
    return NumVal{specdecr<Real>(&amps[0], (int)amps.size())};
}
static Value fn_acorrf0(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2) throw Error{I.filename, I.cur_line(), "acorrf0: 2 args required (sig, sr)"};
    const NumVal& sig = sig_nvec(args[0], "acorrf0");
    Real sr = sig_scalar(args[1], "acorrf0");
    if (sr <= 0) throw Error{I.filename, I.cur_line(), "acorrf0: sample rate must be positive"};
    if (sig.size() == 0) throw Error{I.filename, I.cur_line(), "acorrf0: empty signal"};
    std::valarray<Real> buff(sig.size());
    return NumVal{acfF0Estimate<Real>(sr, &sig[0], &buff[0], (int)sig.size())};
}
static Value fn_energy(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1) throw Error{I.filename, I.cur_line(), "energy: 1 argument required"};
    const NumVal& sig = sig_nvec(args[0], "energy");
    return NumVal{energy<Real>(&sig[0], (int)sig.size())};
}
static Value fn_zcr(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 1) throw Error{I.filename, I.cur_line(), "zcr: 1 argument required"};
    const NumVal& sig = sig_nvec(args[0], "zcr");
    return NumVal{zcr<Real>(&sig[0], (int)sig.size())};
}

// Convolution
static Value fn_conv(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2) throw Error{I.filename, I.cur_line(), "conv: 2 arguments required"};
    const NumVal& x = sig_nvec(args[0], "conv");
    const NumVal& y = sig_nvec(args[1], "conv");
    if (x.size() == 0 || y.size() == 0) throw Error{I.filename, I.cur_line(), "conv: empty signal"};
    return NumVal(conv_one_channel(std::valarray<Real>(x), std::valarray<Real>(y)));
}

// Leave multichannel API as Array-of-vectors for now (less hot-path than STFT internals)
static Value fn_convmc(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2) throw Error{I.filename, I.cur_line(), "convmc: 2 arguments required"};
    if (!std::holds_alternative<ArrayPtr>(args[0]) || !std::holds_alternative<ArrayPtr>(args[1]))
        throw Error{I.filename, I.cur_line(), "convmc: arguments must be arrays of vectors"};
    const auto& matx = std::get<ArrayPtr>(args[0])->elems;
    const auto& maty = std::get<ArrayPtr>(args[1])->elems;
    int nx = (int)matx.size(), ny = (int)maty.size();
    if (nx == 0 || ny == 0) throw Error{I.filename, I.cur_line(), "convmc: empty channel list"};
    int max_ch = std::max(nx, ny);
    auto out = std::make_shared<Array>();
    for (int i = 0; i < max_ch; ++i) {
        sig_yield(I);
        const NumVal& xch = sig_nvec(matx[i < nx ? i : nx-1], "convmc");
        const NumVal& ych = sig_nvec(maty[i < ny ? i : ny-1], "convmc");
        out->elems.push_back(NumVal(conv_one_channel(std::valarray<Real>(xch), std::valarray<Real>(ych))));
    }
    return out;
}

// Vector ops
static Value fn_vslice(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 3) throw Error{I.filename, I.cur_line(), "vslice: 3 args required (vec, start, n)"};
    const NumVal& v = sig_nvec(args[0], "vslice");
    int start = (int)sig_scalar(args[1], "vslice");
    int n = (int)sig_scalar(args[2], "vslice");
    if (start < 0 || n < 0 || start + n > (int)v.size())
        throw Error{I.filename, I.cur_line(), "vslice: range out of bounds"};
    return NumVal(v[std::slice(start, n, 1)]);
}

static Value fn_vaddat(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 3) throw Error{I.filename, I.cur_line(), "vaddat: 3 args required (dst, pos, src)"};
    const NumVal& dst = sig_nvec(args[0], "vaddat");
    int pos = (int)sig_scalar(args[1], "vaddat");
    const NumVal& src = sig_nvec(args[2], "vaddat");
    if (pos < 0) throw Error{I.filename, I.cur_line(), "vaddat: position must be >= 0"};
    std::size_t need = (std::size_t)pos + src.size();
    std::size_t out_sz = std::max(dst.size(), need);
    std::valarray<Real> out(Real(0), out_sz);
    for (std::size_t i = 0; i < dst.size(); ++i) {
        if ((i & 2047u) == 0) sig_yield(I);
        out[i] = dst[i];
    }
    for (std::size_t i = 0; i < src.size(); ++i) {
        if ((i & 2047u) == 0) sig_yield(I);
        out[(std::size_t)pos + i] += src[i];
    }
    return NumVal(out);
}

// Filters
static Value fn_dcblock(std::vector<Value>& args, Interpreter& I) {
    if (args.size() < 1 || args.size() > 2) throw Error{I.filename, I.cur_line(), "dcblock: 1 or 2 args required (sig [, R])"};
    const NumVal& x = sig_nvec(args[0], "dcblock");
    Real R = args.size() == 2 ? sig_scalar(args[1], "dcblock") : Real(0.995);
    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);
    if (N == 0) return NumVal(y);
    y[0] = x[0];
    for (int n = 1; n < N; ++n) {
        if ((n & 1023) == 0) sig_yield(I);
        y[n] = x[n] - x[n-1] + R * y[n-1];
    }
    return NumVal(y);
}

static Value fn_reson(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 4) throw Error{I.filename, I.cur_line(), "reson: 4 args required (sig, sr, freq, tau)"};
    const NumVal& x = sig_nvec(args[0], "reson");
    Real sr = sig_scalar(args[1], "reson");
    Real freq = sig_scalar(args[2], "reson");
    Real tau = sig_scalar(args[3], "reson");
    Real om = 2 * M_PI * (freq / sr);
    Real B = 1.0 / tau, t = 1.0 / sr;
    Real radius = exp(-2.0 * M_PI * B * t);
    Real a1 = -2 * radius * cos(om);
    Real a2 = radius * radius;
    Real gain = radius * sin(om);
    int samps = (int)(sr * tau), insize = (int)x.size();
    std::valarray<Real> out(Real(0), samps);
    Real y1 = 0, y2 = 0;
    for (int i = 0; i < samps; ++i) {
        if ((i & 1023) == 0) sig_yield(I);
        Real xi = i < insize ? x[i] : Real(0);
        Real v = gain * xi - a1 * y1 - a2 * y2;
        y2 = y1;
        y1 = v;
        out[i] = v;
    }
    return NumVal(out);
}

// filter(x, coeffs) or filter(x, b, a)
static Value fn_filter(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2 && args.size() != 3)
        throw Error{I.filename, I.cur_line(), "filter: 2 or 3 args required (sig, coeffs) or (sig, b, a)"};
    const NumVal& x = sig_nvec(args[0], "filter");
    std::valarray<Real> b, a;
    if (args.size() == 2) {
        const NumVal& coeffs = sig_nvec(args[1], "filter");
        unpack_biquad_coeffs(coeffs, b, a, "filter");
    } else {
        b = std::valarray<Real>(sig_nvec(args[1], "filter"));
        a = std::valarray<Real>(sig_nvec(args[2], "filter"));
    }
    if (b.size() == 0) throw Error{I.filename, I.cur_line(), "filter: b must be non-empty"};
    if (a.size() == 0) throw Error{I.filename, I.cur_line(), "filter: a must be non-empty"};
    Real a0 = a[0];
    if (a0 == Real(0)) throw Error{I.filename, I.cur_line(), "filter: a[0] cannot be zero"};
    std::valarray<Real> bn = b / a0, an = a / a0;
    int N = (int)x.size(), Nb = (int)bn.size(), Na = (int)an.size();
    std::valarray<Real> y(Real(0), N);
    for (int n = 0; n < N; ++n) {
        if ((n & 511) == 0) sig_yield(I);
        Real acc = 0;
        for (int k = 0; k < Nb; ++k) if (n-k >= 0) acc += bn[k] * x[n-k];
        for (int k = 1; k < Na; ++k) if (n-k >= 0) acc -= an[k] * y[n-k];
        y[n] = acc;
    }
    return NumVal(y);
}

// filtdesign(type, Fs, f0, Q, gain_db) -> packed NumVal [b0 b1 b2 1 a1 a2]
static Value fn_filtdesign(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 5) throw Error{I.filename, I.cur_line(), "filtdesign: 5 args (type, Fs, f0, Q, gain_db)"};
    if (!std::holds_alternative<std::string>(args[0])) throw Error{I.filename, I.cur_line(), "filtdesign: type must be a string"};
    const std::string& type = std::get<std::string>(args[0]);
    Real Fs = sig_scalar(args[1], "filtdesign");
    Real f0 = sig_scalar(args[2], "filtdesign");
    Real Q = sig_scalar(args[3], "filtdesign");
    Real dBg = sig_scalar(args[4], "filtdesign");
    if (Fs <= 0 || f0 <= 0 || f0 >= Fs/2) throw Error{I.filename, I.cur_line(), "filtdesign: invalid Fs or f0"};
    if (Q <= 0) throw Error{I.filename, I.cur_line(), "filtdesign: Q must be > 0"};
    Real w0 = Real(2.0 * M_PI * f0 / Fs);
    Real cosw = cos(w0), sinw = sin(w0);
    Real alpha = sinw / (2 * Q);
    Real A = pow(Real(10.0), dBg / Real(40.0));
    Real b0=0, b1=0, b2=0, a0=0, a1=0, a2=0;
    if (type == "lowpass") {
        b0=(1-cosw)/2;
        b1=1-cosw;
        b2=(1-cosw)/2;
        a0=1+alpha;
        a1=-2*cosw;
        a2=1-alpha;
    } else if (type == "highpass") {
        b0=(1+cosw)/2;
        b1=-(1+cosw);
        b2=(1+cosw)/2;
        a0=1+alpha;
        a1=-2*cosw;
        a2=1-alpha;
    } else if (type == "notch") {
        b0=1;
        b1=-2*cosw;
        b2=1;
        a0=1+alpha;
        a1=-2*cosw;
        a2=1-alpha;
    } else if (type == "peak" || type == "peaking") {
        b0=1+alpha*A;
        b1=-2*cosw;
        b2=1-alpha*A;
        a0=1+alpha/A;
        a1=-2*cosw;
        a2=1-alpha/A;
    } else if (type == "lowshelf" || type == "loshelf") {
        Real sA = sqrt(A);
        b0=A*((A+1)-(A-1)*cosw+2*sA*alpha);
        b1=2*A*((A-1)-(A+1)*cosw);
        b2=A*((A+1)-(A-1)*cosw-2*sA*alpha);
        a0=(A+1)+(A-1)*cosw+2*sA*alpha;
        a1=-2*((A-1)+(A+1)*cosw);
        a2=(A+1)+(A-1)*cosw-2*sA*alpha;
    } else if (type == "highshelf" || type == "hishelf") {
        Real sA = sqrt(A);
        b0=A*((A+1)+(A-1)*cosw+2*sA*alpha);
        b1=-2*A*((A-1)+(A+1)*cosw);
        b2=A*((A+1)+(A-1)*cosw-2*sA*alpha);
        a0=(A+1)-(A-1)*cosw+2*sA*alpha;
        a1=2*((A-1)-(A+1)*cosw);
        a2=(A+1)-(A-1)*cosw-2*sA*alpha;
    } else {
        throw Error{I.filename, I.cur_line(), "filtdesign: unknown filter type"};
    }
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    a0 = 1;
    std::valarray<Real> out(Real(0), 6);
    out[0]=b0;
    out[1]=b1;
    out[2]=b2;
    out[3]=1.0;
    out[4]=a1;
    out[5]=a2;
    return NumVal(out);
}

static Value fn_delay(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2) throw Error{I.filename, I.cur_line(), "delay: 2 args required (sig, D)"};
    const NumVal& x = sig_nvec(args[0], "delay");
    Real D = sig_scalar(args[1], "delay");
    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);
    if (N == 0) return NumVal(y);
    for (int n = 0; n < N; ++n) {
        Real pos = (Real)n - D;
        if (pos <= (Real)0) y[n] = 0;
        else {
            int i0 = (int)floor(pos);
            Real frac = pos - (Real)i0;
            if (i0 >= N - 1) y[n] = x[N - 1];
            else y[n] = ((Real)1.0 - frac) * x[i0] + frac * x[i0 + 1];
        }
    }
    return NumVal(y);
}

static Value fn_comb(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 3) throw Error{I.filename, I.cur_line(), "comb: 3 args required (sig, D, g)"};
    const NumVal& x = sig_nvec(args[0], "comb");
    int D = (int)sig_scalar(args[1], "comb");
    Real g = sig_scalar(args[2], "comb");
    if (D < 0) throw Error{I.filename, I.cur_line(), "comb: delay must be non-negative"};
    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);
    for (int n = 0; n < N; ++n) {
        if ((n & 1023) == 0) sig_yield(I);
        Real fb = (n - D >= 0) ? g * y[n-D] : Real(0);
        y[n] = x[n] + fb;
    }
    return NumVal(y);
}

static Value fn_allpass(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 3) throw Error{I.filename, I.cur_line(), "allpass: 3 args required (sig, D, g)"};
    const NumVal& x = sig_nvec(args[0], "allpass");
    int D = (int)sig_scalar(args[1], "allpass");
    Real g = sig_scalar(args[2], "allpass");
    if (D < 0) throw Error{I.filename, I.cur_line(), "allpass: delay must be non-negative"};
    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);
    for (int n = 0; n < N; ++n) {
        if ((n & 1023) == 0) sig_yield(I);
        Real xmD = (n-D >= 0) ? x[n-D] : Real(0);
        Real ymD = (n-D >= 0) ? y[n-D] : Real(0);
        y[n] = -g * x[n] + xmD + g * ymD;
    }
    return NumVal(y);
}

static Value fn_resample(std::vector<Value>& args, Interpreter& I) {
    if (args.size() != 2) throw Error{I.filename, I.cur_line(), "resample: 2 args required (sig, factor)"};
    const NumVal& x = sig_nvec(args[0], "resample");
    Real factor = sig_scalar(args[1], "resample");
    return NumVal(fd_resample(std::valarray<Real>(x), factor));
}

inline void add_signals(Environment& env) {
    env.register_builtin("mix",          fn_mix);
    env.register_builtin("gen",          fn_gen);
    env.register_builtin("osc",          fn_osc);
    env.register_builtin("fft",          fn_fft);
    env.register_builtin("ifft",         fn_ifft);
    env.register_builtin("car2pol",      fn_car2pol);
    env.register_builtin("pol2car",      fn_pol2car);
    env.register_builtin("window",       fn_window);
    env.register_builtin("speccent",     fn_speccent);
    env.register_builtin("specspread",   fn_specspread);
    env.register_builtin("specskew",     fn_specskew);
    env.register_builtin("speckurt",     fn_speckurt);
    env.register_builtin("specflux",     fn_specflux);
    env.register_builtin("specirr",      fn_specirr);
    env.register_builtin("specdecr",     fn_specdecr);
    env.register_builtin("acorrf0",      fn_acorrf0);
    env.register_builtin("energy",       fn_energy);
    env.register_builtin("zcr",          fn_zcr);
    env.register_builtin("conv",         fn_conv);
    env.register_builtin("convmc",       fn_convmc);
    env.register_builtin("deinterleave", fn_deinterleave);
    env.register_builtin("interleave",   fn_interleave);
    env.register_builtin("head",         fn_head);
    env.register_builtin("tail",         fn_tail);
    env.register_builtin("vslice",       fn_vslice);
    env.register_builtin("vaddat",       fn_vaddat);
    env.register_builtin("dcblock",      fn_dcblock);
    env.register_builtin("reson",        fn_reson);
    env.register_builtin("aufilter",       fn_filter);
    env.register_builtin("filtdesign",   fn_filtdesign);
    env.register_builtin("delay",        fn_delay);
    env.register_builtin("comb",         fn_comb);
    env.register_builtin("allpass",      fn_allpass);
    env.register_builtin("resample",     fn_resample);
}

#endif // SIGNALS_H
