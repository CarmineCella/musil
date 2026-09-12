# transients: separate the percussive and the harmonic parts of a sound with HPSS
# (harmonic-percussive source separation: median filters over the spectrogram, along time
# for the harmonic part and along frequency for the percussive one, then soft masks), and
# find the attacks in the percussive part.
# Usage: musil transients.mu [sound.wav]   (defaults to data/hpss_demo.wav: chords under a drum
# pattern, made by make_hpss_demo.mu; try data/Gambale_cut.wav for a real recording)
load "system.mu"
load "signals.mu"
load "plot.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/hpss_demo.wav"))
var sr (head w)
var x (head (getidx w 1))
var n 2048
var hop 512
print (fixed (/ (length x) sr) 2) "s at" sr "Hz"

# 1. the separation
var t0 (clock)
var parts (hpss x n hop 31)                  # the kernel: 17-31 frames and bins; larger separates more, blurs more
var harmonic (head parts)
var percussive (last parts)
print "hpss in" (fixed (- (clock) t0) 2) "s; rms harmonic" (fixed (rms harmonic) 3) "percussive" (fixed (rms percussive) 3) "original" (fixed (rms x) 3)
write-wav "/tmp/musil_harmonic.wav" sr harmonic
write-wav "/tmp/musil_percussive.wav" sr percussive
write-wav "/tmp/musil_hpss_sum.wav" sr (+ harmonic percussive)     # the two parts add up to the input
print "sum equals the input within" (fixed (max (abs (- (+ harmonic percussive) x))) 4)

# 2. the attacks, from the percussive part (the harmonic part no longer masks them)
var t-on (onsets percussive sr 1024 256 0.15)
print (length t-on) "attacks in the percussive part:" (fixed (take t-on (min 12 (length t-on))) 3) (if (> (length t-on) 12) "..." "")

# 3. picture: the three spectrograms and the percussive waveform with the attacks
var spec (function (y title) (add-image (figure title) (map (stft-magnitudes (stft y 1024 256)) (function (m) (undb (db m))))))
var fig (figure "percussive part and its attacks")
add-line fig (/ (range (length percussive)) sr) percussive "percussive"
add-scatter fig t-on (zeros (length t-on)) "attacks"
set-labels fig "time (s)" "amplitude"
show (subplots "harmonic / percussive separation" (list (spectrogram x sr 1024 256) (spectrogram harmonic sr 1024 256) (spectrogram percussive sr 1024 256) fig) 2 2)
print "wrote /tmp/musil_harmonic.wav, /tmp/musil_percussive.wav, /tmp/musil_hpss_sum.wav"
