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
check (equal? (delay (vec 1 2 3 4) 1.5) (vec 0 0.5 1.5 2.5)) "delay: fractional (interpolates from silence too)"
check (contains? (error-of (function () (delay (vec 1) -1))) ">= 0") "delay: negative"

# --- phase vocoder ---
check (equal? (local-maxima (vec 0 1 0 2 3 2 0)) (vec 1 4)) "local-maxima"
check (equal? (local-maxima (vec 1 2 3)) (vec)) "local-maxima: none interior"
check (equal? (gather (vec 10 20 30) (vec 2 0 -1)) (vec 30 10 30)) "gather"
check (contains? (error-of (function () (gather (vec 1) (vec 5)))) "out of range") "gather: range"
check (near? (princarg (vec 0 3.5 -3.5 7)) (vec 0 (- 3.5 tau) (- tau 3.5) (- 7 tau)) 1e-12) "princarg"
var tone (osc sr (+ (zeros 8000) 220) (gen 1024 (vec 1 0.5 0.3)))
var st (pvoc-stretch tone 2)
check (near? (length st) (* 2 (length tone)) 300) "pvoc-stretch: about twice as long"
check (near? (acf-f0 (slice st 6000 2048) sr) (acf-f0 (slice tone 2000 2048) sr) 2) "pvoc-stretch: pitch kept"
check (near? (slice (pvoc-stretch tone 1) 2048 4000) (slice tone 2048 4000) 1e-9) "pvoc-stretch: factor 1 is the identity"
check (near? (rms (slice st 4000 8000)) (rms tone) 0.1) "pvoc-stretch: level kept"
var st4 (pvoc-stretch tone 4)
var env4 (envelope-follow (slice st4 4000 24000) 2000)
check (< (/ (- (max env4) (min env4)) (mean env4)) 0.05) "pvoc-stretch x4: no amplitude modulation (fixed synthesis hop)"
check (near? (length (pvoc-pitch tone 1.5)) (length tone) 300) "pvoc-pitch: same length"
check (near? (acf-f0 (slice (pvoc-pitch tone 1.5) 2000 2048) sr) 333.3 3) "pvoc-pitch: a fifth up"
check (near? (acf-f0 (slice (pvoc-pitch tone 0.5) 2000 2048) sr) 111 2) "pvoc-pitch: an octave down"
check (near? (length (pvoc tone (list (list "stretch" (list 1 3))))) (* 1.5 (length tone)) 400) "pvoc: a ramped stretch 1->3 (the hop ramps linearly, as in sparkle: 1.5x overall)"
check (equal? (pvoc (vec) (list)) (vec)) "pvoc: empty input"
check (contains? (error-of (function () (pvoc tone (list (list "nope" 1))))) "unknown option") "pvoc: unknown option"
check (contains? (error-of (function () (pvoc tone (list (list "cross" (list 2 0.5 tone)))))) "needs an envelope") "pvoc: cross mode 2 needs an envelope"
var voiced (bandpass (osc sr (+ (zeros 8000) 200) (gen 512 (ones 12))) sr 900 3)
function formant-bin (v) (argmax (take (spectral-envelope (magnitudes (fft (* (slice v 2000 1024) (hann 1024)))) 20) 512))
function bin-mag (v k) (getidx (magnitude-spectrum (* (slice v 2000 2048) (hann 2048))) k)
var shifted (pvoc-pitch-formant voiced 1.5 100)
check (> (bin-mag shifted 77) (* 10 (+ 1e-9 (bin-mag shifted 51)))) "pvoc-pitch-formant: harmonics moved to 300 Hz (bin 77), none left at 200 (bin 51)"
check (< (abs (- (formant-bin shifted) (formant-bin voiced))) 15) "pvoc-pitch-formant: formant stayed"
check (> (abs (- (formant-bin (pvoc-pitch voiced 1.5)) (formant-bin voiced))) 15) "pvoc-pitch: formant moves without preservation"
check (< (formant-bin (pvoc-formants voiced 0.5 100)) (* 0.7 (formant-bin voiced))) "pvoc-formants: formants move down, pitch kept"
check (near? (acf-f0 (slice (pvoc-formants voiced 0.5 100) 2000 2048) sr) 200 3) "pvoc-formants: pitch kept"
check (equal? (gate-spectrum (vec 1 0.05 0.5) 0.1) (vec 1 0 0.5)) "gate-spectrum"
check (near? (length (denoise tone 0.05)) (length tone) 300) "denoise: length"
check (< (rms (denoise (* 0.01 (noise 8000)) 0.5)) 0.002) "denoise: a quiet noise below the threshold is removed"
check (near? (length (robotize tone)) (length tone) 300) "robotize: length"
check (near? (length (whisperize tone)) (length tone) 300) "whisperize: length"
var m0 (spectral-morph tone voiced 0 1024 256)
var m1 (spectral-morph tone voiced 1 1024 256)
check (near? (slice m0 2048 2000) (slice tone 2048 2000) 1e-9) "spectral-morph: t = 0 is the first sound"
check (near? (slice m1 2048 2000) (slice voiced 2048 2000) 1e-9) "spectral-morph: t = 1 is the second"
check (== (length (spectral-morph tone voiced 0.5 1024 256)) (length m0)) "spectral-morph: halfway"
var xs (pvoc-cross voiced tone 2 1 100)
check (near? (length xs) (length voiced) 300) "pvoc-cross: length"
check (> (rms xs) 0) "pvoc-cross mode 2: something comes out"
check (> (rms (pvoc-cross voiced tone 1 0.5 100)) 0) "pvoc-cross mode 1"
check (near? (slice (pvoc-cross voiced tone 3 0 100) 4096 2000) (slice (pvoc voiced (list)) 4096 2000) 1e-9) "pvoc-cross mode 3 at 0 is the first sound"

