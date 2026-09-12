# live_loops: patterns that repeat on the clock, and change while they run
# Usage: musil live_loops.mu        (or drop it on the Listener and keep editing it)
#
# A loop is a function of the cycle number returning events; (live-loop 'name beats) calls
# the function of that name every cycle, a little ahead of the clock, so everything lands
# sample-accurately. Redefine the function (send the new definition from the editor, or
# just save this file while the Listener watches it) and the next cycle plays the new one.
load "live.mu"
audio-init
var sr (audio-sr)
tempo 110

# instruments
function kick (gate) (* (adsr sr gate 0.001 0.15 0 0.05) (osc sr (+ 50 (* 80 (adsr sr gate 0 0.05 0 0.01))) sine-table))
function hat (gate) (* 0.3 (adsr sr gate 0.001 0.04 0 0.02) (highpass (noise 0) sr 6000 1))
function bass (gate freq cutoff) (* 0.6 (adsr sr gate 0.005 0.1 0.6 0.1) (lowpass (osc sr freq saw-table) sr cutoff 3))
var k (synth kick)
var h (synth hat)
var b (synth bass)
set-param b 'cutoff 900

# patterns: functions of the cycle, returning event lists (beat, duration, what to do)
function drums (cycle) (stack (list
    (steps 1 (list 1 nil 1 nil) (function (x) (function (t d) (note-at t k 0.1))))
    (every 4 cycle (function (e) (fast e 2)) (steps 0.5 (list nil 1 nil 1 nil 1 nil 1) (function (x) (function (t d) (note-at t h 0.05)))))))
function bassline (cycle) (steps 0.5 (list 55 nil 55 65.4 nil 55 82.4 73.4) (function (f) (function (t d) {
    set-param b 'freq f 0 t
    note-at t b (* d 0.9)
})))
live-loop 'drums 4
live-loop 'bassline 4
print "loops:" (loops) "  at" (bpm) "bpm; sleeping 8 s..."
sleep 8

# live coding: redefine a pattern while it runs; the next cycle uses it
function bassline (cycle) (rev (steps 0.5 (list 55 82.4 55 98 65.4 nil 73.4 nil) (function (f) (function (t d) {
    set-param b 'freq f 0 t
    note-at t b (* d 0.5)
}))) 4)
print "bassline redefined (reversed, shorter notes)"
sleep 8
tempo 140
print "tempo 140; the loops follow"
sleep 6
stop-loops
sleep 0.5
free-all
audio-quit
print "done"
