# onsets: where do things start? Spectral flux (the increase of every bin's magnitude from one
# frame to the next, rectified and summed) rises at attacks; its peaks above a threshold, at least
# a window apart, are the onsets, each placed at the sample where the energy jumps inside its
# frame. The sound is then cut into segments at those times.
# Usage: musil onsets.mu [sound.wav]   (defaults to the bundled data/ file)
load "system.mu"
load "signals.mu"
load "plot.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/drums.wav"))
var sr (head w)
var x (head (getidx w 1))
var n 1024
var hop 256
print (fixed (/ (length x) sr) 2) "s at" sr "Hz"

# 1. the onset strength: one number per frame
var flux (onset-strength x n hop)
var times (/ (* (range (length flux)) hop) sr)
# 2. the onsets: peaks of the flux above a fraction of its maximum, at least one window apart
var t-on (onsets x sr n hop 0.25)
print (length t-on) "onsets:" (fixed t-on 3)
print "adaptive:" (fixed (onsets-adaptive x sr n hop 3 9) 3) "(a peak counts when it exceeds 3x the flux's moving median: for material whose loudness drifts)"

# 3. picture: the waveform with the onsets marked, and the flux with its threshold
var fig (figure "onsets by spectral flux")
add-line fig (/ (range (length x)) sr) x "waveform"
add-scatter fig t-on (zeros (length t-on)) "onsets"
set-labels fig "time (s)" "amplitude"
var fig2 (figure "onset strength (spectral flux)")
add-line fig2 times flux "flux"
add-line fig2 times (+ (zeros (length flux)) (* 0.25 (max flux))) "threshold"
set-labels fig2 "time (s)" "flux"
show (subplots "onset detection" (list fig fig2) 2 1)

# 4. the segments, written as files; and a re-assembly in reverse order, as a check that they tile the sound
var pieces (segments x sr t-on)
print "segment lengths (s):" (fixed (/ (vec (map pieces length)) sr) 3)
each (zip (range (length pieces)) pieces) (function (p) (write-wav (concat "/tmp/musil_onset_" (str (head p)) ".wav") sr (last p)))
var reversed (mix (map (zip (range (length pieces)) (reverse pieces)) (function (p) (list (sum (vec (map (take (reverse pieces) (head p)) length))) (last p)))))
write-wav "/tmp/musil_onsets_reversed.wav" sr reversed
print "wrote /tmp/musil_onset_<k>.wav and /tmp/musil_onsets_reversed.wav (the segments in reverse order)"
