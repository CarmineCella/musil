# ─────────────────────────────────────────────────────────────────────────────
# signals.mu  —  Musil signals library  (v0.5)
#
# Requires: stdlib.mu loaded, and the signals C++ builtins registered
#           via add_signals(env) in your application.
#
# C++ primitives provided by signals.h:
#   mix, gen, osc
#   fft, ifft, car2pol, pol2car, window
#   speccent, specspread, specskew, speckurt, specflux, specirr, specdecr
#   acorrf0, energy, zcr
#   conv, convmc, deinterleave, interleave
#   vslice(v, start, n)
#   dcblock, reson, filter, filtdesign, delay, comb, allpass, resample
# ─────────────────────────────────────────────────────────────────────────────

# ── RMS and normalisation ─────────────────────────────────────────────────────

# rms(sig) — alias for energy(), which computes sqrt(sum(x^2) / N)
proc rms (sig) { return energy(sig) }

# normalize(sig, target_rms) — scale signal so its RMS equals target
proc normalize (sig, target) {
    var r = rms(sig)
    if (r == 0) { return sig }
    return sig * (target / r)
}

# ── Filter design + application ───────────────────────────────────────────────

# design_biquad(type, Fs, f0, Q, gain_db) -> [b, a]
# type: "lowpass" "highpass" "notch" "peak" "lowshelf" "highshelf"
# Returns an Array [b_vec, a_vec] suitable for apply_biquad.
proc design_biquad (type, Fs, f0, Q, gain_db) {
    return filtdesign(type, Fs, f0, Q, gain_db)
}

# apply_biquad(sig, coeffs) — apply a single biquad filter
# coeffs: [b, a] as returned by design_biquad / filtdesign
proc apply_biquad (sig, coeffs) {
    var b = coeffs[0]
    var a = coeffs[1]
    return filter(sig, b, a)
}

# apply_biquad_chain2(sig, coeffs1, coeffs2) — apply two biquads in series
proc apply_biquad_chain2 (sig, coeffs1, coeffs2) {
    return apply_biquad(apply_biquad(sig, coeffs1), coeffs2)
}

# ── Named EQ helpers ──────────────────────────────────────────────────────────

proc lowpass (sig, Fs, f0, Q) {
    return apply_biquad(sig, design_biquad("lowpass", Fs, f0, Q, 0.0))
}

proc highpass (sig, Fs, f0, Q) {
    return apply_biquad(sig, design_biquad("highpass", Fs, f0, Q, 0.0))
}

proc notch_filter (sig, Fs, f0, Q) {
    return apply_biquad(sig, design_biquad("notch", Fs, f0, Q, 0.0))
}

proc peak_eq (sig, Fs, f0, Q, gain_db) {
    return apply_biquad(sig, design_biquad("peak", Fs, f0, Q, gain_db))
}

proc lowshelf (sig, Fs, f0, Q, gain_db) {
    return apply_biquad(sig, design_biquad("lowshelf", Fs, f0, Q, gain_db))
}

proc highshelf (sig, Fs, f0, Q, gain_db) {
    return apply_biquad(sig, design_biquad("highshelf", Fs, f0, Q, gain_db))
}

# ── Delay / comb / allpass helpers ────────────────────────────────────────────

# frac_delay(sig, delay_samples) — fractional sample delay (linear interp)
proc frac_delay (sig, delay_samples) {
    return delay(sig, delay_samples)
}

# comb_filter(sig, delay_samples, g) — feedback comb filter
proc comb_filter (sig, delay_samples, g) {
    return comb(sig, delay_samples, g)
}

# allpass_filter(sig, delay_samples, g) — Schroeder allpass section
proc allpass_filter (sig, delay_samples, g) {
    return allpass(sig, delay_samples, g)
}

