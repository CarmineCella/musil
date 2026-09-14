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
# (noise n)                white noise in [-1, 1]; n may also be a signal, giving noise of its length
#                          (so an instrument can write (noise gate): offline as long as the gate, streamed forever)
function noise (n) (- (* 2 (rand (if (> (length n) 1) (length n) n))) 1)
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
function mix (layers) {
    if (== (length layers) 0) { return (vec) }
    var total (max-of (map layers (function (p) (+ (head p) (length (last p))))))
    var out (zeros (max 0 total))
    each layers (function (p) (add-at! out (head p) (last p)))
    return out
}
# (add-at dst pos src)     dst with src added starting at pos; dst grows if needed
function add-at (dst pos src) {
    var need (+ pos (length src))
    var base (if (< (length dst) need) (vec dst (zeros (- need (length dst)))) dst)
    return (vec (take base pos) (+ (slice base pos (length src)) src) (drop base need))
}
# (pan x pos)              a mono signal to (list left right), equal power; pos from -1 (left) to 1 (right)
function pan (x pos) {
    var p (max -1 (min 1 pos))
    return (list (* x (sqrt (* 0.5 (- 1 p)))) (* x (sqrt (* 0.5 (+ 1 p)))))
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
    var out (zeros (+ (* hop (- (length frames) 1)) n))
    var wsum (zeros (length out))
    for (var k 0) (< k (length frames)) (var k (+ k 1)) {
        add-at! out (* k hop) (* (take (ifft (getidx frames k)) n) w)
        add-at! wsum (* k hop) (* w w)
    }
    return (/ out (max wsum 1e-6))                     # exact where the windows overlap fully, and at the edges too
}
# (stft-magnitudes frames)   list of positive-frequency magnitude vectors, one per frame
function stft-magnitudes (frames) (map frames (function (s) (take (magnitudes s) (/ (length (head s)) 2))))

# --- phase vocoder --------------------------------------------------------------
# (princarg x)             wrap a phase to (-pi, pi]
function princarg (x) (- x (* tau (round (/ x tau))))
# (ramp v)                 a parameter given as a number or as (list start end) -> (list start end)
function ramp (v) (if (equal? (type v) "list") v (list v v))
# (fftshift v)             rotate a vector by half its length (zero-phase windowing)
function fftshift (v) {
    var h (floor (/ (length v) 2))
    return (vec (drop v h) (take v h))
}
# (pvoc x opts)            the phase vocoder (signals.h): analysis, phase-locked resynthesis and
#                          every transformation in one pass, as sparkle did. opts is a list of
#                          (list key value); a value given as (list start end) ramps over the file.
#                            "stretch"   time-stretch ratio (2 = twice as long)              default 1
#                            "pitch"     pitch-shift ratio (2 = an octave up)                  1
#                            "formants"  formant-move ratio, needs "envelope"                  1
#                            "envelope"  cepstral order for envelope preservation: about sr/100
#                                        (80 at 8 kHz, 400 at 44.1 kHz); 0 = off                0
#                            "window"    analysis window in samples                            2048
#                            "overlap"   synthesis overlap factor: output hop = window/overlap  8
#                            "pad"       zero-padding: fft size = next-pow2(window) * 2^pad     1
#                            "threshold" denoise: bins below threshold x the level of a full-scale sine at the
#                                        signal's peak are zeroed (0.01 - 0.1 removes a noise floor)  0
#                            "cross"     (list mode amount other) or (list mode start end other):
#                                        mode 1 multiplicative, 2 spectral flattener (needs "envelope"),
#                                        3 morphing; amount 0..1; other is the second signal
#                            "phase"     "robot" (zero phases) or "whisper" (random phases)
#                          The output hop is fixed and the input hop is output-hop/stretch, so the
#                          synthesis overlap never thins out however large the stretch. Ramps are
#                          linear in the hop (as in sparkle): a stretch of (list 1 3) is 1.5x overall.
# The specific uses, as wrappers around pvoc
# (pvoc-stretch x factor)                time-stretch by factor with the pitch kept
function pvoc-stretch (x factor) (pvoc x (list (list "stretch" factor)))
# (pvoc-pitch x ratio)                   pitch-shift by ratio at the same length (formants move too)
function pvoc-pitch (x ratio) (pvoc x (list (list "pitch" ratio)))
# (pvoc-pitch-formant x ratio order)     pitch-shift keeping the formants (order about sr/100: 400 at 44.1 kHz)
function pvoc-pitch-formant (x ratio order) (pvoc x (list (list "pitch" ratio) (list "envelope" order)))
# (pvoc-formants x ratio order)          move the formants by ratio, the pitch kept
function pvoc-formants (x ratio order) (pvoc x (list (list "formants" ratio) (list "envelope" order)))
# (pvoc-cross x y mode amount order)     cross synthesis of x by y: mode 1 multiplicative,
#                                        2 spectral flattener (y's envelope on x), 3 morphing
function pvoc-cross (x y mode amount order) (pvoc x (list (list "cross" (list mode amount y)) (list "envelope" order)))
# (robotize x)                           zero phases: a buzz at the frame rate
function robotize (x) (pvoc x (list (list "phase" "robot") (list "overlap" 16)))
# (whisperize x)                         random phases: the spectral envelope on noise
function whisperize (x) (pvoc x (list (list "phase" "whisper") (list "window" 256) (list "overlap" 4)))
# (denoise x threshold)                  magnitudes below threshold x the peak are zeroed (0.01 - 0.1)
function denoise (x threshold) (pvoc x (list (list "threshold" threshold)))
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