# --- hpss and onsets ---
var tone2 (* 0.5 (sin (* tau 220 (/ (range 16000) sr))))
var clicks2 (mix (map (list 0 4000 8000 12000) (function (p) (list p (* 0.8 (noise 200))))))
var mixed2 (+ tone2 (vec clicks2 (zeros (- 16000 (length clicks2)))))
var hp (hpss mixed2 512 128 17)
check (== (length (head hp)) 16000) "hpss: outputs as long as the input"
check (> (/ (dot (head hp) tone2) (* (norm (head hp)) (norm tone2))) 0.98) "hpss: the harmonic part is the tone"
check (> (rms (slice (last hp) 4000 200)) (* 20 (+ 1e-9 (rms (slice (last hp) 2000 1000))))) "hpss: the percussive part is the clicks"
check (< (max (abs (- (+ (head hp) (last hp)) mixed2))) 0.02) "hpss: the parts add up to the input (soft masks sum to one)"
check (contains? (error-of (function () (hpss mixed2 500 128 17))) "power of 2") "hpss: n must be a power of 2"
check (contains? (error-of (function () (hpss mixed2 512 128 2))) ">= 3") "hpss: kernel"
var on (onsets mixed2 sr 512 128 0.3)
check (== (length on) 4) "onsets: 0 first, then the three later clicks"
check (near? on (vec 0 0.5 1.0 1.5) 0.01) "onsets: at the samples where the hits start (refined inside the frame)"
var close-clicks (+ tone2 (vec (zeros 4000) (* 0.8 (noise 200)) (zeros 100) (* 0.8 (noise 200)) (zeros 11500)))
check (== (length (onsets close-clicks sr 512 128 0.3)) 2) "onsets: a second peak within n samples of the previous onset is not a new onset"
check (and (>= (length (onsets mixed2 sr 512 128 0.98)) 2) (< (length (onsets mixed2 sr 512 128 0.98)) 4)) "onsets: the threshold is relative to the flux maximum (0.98 keeps the strongest peak(s) only, after 0)"
check (== (length (onset-strength mixed2 512 128)) (length (stft mixed2 512 128))) "onset-strength: one value per frame"
check (equal? (onsets (zeros 4000) sr 512 128 0.3) (vec 0)) "onsets: silence has only the start"
var quiet-loud (+ tone2 (vec (zeros 4000) (* 0.1 (noise 200)) (zeros 3800) (* 0.9 (noise 200)) (zeros 7800)))
check (== (length (onsets quiet-loud sr 512 128 0.3)) 2) "onsets: a global threshold misses the quiet hit"
check (== (length (onsets-adaptive quiet-loud sr 512 128 3 9)) 3) "onsets-adaptive: the local median finds it"
check (equal? (onsets-adaptive (zeros 4000) sr 512 128 3 9) (vec 0)) "onsets-adaptive: silence"
check (== (length (segments mixed2 sr on)) 4) "segments: one per onset, from 0 to the end"
check (== (sum (vec (map (segments mixed2 sr on) length))) 16000) "segments: tile the whole sound"

