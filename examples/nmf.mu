# nmf: source separation by non-negative matrix factorization, on a four-stem mix (drums,
# bass, chords, melody: data/stems_demo.wav, made by make_audio_demos.mu, with every stem
# alone as data/stems_<name>.wav to score against).
#
# Unsupervised: the magnitude spectrogram V (bins x frames) ~ W H with k parts; a Wiener
# mask per part; nobody says which part is which instrument. Supervised: learn a few parts
# from each stem alone (nmf-learn-parts), then on the mix only the activations are learnt
# (nmf-separate-with): one source per instrument, by construction. The scores are the
# correlation of each source with the true stem.
# Usage: musil nmf.mu
load "system.mu"
load "signals.mu"
load "plot.mu"

var names (list "drums" "bass" "chords" "melody")
var w (read-wav "data/stems_demo.wav")
var sr (head w)
var x (take (head (getidx w 1)) (* 10 sr))
var stems (map names (function (nm) (take (head (getidx (read-wav (concat "data/stems_" nm ".wav")) 1)) (length x))))
var n 2048
var hop 512
print (fixed (/ (length x) sr) 1) "s of a four-stem mix at" sr "Hz"
function corr (a b) (/ (dot a b) (* (+ 1e-9 (norm a)) (+ 1e-9 (norm b))))
function stem-of (nm) (getidx stems (find names nm))
function score (sources) (each (zip names sources) (function (p) {
    var others (filter names (function (o) (not (equal? o (head p)))))
    print "  " (head p) ": correlation with its stem" (fixed (corr (last p) (stem-of (head p))) 3) "; with the others" (fixed (vec (map others (function (o) (corr (last p) (stem-of o))))) 2)
}))

# --- 1. unsupervised, four parts: each source is matched to the stem it resembles most ---------------
var t0 (clock)
var sep (nmf-separate x n hop 4 60)
print "unsupervised nmf-separate, 4 parts, 60 iterations:" (fixed (- (clock) t0) 1) "s"
var srcs (head sep)
var best (map srcs (function (s) (getidx names (argmax (vec (map stems (function (st) (corr s st))))))))
print "  parts matched to stems:" best "(a part can take a whole instrument, half of one, or two)"
each (zip srcs best) (function (p) (print "   part ~" (last p) ":" (fixed (corr (head p) (stem-of (last p))) 3)))

# --- 2. supervised: three parts learnt from each stem alone, then the mix ------------------------------
set t0 (clock)
var learnt (nmf-learn-parts stems n hop 3 60)
var W (head learnt)
var groups (last learnt)
var sep2 (nmf-separate-with x n hop W groups 60)
print "supervised: 3 parts per stem learnt from the stems (" (ncols W) "parts), activations on the mix:" (fixed (- (clock) t0) 1) "s"
score (head sep2)
each (zip names (head sep2)) (function (p) (write-wav (concat "/tmp/musil_nmf_" (head p) ".wav") sr (normalize-peak (last p))))
print "the sources add up to the mix within" (fixed (max (abs (- (reduce (head sep2) + (zeros (length x))) x))) 5)
print "wrote /tmp/musil_nmf_drums.wav, _bass, _chords, _melody"

# --- 3. picture: the learnt parts and the activations of the supervised separation --------------------
var freqs (* (range (+ (/ n 2) 1)) (/ sr n))
var figw (figure "learnt parts (W): three per stem")
each (range (ncols W)) (function (j) (add-line figw freqs (amp->db (mat-col W j)) (getidx names (floor (/ j 3)))))
set-labels figw "frequency (Hz)" "dB"
set-xrange figw 0 6000
var H (last sep2)
var figh (figure "activations on the mix (H), summed per stem")
var times (* (range (ncols H)) (/ hop sr))
each (range 4) (function (s) (add-line figh times (reduce (getidx groups s) (function (acc j) (+ acc (getidx H j))) (zeros (ncols H))) (getidx names s)))
set-labels figh "time (s)" "gain"
show (subplots "nmf: four stems" (list figw figh) 2 1)
