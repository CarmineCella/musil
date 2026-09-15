# simple_score: a score written by hand, event by event, so that every kind of event is in view.
# The notes are the first phrase of the chorale "O Haupt voll Blut und Wunden" as Bach harmonised
# it (BWV 244/54, the first four bars, in D minor), the four voices on violin, oboe, horn and
# cello; then a sound file, a buffer, a synth and a function join in.
# Usage: musil simple_score.mu      (renders, opens the player, and plays when at a terminal)
load "music.mu"

var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
# var db (db-load "../datasets/TinySOL.spectrum.db")           # the full TinySOL, after ./fetch_tinysol.sh (or any *SOL set)
var s (score "chorale" 44100)
var q 0.6                                  # a quarter note, in seconds (100 bpm)

# --- the chorale: (voice pitch dynamics), one list per beat; each voice is an instrument of the database ---
# soprano on the violin, alto on the oboe, tenor on the horn, bass on the cello; MicroSOL has only C4-G4 of
# each, so most notes are shifted from the nearest sound; a full TinySOL plays every pitch from its own recording
function chord (beat dur sop alt ten bas) {
    event-at s (* beat q) (* dur q) (note db 'Vn sop 'mf 'ord) 30 0
    event-at s (* beat q) (* dur q) (note db 'Ob alt 'mf 'ord) 10 0
    event-at s (* beat q) (* dur q) (note db 'Hn ten 'mf 'ord) -10 0
    event-at s (* beat q) (* dur q) (note db 'Vc bas 'mf 'ord) -30 0
}
# upbeat and bar 1
chord 0 1 'A4 'F4 'D4 'D3                   # upbeat: d minor
chord 1 1 'D5 'F4 'A3 'D3                   # bar 1, beat 1
chord 2 1 'C5 'F4 'A3 'F3                   # beat 2 (the c natural of the modal turn)
chord 3 1 "A#4" 'G4 'G3 'G3                 # beat 3: a sharp is written as a string
chord 4 1 'A4 'F4 'C4 'F3                   # beat 4
# bar 2
chord 5 2 'A4 'E4 'C4 'A2                   # a half note: the cadence on A
chord 7 1 'A4 'E4 'C4 'A3                   # ... and the upbeat of the next phrase
chord 8 1 'D5 'F4 'A3 'D3
chord 9 1 'C5 'F4 'A3 'F3
chord 10 1 "A#4" 'G4 'G3 'G3
chord 11 1 'A4 'F4 'C4 'F3
chord 12 2 'A4 'E4 'C4 'A2

# --- the other kinds of event, after the chorale --------------------------------------------------------
var end (* 14 q)
event-at s end 2.5 "data/gong_c_sharp.wav" 0 20                           # a sound file, placed above and in front
event s (+ end 1) 1.5 (* 0.3 (sine 44100 293.66 1.5))                    # a buffer: a D4 sine, 1.5 s
function pluck-d (gate freq cutoff) (* (adsr 44100 gate 0.005 0.2 0.3 0.3) (lowpass (osc 44100 (sig gate freq) saw-table) 44100 cutoff 2))
event-at s (+ end 2.5) 1 (instrument pluck-d (list (list 'freq 146.83) (list 'cutoff 1500))) -45 0   # a synth: D3 pluck at the right
function bell (d) (* 0.4 (fade-out (sine 44100 1174.66 d) 2000))          # a function of the duration: a D6 tone, faded
event-at s (+ end 3.5) 1.2 bell 45 30                                      # ... called with the event's duration

score-print s
render s "/tmp/musil_chorale.wav" "stereo"
print "wrote /tmp/musil_chorale.wav"
display s                                  # the player: Play from the cursor, drag the bars, Render..., Export...
if (interactive?) { play-score s 0.7 }
