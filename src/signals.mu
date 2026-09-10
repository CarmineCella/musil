# signals.mu — offline signal processing on vectors, Musil half.
#
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# Not loaded automatically: (load "signals.mu"). Needs std.mu and scientific.mu.
# A signal is a vector of samples; a complex spectrum is (list re im); a polar
# spectrum is (list mag phase); a stereo buffer is (list left right).
# The C++ half (signals.h) provides fft, ifft, osc, iir, delay, resample,
# autocorr, interleave, deinterleave. Everything below is vector arithmetic.
load "scientific.mu"

# --- generators -------------------------------------------------------------
# (sine sr freq dur)       a sine of freq Hz lasting dur seconds
function sine (sr freq dur) (sin (* tau freq (/ (range (floor (* sr dur))) sr)))
# (noise n)                white noise in [-1, 1]
function noise (n) (- (* 2 (rand n)) 1)
# (impulse n)              a 1 followed by n-1 zeros
function impulse (n) (vec 1 (zeros (- n 1)))
# (gen n amps)             one period of a wavetable with the given harmonic amplitudes, plus a
#                          guard point (n+1 samples, last = first), normalised to a peak of 1; for osc
function gen (n amps) {
    var t (/ (range (+ n 1)) n)
    var partials (zip (vec->list amps) (vec->list (range 1 (+ (length amps) 1))))
    var table (reduce partials (function (acc p) (+ acc (* (head p) (sin (* tau (last p) t))))) (zeros (+ n 1)))
    setidx table n (head table)               # exact guard point
    var peak (max (abs table))
    if (== peak 0) { return table }
    return (/ table peak)
}
# (oscbank sr amps freqs table)   sum of oscillators; amps and freqs are lists of per-sample envelopes
function oscbank (sr amps freqs table) {
    if (!= (length amps) (length freqs)) { error "oscbank: amps and freqs must have the same length" }
    return (reduce (zip amps freqs) (function (acc p) (+ acc (* (head p) (osc sr (last p) table)))) 0)
}
# (mix layers)             overlay (list (list position signal) ...) into one signal
function mix (layers) (reduce layers (function (acc p) (add-at acc (head p) (last p))) (vec))
# (add-at dst pos src)     dst with src added starting at pos; dst grows if needed
function add-at (dst pos src) {
    var need (+ pos (length src))
    var base (if (< (length dst) need) (vec dst (zeros (- need (length dst)))) dst)
    return (vec (take base pos) (+ (slice base pos (length src)) src) (drop base need))
}
# (fade-in x n) (fade-out x n)   linear fades over n samples
function fade-in (x n) (* x (vec (/ (range n) n) (ones (- (length x) n))))
function fade-out (x n) (reverse (fade-in (reverse x) n))
# (rms x)                  root mean square
function rms (x) (sqrt (mean (* x x)))
# (normalize-peak x)       scale so the peak is 1; (normalize-rms x target) so the rms is target
function normalize-peak (x) {
    var p (max (abs x))
    if (== p 0) { return x }
    return (/ x p)
}
# (normalize-rms x target) scale so the rms is target
function normalize-rms (x target) {
    var r (rms x)
    if (== r 0) { return x }
    return (* x (/ target r))
}
# (db x) (undb d)          amplitude to decibels and back
function db (x) (* 20 (log10 (max x 1e-12)))
function undb (d) (pow 10 (/ d 20))

# (next-pow2 n)            the smallest power of two >= n (fft sizes)
function next-pow2 (n) (pow 2 (ceil (log2 (max n 1))))