# schroeder_reverb(sig, Fs) — toy Schroeder reverb network
# Four feedback comb filters in parallel, then two allpass sections in series.
proc schroeder_reverb (sig, Fs) {
    var d1 = floor(Fs * 0.0297)
    var d2 = floor(Fs * 0.0371)
    var d3 = floor(Fs * 0.0411)
    var d4 = floor(Fs * 0.0437)

    var c1 = comb_filter(sig, d1, 0.773)
    var c2 = comb_filter(sig, d2, 0.802)
    var c3 = comb_filter(sig, d3, 0.753)
    var c4 = comb_filter(sig, d4, 0.733)

    var mixc = c1 + c2 + c3 + c4    # element-wise addition of same-length NumVals

    var a1 = allpass_filter(mixc, floor(Fs * 0.0050), 0.7)
    var a2 = allpass_filter(a1,   floor(Fs * 0.0017), 0.7)
    return a2
}

# ── DC blocker / resonator helpers ────────────────────────────────────────────

proc dc_block (sig)       { return dcblock(sig) }           # default R = 0.995
proc dc_block_R (sig, R)  { return dcblock(sig, R) }
proc resonator (sig, sr, freq, tau) { return reson(sig, sr, freq, tau) }

# ── Resampling helpers ────────────────────────────────────────────────────────

proc resample_factor (sig, factor)    { return resample(sig, factor) }
proc resample_to (sig, Fs_in, Fs_out) { return resample(sig, Fs_out / Fs_in) }

# ── Oscillator bank ───────────────────────────────────────────────────────────
#
# oscbank(sr, amps, freqs, tab) -> NumVal
#   amps  : Array of NumVals — amplitude envelope per partial
#   freqs : Array of NumVals — frequency envelope per partial (Hz, per sample)
#   tab   : NumVal wavetable (with guard point — use gen() to create)
#   sr    : sample rate scalar
#
# Produces the sum of all oscillator partials, each weighted by its amplitude.

proc oscbank (sr, amps, freqs, tab) {
    var n = len(amps)
    if (n != len(freqs)) {
        print "oscbank: amps and freqs must have the same length"
        return zeros(1)
    }
    var i   = 0
    var out = zeros(1)    # placeholder; overwritten on first partial
    while (i < n) {
        var a_i  = amps[i]
        var f_i  = freqs[i]
        var sigw = a_i * osc(sr, f_i, tab)
        if (i == 0) { out = sigw }
        else        { out = out + sigw }
        i = i + 1
    }
    return out
}

# ── STFT / ISTFT ─────────────────────────────────────────────────────────────
#
# stft(sig, N, hop) -> Array of NumVals (spectra)
#   Each spectrum is a complex interleaved NumVal of length 2*next_pow2(N),
#   as returned by fft().  A Hann window is applied before each FFT.
#
# istft(specs, N, hop) -> NumVal (reconstructed signal)
#   Overlap-add reconstruction with Hann window.
#   Normalises by the window energy to compensate for OLA gain.

proc stft (sig, N, hop) {
    var L    = len(sig)
    var win  = window(N, 0.5, 0.5, 0.0)    # Hann window
    var specs = []
    var pos  = 0
    while (pos + N <= L) {
        var frame = vslice(sig, pos, N)
        push(specs, fft(frame * win))
        pos = pos + hop
    }
    return specs
}

proc istft (specs, N, hop) {
    var nframes = len(specs)
    if (nframes == 0) { return zeros(1) }

    var Lout = hop * (nframes - 1) + N
    var out  = zeros(Lout)                  # NumVal — element assignment works in-place

    var win      = window(N, 0.5, 0.5, 0.0)
    var win_energy = sum(win * win)
    var normconst  = win_energy / hop       # OLA normalisation constant

    var k = 0
    while (k < nframes) {
        var frame   = ifft(specs[k]) * win  # inverse FFT + synthesis window
        var pos     = k * hop
        var i       = 0
        while (i < N) {
            out[pos + i] = out[pos + i] + frame[i]   # overlap-add
            i = i + 1
        }
        k = k + 1
    }

    return out * (1.0 / normconst)
}

# eof
