# test_signals.mu — self-checking test of the signals library (signals.h + signals.mu).
#
# Every check is a (check ...) from test.mu. A passing run prints only the final line.
# Run with: musil tests/test_signals.mu

load "test.mu"
load "signals.mu"

function near? (a b eps) (< (max (abs (- a b))) eps)
var sr 8000

# --- generators ---
var s440 (sine sr 440 0.1)
check (== (length s440) 800) "sine: length"
check (near? (rms s440) 0.7071 1e-3) "sine: rms"
check (== (length (noise 100)) 100) "noise: length"
check (all? (noise 100) (function (x) (between? x -1 1))) "noise: range"
check (equal? (impulse 4) (vec 1 0 0 0)) "impulse"
var tbl (gen 16 (vec 1))
check (== (length tbl) 17) "gen: n+1 samples"
check (== (head tbl) (last tbl)) "gen: guard point"
check (near? (max tbl) 1 1e-12) "gen: normalised"
check (near? (getidx tbl 4) 1 1e-12) "gen: sine table peak at a quarter"
check (equal? (mix (list (list 0 (vec 1 1)) (list 3 (vec 2 2)))) (vec 1 1 0 2 2)) "mix"
check (equal? (add-at (zeros 3) 1 (vec 5 5 5)) (vec 0 5 5 5)) "add-at: grows"
check (equal? (add-at (ones 4) 1 (vec 1 1)) (vec 1 2 2 1)) "add-at: adds"
check (equal? (fade-in (ones 4) 2) (vec 0 0.5 1 1)) "fade-in"
check (equal? (fade-out (ones 4) 2) (vec 1 1 0.5 0)) "fade-out"
check (equal? (normalize-peak (vec 1 -4 2)) (vec 0.25 -1 0.5)) "normalize-peak"
check (equal? (normalize-peak (zeros 3)) (zeros 3)) "normalize-peak: silence"
check (near? (rms (normalize-rms s440 0.1)) 0.1 1e-12) "normalize-rms"
check (== (db 1) 0) "db"
check (near? (undb -6.0206) 0.5 1e-4) "undb"
check (near? (db (undb -20)) -20 1e-12) "db round trip"

# --- oscillator (C++) ---
var o (osc 8 (+ (zeros 8) 1) (vec 0 1 0 -1 0))
check (equal? o (vec 0 0.5 1 0.5 0 -0.5 -1 -0.5)) "osc: linear interpolation through the table"
check (near? (acf-f0 (osc sr (+ (zeros 1600) 330) (gen 512 (vec 1))) sr) 333.3 1) "osc: pitch"
var two (oscbank sr (list (ones 100) (* 0.5 (ones 100))) (list (+ (zeros 100) 100) (+ (zeros 100) 200)) tbl)
check (== (length two) 100) "oscbank: length"
check (contains? (error-of (function () (oscbank sr (list (ones 2)) (list) tbl))) "same length") "oscbank: arity"
check (contains? (error-of (function () (osc 8 (vec 1) (vec 1)))) "at least 2") "osc: table too short"

# --- windows and spectra ---
check (equal? (fixed (hann 4) 6) (vec 0 0.5 1 0.5)) "hann: periodic"
check (near? (hamming 8) (vec 0.08 0.2147 0.54 0.8653 1 0.8653 0.54 0.2147) 1e-3) "hamming"
check (near? (head (blackman 8)) 0 1e-12) "blackman: starts at zero"
var x (sin (* tau 3 (/ (range 16) 16)))
var S (fft x)
check (== (length (head S)) 16) "fft: (list re im), power of two"
check (== (length (head (fft (ones 100)))) 128) "fft: zero-pads to the next power of two"
check (near? (getidx (magnitudes S) 3) 8 1e-12) "fft: energy at bin 3"
check (near? (ifft S) x 1e-12) "ifft: round trip"
seed 1
var r (noise 1024)
check (near? (ifft (fft r)) r 1e-12) "fft/ifft: round trip on noise"
check (contains? (error-of (function () (ifft (list (ones 3) (ones 3))))) "power of two") "ifft: size"
check (contains? (error-of (function () (ifft (list (ones 4))))) "(list re im)") "ifft: shape"
var P (car->pol (list (vec 1 0) (vec 0 1)))
check (equal? (head P) (vec 1 1)) "car->pol: magnitude"
check (near? (last P) (vec 0 (/ pi 2)) 1e-12) "car->pol: phase"
check (near? (head (pol->car P)) (vec 1 0) 1e-12) "pol->car: re"
check (near? (last (pol->car P)) (vec 0 1) 1e-12) "pol->car: im"
check (equal? (spectrum-freqs 8 8000) (vec 0 1000 2000 3000)) "spectrum-freqs"
var ms (magnitude-spectrum (* (sine sr 1000 0.032) (hann 256)))
check (== (length ms) 128) "magnitude-spectrum: half the bins"
check (== (getidx (spectrum-freqs 256 sr) (argmax ms)) 1000) "magnitude-spectrum: peak at the sine's frequency"
check (near? (max (magnitude-spectrum (sine sr 1000 0.032))) 1 1e-9) "magnitude-spectrum: full-scale sine reads 1"
check (equal? (fixed (conv (vec 1 2 3) (vec 1 1)) 10) (vec 1 3 5 3)) "conv"
check (equal? (fixed (conv (vec 1 2) (vec 3)) 10) (vec 3 6)) "conv: scalar kernel"
check (== (length (conv (ones 100) (ones 50))) 149) "conv: length"
var cm (complex-mul (list (vec 0) (vec 1)) (list (vec 0) (vec 1)))
check (equal? (list (head cm) (last cm)) (list (vec -1) (vec 0))) "complex-mul: i * i = -1"

