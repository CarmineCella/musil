# live_techno: harder, darker. Open it in the IDE, run blocks 1 to 3 with Cmd-Enter, then keep
# changing the pattern functions while it plays.
#
# The techno kit in live.mu: tkick (deep, saturated), rumble (its tail), that/tohat (metallic
# hats), tclap, perc (tuned), sub (bass), hoover (saws with portamento), tstab, tpad (with an
# amp parameter for ducking), zap; and duck, sweep, accents on top of the pattern tools.
load "live.mu"
audio-init
var sr (audio-sr)
tempo 134

# --- 1. instruments ------------------------------------------------------------------
var kit (techno-kit)                         # kick rumble hat ohat clap perc: tokens bd rm hh oh cp pc
set-param (kit-get kit 'perc) 'freq 220
var bass (synth sub)
var lead (synth hoover)
set-params lead (list (list 'cutoff 700) (list 'glide 0.08) (list 'freq 110))
var stabs (poly tstab 3)
each stabs (function (id) (set-param id 'cutoff 1500))
var pads (poly tpad 3)
each pads (function (id) (set-params id (list (list 'cutoff 600) (list 'amp 1))))
control 'lead-cutoff 200 6000 700
bind-control 'lead-cutoff lead 'cutoff
control 'pad-cutoff 150 4000 600
each pads (function (id) (bind-control 'pad-cutoff id 'cutoff))

# --- 2. patterns ---------------------------------------------------------------------
function kicks (cycle) (drums kit "[bd rm] [bd rm] [bd rm] [bd rm]" 4)
function hats (cycle) (drums kit "[~ hh] [~ oh] [~ hh] [~ oh]" 4)
function claps (cycle) (drums kit "~ cp ~ cp" 4)
function percs (cycle) (pat-events (pat "pc ~ ~ pc ~ ~ pc ~ ~ ~ pc ~ ~ pc ~ ~" 4) (function (x) (function (t d) (note-at t (kit-get kit 'perc) 0.05))))
function bassline (cycle) (melody bass "a1 ~ a1 a1 ~ a1 ~ [a1 c2]" 4 (list))
function leadline (cycle) (every 2 cycle (function (e) (melody lead "a2 ~ ~ e3 ~ ~ g2 ~" 4 (list))) (melody lead "a2 ~ ~ c3 ~ ~ a2 ~" 4 (list)))
function stabline (cycle) (list (chord-ev 1.75 0.2 stabs (chord "A2" 'min) (list)) (chord-ev 3.5 0.2 stabs (chord "A2" 'min) (list)))
function padline (cycle) (stack (list (list (chord-ev 0 3.9 pads (chord "A2" 'min) (list))) (duck pads 4 0.15)))

# --- 3. play -------------------------------------------------------------------------
live-loop 'kicks 4
live-loop 'hats 4
live-loop 'bassline 4
print "playing at" (bpm) "bpm; loops:" (loops)

# --- 4. moves, one line at a time (Cmd-Alt-Enter on the line, or paste in the console) --------
#   live-loop 'claps 4
#   live-loop 'percs 4
#   live-loop 'leadline 4
#   sweep lead 'cutoff 700 4000 16                    the hoover opens over 16 beats
#   live-loop 'stabline 4
#   live-loop 'padline 4                              the pad pumps under the kick (duck)
#   function hats (cycle) (drums kit "[hh hh] [hh oh] [hh hh] [hh oh*2]" 4)
#   function kicks (cycle) (every 8 cycle (function (e) (drums kit "[bd rm] [bd rm] [bd rm] [bd bd bd bd]" 4)) (drums kit "[bd rm] [bd rm] [bd rm] [bd rm]" 4))
#   function percs (cycle) (pat-events (pat "pc*3 ~ pc ~ pc*2 ~" 4) (function (x) (function (t d) (note-at t (kit-get kit 'perc) 0.05))))
#   set-param lead 'glide 0.3                         longer portamento
#   function bassline (cycle) (chance 0.8 (melody bass "a1*8" 4 (list)))
#   stop-loop 'kicks                                  the breakdown...
#   set-control 'pad-cutoff 300
#   sweep lead 'cutoff 4000 300 8
#   live-loop 'kicks 4                                ...and back
#   set-control 'pad-cutoff 1500
#   tempo 138

# --- 5. a scripted arrangement when run from the command line ------------------------------
if (== (length args) 0) {
    var bar (/ 240 (bpm))
    sleep (* 4 bar)
    live-loop 'claps 4
    live-loop 'percs 4
    sleep (* 4 bar)
    live-loop 'leadline 4
    sweep lead 'cutoff 700 4000 16
    sleep (* 4 bar)
    live-loop 'stabline 4
    live-loop 'padline 4
    sleep (* 8 bar)
    stop-loop 'kicks
    stop-loop 'claps
    sweep lead 'cutoff 4000 300 8
    sleep (* 4 bar)
    live-loop 'kicks 4
    live-loop 'claps 4
    sweep lead 'cutoff 300 4000 4
    sleep (* 8 bar)
    stop-loops
    sleep 1.5
    free-all
    audio-quit
    print "done"
}