# --- windows and spectra ---------------------------------------------------------
# (window n a0 a1 a2)      generalised cosine window, periodic (so hann overlap-adds exactly at hop n/4);
#                          (hann n) (hamming n) (blackman n) are the usual ones
function window (n a0 a1 a2) {
    var t (/ (* tau (range n)) n)
    return (+ (- a0 (* a1 (cos t))) (* a2 (cos (* 2 t))))
}
# (hann n) (hamming n) (blackman n) the usual windows, periodic
function hann (n) (window n 0.5 0.5 0)
# (hamming n) the Hamming window
function hamming (n) (window n 0.54 0.46 0)
# (blackman n) the Blackman window
function blackman (n) (window n 0.42 0.5 0.08)
# (car->pol spec)          (list re im) -> (list mag phase); (pol->car) the other way
function car->pol (spec) {
    var re (head spec)
    var im (last spec)
    return (list (sqrt (+ (* re re) (* im im))) (atan2 im re))
}
# (pol->car spec) (list mag phase) -> (list re im)
function pol->car (spec) {
    var mag (head spec)
    var phase (last spec)
    return (list (* mag (cos phase)) (* mag (sin phase)))
}
# (magnitudes spec) (phases spec)   from a complex spectrum
function magnitudes (spec) (head (car->pol spec))
function phases (spec) (last (car->pol spec))
# (spectrum-freqs n sr)    the frequency of each of the first n/2 bins
function spectrum-freqs (n sr) (* (range (/ n 2)) (/ sr n))
# (magnitude-spectrum x)   the positive-frequency magnitudes of x, normalised so a full-scale sine reads 1
function magnitude-spectrum (x) {
    var m (magnitudes (fft x))
    var n (length m)
    return (* (take m (/ n 2)) (/ 2 (length x)))
}
# (complex-mul a b)        product of two (list re im) spectra
function complex-mul (a b) {
    var ar (head a)
    var ai (last a)
    var br (head b)
    var bi (last b)
    return (list (- (* ar br) (* ai bi)) (+ (* ar bi) (* ai br)))
}
# (conv x h)               linear convolution through the FFT, length (+ (length x) (length h) -1)
function conv (x h) {
    var n (+ (length x) (length h) -1)
    var padded-x (vec x (zeros (- n (length x))))
    var padded-h (vec h (zeros (- n (length h))))
    return (take (ifft (complex-mul (fft padded-x) (fft padded-h))) n)
}

# (cepstrum mags)          real cepstrum of a full (symmetric) magnitude spectrum
function cepstrum (mags) (ifft (list (log (max mags 1e-12)) (zeros (length mags))))
# (spectral-envelope mags order)   smooth envelope of a full magnitude spectrum: the "true envelope",
#                          a cepstral smoothing (first `order` coefficients) iterated so that it rides
#                          on the harmonic peaks instead of averaging peaks and valleys. The order sets
#                          the resolution: about sr / (2 f0) of the sound (e.g. 150 for a voice at
#                          44.1 kHz) follows the formants without rippling at the harmonics
function spectral-envelope (mags order) {
    var n (length mags)
    var lifter (vec (ones (+ order 1)) (zeros (- n (* 2 order) 1)) (ones order))
    function smooth (log-spec) (head (fft (* (ifft (list log-spec (zeros n))) lifter)))
    var target (log (max mags (* 1e-4 (max mags))))
    var env (smooth target)
    times 12 (function (k) (set env (smooth (max target env))))
    return (exp env)
}
# (flatten-spectrum mags order)    the magnitudes divided by their envelope: the fine structure alone
function flatten-spectrum (mags order) (/ mags (spectral-envelope mags order))
# (impose-envelope mags source order)   mags reshaped to carry the spectral envelope of source
function impose-envelope (mags source order) (* (flatten-spectrum mags order) (spectral-envelope source order))