# --- cepstral envelope ---
var tone (osc sr (+ (zeros 1024) 200) (gen 512 (ones 12)))
var shaped (magnitudes (fft (* (bandpass tone sr 900 3) (hann 1024))))
var picks (function (m) (vec (map (list 26 51 77 102 128 154) (function (k) (getidx m k)))))
var env (spectral-envelope shaped 20)
check (== (length env) 1024) "spectral-envelope: full length"
check (< (spectral-irregularity (take env 512)) (* 0.3 (spectral-irregularity (take shaped 512)))) "spectral-envelope: smoother than the spectrum"
check (< (abs (- (argmax (take env 512)) 115)) 40) "spectral-envelope: peaks near the formant"
check (> (min (picks (/ env shaped))) 0.1) "spectral-envelope: stays within an order of magnitude of the harmonic peaks"
check (< (/ (max (picks (/ env shaped))) (min (picks (/ env shaped)))) 3) "spectral-envelope: peak-to-envelope ratio is consistent across harmonics"
var flat (flatten-spectrum shaped 20)
check (< (stdev (log (picks flat))) (* 0.5 (stdev (log (picks shaped))))) "flatten-spectrum: harmonics level out"
check (near? (impose-envelope shaped shaped 20) shaped 1e-9) "impose-envelope: a spectrum given its own envelope is unchanged"
check (== (length (cepstrum (ones 8))) 8) "cepstrum: length"
check (near? (cepstrum (ones 8)) (zeros 8) 1e-9) "cepstrum: flat spectrum has zero cepstrum"

# --- stft ---
var sig (sine sr 200 0.1)
var frames (stft sig 256 64)
check (== (length frames) 9) "stft: frame count"
check (== (length (head (head frames))) 256) "stft: frame size"
var y (istft frames 256 64)
check (== (length y) (+ (* 64 8) 256)) "istft: length"
check (near? (slice y 256 300) (slice sig 256 300) 1e-12) "istft: exact reconstruction where frames fully overlap"
check (equal? (istft (list) 256 64) (vec)) "istft: no frames"
check (== (length (head (stft-magnitudes frames))) 128) "stft-magnitudes"

# --- features ---
var a (vec 0 1 0 0)
var f (vec 0 100 200 300)
check (== (spectral-centroid a f) 100) "spectral-centroid"
check (== (spectral-centroid (zeros 4) f) 0) "spectral-centroid: silence"
check (== (spectral-spread a f) 0) "spectral-spread: single line"
check (near? (spectral-spread (vec 1 1) (vec 0 100)) 50 1e-12) "spectral-spread"
check (== (spectral-skewness a f) 0) "spectral-skewness: degenerate"
check (near? (spectral-kurtosis (vec 1 1) (vec 0 100)) 1 1e-12) "spectral-kurtosis: two equal lines"
check (== (spectral-flux (vec 2 0 3) (vec 1 1 1)) 3) "spectral-flux: rectified"
check (== (spectral-irregularity (vec 1 3 2)) 3) "spectral-irregularity"
check (== (spectral-decrease (vec 1 1 1)) 0) "spectral-decrease: flat"
check (< (spectral-decrease (vec 4 2 1)) 0) "spectral-decrease: falling"
check (near? (spectral-flatness (ones 8)) 1 1e-9) "spectral-flatness: white"
check (< (spectral-flatness (vec 1 0 0 0)) 0.01) "spectral-flatness: tonal"
check (== (spectral-rolloff (vec 1 1 1 1) (vec 0 1 2 3) 0.5) 1) "spectral-rolloff"
check (near? (hfc (vec 0 0 1)) (/ 2 3) 1e-12) "hfc: sum of a^2 i over sum of i"
check (near? (energy s440) 0.7071 1e-3) "energy"
check (near? (zcr s440) 0.11 1e-3) "zcr: 440 Hz at 8 kHz crosses ~0.11 per sample"
check (== (zcr (ones 10)) 0) "zcr: no crossings"
check (near? (acf-f0 sig sr) 200 1e-9) "acf-f0: 200 Hz"
check (== (acf-f0 (noise 800) sr) 0) "acf-f0: noise is unvoiced"
check (== (acf-f0 (zeros 100) sr) 0) "acf-f0: silence"
check (== (length (autocorr (ones 10))) 5) "autocorr: n/2 lags"
check (contains? (error-of (function () (autocorr (vec 1 2)))) "too short") "autocorr: short"