# --- onsets ---------------------------------------------------------------------------
# (onset-strength x n hop)   the spectral flux of every frame, as a vector (one value per hop)
function onset-strength (x n hop) {
    var mags (stft-magnitudes (stft x n hop))
    var flux (list 0)
    for (var k 1) (< k (length mags)) (var k (+ k 1)) {
        push flux (spectral-flux (getidx mags k) (getidx mags (- k 1)))
    }
    return (vec flux)
}
# (onsets x sr n hop threshold)   onset times in seconds, always starting with 0 (so the segments cover the
#                          sound from its beginning): then the peaks of the spectral flux (local maxima) above
#                          threshold x the flux's maximum, at least n samples apart from the previous onset, each
#                          placed at the sample where the energy jumps inside its frame (64-sample blocks: the
#                          block with the largest rise), so a hit is found where it starts, not a window early
function onsets (x sr n hop threshold) {
    var flux (onset-strength x n hop)
    return (onsets-of-flux x sr n hop flux (+ (zeros (length flux)) (* threshold (max flux))))
}
# (onsets-adaptive x sr n hop threshold width)   the same with a threshold that follows the material: a peak
#                          counts when it exceeds threshold x the moving median of the flux over width frames
#                          (odd, e.g. 9); for sounds whose loudness drifts, where a global level misses the
#                          quiet hits or admits the loud sustain
function onsets-adaptive (x sr n hop threshold width) {
    var flux (onset-strength x n hop)
    if (< (length flux) 3) { return (vec 0) }
    return (onsets-of-flux x sr n hop flux (* threshold (+ (median-filter flux width) (* 0.01 (max flux)))))
}
# (onsets-of-flux x sr n hop flux level)   the shared picker: peaks of flux above level (a vector), spaced and refined
function onsets-of-flux (x sr n hop flux level) {
    var out (list 0)
    if (< (length flux) 3) { return (vec out) }
    var last 0
    each (vec->list (local-maxima flux)) (function (k) {
        if (> (getidx flux k) (getidx level k)) {
            var sample (onset-refine x (* k hop) n)
            if (>= (- sample last) n) {
                push out (/ sample sr)
                set last sample
            }
        }
    })
    return (vec out)
}
# (onset-refine x start n)   the sample within [start, start + n) where the energy rises most (64-sample blocks)
function onset-refine (x start n) {
    var blk 64
    var seg (slice x start (min n (- (length x) start)))
    if (< (length seg) (* 2 blk)) { return start }
    var e (envelope-follow seg blk)
    var d (diff e)
    if (<= (max d) 0) { return start }
    return (+ start (* blk (+ 1 (argmax d))))         # d[j] = e[j+1] - e[j]: the rise is in block j+1
}
# (segments x sr times)    the pieces of x between consecutive onset times (and the end), as a list
function segments (x sr times) {
    var starts (vec->list (floor (* times sr)))
    var out (list)
    for (var k 0) (< k (length starts)) (var k (+ k 1)) {
        var a (getidx starts k)
        var b (if (< (+ k 1) (length starts)) (getidx starts (+ k 1)) (length x))
        push out (slice x a (- b a))
    }
    return out
}

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
    if (or (> (length f0) 1) (> (length q) 1) (> (length gain-db) 1)) { error "biquad: a moving cutoff, q or gain is a streaming feature (a synth); offline they are single numbers" }
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
# (bpf start segments)     break-point function: piecewise linear segments as one vector; segments is a
#                          list of (list length end): (bpf 0 (list (list 4 1) (list 4 0))) rises then falls.
#                          Each segment's end value is excluded (it starts the next one).
function bpf (start segments) {
    var out (vec)
    var cur start
    each segments (function (seg) {
        var len (head seg)
        var end (last seg)
        if (< len 1) { error "bpf: segment length must be >= 1" }
        set out (vec out (+ cur (* (range len) (/ (- end cur) len))))
        set cur end
    })
    return out
}
# (envelope-follow x n)    rms over hops of n samples, one value per hop
function envelope-follow (x n) {
    var out (list)
    for (var pos 0) (<= (+ pos n) (length x)) (var pos (+ pos n)) { push out (rms (slice x pos n)) }
    return (vec out)
}
# (envelope-from-values v hop)   piecewise-linear signal through successive values, hop samples apart
function envelope-from-values (v hop) (bpf (head v) (map (vec->list (drop v 1)) (function (e) (list hop e))))

