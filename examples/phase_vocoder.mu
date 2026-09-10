# phase vocoder: time stretching and pitch shifting that keep partials coherent
# Usage: musil phase_vocoder.mu [sound.wav]   (defaults to the bundled data/ file)
#
# stft.mu overlap-added the analysis frames at a larger hop and left the phases alone,
# which smears everything. pvoc-stretch advances each bin's phase by the frequency it
# actually measured, and locks every bin to the nearest spectral peak (Laroche and
# Dolson's identity phase locking), so a partial stays one partial when stretched.
load "system.mu"
load "signals.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var sr (head w)
var x (head (getidx w 1))
var n (next-pow2 (/ sr 16))
var hop (/ n 4)
print (length x) "samples at" sr "Hz; frames of" n ", hop" hop

each (list 0.5 2 4) (function (factor) {
    var t0 (clock)
    var y (pvoc-stretch x n hop factor)
    var path (concat "/tmp/musil_pvoc_stretch_" (str factor) ".wav")
    write-wav path sr (normalize-peak y)
    print "stretch x" factor ":" (fixed (/ (length y) sr) 2) "s, f0" (fixed (acf-f0 (slice y (floor (/ (length y) 3)) n) sr) 1) "Hz, in" (fixed (- (clock) t0) 2) "s ->" path
})
print "original f0:" (fixed (acf-f0 (slice x (floor (/ (length x) 3)) n) sr) 1) "Hz"

each (list 0.5 1.5 2) (function (ratio) {
    var y (pvoc-pitch x n hop ratio)
    var path (concat "/tmp/musil_pvoc_pitch_" (str ratio) ".wav")
    write-wav path sr (normalize-peak y)
    print "pitch x" ratio ":" (length y) "samples, f0" (fixed (acf-f0 (slice y (floor (/ (length y) 3)) n) sr) 1) "Hz ->" path
})

# the same shifts with the formants preserved: the spectral envelope of the original is
# imposed on each frame of the shifted sound, so a voice keeps its vowel and its size
var order (floor (/ sr 300))
each (list 0.5 1.5) (function (ratio) {
    var y (pvoc-pitch-formant x n hop ratio order)
    var path (concat "/tmp/musil_pvoc_pitch_formant_" (str ratio) ".wav")
    write-wav path sr (normalize-peak y)
    print "pitch x" ratio "with formants kept:" (length y) "samples ->" path
})

# two classic phase effects: zero phases (robot) and random phases (whisper)
write-wav "/tmp/musil_pvoc_robot.wav" sr (normalize-peak (robotize x 512 128))
write-wav "/tmp/musil_pvoc_whisper.wav" sr (normalize-peak (whisperize x n hop))
print "robot and whisper -> /tmp/musil_pvoc_robot.wav /tmp/musil_pvoc_whisper.wav"
