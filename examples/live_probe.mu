# live_probe: is the audio device fine? a scale of sines and the load/latency figures
# Usage: musil live_probe.mu
load "live.mu"

print "devices:" (audio-devices)
audio-init
audio-report
var sr (audio-sr)
each (list 261.63 293.66 329.63 349.23 392 440 493.88 523.25) (function (f) {
    var note (sine sr f 0.25)
    var v (play (* 0.3 (fade-out (fade-in note 200) 1500)) sr)
    wait-for v
})
print "block" (opt (audio-status) "block" 0) "samples =" (fixed (* 1000 (/ (opt (audio-status) "block" 0) sr)) 1) "ms; load" (fixed (opt (audio-status) "load" 0) 3) "of the block time"
audio-quit