# --- spatial: stereo, speaker rings, ambisonics, binaural --------------------------------
# Conventions (AmbiX): azimuth in degrees, 0 in front, positive to the left; elevation positive up.
# A multichannel signal is a list of channel vectors (what write-wav and play take).
# (pan-azimuth x az)       a mono signal to (list left right), equal power from its azimuth (-90 right .. 90 left;
#                          behind is folded onto the front)
function pan-azimuth (x az) {
    var a (- 0 (max -90 (min 90 (if (> az 90) (- 180 az) (if (< az -90) (- -180 az) az)))))   # to pan's -1..1: left is negative
    return (pan x (/ a 90))
}
# (speaker-ring n)         n speakers evenly around, as azimuths starting in front and going left
function speaker-ring (n) (- (* (range n) (/ 360 n)) (* 360 (floor (/ (* (range n) (/ 360 n)) 180.0001))))
# (pan-n x speakers az)    a mono signal onto a horizontal ring of speakers (a list of azimuths) by amplitude
#                          panning between the two nearest speakers (VBAP in 2D); => a list of n channels
function pan-n (x speakers az) {
    var n (length speakers)
    var best (map speakers (function (s) (abs (princarg-deg (- az s)))))
    var i1 (argmin (vec best))
    var d1 (getidx best i1)
    var others (map (vec->list (range n)) (function (k) (if (== k i1) 1e9 (getidx best k))))
    var i2 (argmin (vec others))
    var d2 (getidx others i2)
    var g1 (if (< (+ d1 d2) 1e-9) 1 (/ d2 (+ d1 d2)))
    var g2 (- 1 g1)
    var norm (sqrt (+ (* g1 g1) (* g2 g2)))
    return (map (vec->list (range n)) (function (k) (* x (if (== k i1) (/ g1 norm) (if (== k i2) (/ g2 norm) 0)))))
}
# (princarg-deg a)         an angle wrapped to -180..180
function princarg-deg (a) (- a (* 360 (round (/ a 360))))
# (ambi-encode x order az el)   a mono signal at a direction as B-format: (order+1)^2 channels (ACN, SN3D)
function ambi-encode (x order az el) (map (vec->list (ambi-gains order az el)) (function (g) (* g x)))
# (ambi-add a b)           two B-format signals of the same order summed
function ambi-add (a b) (map (zip a b) (function (p) (+ (head p) (last p))))
# (ambi-decode B speakers)   B-format to a list of speakers, each (list az el) or a bare azimuth, by sampling
#                          the sound field at every speaker direction (the basic decoder), with max-rE weights
#                          per order for a smoother field off-centre
function ambi-decode (B speakers) {
    var order (- (round (sqrt (length B))) 1)
    var w (max-re-weights order)
    var sps (if (equal? (type speakers) "vec") (vec->list speakers) speakers)
    return (map sps (function (sp) {
        var az (if (equal? (type sp) "list") (head sp) sp)
        var el (if (equal? (type sp) "list") (last sp) 0)
        var g (* (ambi-gains order az el) w)
        var acc (* 0 (head B))
        each (zip (vec->list g) B) (function (p) (set acc (+ acc (* (head p) (last p)))))
        return (/ acc (length sps))
    }))
}
# (max-re-weights order)   the per-channel max-rE weights of an order (Zotter & Frank): concentrates the energy vector
function max-re-weights (order) {
    var out (list)
    for (var n 0) (<= n order) (var n (+ n 1)) {
        var wn (cos (/ (* n pi) (+ (* 2 order) 2)))
        each (range (+ (* 2 n) 1)) (function (k) (push out wn))
    }
    return (vec out)
}
# (binaural x az el sr)    a mono signal at a direction for headphones: convolved with the two ears' impulse
#                          responses of a spherical head (hrir); => (list left right)
function binaural (x az el sr) {
    var h (hrir az el sr)
    return (list (conv x (head h)) (conv x (last h)))
}
# (channel sig k)          one channel of a multichannel signal (a list of vectors); a vector is its own channel
function channel (sig k) (if (equal? (type sig) "list") (getidx sig k) sig)
# (stereo-add a b) (stereo-scale a g)   stereo signals summed, scaled (written with channel, so they stream)
function stereo-add (a b) (list (+ (channel a 0) (channel b 0)) (+ (channel a 1) (channel b 1)))
function stereo-scale (a g) (list (* g (channel a 0)) (* g (channel a 1)))
# (binaural-decode B sr)   B-format for headphones: decoded to eight virtual speakers around the listener
#                          and two above, each rendered binaurally with the spherical head, summed;
#                          => (list left right). One expression, so it streams in a synth too.
var binaural-speakers (list (list 0 0) (list 45 0) (list 90 0) (list 135 0) (list 180 0) (list -135 0) (list -90 0) (list -45 0) (list 90 45) (list -90 45))
function bin-feed (feeds k az el sr) (list (conv (channel feeds k) (head (hrir az el sr))) (conv (channel feeds k) (last (hrir az el sr))))
function binaural-feeds (f sr) (stereo-scale (stereo-add (stereo-add (stereo-add (stereo-add (bin-feed f 0 0 0 sr) (bin-feed f 1 45 0 sr)) (stereo-add (bin-feed f 2 90 0 sr) (bin-feed f 3 135 0 sr))) (stereo-add (stereo-add (bin-feed f 4 180 0 sr) (bin-feed f 5 -135 0 sr)) (stereo-add (bin-feed f 6 -90 0 sr) (bin-feed f 7 -45 0 sr)))) (stereo-add (bin-feed f 8 90 45 sr) (bin-feed f 9 -90 45 sr))) 1.35)
function binaural-decode (B sr) (binaural-feeds (ambi-decode B binaural-speakers) sr)
# (normalize-peak-stereo s)   a stereo (or any multichannel) signal scaled so its loudest sample is 1
function normalize-peak-stereo (s) {
    var peak (max-of (map s (function (c) (max (abs c)))))
    return (map s (function (c) (/ c (max peak 1e-9))))
}
# (ambi-render sources order)   several (list x az el) placed and summed as one B-format signal
function ambi-render (sources order) {
    var acc nil
    each sources (function (s) {
        var b (ambi-encode (head s) order (getidx s 1) (last s))
        set acc (if (equal? (type acc) "nil") b (ambi-add acc b))
    })
    return acc
}
# (moving-source x order az-curve el-curve blocks)   a signal whose direction changes: encoded block by block
#                          (blocks samples each) at the azimuth and elevation curves (vectors, one value per block)
function moving-source (x order azs els blocks) {
    var nb (length azs)
    var out (map (vec->list (range (* (+ order 1) (+ order 1)))) (function (k) (zeros (length x))))
    each (range nb) (function (b) {
        var seg (slice x (* b blocks) blocks)
        if (> (length seg) 0) {
            var enc (ambi-encode (* seg (hann-fade seg)) order (getidx azs b) (getidx els b))
            each (zip out enc) (function (p) (add-at! (head p) (* b blocks) (last p)))
        }
    })
    return out
}
function hann-fade (seg) (ones (length seg))