# --- STFT: a list of complex spectra, one per hop, Hann-windowed ---------------------
# (stft x n hop) => list of (list re im); (istft frames n hop) => signal by overlap-add
function stft (x n hop) {
    var w (hann n)
    var frames (list)
    for (var pos 0) (<= (+ pos n) (length x)) (var pos (+ pos hop)) {
        push frames (fft (* (slice x pos n) w))
    }
    return frames
}
# (istft frames n hop) the signal back from stft frames by windowed overlap-add
function istft (frames n hop) {
    if (== (length frames) 0) { return (vec) }
    var w (hann n)
    var gain (/ (sum (* w w)) hop)
    var out (zeros (+ (* hop (- (length frames) 1)) n))
    for (var k 0) (< k (length frames)) (var k (+ k 1)) {
        var out (add-at out (* k hop) (* (take (ifft (getidx frames k)) n) w))
    }
    return (/ out gain)
}
# (stft-magnitudes frames)   list of positive-frequency magnitude vectors, one per frame
function stft-magnitudes (frames) (map frames (function (s) (take (magnitudes s) (/ (length (head s)) 2))))

# --- phase vocoder --------------------------------------------------------------
# (princarg x)             wrap a phase to (-pi, pi]
function princarg (x) (- x (* tau (round (/ x tau))))
# (pvoc-stretch x n hop factor)   time-stretch by factor (2 = twice as long) with the pitch kept:
#                          analysis at hop, synthesis at hop*factor, phases advanced by each bin's
#                          measured frequency and locked to the nearest spectral peak (Laroche-Dolson),
#                          which keeps partials coherent instead of smearing them
function pvoc-stretch (x n hop factor) {
    var frames (stft x n hop)
    if (== (length frames) 0) { return (vec) }
    var hs (max 1 (round (* hop factor)))
    var w (hann n)
    var gain (/ (sum (* w w)) hs)
    var omega (* tau (range n) (/ hop n))            # expected phase advance per analysis hop, per bin
    var ratio (/ hs hop)
    var out (zeros (+ (* hs (- (length frames) 1)) n))
    var prev (zeros n)
    var psi (zeros n)
    for (var k 0) (< k (length frames)) (var k (+ k 1)) {
        var polar (car->pol (getidx frames k))
        var mags (head polar)
        var phase (last polar)
        if (== k 0) {
            set psi phase
        } {
            var inc (+ omega (princarg (- phase prev omega)))       # true phase advance per hop, per bin
            var peaks (local-maxima mags)
            if (== (length peaks) 0) { set peaks (vec 0) }
            # each bin belongs to the region of its nearest peak: regions start halfway between peaks
            var starts (floor (/ (+ (take peaks (- (length peaks) 1)) (drop peaks 1)) 2))
            var marks (zeros n)
            each starts (function (b) (setidx marks (+ b 1) 1))
            var owner (gather peaks (cumsum marks))               # the peak bin that owns each bin
            var peak-psi (+ (gather psi owner) (* ratio (gather inc owner)))
            set psi (+ peak-psi (- phase (gather phase owner)))    # bins follow their peak's rotation
        }
        set prev phase
        set out (add-at out (* k hs) (* (take (ifft (pol->car (list mags psi))) n) w))
    }
    return (/ out gain)
}
# (pvoc-pitch x n hop ratio)   pitch-shift by ratio (2 = an octave up) at the same length:
#                          a time-stretch by ratio followed by resampling back
function pvoc-pitch (x n hop ratio) {
    var y (resample (pvoc-stretch x n hop ratio) (/ 1 ratio))
    if (>= (length y) (length x)) { return (take y (length x)) }
    return (vec y (zeros (- (length x) (length y))))
}
# (pvoc-pitch-formant x n hop ratio order)   pitch-shift keeping the formants: the shifted sound
#                          gets, frame by frame, the spectral envelope of the original (cepstral,
#                          `order` coefficients, about sr / (2 f0)), so a voice stays the same voice
function pvoc-pitch-formant (x n hop ratio order) {
    var shifted (stft (pvoc-pitch x n hop ratio) n hop)
    var original (stft x n hop)
    var frames (map (zip shifted original) (function (p) {
        var ps (car->pol (head p))
        return (pol->car (list (impose-envelope (head ps) (magnitudes (last p)) order) (last ps)))
    }))
    return (istft frames n hop)
}
# (gate-spectrum mags threshold)   magnitudes below threshold times the frame's peak set to zero
function gate-spectrum (mags threshold) (* mags (> mags (* threshold (max mags))))
# (spectral-morph a b t n hop)   between two sounds of the same length: magnitudes interpolated
#                          linearly, phases taken from a below t = 0.5 and from b above
function spectral-morph (a b t n hop) {
    var fa (stft a n hop)
    var fb (stft b n hop)
    var frames (map (zip fa fb) (function (p) {
        var pa (car->pol (head p))
        var pb (car->pol (last p))
        return (pol->car (list (lerp (head pa) (head pb) t) (if (< t 0.5) (last pa) (last pb))))
    }))
    return (istft frames n hop)
}
# (robotize x n hop)       every frame with zero phase: a buzz at the frame rate
function robotize (x n hop) (istft (map (stft x n hop) (function (s) (pol->car (list (magnitudes s) (zeros n))))) n hop)
# (whisperize x n hop)     every frame with random phases: the spectral envelope on noise
function whisperize (x n hop) (istft (map (stft x n hop) (function (s) (pol->car (list (magnitudes s) (* tau (rand n)))))) n hop)

