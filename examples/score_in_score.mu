# score_in_score: a score is a thing a score can hold, next to ordinary notes. Two phrases are
# written as scores of their own (atonal lines on the oboe over a cello pedal, a chordal one on
# the horn), a section places them as events among its own notes, and a piece places the
# section twice, the second time with a texture over it. Everything renders as one, in the
# hall. In the roll, a "score" bar sits on the scores row: double-click it to open that score.
# Usage: musil score_in_score.mu
load "music.mu"
seed 5
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
# var db (db-load "../datasets/TinySOL.spectrum.db")           # the full TinySOL, after ./fetch_tinysol.sh (or any *SOL set)
var sr 44100
var row (list "C4" "F#4" "D4" "G#4" "E4" "A#4" "B3" "F4")     # an all-interval-ish row; the phrases draw on it

# --- 1. phrases as scores ---------------------------------------------------------------------------------
function phrase (name pitches) {
    var p (score name sr)
    add! p 0 (notes db 'Ob 'mf 'ord pitches (beats 96 (rhythm (list 1 0.5 0.5 1 -0.5 0.5 0.75 0.25))))
    event-at p 0 (* 5 (/ 60 96)) (note db 'Vc (- (pitch->number (head pitches)) 12) 'p 'ord) -20 0   # a pedal an octave under the first note
    return p
}
var a (phrase "phrase a" row)
var b (phrase "phrase b" (transpose-pitches (retrograde row) 3))                          # backwards, a minor third up
var c (fragment->score "chordal" sr (chords db 'Hn 'p 'ord (list (list "C4" "F#4") (list "D#4" "G4") (list "C#4" "G#4")) (beats 96 (rhythm (list 1.5 1.5 3)))))
print "phrases:" (fixed (score-duration a) 2) (fixed (score-duration b) 2) (fixed (score-duration c) 2) "s"

# --- 2. a section: the phrases as events, and its own notes between and over them --------------------------
var section (score "section" sr)
event section 0 0 a                                                                  # the whole phrase
event-at section 3.5 0 b 40 0                                                        # placed at the left
event section 7 1.2 a                                                                # only its first 1.2 s
event section 8.5 0 c
add-placed! section 1 (notes db 'Vn 'pp 'ord (list "A#5" nil "G5" "C#5") (beats 96 (rhythm (list 0.5 0.5 1 2)))) -30 20   # violin notes of the section itself
add! section 6 (arpeggio db 'Vn 'p 'ord (list "D4" "G#4" "C#5" "F5") 0.15 'up 1.5)   # an arpeggio, one event per note
event-at section 9.5 2 (note db 'Vc "F#3" 'ff 'ord) 0 0                            # a single long cello note
print "section:" (length (score-events section)) "events, of which" (length (score-select section (function (e) (equal? (get e 'kind) 'score)))) "are scores," (fixed (score-duration section) 2) "s"

# --- 3. the piece: the section, then the section again with a four-voice texture over it --------------------------
var piece (score "piece" sr)
event piece 0 0 section
var second (score "section 2" sr)
event second 0 0 section
add-placed! second 0 (texture db (list 'Vn 'Vc 'Ob 'Hn) 'pp 'ord (list "G#4" "A4" "F4" "F#4" "E4") (beats 96 (rhythm (list 0.25 0.25 0.5 0.25))) (list 1 1.15 1.3 1.5)) 30 10
event piece (+ (score-duration section) 1) 0 second
event-at piece (+ (score-duration section) 0.5) 1 (note db 'Ob "B4" 'ff 'ord) 0 30   # a note of the piece itself, between the sections
print "a score in itself:" (try (event piece 0 0 piece) catch err "refused")
score-print piece
print "the roll: two section bars and one note; double-click a section to open it, then a phrase inside it"

render piece "/tmp/musil_score_in_score.wav" "stereo"
print "wrote /tmp/musil_score_in_score.wav"
display piece
# play it: press Play in the roll, or (play-score piece 0.8) here
