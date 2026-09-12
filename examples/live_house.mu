# live_house: four-on-the-floor, live-coded. Open this file in the IDE and run it block by
# block (Cmd-Enter on each block, in order), then keep editing the pattern blocks and
# re-running them: each change is heard at the next bar.
#
# What live.mu gives for this: a kit of drum synths (kick snare clap hat ohat), an acid
# bass, a chord stab and a pad, note names (hz, chord), a step notation for patterns
# ("bd ~ sn ~": spaces are steps, ~ a rest, [a b] a subdivision, a*2 a repeat), Euclidean
# rhythms, swing, humanize, sometimes, and controls for the hands. (Mind the names: beat,
# bassline, drums, hat are library functions; a pattern function needs a name of its own.)
load "live.mu"
audio-init
var sr (audio-sr)
tempo 124

# --- 1. instruments ------------------------------------------------------------------
var kit (house-kit)                          # kick snare clap hat ohat, as synths
var bass (synth acid)
set-params bass (list (list 'cutoff 900) (list 'res 4))
var stabs (poly stab 4)                      # four voices for chords
each stabs (function (id) (set-param id 'cutoff 2200))
var pads (poly pad 3)
each pads (function (id) (set-param id 'cutoff 900))
control 'cutoff 200 5000 900                 # the hands: (controls) opens the sliders
bind-control 'cutoff bass 'cutoff
control 'stab-cutoff 300 6000 2200
each stabs (function (id) (bind-control 'stab-cutoff id 'cutoff))

# --- 2. patterns: functions of the cycle, four beats each ------------------------------
function kicks (cycle) (drums kit "bd bd bd bd" 4)
function hats (cycle) (swing (drums kit "[hh hh] [hh oh] [hh hh] [hh oh]" 4) 0.18)
function backbeat (cycle) (every 4 cycle (function (e) (stack (list e (drums kit "~ ~ ~ [cp cp]" 4)))) (drums kit "~ cp ~ cp" 4))
function bassline (cycle) (melody bass "a1 ~ a1 a2 ~ a1 g1 a2" 4 (list (list 'res 4)))
function chords (cycle) \
    (list (chord-ev 0.5 0.4 stabs (chord "A3" 'min7) (list)) \
          (chord-ev 2.5 0.4 stabs (chord "G3" 'maj7) (list)) \
          (chord-ev 3.5 0.3 stabs (chord "F3" 'maj7) (list)))

# --- 3. play: each loop reads its function every cycle ---------------------------------
live-loop 'kicks 4
live-loop 'hats 4
live-loop 'backbeat 4
live-loop 'bassline 4
live-loop 'chords 4
print "playing at" (bpm) "bpm; loops:" (loops)

# --- 4. things to change while it plays (edit, then Cmd-Enter on the block) ----------------
# a different bass line, with a Euclidean accent:
#   function bassline (cycle) (melody bass "a1 a1 [~ a1] c2 a1 [~ g1] e2 ~" 4 (list))
# an off-beat open hat:
#   function hats (cycle) (drums kit "hh oh hh oh" 4)
# a fill every 8 bars:
#   function kicks (cycle) (every 8 cycle (function (e) (drums kit "bd bd bd [bd bd bd bd]" 4)) (drums kit "bd bd bd bd" 4))
# thin the hats out at random:
#   function hats (cycle) (chance 0.7 (drums kit "hh*8" 4))
# a breakdown: stop the drums, hold a pad chord, bring them back
#   stop-loop 'kicks
#   stop-loop 'backbeat
#   function padline (cycle) (list (chord-ev 0 3.8 pads (chord "A2" 'min) (list)))
#   live-loop 'padline 4
#   set-control 'cutoff 300
#   ... and then: live-loop 'kicks 4   live-loop 'backbeat 4   stop-loop 'padline   set-control 'cutoff 1500
# the filter, by hand: (controls), or from code, ramped over 8 beats:
#   set-param bass 'cutoff 3000 (* 8 (/ 60 (bpm)))
# tempo: tempo 128

# --- 5. a scripted arrangement, so the file also plays on its own from the command line -----
if (== (length args) 0) {
    sleep (* 8 (/ 240 (bpm)))                # 8 bars
    set-control 'cutoff 2500
    function bassline (cycle) (melody bass "a1 a1 [~ a1] c2 a1 [~ g1] e2 ~" 4 (list))
    sleep (* 8 (/ 240 (bpm)))
    stop-loop 'kicks
    stop-loop 'backbeat
    function padline (cycle) (list (chord-ev 0 3.8 pads (chord "A2" 'min) (list)))
    live-loop 'padline 4
    sleep (* 4 (/ 240 (bpm)))
    stop-loop 'padline
    live-loop 'kicks 4
    live-loop 'backbeat 4
    sleep (* 8 (/ 240 (bpm)))
    stop-loops
    sleep 1
    free-all
    audio-quit
    print "done"
}
