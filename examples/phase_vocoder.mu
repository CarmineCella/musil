# phase vocoder: time stretching, pitch shifting, formants, denoising, phase effects
# Usage: musil phase_vocoder.mu [sound.wav]   (defaults to the bundled data/ file)
#
# (pvoc x opts) does everything in one pass, as sparkle did: the output hop is fixed
# (window / overlap) and the input hop is output-hop / stretch, so the synthesis overlap
# never thins out however large the stretch; phases are locked to the nearest spectral
# peak; pitch shift and formant move happen in the spectrum; formants are preserved
# through the cepstral envelope. Any parameter given as (list start end) ramps over the file.
load "system.mu"
load "signals.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var sr (head w)
var x (head (getidx w 1))
var order 80 #(floor (/ sr 100))            # cepstral order for the envelope: ~80 at 8 kHz, ~400 at 44.1 kHz
print (length x) "samples at" sr "Hz; envelope order" order

function out (name y) {
    var path (concat "/tmp/musil_pvoc_" name ".wav")
    write-wav path sr (normalize-peak y)
    print (pad-right name 18 " ") (fixed (/ (length y) sr) 2) "s ->" path
}
var t0 (clock)
out "stretch_2" (pvoc-stretch x 2)
out "stretch_4" (pvoc-stretch x 4)
out "stretch_ramp_1_3" (pvoc x (list (list "stretch" (list 1 3))))
out "pitch_1.5" (pvoc-pitch x 1.5)
out "pitch_1.5_formants" (pvoc-pitch-formant x 1.5 order)
out "pitch_0.5_formants" (pvoc-pitch-formant x 0.5 order)
out "formants_0.8" (pvoc-formants x 0.8 order)
out "glissando" (pvoc x (list (list "pitch" (list 1 2)) (list "envelope" order)))
out "denoise" (denoise x 0.02)
out "robot" (robotize x)
out "whisper" (whisperize x)
out "everything" (pvoc x (list (list "stretch" 1.5) (list "pitch" 0.75) (list "envelope" order) (list "threshold" 0.01)))
print "all in" (fixed (- (clock) t0) 2) "s"
print "f0 original" (fixed (acf-f0 (slice x (floor (/ (length x) 3)) 2048) sr) 1) "Hz, pitch x1.5" (fixed (acf-f0 (slice (pvoc-pitch x 1.5) (floor (/ (length x) 3)) 2048) sr) 1) "Hz"
