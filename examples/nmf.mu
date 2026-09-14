# nmf: source separation by non-negative matrix factorization. The magnitude spectrogram
# V (bins x frames) is approximated by W H with k parts: the columns of W are spectral shapes,
# the rows of H say when each sounds. A Wiener mask per part splits the sound into k sources
# that add up to the input. Which part is which sound is the listener's (or a rule's) call.
# Usage: musil nmf.mu [sound.wav] [k]   (defaults to data/hpss_demo.wav and 4 parts)
load "system.mu"
load "signals.mu"
load "plot.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/hpss_demo.wav"))
var sr (head w)
var x (take (head (getidx w 1)) (* 6 sr))
var k (if (> (length args) 1) (num (getidx args 1)) 4)
var n 2048
var hop 512
print (fixed (/ (length x) sr) 2) "s at" sr "Hz;" k "parts"

var t0 (clock)
var sep (nmf-separate x n hop k 80)
var sources (head sep)
var W (getidx sep 1)
var H (getidx sep 2)
print "nmf-separate in" (fixed (- (clock) t0) 2) "s; sources add up to the input within" (fixed (max (abs (- (reduce sources + (zeros (length x))) x))) 5)

# a rule to tell percussive parts from harmonic ones: how spiky the activation is (its peak / mean)
var spikiness (vec (map H (function (h) (/ (max h) (+ 1e-9 (mean h))))))
var freqs (* (range (+ (/ n 2) 1)) (/ sr n))            # one frequency per bin, 0 .. sr/2
each (range k) (function (j) {
    var shape (mat-col W j)
    var centroid (spectral-centroid shape freqs)
    print "part" j ": centroid" (fixed centroid 0) "Hz, activation spikiness" (fixed (getidx spikiness j) 1) (if (> (getidx spikiness j) 6) "-> percussive" "-> sustained")
    write-wav (concat "/tmp/musil_nmf_part_" (str j) ".wav") sr (normalize-peak (getidx sources j))
})
var perc (reduce (filter (vec->list (range k)) (function (j) (> (getidx spikiness j) 6))) (function (acc j) (+ acc (getidx sources j))) (zeros (length x)))
var harm (- x perc)
write-wav "/tmp/musil_nmf_percussive.wav" sr perc
write-wav "/tmp/musil_nmf_sustained.wav" sr harm
print "wrote /tmp/musil_nmf_part_<j>.wav, and the percussive / sustained groups"

# picture: the parts (W, as spectra) and their activations (H)
var figw (figure "parts: spectral shapes (W)")
each (range k) (function (j) (add-line figw freqs (db (mat-col W j)) (concat "part " (str j))))
set-labels figw "frequency (Hz)" "dB"
set-xrange figw 0 8000
var figh (figure "activations over time (H)")
var times (* (range (ncols H)) (/ hop sr))
each (range k) (function (j) (add-line figh times (getidx H j) (concat "part " (str j))))
set-labels figh "time (s)" "gain"
show (subplots "nmf" (list figw figh) 2 1)
