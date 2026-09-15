# reverb: convolution with an impulse response (the shipped concert hall, or any response), and a Schroeder network
# Usage: musil reverb.mu [sound.wav impulse-response.wav]   (defaults to the bundled data/anechoic1.wav and the hall)
load "system.mu"
load "signals.mu"

var dry (read-wav (if (> (length args) 0) (getidx args 0) "data/anechoic1.wav"))
var sr (head dry)
var x (head (getidx dry 1))
print "input" (length x) "samples at" sr "Hz"

# the Concertgebouw, shipped with the libraries: (concerthall x sr dry wet) gives stereo, dry plus the hall
var hall (concerthall x sr 0.7 0.3)
print "concerthall:" (length hall) "channels of" (length (head hall)) "samples (the input plus the hall's tail)"
write-wav "/tmp/musil_conv_reverb.wav" sr (normalize-peak-stereo hall)
# any response, with converb: a vector for every channel, or one response per channel
if (> (length args) 1) {
    var ir (read-wav (getidx args 1))
    var own (converb x (map (getidx ir 1) (function (c) (resample-to c (head ir) sr))) 0.7 0.3)
    write-wav "/tmp/musil_conv_reverb_own.wav" sr (normalize-peak-stereo own)
    print "converb with" (getidx args 1) "->" "/tmp/musil_conv_reverb_own.wav"
}
var outL (head hall)

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
