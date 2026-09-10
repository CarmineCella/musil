# features: low-level audio descriptors frame by frame, then a resynthesis from f0 and energy
# Usage: musil features.mu [sound.wav]   (defaults to the bundled data/ files)
load "system.mu"
load "signals.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var sr (head w)
var x (head (getidx w 1))
var n (next-pow2 (/ sr 8))
var hop (/ n 4)
var freqs (spectrum-freqs n sr)
var frames (stft-magnitudes (stft x n hop))
print (length frames) "frames of" n "samples, hop" hop

# spectral descriptors per frame
var centroid (vec (map frames (function (a) (spectral-centroid a freqs))))
var spread   (vec (map frames (function (a) (spectral-spread a freqs))))
var flatness (vec (map frames spectral-flatness))
var rolloff  (vec (map frames (function (a) (spectral-rolloff a freqs 0.85))))
var flux (vec (map (zip (drop frames 1) frames) (function (p) (spectral-flux (head p) (last p)))))
# temporal descriptors per frame
var segments (map (vec->list (range (length frames))) (function (k) (slice x (* k hop) n)))
var f0 (median-filter (vec (map segments (function (s) (acf-f0 s sr)))) 3)
var energy-track (vec (map segments energy))
var zc (vec (map segments zcr))

print ""
print "frame  centroid  spread  flatness  rolloff   f0    energy   zcr"
each (vec->list (range 0 (length frames) 6)) (function (k) {
    function cell (v d w) (pad-left (fmt-fixed v d) w " ")
    print (pad-left (str k) 5 " ") (cell (getidx centroid k) 0 9) (cell (getidx spread k) 0 7) \
          (cell (getidx flatness k) 3 9) (cell (getidx rolloff k) 0 8) \
          (cell (getidx f0 k) 0 5) (cell (getidx energy-track k) 3 8) (cell (getidx zc k) 3 6)
})
print ""
print "mean f0 where voiced:" (fixed (mean (filter f0 (function (v) (> v 0)))) 1) "Hz"
print "largest spectral flux at frame" (argmax flux)

# resynthesis: a pulse-like wavetable driven by the f0 and energy tracks.
# A frame is voiced only when the tracker found a plausible f0 (between f0-min and
# f0-max) and the frame is not quiet; everywhere else the synth is silent. The f0
# of unvoiced frames is replaced by the last voiced one, so the envelope never
# glides through nonsense on its way into or out of a silence.
var f0-min 20
var f0-max 2000
var quiet (* 0.01 (max energy-track))
var voiced (* (>= f0 f0-min) (<= f0 f0-max) (> energy-track quiet))
var held (copy f0)
var last-good 0
for (var k 0) (< k (length held)) (var k (+ k 1)) {
    if (getidx voiced k) { set last-good (getidx held k) } { setidx held k last-good }
}
print "voiced frames:" (sum voiced) "of" (length voiced) " (f0 in" f0-min "-" f0-max "Hz and energy above" (fixed quiet 3) ")"
var f0-env (envelope-from-values held hop)
var amp-env (envelope-from-values energy-track hop)
var gate (envelope-from-values voiced hop)           # ramps over one hop: a short fade at each edge
var synth (* amp-env gate (osc sr f0-env (gen 1024 (vec 1 0.7 0.5 0.3 0.2))))
write-wav "/tmp/musil_features_resynth.wav" sr (normalize-peak synth)
print "wrote /tmp/musil_features_resynth.wav," (length synth) "samples"