# --- source separation by NMF --------------------------------------------------------------
# (nmf-separate x n hop k iterations)   x split into k sources: NMF of its magnitude spectrogram
#                          (V = magnitudes, bins x frames) into k parts, then a Wiener mask per part
#                          (its share of W H at every bin and frame) applied to the complex spectra
#                          and inverted; => (list (list source-1 ... source-k) W H). The sources add
#                          up to the input. Which part is which sound is for the caller to decide
#                          (their spectral shapes W and activations H say).
function nmf-separate (x n hop k iterations) {
    var frames (stft (vec (zeros n) x (zeros n)) n hop)                    # padded, so the ends are covered by full windows
    var V (nmf-spectrogram frames n)
    var fac (nmf V k iterations)
    var W (head fac)
    var H (last fac)
    var groups (map (vec->list (range k)) (function (j) (list j)))         # every part its own source
    return (list (nmf-sources x frames n hop W H groups) W H)
}
# (nmf-spectrogram frames n)   the magnitude spectrogram of stft frames as a bins x frames matrix
function nmf-spectrogram (frames n) (transpose (map frames (function (f) (take (magnitudes f) (+ (/ n 2) 1)))))
# (nmf-sources x frames n hop W H groups)   the sources of a factorization: groups is a list of lists of part
#                          indices, one source per group (a source may be several parts); Wiener masks, ISTFT
function nmf-sources (x frames n hop W H groups) {
    var bins (+ (/ n 2) 1)
    var WH (mat-shift (mat-mul W H) 1e-9)
    return (map groups (function (grp) {
        var part (mat-fill bins (ncols H) 0)
        each grp (function (j) (set part (mat-add part (mat-mul (transpose (list->mat (list (mat-col W j)))) (list->mat (list (getidx H j)))))))
        var mask (transpose (mat-div part WH))                            # frames x bins
        var masked (map (zip frames mask) (function (p) {
            var m (last p)
            var full (vec m (reverse (slice m 1 (- bins 2))))             # the mask mirrored onto the negative frequencies
            return (list (* (head (head p)) full) (* (last (head p)) full))
        }))
        var y (drop (istft masked n hop) n)
        return (take (vec y (zeros (max 0 (- (length x) (length y))))) (length x))
    }))
}
# (nmf-learn-parts examples n hop k iterations)   supervised separation, step one: for each example sound
#                          (a list of vectors, one per source) learn k spectral parts by NMF; => W with
#                          k columns per source, and the groups (which columns belong to which source)
function nmf-learn-parts (examples n hop k iterations) {
    var Ws (map examples (function (ex) (head (nmf (nmf-spectrogram (stft (vec (zeros n) ex (zeros n)) n hop) n) k iterations))))
    var W (transpose (reduce Ws (function (acc w) (concat-list acc (transpose w))) (list)))
    var groups (map (vec->list (range (length examples))) (function (s) (vec->list (+ (* s k) (range k)))))
    return (list W groups)
}
# (nmf-separate-with x n hop W groups iterations)   supervised separation, step two: the mix x against known
#                          parts W (nmf-with-parts learns only the activations), one source per group
function nmf-separate-with (x n hop W groups iterations) {
    var frames (stft (vec (zeros n) x (zeros n)) n hop)
    var H (nmf-with-parts (nmf-spectrogram frames n) W iterations)
    return (list (nmf-sources x frames n hop W H groups) H)
}