# --- nmf separation ---
seed 3
var env1 (bpf 0 (list (list 4000 1) (list 4000 0) (list 4000 1) (list 4000 0)))
var env2 (bpf 1 (list (list 8000 0) (list 8000 1)))
var pa (* env1 (+ (sin (* tau 220 (/ (range 16000) sr))) (* 0.5 (sin (* tau 440 (/ (range 16000) sr))))))
var pb (* env2 (+ (sin (* tau 330 (/ (range 16000) sr))) (* 0.5 (sin (* tau 990 (/ (range 16000) sr))))))
var sep (nmf-separate (+ pa pb) 512 128 2 60)
var srcs (head sep)
check (== (length srcs) 2) "nmf-separate: k sources"
check (== (length (head srcs)) 16000) "nmf-separate: as long as the input"
check (< (max (abs (- (+ (head srcs) (last srcs)) (+ pa pb)))) 1e-6) "nmf-separate: the sources add up to the input"
function corr (p q) (/ (dot p q) (* (norm p) (norm q)))
var c1 (max (corr (head srcs) pa) (corr (last srcs) pa))
var c2 (max (corr (head srcs) pb) (corr (last srcs) pb))
check (and (> c1 0.8) (> c2 0.8)) "nmf-separate: each source matches one of the two mixed sounds"
check (equal? (mat-shape (getidx sep 1)) (list 257 2)) "nmf-separate: W is bins x k"
check (== (nrows (getidx sep 2)) 2) "nmf-separate: H is k x frames"
var learnt (nmf-learn-parts (list pa pb) 512 128 2 40)
check (equal? (mat-shape (head learnt)) (list 257 4)) "nmf-learn-parts: two parts per example, side by side"
check (equal? (last learnt) (list (list 0 1) (list 2 3))) "nmf-learn-parts: the groups"
var sup (nmf-separate-with (+ pa pb) 512 128 (head learnt) (last learnt) 40)
check (== (length (head sup)) 2) "nmf-separate-with: one source per group"
check (and (> (corr (head (head sup)) pa) 0.85) (> (corr (last (head sup)) pb) 0.85)) "nmf-separate-with: supervised, each source is its instrument"
check (< (max (abs (- (+ (head (head sup)) (last (head sup))) (+ pa pb)))) 1e-6) "nmf-separate-with: the sources add up to the mix"
check (== (nrows (last sup)) 4) "nmf-separate-with: the activations, one row per part"

