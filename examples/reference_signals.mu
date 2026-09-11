# musil — library reference: signals (signals.h + signals.mu)
#
# Offline signal processing on vectors. A signal is a vector of samples; a
# complex spectrum is (list re im); a polar one is (list mag phase); a stereo
# buffer is (list left right), as read-wav returns it. signals.mu loads
# scientific.mu (and so std.mu). The FFT, the oscillator, the IIR filter,
# delay, resampling, autocorrelation and channel (de)interleaving are C++;
# everything else is vector arithmetic in Musil.
# Run with: musil reference_signals.mu

load "signals.mu"
print ""
print "================================================================"
print "  musil: library reference (signals)"
print "================================================================"
var sr 8000

# --- 1. Generators ----------------------------------------------------------
print ""
print "--- generators ---"
var s (sine sr 440 0.01)
print "sine sr 440 0.01 :" (length s) "samples, rms" (fixed (rms s) 4)
seed 7
print "noise 5          :" (fixed (noise 5) 3)
print "impulse 5        :" (impulse 5)
print "gen 8 (1)        :" (fixed (gen 8 (vec 1)) 3) "(one period plus a guard point)"
print "gen 8 (1 0.5)    :" (fixed (gen 8 (vec 1 0.5)) 3) "(two harmonics)"
print "osc              :" (fixed (osc 8 (+ (zeros 8) 1) (gen 8 (vec 1))) 3) "(1 Hz at 8 Hz, table lookup)"
print "oscbank          :" (length (oscbank sr (list (ones 80) (ones 80)) (list (+ (zeros 80) 200) (+ (zeros 80) 300)) (gen 512 (vec 1))))
print "mix              :" (mix (list (list 0 (vec 1 1)) (list 3 (vec 2 2)))) "(layers at positions)"
print "add-at           :" (add-at (zeros 3) 1 (vec 5 5 5))
print "fade-in, fade-out:" (fade-in (ones 4) 2) (fade-out (ones 4) 2)
print "normalize-peak   :" (normalize-peak (vec 1 -4 2))
print "normalize-rms    :" (fixed (rms (normalize-rms s 0.1)) 3)
print "db, undb         :" (db 0.5) (undb -6)

# --- 2. Windows and spectra --------------------------------------------------
print ""
print "--- windows and spectra ---"
print "hann 8           :" (fixed (hann 8) 3) "(periodic: overlap-adds exactly at hop n/4)"
print "hamming 8        :" (fixed (hamming 8) 3)
print "blackman 8       :" (fixed (blackman 8) 3)
var x (sin (* tau 3 (/ (range 16) 16)))
var S (fft x)
print "fft              : (list re im), re =" (fixed (head S) 3)
print "magnitudes       :" (fixed (magnitudes S) 3)
print "ifft round trip  :" (fixed (max (abs (- (ifft S) x))) 15)
print "car->pol         :" (car->pol (list (vec 1 0) (vec 0 1)))
print "pol->car         :" (mat-round (pol->car (list (vec 1 1) (vec 0 (/ pi 2)))) 12)
print "spectrum-freqs   :" (spectrum-freqs 8 sr)
var ms (magnitude-spectrum (* (sine sr 1000 0.032) (hann 256)))
print "magnitude-spectrum: peak at" (getidx (spectrum-freqs 256 sr) (argmax ms)) "Hz, height" (fixed (max ms) 3)
print "conv             :" (fixed (conv (vec 1 2 3) (vec 1 1)) 6)
print "complex-mul      :" (complex-mul (list (vec 0) (vec 1)) (list (vec 0) (vec 1))) "(i times i)"

# --- 3. Spectral envelope (cepstral) ------------------------------------------------
print ""
print "--- spectral envelope ---"
var tone (osc sr (+ (zeros 1024) 200) (gen 512 (ones 12)))
var shaped (magnitudes (fft (* (bandpass tone sr 900 3) (hann 1024))))
print "spectral-envelope: peak bin of the spectrum" (argmax (take shaped 512)) ", of its envelope (order 20)" (argmax (take (spectral-envelope shaped 20) 512))
print "flatten-spectrum : harmonics 1-4 raw" (fixed (vec (map (list 26 51 77 102) (function (k) (getidx shaped k)))) 1) "flattened" (fixed (vec (map (list 26 51 77 102) (function (k) (getidx (flatten-spectrum shaped 20) k)))) 2)
print "impose-envelope  : carrier reshaped by a source's envelope; cepstrum: the cepstrum itself"

# --- 4. STFT --------------------------------------------------------------------
print ""
print "--- stft ---"
var sig (sine sr 200 0.1)
var frames (stft sig 256 64)
print "stft             :" (length frames) "frames of 256, hop 64"
var y (istft frames 256 64)
print "istft            :" (length y) "samples, error where frames overlap fully:" (fixed (max (abs (- (slice y 256 300) (slice sig 256 300)))) 12)
print "stft-magnitudes  :" (length (head (stft-magnitudes frames))) "bins per frame"

