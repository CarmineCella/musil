# reverb: convolution with an impulse response, and a Schroeder network
# Usage: musil reverb.mu [sound.wav impulse-response.wav]   (defaults to the bundled data/ files)
load "system.mu"
load "signals.mu"

var dry (read-wav (if (> (length args) 0) (getidx args 0) "data/anechoic1.wav"))
var ir (read-wav (if (> (length args) 1) (getidx args 1) "data/Concertgebouw-s.wav"))
var sr (head dry)
var x (head (getidx dry 1))
var irL (head (getidx ir 1))
var irR (last (getidx ir 1))
print "input" (length x) "samples, IR" (length irL) "samples per channel"

var wet-gain 0.3
var dry-gain 0.7
var wetL (* wet-gain (conv x irL))
var wetR (* wet-gain (conv x irR))
var outL (mix (list (list 0 wetL) (list 0 (* dry-gain x))))
var outR (mix (list (list 0 wetR) (list 0 (* dry-gain x))))
print "convolved:" (length outL) "samples (input + IR - 1); peak" (fixed (max (abs outL)) 3)
write-wav "/tmp/musil_conv_reverb.wav" sr (list (normalize-peak outL) (normalize-peak outR))

# Schroeder: an algorithmic reverb; rt60 is the decay time, and the output includes the tail
var rt60 2.5
var schroeder (schroeder-reverb x sr rt60)
print "schroeder rt60" rt60 "s:" (length schroeder) "samples =" (fixed (/ (length schroeder) sr) 2) "s (input + tail)"
print "tail level after the input ends, per half second:"
var start (length x)
each (range 0 (floor (* rt60 2)) 1) (function (k) {
    var from (+ start (floor (* k 0.5 sr)))
    var chunk (slice schroeder from (floor (* 0.5 sr)))
    if (> (length chunk) 0) { print "  " (fixed (* k 0.5) 1) "s:" (fixed (db (rms chunk)) 1) "dB" }
})
write-wav "/tmp/musil_schroeder.wav" sr (normalize-peak (mix (list (list 0 x) (list 0 (* 0.5 schroeder)))))
print "wrote /tmp/musil_schroeder.wav"