# --- spectral and temporal features (amps: positive-frequency magnitudes; freqs: their frequencies) ---
# (spectral-moment amps freqs order centroid) weighted moment of the frequencies about a centroid
function spectral-moment (amps freqs order centroid) {
    var total (sum amps)
    if (== total 0) { return 0 }
    return (/ (sum (* amps (pow (- freqs centroid) order))) total)
}
# (spectral-centroid amps freqs) amplitude-weighted mean frequency
function spectral-centroid (amps freqs) (spectral-moment amps freqs 1 0)
# (spectral-spread amps freqs) standard deviation around the centroid
function spectral-spread (amps freqs) (sqrt (spectral-moment amps freqs 2 (spectral-centroid amps freqs)))
# (spectral-skewness amps freqs) asymmetry of the spectrum about its centroid
function spectral-skewness (amps freqs) {
    var c (spectral-centroid amps freqs)
    var s (spectral-spread amps freqs)
    if (== s 0) { return 0 }
    return (/ (spectral-moment amps freqs 3 c) (pow s 3))
}
# (spectral-kurtosis amps freqs) peakedness of the spectrum about its centroid
function spectral-kurtosis (amps freqs) {
    var c (spectral-centroid amps freqs)
    var s (spectral-spread amps freqs)
    if (== s 0) { return 0 }
    return (/ (spectral-moment amps freqs 4 c) (pow s 4))
}
# (spectral-flux amps previous)   rectified increase from the previous frame
function spectral-flux (amps previous) (sum (max (- amps previous) 0))
# (spectral-irregularity amps)   sum of absolute differences between neighbouring bins
function spectral-irregularity (amps) (sum (abs (diff amps)))
# (spectral-decrease amps)       weighted decrease relative to the first bin
function spectral-decrease (amps) {
    var rest (drop amps 1)
    var total (sum rest)
    if (== total 0) { return 0 }
    return (/ (sum (/ (- rest (head amps)) (range 1 (length amps)))) total)
}
# (spectral-flatness amps)       geometric over arithmetic mean: 1 for noise, 0 for a pure tone
function spectral-flatness (amps) {
    var a (+ amps 1e-12)
    return (/ (exp (mean (log a))) (mean a))
}
# (spectral-rolloff amps freqs fraction)   frequency below which the fraction of the energy lies
function spectral-rolloff (amps freqs fraction) {
    var c (cumsum (* amps amps))
    var target (* fraction (last c))
    return (getidx freqs (find (>= c target) 1))
}
# (hfc amps)                     high-frequency content
function hfc (amps) (/ (sum (* amps amps (range (length amps)))) (max 1 (sum (range (length amps)))))
# (energy x)                     rms of a frame; (zcr x) zero-crossing rate per sample
function energy (x) (rms x)
# (zcr x) zero-crossing rate per sample
function zcr (x) (/ (sum (!= (sign (drop x 1)) (sign (take x (- (length x) 1))))) (length x))
# (acf-f0 x sr)                  fundamental by autocorrelation; 0 when no clear peak
function acf-f0 (x sr) {
    var r (autocorr x)
    if (== (head r) 0) { return 0 }
    var k 1
    while (and (< k (- (length r) 1)) (<= (getidx r k) (getidx r (- k 1)))) { var k (+ k 1) }
    var rest (drop r k)
    if (== (length rest) 0) { return 0 }
    var peak (+ k (argmax rest))
    if (< (/ (getidx r peak) (head r)) 0.3) { return 0 }
    return (/ sr peak)
}

