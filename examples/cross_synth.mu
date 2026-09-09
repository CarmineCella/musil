# cross synthesis: one sound shaped by another, frame by frame, in two ways
# Usage: musil cross_synth.mu [modulator.wav carrier.wav]   (defaults to the bundled data/ files)
#
# The modulator (a voice) lends its spectral shape; the carrier (an orchestra) lends its
# fine structure and phases. Two methods:
#   mean:     magnitudes = sqrt(voice * carrier), the classic geometric mean
#   envelope: the carrier flattened by its own spectral envelope, then shaped by the
#             voice's envelope (cepstral, `order` coefficients): the "voice is the
#             orchestra" effect, robust to the carrier's own formants
load "system.mu"
load "signals.mu"

var a (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var b (read-wav (if (> (length args) 1) (getidx args 1) "data/Beethoven_Symph7.wav"))
var sr (head a)
var voice (head (getidx a 1))
var carrier (head (getidx b 1))
var n (next-pow2 (/ sr 20))          # ~50 ms: short enough to follow the voice's articulation (2048 at 44.1 kHz)
var hop (/ n 4)
var order 80 #(floor (/ sr 300))         # cepstral order ~ sr / (2 f0): follows the formants, not the harmonics (147 at 44.1 kHz)
var gate 0.01                        # voice bins below 1% of the frame's peak are silenced (noise floor)
var fa (stft voice n hop)
var fb (stft carrier n hop)
var frames (min (length fa) (length fb))
print "cross-synthesising" frames "frames of" n "samples, hop" hop ", cepstral order" order

var pairs (zip (take fa frames) (take fb frames))
function gated (mags) (* mags (> mags (* gate (max mags))))

var mean-frames (map pairs (function (p) {
    var ma (gated (magnitudes (head p)))
    var pb (car->pol (last p))
    return (pol->car (list (sqrt (* ma (head pb))) (last pb)))
}))
var out-mean (istft mean-frames n hop)
write-wav "/tmp/musil_cross_synth_mean.wav" sr (normalize-peak out-mean)

var env-frames (map pairs (function (p) {
    var ma (gated (magnitudes (head p)))
    var pb (car->pol (last p))
    return (pol->car (list (impose-envelope (head pb) ma order) (last pb)))
}))
var out-env (istft env-frames n hop)
write-wav "/tmp/musil_cross_synth_envelope.wav" sr (normalize-peak out-env)

print "mean method    :" (length out-mean) "samples, rms" (fixed (rms out-mean) 4) "-> /tmp/musil_cross_synth_mean.wav"
print "envelope method:" (length out-env) "samples, rms" (fixed (rms out-env) 4) "-> /tmp/musil_cross_synth_envelope.wav"