# --- spatial ---
check (equal? (fixed (ambi-gains 1 0 0) 6) (vec 1 0 0 1)) "ambi-gains: front is (W 0 0 X)"
check (equal? (fixed (ambi-gains 1 90 0) 6) (vec 1 1 0 0)) "ambi-gains: left is Y"
check (equal? (fixed (ambi-gains 1 0 90) 6) (vec 1 0 1 0)) "ambi-gains: up is Z"
check (== (length (ambi-gains 3 20 10)) 16) "ambi-gains: (order+1)^2 channels"
check (near? (ambi-gains 1 30 20) (vec 1 (* (cos (/ (* 20 pi) 180)) (sin (/ (* 30 pi) 180))) (sin (/ (* 20 pi) 180)) (* (cos (/ (* 20 pi) 180)) (cos (/ (* 30 pi) 180)))) 1e-9) "ambi-gains: SN3D first order is (1 y z x)"
check (contains? (error-of (function () (ambi-gains 9 0 0))) "0..7") "ambi-gains: order bounds"
var sx (sine sr 440 0.05)
check (== (length (ambi-encode sx 2 30 0)) 9) "ambi-encode: second order, nine channels"
check (near? (getidx (ambi-rotate (ambi-encode sx 1 90 0) -90) 3) (getidx (ambi-encode sx 1 0 0) 3) 1e-9) "ambi-rotate: turning a left source by -90 puts it in front"
check (near? (getidx (ambi-rotate (ambi-encode sx 3 40 15) 25) 9) (getidx (ambi-encode sx 3 65 15) 9) 1e-9) "ambi-rotate: exact at third order"
check (contains? (error-of (function () (ambi-rotate (list sx sx sx) 10))) "(order+1)^2") "ambi-rotate: channel count"
check (equal? (speaker-ring 4) (vec 0 90 180 -90)) "speaker-ring"
var dec (vec (map (ambi-decode (ambi-encode sx 1 45 0) (speaker-ring 4)) rms))
check (and (near? (getidx dec 0) (getidx dec 1) 1e-6) (> (getidx dec 0) (* 2 (getidx dec 2)))) "ambi-decode: a source at 45 feeds the front and left speakers equally, the others less"
var pn (vec (map (pan-n sx (speaker-ring 8) 100) rms))
check (and (> (getidx pn 2) (getidx pn 3)) (== (getidx pn 0) 0) (== (getidx pn 5) 0)) "pan-n: between the two nearest speakers only"
check (near? (vec (map (pan-azimuth sx 90) rms)) (vec (rms sx) 0) 1e-6) "pan-azimuth: 90 is the left"
check (near? (vec (map (pan-azimuth sx -90) rms)) (vec 0 (rms sx)) 1e-6) "pan-azimuth: -90 is the right"
check (near? (vec (map (pan-azimuth sx 0) rms)) (* (rms sx) (vec 0.7071 0.7071)) 1e-3) "pan-azimuth: front is equal power"
var h (hrir 90 0 sr)
check (== (hrtf-loaded) 710) "hrtf: the bundled KEMAR set loads on first use (710 directions)"
check (== (length h) 2) "hrir: two ears"
check (== (length (head (hrir 30 0 44100))) 128) "hrir: 128 taps at 44100"
check (== (length (head (hrir 30 0 48000))) 139) "hrir: resampled to another rate"
check (> (rms (head h)) (* 2 (rms (last h)))) "hrir: a left source is louder at the left ear"
check (> (argmax (abs (last h))) (argmax (abs (head h)))) "hrir: and arrives later at the right ear"
var hm (hrir-model 90 0 sr)
check (near? (* 1000 (/ (- (argmax (abs (last hm))) (argmax (abs (head hm)))) sr)) 0.66 0.15) "hrir-model: the interaural delay is about 0.65 ms"
var hf (magnitude-spectrum (head (hrir 0 0 44100)))
var hb (magnitude-spectrum (head (hrir 180 0 44100)))
var fr (take (spectrum-freqs 128 44100) (length hf))
check (> (spectral-centroid hf fr) (* 1.15 (spectral-centroid hb fr))) "hrtf: behind is duller than in front (the pinna): a cue the model cannot give"
check (not (equal? (head (hrir 0 0 44100)) (head (hrir 0 60 44100)))) "hrtf: elevation changes the response"
hrtf-unload
check (== (hrtf-loaded) 0) "hrtf-unload: back to the model"
check (> (length (head (hrir 90 0 44100))) 128) "hrir: the model when nothing is loaded"
check (== (hrtf-load "hrtf_kemar.csv") 710) "hrtf-load: explicit"
check (contains? (error-of (function () (hrtf-load "nope.csv"))) "cannot open") "hrtf-load: missing file"
var click (vec (noise 400) (zeros 2000))
var bl (vec (map (binaural click 90 0 sr) rms))
check (> (getidx bl 0) (* 2 (getidx bl 1))) "binaural: a left click is much louder in the left ear"
var bf (vec (map (binaural-decode (ambi-encode sx 1 0 0) sr) rms))
check (and (near? (getidx bf 0) (getidx bf 1) 1e-6) (near? (getidx bf 0) (rms sx) 0.05)) "binaural-decode: a front source is equal and at its level"
var br (vec (map (binaural-decode (ambi-encode click 1 -90 0) sr) rms))
check (> (getidx br 1) (* 1.5 (getidx br 0))) "binaural-decode: a right source is louder at the right ear"
check (== (length (moving-source sx 1 (vec 0 90 180) (vec 0 0 0) 800)) 4) "moving-source: B-format"
check (equal? (channel (list sx (* 2 sx)) 1) (* 2 sx)) "channel"
check (equal? (channel sx 1) sx) "channel: a vector is its own channel"
check (== (max (head (normalize-peak-stereo (list (* 3 sx) sx)))) 1) "normalize-peak-stereo"