# --- filters --------------------------------------------------------------------
# (biquad type sr f0 q gain-db) => (list b a) of an RBJ biquad; type is one of
#   "lowpass" "highpass" "bandpass" "notch" "peak" "lowshelf" "highshelf"
function biquad (type sr f0 q gain-db) {
    var w0 (/ (* tau f0) sr)
    var c (cos w0)
    var s (sin w0)
    var alpha (/ s (* 2 q))
    var A (pow 10 (/ gain-db 40))
    var sA (sqrt A)
    if (equal? type "lowpass")  { return (list (vec (/ (- 1 c) 2) (- 1 c) (/ (- 1 c) 2)) (vec (+ 1 alpha) (* -2 c) (- 1 alpha))) }
    if (equal? type "highpass") { return (list (vec (/ (+ 1 c) 2) (- 0 (+ 1 c)) (/ (+ 1 c) 2)) (vec (+ 1 alpha) (* -2 c) (- 1 alpha))) }
    if (equal? type "bandpass") { return (list (vec alpha 0 (- 0 alpha)) (vec (+ 1 alpha) (* -2 c) (- 1 alpha))) }
    if (equal? type "notch")    { return (list (vec 1 (* -2 c) 1) (vec (+ 1 alpha) (* -2 c) (- 1 alpha))) }
    if (equal? type "peak")     { return (list (vec (+ 1 (* alpha A)) (* -2 c) (- 1 (* alpha A))) (vec (+ 1 (/ alpha A)) (* -2 c) (- 1 (/ alpha A)))) }
    if (equal? type "lowshelf") {
        return (list (vec (* A (+ (- (+ A 1) (* (- A 1) c)) (* 2 sA alpha))) (* 2 A (- (- A 1) (* (+ A 1) c))) (* A (- (- (+ A 1) (* (- A 1) c)) (* 2 sA alpha))))
                     (vec (+ (+ A 1) (* (- A 1) c) (* 2 sA alpha)) (* -2 (+ (- A 1) (* (+ A 1) c))) (- (+ (+ A 1) (* (- A 1) c)) (* 2 sA alpha))))
    }
    if (equal? type "highshelf") {
        return (list (vec (* A (+ (+ (+ A 1) (* (- A 1) c)) (* 2 sA alpha))) (* -2 A (+ (- A 1) (* (+ A 1) c))) (* A (- (+ (+ A 1) (* (- A 1) c)) (* 2 sA alpha))))
                     (vec (+ (- (+ A 1) (* (- A 1) c)) (* 2 sA alpha)) (* 2 (- (- A 1) (* (+ A 1) c))) (- (- (+ A 1) (* (- A 1) c)) (* 2 sA alpha))))
    }
    error "biquad: unknown type " type
}
# (apply-filter x coeffs)  run x through (list b a) as returned by biquad
function apply-filter (x coeffs) (iir x (head coeffs) (last coeffs))
# (lowpass x sr f0 q) (highpass x sr f0 q) (bandpass x sr f0 q) (notch x sr f0 q) one biquad, applied
function lowpass (x sr f0 q) (apply-filter x (biquad "lowpass" sr f0 q 0))
# (highpass x sr f0 q) a highpass biquad, applied
function highpass (x sr f0 q) (apply-filter x (biquad "highpass" sr f0 q 0))
# (bandpass x sr f0 q) a bandpass biquad, applied
function bandpass (x sr f0 q) (apply-filter x (biquad "bandpass" sr f0 q 0))
# (notch x sr f0 q) a notch biquad, applied
function notch (x sr f0 q) (apply-filter x (biquad "notch" sr f0 q 0))
# (peak-eq x sr f0 q gain-db) (lowshelf x sr f0 q gain-db) (highshelf x sr f0 q gain-db) the equaliser biquads, applied
function peak-eq (x sr f0 q gain-db) (apply-filter x (biquad "peak" sr f0 q gain-db))
# (lowshelf x sr f0 q gain-db) a low shelf, applied
function lowshelf (x sr f0 q gain-db) (apply-filter x (biquad "lowshelf" sr f0 q gain-db))
# (highshelf x sr f0 q gain-db) a high shelf, applied
function highshelf (x sr f0 q gain-db) (apply-filter x (biquad "highshelf" sr f0 q gain-db))
# (dc-block x)             remove the DC offset; (dc-block-r x r) with pole radius r (default 0.995)
# (dc-block-r x r) a DC blocker with pole radius r
function dc-block-r (x r) (iir x (vec 1 -1) (vec 1 (- 0 r)))
function dc-block (x) (dc-block-r x 0.995)
# (reson x sr freq tau)    two-pole resonator at freq Hz with decay time tau seconds; the output
#                          lasts tau seconds, so a short excitation rings out
function reson (x sr freq tau) {
    var om (/ (* tau-const freq) sr)
    var radius (exp (/ (* -1 tau-const) (* tau sr)))
    var n (floor (* sr tau))
    var input (if (< (length x) n) (vec x (zeros (- n (length x)))) (take x n))
    return (iir input (vec (* radius (sin om))) (vec 1 (* -2 radius (cos om)) (* radius radius)))
}
var tau-const tau                          # tau the constant 2 pi, kept apart from the decay-time parameter
# (comb x d g)             feedback comb: y[n] = x[n] + g y[n-d]
function comb (x d g) (iir x (vec 1) (vec 1 (zeros (- d 1)) (- 0 g)))
# (allpass x d g)          Schroeder allpass section: y[n] = -g x[n] + x[n-d] + g y[n-d]
function allpass (x d g) (iir x (vec (- 0 g) (zeros (- d 1)) 1) (vec 1 (zeros (- d 1)) (- 0 g)))
# (schroeder-reverb x sr rt60)   four parallel combs into two allpasses; rt60 is the decay time in
#                          seconds (how long the tail takes to fall by 60 dB). The output is the input
#                          plus rt60 seconds, so the tail is not cut off.
function schroeder-reverb (x sr rt60) {
    var padded (vec x (zeros (floor (* sr rt60))))
    function comb-gain (d) (pow 10 (/ (* -3 d) (* rt60 sr)))       # the gain that decays 60 dB in rt60
    function comb-at (seconds) {
        var d (floor (* sr seconds))
        return (comb padded d (comb-gain d))
    }
    var combs (+ (comb-at 0.0297) (comb-at 0.0371) (comb-at 0.0411) (comb-at 0.0437))
    var a1 (allpass combs (floor (* sr 0.005)) 0.7)
    return (* 0.25 (allpass a1 (floor (* sr 0.0017)) 0.7))
}
# (resample-to x sr-in sr-out)
function resample-to (x sr-in sr-out) (resample x (/ sr-out sr-in))

# --- envelopes ---------------------------------------------------------------
# (envelope-follow x n)    rms over hops of n samples, one value per hop
function envelope-follow (x n) {
    var out (list)
    for (var pos 0) (<= (+ pos n) (length x)) (var pos (+ pos n)) { push out (rms (slice x pos n)) }
    return (vec out)
}
# (envelope-from-values v hop)   piecewise-linear signal through successive values, hop samples apart
function envelope-from-values (v hop) (bpf (head v) (map (vec->list (drop v 1)) (function (e) (list hop e))))
