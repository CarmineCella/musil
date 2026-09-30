# live_emptyset: in the style of Emptyset (Bristol): monolithic. Sub-bass impacts driven into saturation, the
# transient split off as a clipped noise crack, struck iron ringing in combs, a saturated drone for a floor, a wall
# of clipped hiss that opens slowly and is cut dead; a stone room on the whole bus. The grid is strict and the
# rhythms are Euclidean and lopsided; the form is made of ruptures: every fourth bar of the impacts is silence,
# and the arrangement stops everything at once and starts it again louder.
# Open it in the IDE, run blocks 1 to 3 with Cmd-Enter, then keep changing the pattern functions while it plays;
# from the command line it plays a two-minute arrangement.
#
# Only the live library's tools are used: osc noise adsr lag comb the filters tanh clip, and the pattern tools
# (pat euclid every chance sweep). The instruments are defined here, not in live.mu: a style is a few functions.
load "live.mu"
audio-init
var sr (audio-sr)
tempo 112
bus-reverb (hall-ir sr) 1 0.18                # a big stone room, mostly dry: the tails of the impacts are what ring

# --- 1. instruments (functions of a gate: synth them to play) ----------------------------------------------------
# (slab gate freq drive)   the impact: a sub sine with a fast pitch drop, driven hard into tanh, a long decay
function slab (gate freq drive) (* 0.9 (tanh (* drive (adsr sr gate 0.0005 1.1 0 0.3) (osc sr (* freq (+ 1 (* 5 (adsr sr gate 0 0.06 0 0.01)))) sine-table))))
# (crack gate)             the transient of the impact on its own: a noise burst through a resonant bandpass, clipped
function crack (gate) (* 0.8 (clip (* 6 (adsr sr gate 0.0003 0.04 0 0.02) (bandpass (noise gate) sr 900 4)) -0.7 0.7))
# (iron gate)              struck iron: a noise burst ringing in two combs (531 and 336 Hz), ring-modulated, highpassed
function iron (gate) (* 0.2 (adsr sr gate 0.0005 1.6 0 0.4) (highpass (* (+ (comb (* (adsr sr gate 0.0005 0.01 0 0.005) (noise gate)) 83 0.985) (comb (* (adsr sr gate 0.0005 0.01 0 0.005) (noise gate)) 131 0.98)) (+ 0.6 (* 0.4 (osc sr (sig gate 1370) sine-table)))) sr 300 0.7))
# (drone gate freq cutoff)   the floor: squares an octave apart, slightly detuned, lowpassed and saturated, slow to rise
function drone (gate freq cutoff) (* 0.45 (tanh (* 2.5 (adsr sr gate 2.5 1 0.9 3) (lowpass (+ (osc sr freq square-table) (* 0.7 (osc sr (* freq 1.004) square-table)) (* 0.8 (osc sr (* 0.5 freq) sine-table))) sr cutoff 0.9))))
# (hiss gate cutoff)       the wall: highpassed noise, clipped, opening over half a second and cut dead
function hiss (gate cutoff) (* 0.5 (clip (* 3 (adsr sr gate 0.6 0.1 1 0.02) (highpass (noise gate) sr cutoff 0.7)) -0.5 0.5))
# (howl gate freq)         feedback: a sine into a comb near unity, saturated; the room that will not stop ringing
function howl (gate freq) (* 0.35 (adsr sr gate 0.05 0.5 0.7 2.5) (tanh (* 3 (comb (osc sr freq sine-table) 441 0.97))))

var impact (synth slab)
set-params impact (list (list 'freq 38) (list 'drive 6))          # D1 and a half; drive 6 is the start, 24 the end
var edge (synth crack)
var irons (poly iron 2)                                            # two, so hits close together do not cut each other
var ground (synth drone)
set-params ground (list (list 'freq 36.7) (list 'cutoff 240))      # D1
var wall (synth hiss)
set-param wall 'cutoff 2500
var ring (synth howl)
set-param ring 'freq 97
control 'drive 2 30 6
bind-control 'drive impact 'drive
control 'floor-cutoff 80 2000 240
bind-control 'floor-cutoff ground 'cutoff

