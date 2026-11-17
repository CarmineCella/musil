; notation: a score to Dorico (or Sibelius, Finale, MuseScore) as MusicXML. A score's times are seconds; a part
; is written in beats and bars, so the export quantizes: at the score's tempo and meter every beat of every part
; takes the division (binary, triplet, quintuplet, sextuplet) that moves its notes least, a small cost keeping the
; simpler one when it is nearly as good ('tolerance: a displacement about that size is accepted). Every note
; carries its instrument, pitch (quarter tones as accidentals), dynamics and the technique tag of the database as
; it is (ord, pizz-lv, trem...); files, synths and calls go on one-line staves with their labels.
; Three kinds of material, each a different problem for the quantizer: a line written in beats (exact), a chord
; from Orchidea (players at once: chords and voices), a granular cloud (free timing: tuplets, or not, by tolerance).
; Usage: musil notation.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 1)
var db (db-load-first (list "../datasets/TinySOL.spectrum.db" "data/microsol/microsol.spectrum.db"))   ; TinySOL after ./fetch_tinysol.sh, else the bundled MicroSOL
var sr 44100
var here (db-instruments-available db)
var orch (orchestra (filter (list 'Ob 'Hn 'Vn 'Vn 'Vc) (function (i) (contains? here (str i)))))
var s (score "notation" sr)
score-tempo! s 96                                                ; a quarter is 0.625 s
score-meter! s 4 4

; --- 1. a line written in beats: quarters, eighths, a triplet, a quarter tone, a chord; it comes back exactly -------------
var b (/ 60 96)
add! s 0 (notes db 'Vn 'mf 'ord (list "C4" "D4" "E4" "F4" "G4" "E4") (list b (* b 0.5) (* b 0.5) (/ b 3) (/ b 3) (/ b 3)))
add! s (* 3 b) (list (list 0 (* 3 b) (note-cents! (note db 'Vn "F4" 'p 'ord) 50)))                       ; F quarter-sharp, over the bar
event s (* 4 b) (* 2 b) (chord db 'Vc 'mf 'ord (list "C3" "G3"))
add! s (* 6 b) (notes db 'Vc 'ff 'pizz-lv (list "C3" "C3" "C3") (list (* b 0.25) (* b 0.25) (* b 0.5)))

; --- 2. a chord orchestrated by Orchidea: the bell, the players at once (chords where they share a rhythm, voices otherwise)
var w (read-wav "data/archeos-bell.wav")
var bell (head (getidx w 1))
var mimetic (orchestrate-mimetic db orch bell (head w) (record (list 'population 60 'epochs 40 'sparsity 0.01 'threshold 2 'partials 0 'solutions 3)))
connect! s (* 8 b) mimetic (get mimetic 'choices)

; --- 3. a granular cloud: free timing ----------------------------------------------------------------------------------
var cloud (orchestrate-granular db orch (* 8 b) (record (list 'density (list 3 6) 'duration (list 0.15 1) 'register (list 4 4) 'dynamics 0.4)))
connect! s (* 16 b) cloud (list 0)
event s (* 16 b) (* 8 b) "data/cage.wav"                          ; a sound file: a one-line staff with its name

; --- the export: MusicXML, and what the quantization did ---------------------------------------------------------------
var m (score-musicxml s "/tmp/musil_notation.musicxml" (record (list)))
notation-report m                                                ; per part: notes, voices, divisions used, how far onsets moved
var loose (score-notate s (record (list 'tolerance 0.15)))       ; the same, accepting larger displacements: fewer tuplets
print "with tolerance 0.15, the cloud's divisions:" (get (last (filter (get loose 'parts) (function (p) (equal? (get p 'code) "Vn")))) 'chosen)
var plain (score-musicxml s "/tmp/musil_notation_plain.musicxml" (record (list 'divisions (list 1 2 4 8))))   ; no tuplets at all
print "wrote /tmp/musil_notation.musicxml and /tmp/musil_notation_plain.musicxml: open them in Dorico"

; --- hearing the grid: the score's times moved to it, the roll showing them ----------------------------------------------
score-quantize! s (record (list))
render s "/tmp/musil_notation.wav" "stereo"
display s
