# live_offline: the same instrument rendered offline (no device), with parameter curves,
# through the exact graph the engine streams; then written as a file and played
# Usage: musil live_offline.mu
load "live.mu"
var sr 44100

function voice (gate freq cutoff) \
    (pan (* (adsr sr gate 0.02 0.1 0.7 0.5) (lowpass (osc sr freq saw-table) sr cutoff 2)) 0)

# parameters as curves: one value per sample for a glissando and a filter sweep, a gate that opens twice
var n (* 3 sr)
var t (/ (range n) sr)
var freq (* 110 (pow 2 (/ t 3)))                            # one octave up over three seconds
var cutoff (+ 300 (* 2500 (* 0.5 (+ 1 (sin (* tau 0.5 t))))))   # sweeping 300..2800 Hz at 0.5 Hz
var gate (vec (ones (* 1.2 sr)) (zeros (* 0.3 sr)) (ones (* 1.2 sr)) (zeros (* 0.3 sr)))
var y (synth-render voice (list (list 'gate gate) (list 'freq freq) (list 'cutoff cutoff)) 3 sr)
write-wav "/tmp/musil_live_offline.wav" sr y
print "rendered" (length (head y)) "samples per channel to /tmp/musil_live_offline.wav"

# calling the function directly gives the same samples (a synth is an ordinary function).
# One difference to know: offline, a filter's cutoff is one number (biquad computes its
# coefficients once), while the engine reads a control every block; so the comparison is
# made with a fixed cutoff, where both are defined
var fixed-cutoff (synth-render voice (list (list 'gate gate) (list 'freq freq) (list 'cutoff 1200)) 3 sr)
var direct (voice gate freq 1200)
print "same as the direct call within" (max (abs (- (head direct) (head fixed-cutoff))))

# and the engine can play the result like any buffer
audio-init
var v (play y sr)
wait-for v
audio-quit
