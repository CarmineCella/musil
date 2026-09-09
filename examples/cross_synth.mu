# cross synthesis: the magnitudes of one sound with the phases of another, frame by frame
# Usage: musil cross_synth.mu [sound.wav second.wav]   (defaults to the bundled data/ files)
load "system.mu"
load "signals.mu"

var a (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var b (read-wav (if (> (length args) 1) (getidx args 1) "data/Beethoven_Symph7.wav"))
var sr (head a)
var voice (head (getidx a 1))
var drums (head (getidx b 1))
var n (next-pow2 (/ sr 8))
var hop (/ n 4)
var frames (min (length (stft voice n hop)) (length (stft drums n hop)))
print "cross-synthesising" frames "frames"

var fa (take (stft voice n hop) frames)
var fb (take (stft drums n hop) frames)
var out-frames (map (zip fa fb) (function (p) {
    var pa (car->pol (head p))
    var pb (car->pol (last p))
    var mags (sqrt (* (head pa) (head pb)))       # geometric mean of the two magnitude spectra
    return (pol->car (list mags (last pb)))       # with the phases of the second sound
}))
var out (istft out-frames n hop)
print "output:" (length out) "samples; rms" (fixed (rms out) 4)
write-wav "/tmp/musil_cross_synth.wav" sr (normalize-peak out)