# --- resampling ---
var slow (sine sr 100 0.02)
check (== (length (resample slow 2)) 320) "resample: length"
check (near? (drop (take (resample slow 2) 300) 40) (drop (take (sine (* 2 sr) 100 0.02) 300) 40) 1e-3) "resample x2 matches the sine at the higher rate"
check (near? (drop (take (resample slow 1.5) 200) 40) (drop (take (sine (* 1.5 sr) 100 0.02) 200) 40) 1e-3) "resample x1.5: any ratio"
check (near? (slice (resample (ones 100) 0.7) 30 4) (ones 4) 1e-9) "resample: a constant stays a constant"
check (near? (acf-f0 (resample (sine sr 1000 0.5) 0.5) 4000) 1000 1) "resample: downsampling keeps a tone below the new Nyquist"
check (== (length (resample-to slow sr 4000)) 80) "resample-to"
check (contains? (error-of (function () (resample slow 0))) "> 0") "resample: factor"

# --- channels ---
check (equal? (interleave (list (vec 1 2) (vec 3 4))) (vec 1 3 2 4)) "interleave"
check (equal? (deinterleave (vec 1 3 2 4) 2) (list (vec 1 2) (vec 3 4))) "deinterleave"
check (contains? (error-of (function () (interleave (list (vec 1) (vec 1 2))))) "same length") "interleave: lengths"
check (contains? (error-of (function () (deinterleave (vec 1 2 3) 2))) "multiple") "deinterleave: length"

# --- envelopes ---
check (equal? (bpf 0 (list (list 4 1))) (vec 0 0.25 0.5 0.75)) "bpf: one segment, end excluded"
check (equal? (bpf 0 (list (list 2 1) (list 2 0))) (vec 0 0.5 1 0.5)) "bpf: two segments"
check (equal? (bpf 5 (list)) (vec)) "bpf: no segments"
check (contains? (error-of (function () (bpf 0 (list (list 0 1))))) ">= 1") "bpf: zero-length segment"
check (equal? (envelope-follow (vec 1 1 -1 -1 0 0) 2) (vec 1 1 0)) "envelope-follow"
check (equal? (envelope-from-values (vec 0 1 0) 2) (vec 0 0.5 1 0.5)) "envelope-from-values"

report "test_signals"
