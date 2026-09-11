# cross synthesis: one sound shaped by another, with the phase vocoder's three modes
# Usage: musil cross_synth.mu [modulator.wav carrier.wav]   (defaults to the bundled data/ files)
#
#   mode 1  multiplicative: magnitudes = sqrt(voice * orchestra), phases blended by the amount
#   mode 2  spectral flattener: the orchestra flattened by its own envelope and shaped by the
#           voice's ("voice is the orchestra"); needs the envelope order
#   mode 3  morphing: magnitudes interpolated, phases from one sound below 0.5 and the other above
# The amount may ramp: (list mode 0 1 other) goes from none to full over the file.
load "system.mu"
load "signals.mu"

var a (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var b (read-wav (if (> (length args) 1) (getidx args 1) "data/Beethoven_Symph7.wav"))
var sr (head a)
var voice (head (getidx a 1))
var orchestra (head (getidx b 1))
var order 80 #(floor (/ sr 100))
print "voice" (length voice) "samples, orchestra" (length orchestra) "samples at" sr "Hz"

# the voice is the input, the orchestra the second signal (looped if shorter)
var m1 (pvoc-cross voice orchestra 1 2 order)
write-wav "/tmp/musil_cross_1_multiplicative.wav" sr (normalize-peak m1)
var m2 (pvoc-cross orchestra voice 2 1 order)                 # here the orchestra is the input: it takes the voice's envelope
write-wav "/tmp/musil_cross_2_flattener.wav" sr (normalize-peak m2)
var m3 (pvoc orchestra (list (list "cross" (list 3 0 1 voice))))   # a morph from the orchestra to the voice over the file
write-wav "/tmp/musil_cross_3_morph.wav" sr (normalize-peak m3)
# denoising the voice first removes its noise floor from the product
var m1d (pvoc voice (list (list "threshold" 0.01) (list "cross" (list 1 1 orchestra))))
write-wav "/tmp/musil_cross_1_denoised.wav" sr (normalize-peak m1d)
print "mode 1:" (fixed (rms m1) 4) " mode 2:" (fixed (rms m2) 4) " mode 3:" (fixed (rms m3) 4) " mode 1 denoised:" (fixed (rms m1d) 4)
print "written to /tmp/musil_cross_*.wav"
