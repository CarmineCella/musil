# live_trance: supersaws, an arpeggio, an off-beat bass, a gated pad, a build with a riser and a
# roll, at 138. Open it in the IDE, run blocks 1 to 3, then keep changing things (section 4).
#
# From live.mu: supersaw (seven detuned saws), pluck, offbass, riser, trance-kit; arp / arp-events
# (up, down, updown, random arpeggios), gater (the trance gate on a pad's amp), roll, chord, and
# duck and sweep from the techno set.
load "live.mu"
audio-init
var sr (audio-sr)
tempo 138

# --- 1. instruments -----------------------------------------------------------------------
var kit (trance-kit)                          # bd cp hh oh
var bass (synth offbass)
var plucks (synth pluck)
set-param plucks 'cutoff 3000
var leads (poly supersaw 3)
each leads (function (id) (set-params id (list (list 'cutoff 1800) (list 'spread 14))))
var pads (poly tpad 3)
each pads (function (id) (set-params id (list (list 'cutoff 1200) (list 'amp 1))))
var rise (synth riser)
set-param rise 'rate 8
control 'lead-cutoff 300 8000 1800
each leads (function (id) (bind-control 'lead-cutoff id 'cutoff))
control 'pluck-cutoff 500 8000 3000
bind-control 'pluck-cutoff plucks 'cutoff

# --- 2. patterns (four beats a cycle; the progression Am F C G, one chord per cycle) ---------
var progression (list (chord "A3" 'min) (chord "F3" 'maj) (chord "C4" 'maj) (chord "G3" 'maj))
function chord-of (cycle) (getidx progression (mod cycle 4))
function kicks (cycle) (drums kit "bd bd bd bd" 4)
function hats (cycle) (drums kit "[~ hh] [~ oh] [~ hh] [~ oh]" 4)
function claps (cycle) (drums kit "~ cp ~ cp" 4)
function bassline (cycle) (melody bass "~ a1 ~ a1 ~ a1 ~ a1" 4 (list))
function arpline (cycle) (arp-events plucks (map (chord-of cycle) (function (n) (+ n 12))) 'updown 16 4 (list))
function leadline (cycle) (list (chord-ev 0 3.9 leads (chord-of cycle) (list)))
function padline (cycle) (stack (list (list (chord-ev 0 3.9 pads (map (chord-of cycle) (function (n) (- n 12))) (list))) (gater pads "x ~ x x ~ x ~ x x ~ x ~ x x ~ x" 4 0.1)))

# --- 3. play ------------------------------------------------------------------------------
live-loop 'kicks 4
live-loop 'hats 4
live-loop 'bassline 4
live-loop 'arpline 4
print "playing at" (bpm) "bpm; loops:" (loops)

# --- 4. moves --------------------------------------------------------------------------------
#   live-loop 'claps 4
#   live-loop 'leadline 4                             the supersaw chords
#   live-loop 'padline 4                              the gated pad
#   function arpline (cycle) (arp-events plucks (map (chord-of cycle) (function (n) (+ n 12))) 'random 16 4 (list))
#   function arpline (cycle) (arp-events plucks (map (chord-of cycle) (function (n) (+ n 24))) 'up 8 4 (list))
#   function padline (cycle) (stack (list (list (chord-ev 0 3.9 pads (map (chord-of cycle) (function (n) (- n 12))) (list))) (gater pads "x ~ ~ x ~ x ~ ~" 4 0.05)))
#   sweep (head leads) 'cutoff 400 6000 32
#   the build: a riser over 8 bars, a clap roll on the last, then everything back
#   note rise 16
#   function rolls (cycle) (roll (kit-get kit 'clap) 4 24)
#   live-loop 'rolls 4      ...then stop-loop 'rolls
#   stop-loop 'kicks   stop-loop 'bassline   (the breakdown)   live-loop 'kicks 4   live-loop 'bassline 4
#   set-control 'lead-cutoff 6000
#   tempo 140

# --- 5. a scripted arrangement when run from the command line ----------------------------------
if (== (length args) 0) {
    var bar (/ 240 (bpm))
    sleep (* 4 bar)
    live-loop 'claps 4
    live-loop 'leadline 4
    sleep (* 4 bar)
    live-loop 'padline 4
    each leads (function (id) (sweep id 'cutoff 1800 6000 16))
    sleep (* 4 bar)
    stop-loop 'kicks
    stop-loop 'bassline
    stop-loop 'claps
    note rise 8
    function rolls (cycle) (roll (kit-get kit 'clap) 4 24)
    sleep (* 3 bar)
    live-loop 'rolls 4
    sleep (* 1 bar)
    stop-loop 'rolls
    live-loop 'kicks 4
    live-loop 'bassline 4
    live-loop 'claps 4
    sleep (* 8 bar)
    stop-loops
    sleep 1.5
    free-all
    audio-quit
    print "done"
}