# --- 2. patterns: strict grid, lopsided rhythms, silence as a part of the form ----------------------------------------
# (hits id s beats)        events for one instrument from a pattern of x and ~ (a token is a hit, its length the step)
function hits (id s beats) (pat-events (pat s beats) (function (tok) (function (t d) (note-at t id 0.05))))
# (euclid-hits id k n beats)   the same from a Euclidean rhythm of k hits over n steps
function euclid-hits (id k n beats) (hits id (join (map (euclid k n) (function (h) (if (equal? (type h) "nil") "~" "x"))) " ") beats)
# (blackout n cycle events)   nothing on every n-th cycle (the last of n): the silence that is part of the pattern
function blackout (n cycle events) (if (== (mod cycle n) (- n 1)) (list) events)

function impacts (cycle) (blackout 4 cycle (hits impact "x ~ ~ x ~ ~ x ~ ~ ~ x ~ x ~ ~ ~" 4))
function edges (cycle) (chance 0.75 (hits edge "x [x x] ~ x ~ x*3 ~ ~" 4))
function irons-line (cycle) (chance 0.6 (euclid-hits (head irons) 3 8 8))
function irons-line2 (cycle) (every 3 cycle (function (e) (chance 0.5 (euclid-hits (last irons) 5 13 8))) (list))
function floor-line (cycle) (list (synth-ev 0 14 ground (list)))                       # sixteen beats: rises for two, falls for the last
function wall-line (cycle) (every 2 cycle (function (e) (list (synth-ev 2 1.9 wall (list)))) (list))   # a burst, cut at the bar
function ring-line (cycle) (if (== (mod cycle 4) 1) (list (synth-ev 1 6 ring (list))) (list))

# --- 3. play ------------------------------------------------------------------------------------------------------------
live-loop 'impacts 4
live-loop 'edges 4
live-loop 'floor-line 16
print "playing at" (bpm) "bpm; loops:" (loops)

# --- 4. moves, one line at a time (Cmd-Alt-Enter on the line, or paste in the console) --------------------------------------
#   live-loop 'irons-line 8
#   live-loop 'irons-line2 8
#   live-loop 'wall-line 4
#   sweep impact 'drive 6 24 32                        the impacts harden over eight bars
#   set-control 'floor-cutoff 900                      the floor opens
#   live-loop 'ring-line 8
#   function impacts (cycle) (blackout 4 cycle (euclid-hits impact 5 16 4))
#   function impacts (cycle) (hits impact "x*4 ~ ~ ~ ~ x ~ ~ ~ x ~ x ~" 4)
#   function edges (cycle) (chance 0.9 (fast (hits edge "x ~ x [x x x] ~ x ~ ~" 4) 2))
#   set-param impact 'freq 29                          a fourth lower: the room starts to move
#   set-param wall 'cutoff 6000
#   stop-loops                                         the rupture: silence...
#   live-loop 'impacts 4                               ...and only the impacts back
#   tempo 96

# --- 5. a scripted arrangement when run from the command line ----------------------------------------------------------------
if (== (length args) 0) {
    var bar (/ 240 (bpm))
    sleep (* 8 bar)
    live-loop 'irons-line 8
    live-loop 'wall-line 4
    sleep (* 8 bar)
    sweep impact 'drive 6 24 32
    live-loop 'irons-line2 8
    sleep (* 8 bar)
    set-control 'floor-cutoff 900
    live-loop 'ring-line 8
    sleep (* 8 bar)
    stop-loops                                                   # the rupture
    sleep (* 1 bar)
    function impacts (cycle) (hits impact "x*4 ~ ~ ~ ~ x ~ ~ ~ x ~ x ~" 4)
    live-loop 'impacts 4
    sleep (* 4 bar)
    set-param impact 'freq 29
    live-loop 'edges 4
    live-loop 'floor-line 16
    live-loop 'wall-line 4
    sleep (* 8 bar)
    stop-loops
    sleep 4
    free-all
    audio-quit
    print "done"
}
