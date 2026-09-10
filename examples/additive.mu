# additive: analyse a sound's spectrum and resynthesise it with a bank of oscillators
# Usage: musil additive.mu [sound.wav]   (defaults to the bundled data/ files)
load "system.mu"
load "signals.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/gong_c_sharp.wav"))
var sr (head w)
var x (head (getidx w 1))
var seconds (min 6 (/ (length x) sr))
var samples (floor (* seconds sr))

# one long FFT of the whole sound gives its average spectrum
var m (magnitude-spectrum (* x (hann (length x))))
var freqs (spectrum-freqs (* 2 (length m)) sr)
print "spectrum:" (length m) "bins," (fixed (/ sr (* 2 (length m))) 2) "Hz apart"

# keep the strongest peaks: a bin is a peak when it is larger than both neighbours
var mid (slice m 1 (- (length m) 2))
var is-peak (* (> mid (take m (- (length m) 2))) (> mid (drop m 2)))   # elementwise and
var peak-bins (filter (range 1 (- (length m) 1)) (function (k) (getidx is-peak (- k 1))))
var peak-amps (map peak-bins (function (k) (getidx m k)))
var threshold (max (* 0.02 (max m)) (getidx (reverse (sort peak-amps)) (min 11 (- (length peak-amps) 1))))
var strong (filter peak-bins (function (k) (>= (getidx m k) threshold)))   # at most 12 peaks, all above 2% of the strongest
print "partials kept:" (length strong)
each (vec->list strong) (function (k) (print "  " (fixed (getidx freqs k) 1) "Hz  amp" (fixed (getidx m k) 4)))

# resynthesis: each partial is an oscillator with a constant frequency and a decaying amplitude
var table (gen 1024 (vec 1))
var decay (exp (* -2.5 (/ (range samples) sr)))
var amps (map (vec->list strong) (function (k) (* (getidx m k) decay)))
var fs (map (vec->list strong) (function (k) (+ (zeros samples) (getidx freqs k))))
var out (oscbank sr amps fs table)
print "resynthesised" (length out) "samples; f0 of the original" (fixed (acf-f0 (take x 2048) sr) 1) "Hz, of the copy" (fixed (acf-f0 (take out 2048) sr) 1) "Hz"
write-wav "/tmp/musil_additive.wav" sr (normalize-peak out)
