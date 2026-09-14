# live_spatial: a sound turning around your head, live, for headphones
# Usage: musil live_spatial.mu   (or open it in the IDE; the controls window has the direction)
#
# The same spatial functions as offline (spatial.mu), inside an instrument: ambi-encode at an
# azimuth and an elevation that are hot parameters, then binaural-decode (a Musil function of
# convolutions with the KEMAR head's measured impulse responses, inlined into the graph). A
# loop moves the azimuth every beat; the controls window lets you place it by hand. The voice
# is broadband on purpose (saws through a lowpass at 3 kHz): height and behind are cues in
# the spectrum above 4 kHz, so a dull sound hardly shows them.
load "live.mu"
audio-init
var sr (audio-sr)

# a voice: two detuned saws through a lowpass, placed in 3D and rendered for the two ears
function voice (gate freq cutoff az el) (binaural-decode (ambi-encode (* 0.5 (adsr sr gate 0.02 0.2 0.7 0.3) (lowpass (+ (osc sr (sig gate freq) saw-table) (osc sr (sig gate (* 1.005 freq)) saw-table)) sr cutoff 1)) 1 az el) sr)
var v (synth voice)
set-params v (list (list 'freq 220) (list 'cutoff 3000) (list 'az 0) (list 'el 0))
note-on v
control 'azimuth -180 180 0
control 'elevation -40 80 0
bind-control 'azimuth v 'az
bind-control 'elevation v 'el
controls
print "turning: one revolution every 8 s; the controls window sets the direction by hand (Esc closes it)"

# the turn: every beat the azimuth moves by 15 degrees, ramped, so it glides (a loop of one event)
tempo 120
function turn (cycle) (list (ev 0 1 (function (t d) (set-control 'azimuth (princarg-deg (* 15 cycle))))))
live-loop 'turn 1

# a second sound, a click, at the opposite side, so the space has two points
function tick (gate az) (binaural-decode (ambi-encode (* 0.4 (adsr sr gate 0.001 0.05 0 0.02) (noise gate)) 1 az 20) sr)
var k (synth tick)
function ticks (cycle) (list (ev 0.5 0.1 (function (t d) {
    set-param k 'az (princarg-deg (+ 180 (* 15 cycle))) 0 t
    note-at t k 0.02
})))
live-loop 'ticks 1

if (== (length args) 0) {
    sleep 16
    stop-loops
    note-off v
    sleep 1
    free-all
    audio-quit
    print "done"
}
