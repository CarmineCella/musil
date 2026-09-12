# live_synth: instruments as functions, streamed and changed while they play
# Usage: musil live_synth.mu
#
# An instrument is a Musil function of its parameters. (synth f) compiles its body into
# a graph on the audio thread; set-param changes a parameter of the running sound, with
# an optional ramp; note-on / note-off drive the parameter called gate. The same function
# called with vectors returns the sound as a buffer: one definition, two executions.
load "live.mu"
audio-init
var sr (audio-sr)

# --- 1. a subtractive voice: saw into a resonant lowpass with an envelope on both -----------
function lead (gate freq cutoff res) \
    (pan (* (adsr sr gate 0.01 0.2 0.5 0.4) (lowpass (osc sr freq saw-table) sr (* cutoff (+ 0.5 (adsr sr gate 0.05 0.3 0.3 0.4))) res)) 0)
var a (synth lead)
set-params a (list (list 'freq 110) (list 'cutoff 1200) (list 'res 3))
each (list 110 146.83 164.81 220) (function (f) {
    set-param a 'freq f
    note a 0.35
    sleep 0.45
})
set-param a 'cutoff 300 1.5          # a slow filter sweep on a held note
note-on a
sleep 1.5
note-off a
sleep 0.5
free a

# --- 2. FM: the frequency input of an oscillator is any signal -----------------------------
function fm (gate freq ratio index) \
    (* (adsr sr gate 0.005 0.3 0.4 0.3) (osc sr (+ freq (* index freq (osc sr (* ratio freq) sine-table))) sine-table))
var b (synth fm)
set-params b (list (list 'freq 220) (list 'ratio 2) (list 'index 1))
note-on b
each (list 0.5 1 2 3 5) (function (idx) {
    set-param b 'index idx 0.2
    sleep 0.4
})
note-off b
sleep 0.4
free b

# --- 3. a physical string: a noise burst into a tuned comb, filtered -------------------------
function string (gate freq bright) \
    (pan (lowpass (comb (* (noise gate) (adsr sr gate 0 0.005 0 0.005)) (floor (/ sr 220)) 0.995) sr bright 0.7) 0)
var c (synth string)
set-params c (list (list 'freq 220) (list 'bright 2000))
each (range 6) (function (k) { note c 0.01
                               sleep 0.3 })
free c

# --- 4. effects are synths too: noise through a delay with feedback into a stereo pan --------
function echoes (gate time fb) \
    (pan (+ (* (adsr sr gate 0.001 0.05 0 0.05) (noise gate)) (* fb (delay (lag (noise gate) sr 0.5) time))) 0)
var d (synth echoes)
set-params d (list (list 'time (* sr 0.25)) (list 'fb 0.5))
each (range 4) (function (k) { note d 0.02
                               sleep 0.5 })
sleep 1
free-all
audio-quit
print "done"