# --- filters ---
var lp (biquad "lowpass" sr 1000 0.707 0)
check (== (length (head lp)) 3) "biquad: b"
check (== (length (last lp)) 3) "biquad: a"
check (near? (sum (head lp)) (sum (last lp)) 1e-12) "biquad lowpass: unity gain at DC"
var hp (biquad "highpass" sr 1000 0.707 0)
check (near? (sum (head hp)) 0 1e-12) "biquad highpass: zero gain at DC"
check (contains? (error-of (function () (biquad "weird" sr 1 1 0))) "unknown type") "biquad: unknown type"
check (equal? (iir (vec 1 0 0 0) (vec 1) (vec 1 -0.5)) (vec 1 0.5 0.25 0.125)) "iir: one-pole decay"
check (equal? (iir (vec 1 0 0) (vec 2 2) (vec 2)) (vec 1 1 0)) "iir: a0 scaling"
check (contains? (error-of (function () (iir (vec 1) (vec 1) (vec 0)))) "nonzero") "iir: a0 zero"
var n1 (noise 4000)
var freqs (spectrum-freqs 4096 sr)
function centroid-of (v) (spectral-centroid (magnitude-spectrum v) freqs)
check (< (centroid-of (lowpass n1 sr 300 0.707)) (* 0.5 (centroid-of n1))) "lowpass lowers the centroid"
check (> (centroid-of (highpass n1 sr 2000 0.707)) (* 1.3 (centroid-of n1))) "highpass raises the centroid"
var notched (notch (sine sr 1000 0.5) sr 1000 5)
check (< (rms (drop notched 2000)) (* 0.1 (rms (sine sr 1000 0.5)))) "notch removes its frequency"
check (near? (sum (head (biquad "peak" sr 1000 1 0))) (sum (last (biquad "peak" sr 1000 1 0))) 1e-12) "peak with 0 dB is flat at DC"
check (equal? (take (comb (impulse 8) 2 0.5) 6) (vec 1 0 0.5 0 0.25 0)) "comb"
check (equal? (fixed (take (allpass (impulse 6) 2 0.5) 4) 6) (vec -0.5 0 0.75 0)) "allpass"
var dc (dc-block (+ (zeros 400) 1))
check (< (abs (last dc)) 0.15) "dc-block: constant decays"
check (== (head dc) 1) "dc-block: passes the step edge"
var ring (reson (impulse 10) sr 440 0.05)
check (== (length ring) 400) "reson: lasts tau seconds"
check (near? (acf-f0 (take ring 400) sr) 440 20) "reson: rings at its frequency"
var rev (schroeder-reverb (impulse 100) sr 0.5)
check (== (length rev) (+ 100 4000)) "schroeder-reverb: input plus rt60 seconds"
var early (db (rms (slice rev 100 800)))
var late (db (rms (slice rev 3300 800)))
check (< late (- early 30)) "schroeder-reverb: the tail decays"
check (> late -120) "schroeder-reverb: but is still there at the end"
check (equal? (delay (vec 1 2 3 4) 1) (vec 0 1 2 3)) "delay: integer"
check (equal? (delay (vec 1 2 3 4) 1.5) (vec 0 0 1.5 2.5)) "delay: fractional"
check (contains? (error-of (function () (delay (vec 1) -1))) ">= 0") "delay: negative"

# --- resampling ---
var slow (sine sr 100 0.02)
check (== (length (resample slow 2)) 320) "resample: length"
check (near? (drop (take (resample slow 2) 300) 20) (drop (take (sine (* 2 sr) 100 0.02) 300) 20) 0.02) "resample x2 matches the sine at the higher rate"
check (== (length (resample-to slow sr 4000)) 80) "resample-to"
check (contains? (error-of (function () (resample slow 0))) "> 0") "resample: factor"

# --- channels ---
check (equal? (interleave (list (vec 1 2) (vec 3 4))) (vec 1 3 2 4)) "interleave"
check (equal? (deinterleave (vec 1 3 2 4) 2) (list (vec 1 2) (vec 3 4))) "deinterleave"
check (contains? (error-of (function () (interleave (list (vec 1) (vec 1 2))))) "same length") "interleave: lengths"
check (contains? (error-of (function () (deinterleave (vec 1 2 3) 2))) "multiple") "deinterleave: length"

# --- envelopes ---
check (equal? (envelope-follow (vec 1 1 -1 -1 0 0) 2) (vec 1 1 0)) "envelope-follow"
check (equal? (envelope-from-values (vec 0 1 0) 2) (vec 0 0.5 1 0.5)) "envelope-from-values"

report "test_signals"