# --- 4. Features ----------------------------------------------------------------
print ""
print "--- features (amps: positive-frequency magnitudes, freqs: their frequencies) ---"
var a (vec 1 0.5 0.25 0.1)
var f (vec 0 1000 2000 3000)
print "spectral-centroid    :" (fixed (spectral-centroid a f) 1)
print "spectral-spread      :" (fixed (spectral-spread a f) 1)
print "spectral-skewness    :" (fixed (spectral-skewness a f) 3)
print "spectral-kurtosis    :" (fixed (spectral-kurtosis a f) 3)
print "spectral-flux        :" (spectral-flux (vec 2 0 3) (vec 1 1 1)) "(rectified difference from the previous frame)"
print "spectral-irregularity:" (spectral-irregularity (vec 1 3 2))
print "spectral-decrease    :" (fixed (spectral-decrease a) 3)
print "spectral-flatness    :" (fixed (spectral-flatness (ones 8)) 3) (fixed (spectral-flatness (vec 1 0 0 0)) 3) "(white, tonal)"
print "spectral-rolloff 0.85:" (spectral-rolloff a f 0.85)
print "hfc                  :" (fixed (hfc a) 4)
print "rms, zcr             :" (fixed (rms s) 4) (fixed (zcr s) 3)
print "acf-f0               :" (fixed (acf-f0 sig sr) 1) "Hz;" (acf-f0 (noise 800) sr) "for noise"
print "autocorr             :" (fixed (take (autocorr (sine sr 1000 0.004)) 5) 3)

# --- 5. Filters -------------------------------------------------------------------
print ""
print "--- filters ---"
var lp (biquad "lowpass" sr 1000 0.707 0)
print "biquad lowpass   : b =" (fixed (head lp) 4) " a =" (fixed (last lp) 4)
print "types            : lowpass highpass bandpass notch peak lowshelf highshelf"
print "apply-filter     :" (fixed (take (apply-filter (impulse 8) lp) 6) 4)
print "iir one-pole     :" (iir (vec 1 0 0 0) (vec 1) (vec 1 -0.5))
var n1 (noise 4000)
var fr (spectrum-freqs 4096 sr)
function centroid-of (v) (fixed (spectral-centroid (magnitude-spectrum v) fr) 0)
print "centroid of noise:" (centroid-of n1) " lowpass 300:" (centroid-of (lowpass n1 sr 300 0.7)) " highpass 2000:" (centroid-of (highpass n1 sr 2000 0.7))
print "comb             :" (take (comb (impulse 8) 2 0.5) 6)
print "allpass          :" (fixed (take (allpass (impulse 6) 2 0.5) 4) 3)
print "dc-block         :" (fixed (take (dc-block (+ (zeros 5) 1)) 5) 3)
print "reson            :" (length (reson (impulse 10) sr 440 0.05)) "samples ringing at" (fixed (acf-f0 (reson (impulse 10) sr 440 0.05) sr) 0) "Hz"
var rev (schroeder-reverb (impulse 100) sr 1)
print "schroeder-reverb :" (length rev) "samples (input + rt60 seconds); level at 0.1 s and 0.9 s:" (fixed (db (rms (slice rev 800 400))) 1) (fixed (db (rms (slice rev 7200 400))) 1) "dB"
print "delay 1.5        :" (delay (vec 1 2 3 4) 1.5)

# --- 6. Phase vocoder ------------------------------------------------------------
print ""
print "--- phase vocoder ---"
var tone (osc sr (+ (zeros 4000) 220) (gen 512 (vec 1 0.5 0.3)))
print "local-maxima     :" (local-maxima (vec 0 1 0 2 3 2 0)) "  gather:" (gather (vec 10 20 30) (vec 2 0 -1))
print "princarg 7       :" (fixed (princarg 7) 4)
print "pvoc x opts      : one pass for everything; opts are (list key value), ramps as (list start end)"
var st (pvoc-stretch tone 2)
print "pvoc-stretch x2  :" (length st) "samples from" (length tone) "; f0" (fixed (acf-f0 (slice st 3000 1024) sr) 1) "Hz, same as the original" (fixed (acf-f0 (slice tone 1000 1024) sr) 1)
print "pvoc-pitch x1.5  :" (length (pvoc-pitch tone 1.5)) "samples; f0" (fixed (acf-f0 (slice (pvoc-pitch tone 1.5) 1000 1024) sr) 1) "Hz"
print "pvoc-pitch-formant, pvoc-formants: with the cepstral envelope (order ~ sr/100)"
print "ramped           :" (length (pvoc tone (list (list "stretch" (list 1 3)) (list "pitch" (list 1 2))))) "samples, stretch 1->3 and pitch 1->2 at once"
print "pvoc-cross       : modes 1 (multiplicative), 2 (flattener), 3 (morph); denoise, robotize, whisperize"
print "robotize, whisperize, denoise: lengths" (length (robotize tone)) (length (whisperize tone)) (length (denoise tone 0.05))

# --- 7. Resampling and channels --------------------------------------------------
print ""
print "--- resampling and channels ---"
var slow (sine sr 100 0.02)
print "resample x2      :" (length (resample slow 2)) "samples"
print "resample-to 4000 :" (length (resample-to slow sr 4000)) "samples"
print "interleave       :" (interleave (list (vec 1 2) (vec 3 4)))
print "deinterleave     :" (deinterleave (vec 1 3 2 4) 2)

# --- 8. Envelopes ----------------------------------------------------------------
print ""
print "--- envelopes ---"
print "bpf 0 ((4 1) (4 0)):" (bpf 0 (list (list 4 1) (list 4 0))) "(an attack and a decay; each end starts the next segment)"
print "envelope-follow  :" (envelope-follow (vec 1 1 -1 -1 0 0) 2)
print "envelope-from-values:" (envelope-from-values (vec 0 1 0) 2)

print ""
print "================================================================"
print "  end of signals reference"
print "================================================================"
