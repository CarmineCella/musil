# stft: analysis, resynthesis, time stretching and resampling
# Usage: musil stft.mu [sound.wav]   (defaults to the bundled data/ files)
load "system.mu"
load "signals.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var sr (head w)
var x (head (getidx w 1))
var n (next-pow2 (/ sr 16))       # 512 at 8 kHz, 4096 at 44.1 kHz
var hop (/ n 8)
print "voice.wav:" (length x) "samples at" sr "Hz"

var frames (stft x n hop)
print "stft:" (length frames) "frames of" n "samples, hop" hop

var y (istft frames n hop)
var common (min (length x) (length y))
var err (max (abs (- (slice y n (- common (* 2 n))) (slice x n (- common (* 2 n))))))
print "istft reconstruction error:" err "(zero up to rounding)"
write-wav "/tmp/musil_stft_resynth.wav" sr y

# time stretch: overlap-add the same frames at twice the hop (phases are not corrected, so
# this is the crude version; a phase vocoder would adjust them)
var stretched (istft frames n (* hop 2))
print "stretched x2:" (length stretched) "samples =" (fixed (/ (length stretched) sr) 2) "s"
write-wav "/tmp/musil_stft_stretch.wav" sr stretched

# resampling by spectral zero-padding
var up (resample x 2)
print "resampled x2:" (length up) "samples; play at" (* 2 sr) "Hz for the same pitch"
write-wav "/tmp/musil_resampled.wav" (* 2 sr) up
var down (resample-to x sr 4000)
print "resampled to 4 kHz:" (length down) "samples"
