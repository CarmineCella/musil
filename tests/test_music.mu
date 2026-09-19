# test_music: the score, the database, notes, rendering, playing, the roll
load "test.mu"
load "music.mu"

# --- the score and its events ---
var s (score "t" 8000)
check (equal? (get s 'name) "t") "score: name"
check (== (score-sr s) 8000) "score-sr"
check (== (score-duration s) 0) "score-duration: empty"
var buf (sine 8000 440 0.5)
var e1 (event s 1 0.25 buf)
check (equal? (get e1 'kind) 'buffer) "event: a vector is a buffer"
check (== (get e1 'at) 1) "event: at"
check (== (score-duration s) 1.25) "score-duration"
var e2 (event s 0 0.5 "../examples/data/drums.wav")
check (equal? (get e2 'kind) 'file) "event: a path is a file"
check (equal? (get e2 'label) "drums.wav") "event: file label"
var e3 (event s 2 0.5 (list buf buf))
check (equal? (get e3 'kind) 'buffer) "event: a list of vectors is a multichannel buffer"
function tone (d) (sine 8000 220 d)
var e4 (event s 3 0.3 (call tone (list 'dur)))
check (equal? (get e4 'kind) 'call) "call: kind"
var e5 (event s 3.5 0.2 tone)
check (equal? (get e5 'kind) 'call) "event: a bare function is a call"
var sr 8000
function ping (gate freq) (* (adsr sr gate 0.01 0.1 0.5 0.1) (osc sr (sig gate freq) sine-table))
var e6 (event s 4 0.3 (instrument ping (list (list 'freq 440))))
check (equal? (get e6 'kind) 'synth) "instrument: kind"
check (contains? (error-of (function () (event s 0 -1 buf))) ">= 0") "event: negative duration"
check (contains? (error-of (function () (event s 0 1 'nope))) "cannot place") "event: an unknown payload"
check (== (length (score-events s)) 6) "score-events"
var e7 (event-at s 5 0.2 buf 90 0)
check (== (get e7 'az) 90) "event-at: azimuth"
event-move! e7 6
event-length! e7 0.1
event-gain! e7 0.5
check (and (== (get e7 'at) 6) (== (get e7 'dur) 0.1) (== (get e7 'gain) 0.5)) "event-move!, event-length!, event-gain!"

# --- rendering ---
var r1 (render-event e1 8000)
check (== (length r1) 1) "render-event: a buffer, one channel"
check (== (length (head r1)) 2000) "render-event: cut to the duration"
check (< (max (abs (drop (head r1) 1990))) (max (abs (take (head r1) 100)))) "render-event: faded at the end when longer"
check (== (length (head (render-event e4 8000))) 2400) "render-event: a call with 'dur among its arguments"
check (== (length (head (render-event e5 8000))) 1600) "render-event: a bare function is called with the duration"
check (== (length (render-event e3 8000)) 2) "render-event: multichannel buffer keeps its channels"
check (== (length (head (render-event e2 8000))) 4000) "render-event: a file, resampled to the score's rate, cut"
check (> (length (head (render-event e6 8000))) 2400) "render-event: a synth renders with its release"
check (== (length (fit-duration (ones 100) 1 8000)) 100) "fit-duration: shorter sounds are left alone"
var mix (score-render s "stereo")
check (== (length mix) 2) "score-render: stereo"
check (== (length (head mix)) (+ (floor (* 6.1 8000)) (floor (* 0.6 8000)))) "score-render: as long as the score plus room for releases"
check (== (length (score-render s "mono")) 1) "score-render: mono"
check (== (length (score-render s "ambi 2")) 9) "score-render: ambisonics"
check (== (length (score-render s "binaural")) 2) "score-render: binaural"
check (== (length (score-render s (speaker-ring 6))) 6) "score-render: a ring of speakers"
check (== (length (score-render s 4)) 4) "score-render: a number of channels"
var placed (place (list buf) "stereo" 90 0 8000)
check (and (> (rms (head placed)) 0.5) (< (rms (last placed)) 1e-6)) "place: a mono sound at the left"
check (== (length (place (list buf buf) "stereo" 90 0 8000)) 2) "place: a stereo sound onto stereo goes as it is"
check (contains? (error-of (function () (score-render s "quad-ish"))) "unknown") "layout: unknown"
render s "/tmp/musil_test_score.wav" "stereo"
check (== (head (read-wav "/tmp/musil_test_score.wav")) 8000) "render: a file at the score's rate"

# --- the roll ---
var rows (score-rows s)
check (equal? (map rows head) (list "files" "buffers" "synths" "calls")) "score-rows: kinds in order"
var roll (score-roll s)
check (equal? (head (head (getidx roll 1))) "roll") "score-roll: a roll layer"
check (== (length (getidx (head (getidx roll 1)) 2)) 7) "score-roll: one bar per event"
check (equal? (event-lanes (list (record (list 'at 0 'dur 2)) (record (list 'at 1 'dur 1)) (record (list 'at 3 'dur 1)))) (list 0 1 0)) "event-lanes: overlaps get their own lane"
var bar (head (getidx (head (getidx roll 1)) 2))
check (== (length bar) 13) "score-roll: a bar is row start dur label tip group lane lanes midi dyn event-id tech cents"
check (== (getidx bar 8) -1) "score-roll: an unpitched event has no pitch"
check (== (length bar) 13) "score-roll: ... tech cents at the end"
check (== (last (head (getidx roll 1))) (register-score s)) "score-roll: carries the score's number"
check (equal? (displayed-score (register-score s)) s) "displayed-score"
check (equal? (type (play-event (register-score s) (get e1 'id))) "nil") "play-event: one event, alone"
check (contains? (error-of (function () (play-event (register-score s) 999))) "no event") "play-event: unknown event"
check (equal? (map (getidx (head (getidx roll 1)) 1) last) (list "none" "none" "none" "none")) "score-roll: unpitched rows have no clef"
save-png roll "/tmp/musil_test_roll.png" 600 300
check (exists? "/tmp/musil_test_roll.png") "the roll draws"

# --- transformations ---
score-shift! s 1
check (== (get e2 'at) 1) "score-shift!"
score-scale! s 2
check (and (== (get e2 'at) 2) (== (get e2 'dur) 1)) "score-scale!"
check (== (length (score-select s (function (e) (equal? (get e 'kind) 'call)))) 2) "score-select"
score-remove! s e7
check (== (length (score-events s)) 6) "score-remove!"
score-sort! s
check (== (get (head (score-events s)) 'at) 2) "score-sort!"
var m (score-merge s s)
check (== (length (score-events m)) 12) "score-merge"

# --- the database and notes ---
var db (db-load "../examples/data/microsol/microsol.spectrum.db")
check (== (db-size db) 32) "db-load: entries"
check (ends-with? (get db 'root) "microsol/microsol") "db-load: the sounds' folder next to the feature file, found by content"
check (== (get db 'ncoeff) 1024) "db-load: coefficients"
check (equal? (get db 'type) "spectrum") "db-load: type"
check (equal? (db-instruments db) (list "Ob" "Hn" "Vn" "Vc")) "db-instruments: orchestral order"
check (equal? (db-techniques db) (list "ord")) "db-techniques"
check (equal? (db-dynamics db) (list "mf" "ff")) "db-dynamics"
check (== (length (db-query db 'Vn nil 'mf nil)) 8) "db-query: instrument and dynamics"
check (== (length (db-query db 'Vn 'C4 'mf 'ord)) 1) "db-query: exact"
check (== (length (db-query db 'Nope nil nil nil)) 0) "db-query: none"
check (equal? (db-range db 'Vn) (list 60 67)) "db-range: C4 to G4"
check (== (last (db-range db 'Hn)) 76) "db-range: the horn's E5"
check (contains? (error-of (function () (db-range db (quote Nope)))) "no pitched sounds") "db-range: unknown instrument"
var e (head (db-query db 'Vn 'C4 'mf 'ord))
check (== (get e 'midi) 60) "entry: midi"
check (equal? (get e 'other) "4c") "entry: the other field (N when absent)"
check (equal? (get (head (db-query db 'Hn 'C4 nil nil)) 'other) "N") "entry: no other field"
check (== (length (db-features e)) 1024) "db-features"
check (db-available? db e) "db-available?: a bundled sound"
check (== (length (db-available db)) 32) "db-available: every sound"
check (ends-with? (db-path db e) "Strings/Vn/Vn-ord-C4-mf-4c.wav") "db-path: relative to the sounds' folder"
check (ends-with? (db-path db (head (db-query db 'Vn "C#4" 'mf 'ord))) "Vn-ord-C#4-mf-4c.wav") "db-path: a sharp on disk (a # in a symbol would start a comment: use a string)"
check (== (pitch->midi "C4") 60) "pitch->midi"
check (== (pitch->midi "Bb3") 58) "pitch->midi: flat"
check (== (pitch->midi "x") -1) "pitch->midi: not a pitch"
write "/tmp/musil_test_names.db" "spectrum 2048 256 2\n/Strings/Vn/Vn-pizz-lv-C4-mf-1c.wav;1;2\n/Strings/Vn/Vn-art-harm-sul-pont-A#3-ff-N.wav;1;2\n/Perc/Snare-hit-N-mf-N.wav;1;2\n/Winds/Fl/Fl-ord-C4-pp-N-N.wav;1;2\n"
var names (db-load "/tmp/musil_test_names.db")
check (equal? (map (get names 'entries) (function (x) (get x 'tech))) (list "pizz-lv" "art-harm-sul-pont" "hit" "ord")) "db-read: a technique of several tokens ends where the pitch begins"
check (equal? (map (get names 'entries) (function (x) (get x 'midi))) (list 60 58 -1 60)) "db-read: pitches, and -1 for an unpitched sound"
check (equal? (map (get names 'entries) (function (x) (get x 'dyn))) (list "mf" "ff" "mf" "pp")) "db-read: the dynamics after the pitch"
check (equal? (db-range names 'Vn) (list 58 60)) "db-range: over the pitched sounds only"
check (contains? (error-of (function () (db-range names 'Snare))) "no pitched") "db-range: an unpitched instrument"
check (not (contains? (db-pitches names) "N")) "db-pitches: no N"
check (equal? (midi->pitch 61) "C#4") "midi->pitch"
var n1 (note db 'Vn 'C4 'mf 'ord)
check (and (equal? (get n1 'kind) 'note) (== (get n1 'shift) 0)) "note: an exact match"
var n2 (note db 'Vn 'B4 'mf 'ord)
check (== (get n2 'shift) 4) "note: the nearest available sound (G4), shifted up 4"
var n3 (note db 'Vc 'A3 'pp 'ord)
check (and (equal? (get (get n3 'entry) 'instr) "Vc") (== (get n3 'shift) -3)) "note: same instrument when the dynamics are not on disk"
check (contains? (error-of (function () (note db 'Fl 'C5 'mf 'ord))) "no sound") "note: no sound of that instrument on disk"
check (contains? (error-of (function () (note db 'Vn 'zz 'mf 'ord))) "not a pitch") "note: not a pitch"
check (> (note-duration n1) 0.5) "note-duration"
check (< (note-duration n2) (note-duration (note db 'Vn 'G4 'mf 'ord))) "note-duration: shorter when shifted up"
var s2 (score "notes" 44100)
event s2 0 1 n1
event s2 0.5 1 n2
var nr (render-event (head (score-events s2)) 44100)
check (== (length (head nr)) 44100) "render-event: a note, cut to its duration"
check (> (rms (head nr)) 0.01) "render-event: a note sounds"
var before (length note-cache)
render-event (head (score-events s2)) 44100
check (== (length note-cache) before) "note-sound: a note rendered twice comes from the cache"
clear-sound-cache
check (== (length note-cache) 0) "clear-sound-cache: the notes too"
check (equal? (map (score-rows s2) head) (list "Vn")) "score-rows: notes by instrument"
var s3 (score "orch" 44100)
event s3 0 1 (note db 'Vc 'C4 'ff 'ord)
event s3 0 1 (note db 'Vn 'C4 'mf 'ord)
event s3 0 1 (note db 'Ob 'C4 'mf 'ord)
check (equal? (map (score-rows s3) head) (list "Ob" "Vn" "Vc")) "score-rows: orchestral order"

# --- more queries ---
check (== (length (db-query db (list 'Vn 'Vc) nil (list 'mf 'ff) nil)) 16) "db-query: lists match any of their members"
check (== (length (db-grep db "Vn-ord-C.?4-mf")) 2) "db-grep: a regular expression on the file name (C4 and C#4)"
check (contains? (error-of (function () (db-grep db "("))) "bad pattern") "db-grep: a bad pattern"
check (== (length (db-find db "Vc-ord-C")) 2) "db-find: a substring"
check (== (length (db-between db 'Vn "C4" "E4")) 5) "db-between: a pitch range"
check (== (length (db-between db 'Vn 60 64)) 5) "db-between: MIDI numbers too"
var near (db-nearest db e 3)
check (equal? (get (head near) 'file) (get e 'file)) "db-nearest: the entry itself comes first"
check (== (length near) 3) "db-nearest: n entries"
check (equal? (db-instruments-available db) (list "Ob" "Hn" "Vn" "Vc")) "db-instruments-available"

# --- db-gen: the features of the bundled sounds, against Orchidea's own ---
var sounds "../examples/data/microsol/microsol"
var n-made (db-gen sounds "/tmp/musil_test_gen.db" "spectrum" 2048 256 1024)
check (== n-made 32) "db-gen: every WAV under the folder"
var mine (db-load "/tmp/musil_test_gen.db")
check (== (db-size mine) 32) "db-gen: the file loads"
check (equal? (get mine 'type) "spectrum") "db-gen: the header"
var made (head (db-find mine "Vn-ord-C4"))
var fa (db-features made)
var fb (db-features e)
check (> (/ (dot fa fb) (* (norm fa) (norm fb))) 0.9999) "db-gen: the spectrum features equal Orchidea's"
check (== (db-gen sounds "/tmp/musil_test_gen2.db" "mfcc" 2048 256 13) 32) "db-gen: mfcc"
check (== (get (db-load "/tmp/musil_test_gen2.db") 'ncoeff) 13) "db-gen: the requested number of coefficients"
check (== (get (db-make sounds "/tmp/musil_test_gen3.db" "moments" 2048 256 4) 'ncoeff) 4) "db-make: generate and load"
check (contains? (error-of (function () (db-gen sounds "/tmp/x.db" "nope" 2048 256 4))) "unknown feature type") "db-gen: unknown type"
check (contains? (error-of (function () (db-gen sounds "/tmp/x.db" "spectrum" 1000 100 4))) "power of 2") "db-gen: block"
check (contains? (error-of (function () (db-gen "/nowhere" "/tmp/x.db" "spectrum" 1024 256 4))) "not a folder") "db-gen: folder"

# --- musical elements ---
seed 4
var rr (rhythm (list 0.5 0.5 -0.25 1))
check (== (rhythm-duration rr) 2.25) "rhythm-duration: pauses count"
check (equal? (beats 120 (list 1 -0.5)) (list 0.5 -0.25)) "beats: at a tempo"
var ch (chord db 'Vn 'mf 'ord (list "C4" "E4" nil "G4"))
check (and (equal? (get ch 'kind) 'chord) (== (length (get ch 'notes)) 3)) "chord: a payload of notes (rests dropped)"
var sc (score "el" 44100)
var ce (event sc 0 0.5 ch)
check (== (length (head (render-event ce 44100))) 22050) "chord: renders as one, cut to the duration"
var ln (notes db 'Ob 'mf 'ord (list "C4" "D4" "E4") (rhythm (list 0.25 0.5)))
check (== (length ln) 3) "notes: the shorter (the rhythm) repeats until the longer ends"
check (equal? (map ln head) (list 0 0.25 0.75)) "notes: onsets from the durations"
check (equal? (map (notes db 'Ob 'mf 'ord (list "C4" nil) (rhythm (list 0.5 -0.5 0.5))) head) (list 0 1)) "notes: rests (nil pitches, negative durations) take time and make no event"
check (== (length (notes db 'Ob 'mf 'ord (list) (rhythm (list 1)))) 0) "notes: nothing from nothing"
check (== (fragment-duration ln) 1) "fragment-duration"
check (equal? (map (fragment-shift ln 2) head) (list 2 2.25 2.75)) "fragment-shift"
check (equal? (map (fragment-scale ln 2) head) (list 0 0.5 1.5)) "fragment-scale"
check (== (length (fragment-repeat ln 3)) 9) "fragment-repeat"
check (== (fragment-duration (fragment-until ln 2.6)) 2.75) "fragment-until: repeated to cover the time"
var cs2 (chords db 'Hn 'mf 'ord (list (list "C4" "E4") (list "D4" "F4")) (rhythm (list 1 -0.5 1)))
check (and (== (length cs2) 2) (equal? (get (last (head cs2)) 'kind) 'chord)) "chords: chord payloads on a rhythm"
var tx (texture db (list 'Vn 'Vc) 'mf 'ord (list "C4" "D4") (rhythm (list 0.5 0.5)) (list 1 2))
check (== (length tx) 6) "texture: the faster voice repeats until the slowest has played once"
check (near? (fragment-duration tx) 2 1e-9) "texture: as long as the slowest voice"
check (equal? (unique (map tx (function (x) (get (last x) 'instr)))) (list "Vn" "Vc")) "texture: instruments cycled over the voices"
var pv (pivots db 'Ob 'mf 'ord (list "C4" "G4") 3 (rhythm (list 0.5 0.5 0.5)))
check (== (length pv) 6) "pivots: one line per pitch, one note per duration"
check (all? (map pv (function (x) (<= (abs (- (get (last x) 'midi) (if (< (get (last x) 'midi) 64) 60 67))) 3))) identity) "pivots: within the interval of the pitch"
var ci (chordinterp db 'Vn 'mf 'ord (list "C4" "E4" "G4") (list "D4" "F4" "A4") (rhythm (list 0.5 0.5 0.5)))
check (== (length ci) 3) "chordinterp: a chord per duration"
check (equal? (map (get (last (head ci)) 'notes) (function (n) (get n 'pitch))) (list "C4" "E4" "G4")) "chordinterp: starts at the first chord"
check (equal? (map (get (last (last ci)) 'notes) (function (n) (get n 'pitch))) (list "D4" "F4" "A4")) "chordinterp: ends at the second"
check (equal? (transpose-pitches (list "C4" nil "E4") 2) (list 62 nil 66)) "transpose"
check (equal? (invert (list 60 64) "C4") (list 60 56)) "invert"
check (equal? (scale-pitches "D4" 'dorian (list 0 1 2 7)) (list 62 64 65 74)) "scale-pitches"
check (== (get (note db 'Vn 62 'mf 'ord) 'midi) 62) "note: a MIDI number is a pitch too"
var nc (note-cents! (note db 'Vn "E4" 'mf 'ord) 50)
check (== (get nc 'cents) 50) "note-cents!"
var sc2 (score "cents" 44100)
var ec (event sc2 0 0.5 nc)
var plain (render-event (event sc2 0 0.5 (note db 'Vn "E4" 'mf 'ord)) 44100)
check (not (near? (head (render-event ec 44100)) (head plain) 1e-6)) "note-cents!: the sound is shifted by the cents"
check (equal? (getidx (head (getidx (head (getidx (score-roll sc2) 1)) 2)) 11) "ord") "score-roll: the technique in the bar"
check (== (getidx (head (getidx (head (getidx (score-roll sc2) 1)) 2)) 12) 50) "score-roll: the cents in the bar"
var placed (add! sc 1 ln)
check (== (length placed) 3) "add!: the fragment's events in the score"
check (== (get (head placed) 'at) 1) "add!: at the offset"
check (== (get (head (add-placed! sc 2 ln 45 0)) 'az) 45) "add-placed!"
var fs (fragment->score "frag" 44100 ln)
check (and (equal? (get fs 'name) "frag") (== (length (score-events fs)) 3)) "fragment->score"

# --- more elements ---
seed 7
score-tempo! sc 120
check (== (beat->sec sc 4) 2) "score-tempo!, beat->sec"
check (equal? (map (add-beats! sc 2 ln) (function (e) (get e 'at))) (list 1 1.125 1.375)) "add-beats!: times in beats at the tempo"
check (near? (get (last (last (fragment-gain ln 0.2 1))) 'gain) 0.8 1e-9) "fragment-gain: a ramp"
check (== (get (head (add! sc 0 (fragment-gain ln 0.2 1))) 'gain) 0.2) "add!: a payload's gain is kept"
check (== (get (head (add! sc 0 (fragment-place ln 30 5))) 'az) 30) "fragment-place: kept by add!"
check (equal? (map (fragment-dynamics db ln (list 'pp 'ff)) (function (x) (get (last x) 'dyn))) (list "pp" "pp" "ff")) "fragment-dynamics"
check (equal? (map (fragment-articulate ln 0.5) (function (x) (getidx x 1))) (list 0.125 0.25 0.125)) "fragment-articulate"
check (< (length (fragment-thin (fragment-repeat ln 20) 0.5)) 60) "fragment-thin"
check (== (length (fragment-density ln (list 0))) 0) "fragment-density: a zero curve keeps nothing"
check (equal? (map (fragment-snap db (notes db 'Vn 'mf 'ord (list 61 63 66) (rhythm (list 1))) (pitch-field "C4" 'major 1)) (function (x) (get (last x) 'midi))) (list 60 62 65)) "fragment-snap, pitch-field"
check (equal? (chord-voicing (list "C4" "E4" "G4" "B4") 'open) (list 60 67 76 83)) "chord-voicing: open"
check (equal? (chord-voicing (list "C4" "E4" "G4" "B4") 'drop2) (list 55 60 64 71)) "chord-voicing: drop2"
check (equal? (chord-voicing (list "C4" "E4" "G4") (list 'invert 1)) (list 64 67 72)) "chord-voicing: an inversion"
check (contains? (error-of (function () (chord-voicing (list "C4") 'nope))) "unknown mode") "chord-voicing: unknown mode"
check (equal? (rotate (list 1 2 3 4) 1) (list 2 3 4 1)) "rotate"
check (equal? (cycle (list 1 2 3) 5) (list 1 2 3 1 2)) "cycle"
check (equal? (interleave (list 1 2 3) (list "a" "b")) (list 1 "a" 2 "b" 3)) "interleave"
check (equal? (harmonic-series "C2" 4) (list 36 48 55 60)) "harmonic-series"
check (equal? (pitches-from-spectrum (+ (sine 44100 110 0.5) (* 0.5 (sine 44100 220 0.5))) 44100 2) (list 45 57)) "pitches-from-spectrum: the strongest partials as pitches"
check (equal? (rhythm-from-pattern "x..x.x" 0.25) (list 0.75 0.5 0.25)) "rhythm-from-pattern"
check (equal? (rhythm-augment (list 1 0.5) 2) (list 2 1)) "rhythm-augment"
check (near? (sum (vec (tuplet 3 1))) 1 1e-9) "tuplet"
check (equal? (map (polyrhythm (list 1 1 1) (list 1 1)) length) (list 6 6)) "polyrhythm: a common cycle"
var wk (walk db 'Ob 'mf 'ord (list "C4") 2 (rhythm (list 0.5 0.5 0.5)))
check (and (== (length wk) 3) (== (get (last (head wk)) 'midi) 60)) "walk: starts at the pitch, one note per duration"
check (== (length (texture-staggered db (list 'Vn 'Vc) 'pp 'ord (list "C4" "D4") (rhythm (list 0.5 0.5)) (list 1 2) (list 0 1))) 8) "texture-staggered"
check (equal? (map (arpeggio db 'Vn 'mf 'ord (list "C4" "E4" "G4") 0.1 'updown 1) (function (x) (get (last x) 'pitch))) (list "C4" "E4" "G4" "E4" "C4")) "arpeggio: up and down"
check (equal? (map (chordinterp-ease db 'Vn 'mf 'ord (list "C4") (list "C5") (rhythm (list 1 1 1 1)) 2) (function (x) (get (head (get (last x) 'notes)) 'pitch))) (list "C4" "C#4" "F4" "C5")) "chordinterp-ease"
check (equal? (map (get (last (last (chordinterp-sets db 'Vn 'mf 'ord (list "C4" "G4") (list "A4" "B3") (rhythm (list 1 1 1))))) 'notes) (function (n) (get n 'pitch))) (list "B3" "A4")) "chordinterp-sets: each voice to the nearest free pitch"
check (== (length (texture-on-chords db (list 'Ob 'Hn) 'pp 'ord (list (list "C4" "E4") (list "D4" "F4")) (rhythm (list 0.5)) (list 1 1.5))) 10) "texture-on-chords"
check (== (length (texture-on-pivots db (list 'Ob 'Hn) 'pp 'ord (list "C4" "G4") 2 (rhythm (list 0.5 0.5)) (list 1 2))) 6) "texture-on-pivots"
check (equal? (map (orchestrate-line db (list "C3" "C4" "C5") (rhythm (list 1)) (list (list 'Vc "C2" "B3") (list 'Vn "C4" "C6")) 'mf 'ord) (function (x) (get (last x) 'instr))) (list "Vc" "Vn" "Vn")) "orchestrate-line"
score-map! sc (function (e) (put e 'gain 0.5))
check (equal? (unique (map (score-events sc) (function (e) (get e 'gain)))) (list 0.5)) "score-map!"
check (> (length (score-instrument sc 'Ob)) 0) "score-instrument"
check (== (length (shuffle (list 1 2 3 4))) 4) "shuffle"

# --- scores in scores ---
var outer (score "outer" 44100)
var se (event outer 0 0 fs)
check (equal? (get se 'kind) 'score) "event: a score is a payload"
check (near? (get se 'dur) 1.6 1e-9) "event: duration 0 means the whole score (with its releases)"
var se2 (event outer 2 0.4 fs)
check (== (get se2 'dur) 0.4) "event: or cut to a duration"
check (contains? (error-of (function () (event fs 0 0 fs))) "cannot contain itself") "event: a score cannot contain itself"
var om (score-render outer "stereo")
check (== (length om) 2) "score-render: nested scores render into the layout"
check (== (length (head om)) (+ (floor (* 2.4 44100)) (floor (* 0.6 44100)))) "score-render: the outer length"
check (== (length (render-event se 44100)) 2) "render-event: a score event alone renders stereo"
check (equal? (map (score-rows outer) head) (list "scores")) "score-rows: a scores row"
var deep (score "deep" 44100)
event deep 0 0 outer
check (== (length (score-render deep "stereo")) 2) "score-render: two levels"
check (equal? (type (play-event (register-score outer) (get se (quote id)))) "nil") "play-event: on a score inside, its roll is opened (nothing shown under MUSIL_NOSHOW)"

# --- MIDI files ---
var mid (midi-read "../examples/data/test.mid")
check (== (get mid 'ppq) 480) "midi-read: ppq"
check (equal? (get mid 'tracks) (list "Piano" "Bass")) "midi-read: track names"
check (== (length (get mid 'notes)) 4) "midi-read: notes paired on and off"
check (near? (get mid 'seconds) 1.75 1e-6) "midi-read: the tempo map (a change to 240 bpm)"
var mn (getidx (get mid 'notes) 3)
check (and (== (get mn 'pitch) 67) (near? (get mn 'at) 0.5 1e-6) (near? (get mn 'dur) 1.25 1e-6)) "midi-read: a note across the tempo change"
check (equal? (map (list 0 30 60 90 120 127) velocity->dynamics) (list 'pp 'p 'mp 'f 'ff 'ff)) "velocity->dynamics"
var mf (midi->fragment "../examples/data/test.mid" db 'Vn 'ord)
check (== (length mf) 4) "midi->fragment"
check (equal? (get (last (head mf)) 'instr) "Vn") "midi->fragment: on the instrument"
var ms (midi->score "../examples/data/test.mid" db (list 'Vn 'Vc) 'ord "m")
check (equal? (map (score-rows ms) head) (list "Vn" "Vc")) "midi->score: an instrument per channel"
check (contains? (error-of (function () (midi-read "/nope.mid"))) "cannot open") "midi-read: missing file"

# --- the index ---
var idx (db-index! db)
check (equal? (sort-by (keys idx) instrument-rank) (list "Ob" "Hn" "Vn" "Vc")) "db-index!: an entry per instrument on disk"
check (== (length (get (get idx "Vn") 'all)) 8) "db-index!: the instrument's pitched available sounds"
check (== (length (db-candidates db 'Vn 'mf 'ord)) 8) "db-candidates: by dynamics and technique"
check (== (length (db-candidates db 'Vc 'pp 'ord)) 8) "db-candidates: falls back to the technique when the dynamics are missing"
check (== (length (db-candidates db 'Nope 'mf 'ord)) 0) "db-candidates: unknown instrument"
check (equal? (db-range-available db 'Hn) (list 60 76)) "db-range-available"
check (equal? (db-techniques-of db 'Vn) (list "ord")) "db-techniques-of"
check (equal? (sort-list (db-dynamics-of db (quote Vc))) (list "ff" "mf")) "db-dynamics-of"
check (has? db 'index) "db-index!: kept on the database"

# --- envelopes, the orchestra, the granulator ---
seed 5
var en (env (list 0 (list 4 6) 10 (list 0.5 1)))
check (equal? (env-at en 0) (list 4 6)) "env-at: at the first breakpoint"
check (equal? (env-at en 5) (list 2.25 3.5)) "env-at: ranges interpolated"
check (equal? (env-at en 20) (list 0.5 1)) "env-at: after the last"
check (== (env-at (env (list 0 0 10 1)) 2.5) 0.25) "env-at: numbers interpolated"
check (equal? (env-at (env (list 0 (list 'ord) 10 (list 'pizz))) 0) (list 'ord)) "env-at: a set at a breakpoint"
var draws (map (vec->list (range 200)) (function (k) (env-at (env (list 0 (list 'a) 10 (list 'b))) 9)))
check (> (length (filter draws (function (d) (equal? d (list 'b))))) 100) "env-at: sets crossfaded by chance, the nearer one more often"
var orch (orchestra (list 'Ob 'Hn "Vn|Vc" (list 'Vn 'Vc)))
check (== (orchestra-size orch) 5) "orchestra: one player per symbol, per ossia, per member of a group"
check (equal? (get (getidx orch 2) 'instrs) (list "Vn" "Vc")) "orchestra: an ossia has two instruments"
check (equal? (map orch (function (p) (get p 'group))) (list -1 -1 -1 0 0)) "orchestra: paired players share a group"
check (equal? (orchestra-instruments orch) (list "Ob" "Hn" "Vn" "Vc")) "orchestra-instruments"
var gp (record (list 'density (env (list 0 (list 6 9) 10 (list 0.5 1))) 'register (env (list 0 (list 3 3) 10 (list 3 6))) 'duration (list 0.2 0.8) 'styles (env (list 0 (list 'ord) 10 (list 'ord 'pizz))) 'dynamics (env (list 0 (list 'pp) 10 (list 'ff))) 'solutions 2))
var gr (orchestrate-granular db orch 10 gp)
check (equal? (get gr 'kind) 'orchestration) "orchestrate-granular: a result"
check (== (length (get gr 'segments)) 1) "orchestrate-granular: one segment"
check (== (length (solutions gr 0)) 2) "solutions: as many as asked"
var gf (best-connection gr)
check (> (length gf) 20) "best-connection: a fragment"
check (equal? (get (last (head gf)) 'dyn) "pp") "granular: dynamics from the envelope at the start"
check (equal? (get (last (last gf)) 'dyn) "ff") "granular: ... and at the end"
check (all? (map (take gf 3) (function (x) (and (>= (get (last x) 'midi) 48) (<= (get (last x) 'midi) 61)))) identity) "granular: the register at the start (octave 3, its top creeping up as the envelope moves)"
function active-at (f t) (length (filter f (function (x) (and (<= (head x) t) (> (+ (head x) (getidx x 1)) t)))))
check (<= (max-of (map (vec->list (range 0 10 0.1)) (function (t) (active-at gf t)))) (orchestra-size orch)) "granular: never more sounding than there are players"
var gs (score "gran" 44100)
connect! gs 0 gr (list 1)
check (and (> (length (score-events gs)) 0) (<= (length (score-events gs)) (length (solution gr 0 1))) (== (length (score-events gs)) (length (connection gr (list 1))))) "connect!: the chosen solution in the score (continuations merged)"
check (equal? (map (connection gr (list 0)) head) (map (best-connection gr) head)) "connection: choices"
var pv2 (best-connection (orchestrate-granular db orch 4 (record (list 'method 'pivots 'chords (list (list "C4" "G4")) 'interval 1 'register (list 4 4) 'density (list 5 5)))))
check (all? (map pv2 (function (x) (contains? (list 59 60 61 66 67 68) (get (last x) 'midi)))) identity) "granular: pivots within the interval of the chord's pitches"
var hm2 (best-connection (orchestrate-granular db orch 4 (record (list 'method 'harmonic 'fundamental "C2" 'harmonicity 1 'register (list 2 6) 'density (list 5 5)))))
check (all? (map hm2 (function (x) (contains? (harmonic-series "C2" 16) (get (last x) 'midi)))) identity) "granular: harmonicity 1 draws only partials"
var model (fragment->score "model" 44100 (notes db 'Ob 'mf 'ord (list "C4" "D4" "E4") (rhythm (list 0.5))))
var tbl (markov-table model)
check (equal? (get tbl 60) (list 62)) "markov-table: transitions"
var mk2 (best-connection (orchestrate-granular db orch 4 (record (list 'method 'markov 'markov-score model 'register (list 4 4) 'density (list 5 5)))))
check (all? (map mk2 (function (x) (or (contains? (list 60 62 64) (get (last x) 'midi)) (not (contains? (db-pitches-of db (get (last x) 'instr) 'ord) 64))))) identity) "granular: markov stays in the model's pitches (an instrument without one plays its nearest)"
check (all? (map mk2 (function (x) (== (get (last x) 'shift) 0))) identity) "granular: every note is a recorded pitch of its instrument (no shift)"
check (all? (map mk2 (function (x) (contains? (db-pitches-of db (get (last x) 'instr) 'ord) (get (last x) 'midi)))) identity) "granular: the pitch is one the instrument was recorded at with the technique"
var wide (best-connection (orchestrate-granular db (orchestra (list 'Ob 'Vn)) 3 (record (list 'register (list 1 2) 'density (list 5 5)))))
check (== (length wide) 0) "granular: a register no instrument of the orchestra has samples in is unplayable: nothing written (and reported)"
var mixed (best-connection (orchestrate-granular db (orchestra (list 'Ob 'Vn 'Hn)) 3 (record (list 'register (list 4 4) 'styles (list 'ord) 'density (list 6 6) 'duration (list 0.2 0.2)))))
check (all? (map mixed (function (x) (contains? (db-pitches-of db (get (last x) 'instr) 'ord) (get (last x) 'midi)))) identity) "granular: an event goes only to a player whose instrument has samples for it"
var mixstyle (best-connection (orchestrate-granular db (orchestra (list 'Ob 'Vn 'Hn)) 3 (record (list 'register (list 4 4) 'styles (list 'ord 'pizz) 'density (list 8 8) 'duration (list 0.2 0.2)))))
check (and (> (length mixstyle) 10) (all? (map mixstyle (function (x) (equal? (get (last x) 'tech) "ord"))) identity)) "granular: a style set: each player takes a style of the set it has samples of (pizz unavailable, ord played, nothing skipped)"
check (equal? (player-styles db (head (orchestra (list 'Vn))) (list 'ord 'pizz 'trem) nil 60 67) (list 'ord)) "player-styles: the styles of a set a player has in a register"
check (equal? (player-styles db (head (orchestra (list 'Vc))) (list 'ord) 'ff 60 67) (list 'ord)) "player-styles: ... at a dynamics (the Vc has a C4 ff)"
check (equal? (player-styles db (head (orchestra (list 'Vn))) (list 'ord) 'ff 60 67) (list)) "player-styles: ... none at a dynamics it was not recorded at"
check (equal? (db-pitches-at db 'Vc 'ord 'ff) (list 60)) "db-pitches-at: the recorded pitches at one dynamics"
var nn (note db 'Vc "C4" 'mf 'ord)
check (and (== (get nn 'shift) 0) (== (get nn 'midi) 60)) "note: the pitch before the dynamics: a C4 recorded only ff is the C4 ff, not a shifted C#4 mf"
check (equal? (db-pitches-of db 'Vn 'ord) (sort-list (unique (map (db-query db 'Vn nil nil 'ord) (function (e) (get e 'midi)))))) "db-pitches-of: the recorded pitches of an instrument with a technique"
check (== (length (player-pitches db (head (orchestra (list "Vn|Vc"))) 'ord nil 60 62)) 3) "player-pitches: an ossia player's pitches in a register"
check (equal? (list (level->dynamics 0) (level->dynamics 0.5) (level->dynamics 1)) (list 'ppp 'mf 'fff)) "level->dynamics"
check (near? (dynamics->level "mf") (/ 4 7) 1e-9) "dynamics->level"
check (and (== (level->gain 0) 0.25) (== (level->gain 1) 1)) "level->gain"
var cres (best-connection (orchestrate-granular db orch 10 (record (list 'density (list 4 4) 'dynamics (env (list 0 0 10 1)) 'register (list 4 4)))))
check (and (equal? (get (last (head cres)) 'dyn) "ppp") (equal? (get (last (last cres)) 'dyn) "fff")) "granular: a dynamics level interpolates ppp to fff"
check (< (get (last (head cres)) 'gain) (get (last (last cres)) 'gain)) "granular: ... and the gain with it"
var dissolve (best-connection (orchestrate-granular db orch 10 (record (list 'density (list 6 6) 'chords (list (list "E4")) 'chord-weight (env (list 0 1 3 1 10 0)) 'register (list 4 4)))))
check (all? (map (take dissolve 6) (function (x) (or (== (get (last x) 'midi) 64) (not (contains? (db-pitches-of db (get (last x) 'instr) 'ord) 64))))) identity) "granular: chord-weight 1 at the start keeps the unison (on the instruments that have the note)"
check (any? (drop dissolve (- (length dissolve) 12)) (function (x) (!= (get (last x) 'midi) 64))) "granular: chord-weight 0 at the end frees the pitches"
var cp (record (list 'density (list 8 8) 'duration (list 0.05 0.1) 'coupling 0))
var uncoupled (best-connection (orchestrate-granular db (orchestra (list (list 'Vn 'Vc))) 3 cp))
check (== (length (unique (map uncoupled head))) (length uncoupled)) "granular: coupling 0, a paired group's players sound one at a time"
var coupled (best-connection (orchestrate-granular db (orchestra (list (list 'Vn 'Vc))) 3 (put cp 'coupling 1)))
check (< (length (unique (map coupled head))) (length coupled)) "granular: coupling 1, they sound together"
# --- the players as people: state, constraints, the report, the validation ---
seed 9
var rep (get (orchestrate-granular db orch 10 (record (list 'density (list 6 6) 'register (list 4 4) 'duration (list 0.5 1)))) 'report)
check (and (>= (get rep 'due) 60) (<= (get rep 'due) 61) (== (+ (get rep 'events) (get rep 'skipped-busy) (get rep 'skipped-unplayable)) (get rep 'due)) (>= (get rep 'notes) (get rep 'events))) "report: every event due is played, skipped busy or unplayable (a paired group's event writes more notes than one)"
check (and (== (length (get rep 'players)) 5) (== (sum (vec (map (get rep 'players) (function (p) (get p 'notes))))) (get rep 'notes))) "report: a line per player, their notes adding up"
check (== (get (get (orchestrate-granular db (orchestra (list 'Ob 'Vn)) 3 (record (list 'register (list 1 2) 'density (list 4 4)))) 'report) 'skipped-unplayable) 12) "report: the unplayable events counted (and listed once each)"
var strict (orchestrate-granular db orch 4 (record (list 'dynamics (list 'ff) 'dynamics-strict 1 'register (list 4 4) 'density (list 5 5) 'duration (list 0.1 0.1))))
check (and (> (length (best-connection strict)) 0) (all? (map (best-connection strict) (function (x) (and (equal? (get (last x) 'instr) "Vc") (== (get (last x) 'midi) 60) (equal? (get (get (last x) 'entry) 'dyn) "ff")))) identity)) "dynamics-strict: only the sample at the dynamics asked for (the Vc's C4 ff), on the players that have it"
check (== (get (get strict 'report) 'substituted) 0) "dynamics-strict: nothing substituted"
check (> (get (get (orchestrate-granular db orch 4 (record (list 'dynamics (list 'ff) 'register (list 4 4) 'density (list 5 5)))) 'report) 'substituted) 0) "dynamics not strict: the nearest recorded dynamics, counted as substituted"
var seated (best-connection (orchestrate-granular db (orchestra (list "Vn|Vc")) 6 (record (list 'seat 100 'density (list 4 4) 'register (list 4 4) 'duration (list 0.1 0.1)))))
check (== (length (unique (map seated (function (x) (get (last x) 'instr))))) 1) "seat: an ossia player keeps its instrument for the seat's time"
var unseated (best-connection (orchestrate-granular db (orchestra (list "Vn|Vc")) 6 (record (list 'density (list 4 4) 'register (list 4 4) 'duration (list 0.1 0.1)))))
check (== (length (unique (map unseated (function (x) (get (last x) 'instr))))) 2) "seat 0: the band decides the instrument note by note"
var stepwise (best-connection (orchestrate-granular db (orchestra (list 'Vn)) 8 (record (list 'leap 1 'density (list 4 4) 'register (list 4 4) 'duration (list 0.1 0.1)))))
check (all? (map (zip (tail stepwise) stepwise) (function (p) (<= (abs (- (get (last (head p)) 'midi) (get (last (last p)) 'midi))) 1))) identity) "leap: a player's next pitch within the interval of its last"
var falling (best-connection (orchestrate-granular db (orchestra (list 'Hn)) 8 (record (list 'leap (list -2 -1) 'density (list 4 4) 'register (list 4 5) 'duration (list 0.1 0.1)))))
var steps (map (zip (tail falling) falling) (function (p) (- (get (last (head p)) 'midi) (get (last (last p)) 'midi))))
check (> (length (filter steps (function (d) (and (>= d -2) (<= d -1))))) (* 0.6 (length steps))) "leap: a signed range is a direction (descending by one or two semitones, restarting when the band runs out)"
check (equal? (list (just-cents "C2" "A#4" 16) (just-cents "C2" "F#5" 16) (just-cents "C2" "G#5" 16) (just-cents "C2" "C4" 16) (just-cents "C2" "C#4" 16)) (list -31 -49 41 0 nil)) "just-cents: the 7th, 11th and 13th partials' deviations; an octave 0; not a partial: nil"
var justly (best-connection (orchestrate-granular db (orchestra (list 'Hn 'Vn 'Vc 'Ob)) 6 (record (list 'method 'harmonic 'fundamental "C2" 'harmonicity 1 'just 1 'register (list 4 5) 'density (list 4 4)))))
check (all? (map justly (function (x) (equal? (get (last x) 'cents) (just-cents "C2" (get (last x) 'midi) 16)))) identity) "just: the harmonic method's partials carry their just-intonation cents"
check (any? justly (function (x) (!= (get (last x) 'cents) 0))) "just: ... some of them non-zero (the seventh partial, Bb)"
var clustered (best-connection (orchestrate-granular db (orchestra (list 'Vn 'Vn)) 4 (record (list 'method 'cluster 'density (list 2 2) 'register (list 4 4) 'duration (list 0.1 0.1)))))
check (== (length (unique (map (take clustered 8) (function (x) (get (last x) 'midi))))) 8) "cluster: every pitch of the band once before any is repeated"
check (> (length (unique (map (take clustered 8) (function (x) (get (last x) 'midi))))) (length (unique (map (take unseated 8) (function (x) (get (last x) 'midi)))))) "cluster: ... where random draws repeat"
var middle (best-connection (orchestrate-granular db (orchestra (list 'Hn)) 60 (record (list 'weight 1 'density (list 4 4) 'register (list 4 5) 'duration (list 0.1 0.1)))))
var edges (best-connection (orchestrate-granular db (orchestra (list 'Hn)) 60 (record (list 'weight -1 'density (list 4 4) 'register (list 4 5) 'duration (list 0.1 0.1)))))
function band-spread (f) (mean (vec (map f (function (x) (abs (- (get (last x) 'midi) 71.5))))))
check (< (band-spread middle) (band-spread edges)) "weight: 1 draws the middle of the band, -1 its edges"
var high (best-connection (orchestrate-granular db (orchestra (list 'Hn)) 60 (record (list 'tilt 1 'density (list 4 4) 'register (list 4 5) 'duration (list 0.1 0.1)))))
check (> (mean (vec (map high (function (x) (get (last x) 'midi))))) (mean (vec (map edges (function (x) (get (last x) 'midi)))))) "tilt: 1 draws the top of the band more often"
var inert (orchestrate-granular db (orchestra (list 'Vn)) 6 (record (list 'inertia 1 'styles (list 'ord) 'density (list 4 4) 'register (list 4 4))))
check (all? (map (best-connection inert) (function (x) (equal? (get (last x) 'tech) "ord"))) identity) "inertia: a player keeps its technique (the set holding it)"
var scattered (best-connection (orchestrate-granular db (orchestra (list (list 'Vn 'Vc 'Ob))) 3 (record (list 'spread 0.05 'density (list 2 2) 'duration (list 0.2 0.2) 'coupling 1))))
check (and (> (length scattered) 3) (> (length (unique (map scattered head))) (/ (length scattered) 3))) "spread: a group struck together is scattered within the spread"
check (all? (map scattered (function (x) (< (mod (head x) 0.5) 0.051))) identity) "spread: ... and by no more than it"
var logdur (map (best-connection (orchestrate-granular db (orchestra (list 'Vn 'Vn 'Vn 'Vn)) 40 (record (list 'duration (list 0.05 5) 'duration-law 'log 'density (list 2 2))))) (function (x) (getidx x 1)))
var unidur (map (best-connection (orchestrate-granular db (orchestra (list 'Vn 'Vn 'Vn 'Vn)) 40 (record (list 'duration (list 0.05 5) 'density (list 2 2))))) (function (x) (getidx x 1)))
check (< (median (vec logdur)) (median (vec unidur))) "duration-law: 'log draws as many short as long by ratio, the median well under the uniform one"
check (< (median (vec (map (best-connection (orchestrate-granular db (orchestra (list 'Vn 'Vn 'Vn 'Vn)) 40 (record (list 'duration (list 0.05 5) 'duration-law 3 'density (list 2 2))))) (function (x) (getidx x 1))))) (median (vec unidur))) "duration-law: an exponent above 1 draws mostly short"
var gdr (get (orchestrate-granular db (orchestra (list (list 'Ob 'Hn) (list 'Vn 'Vc))) 20 (record (list 'group-density (list (list 1 1) (list 8 8)) 'register (list 4 4) 'duration (list 0.05 0.05) 'coupling 0))) 'report)
var gd-notes (map (get gdr 'players) (function (p) (get p 'notes)))
check (> (+ (getidx gd-notes 2) (getidx gd-notes 3)) (* 3 (+ (getidx gd-notes 0) (getidx gd-notes 1)))) "group-density: each paired group at its own rate (the strings eight times the winds)"
check (near? (balance-gain (record (list 'brass -6 'Hn -12)) 'Hn) (pow 10 -0.6) 1e-9) "balance-gain: the instrument's own decibels"
check (near? (balance-gain (record (list 'brass -6)) 'Tbn) (pow 10 -0.3) 1e-9) "balance-gain: ... else its family's"
check (== (balance-gain (record (list 'brass -6)) 'Vn) 1) "balance-gain: ... else 0 dB"
check (equal? (list (instrument-family 'Fl) (instrument-family 'Tbn) (instrument-family 'Vc) (instrument-family 'Timp) (instrument-family 'Hp) (instrument-family 'Xyz)) (list 'winds 'brass 'strings 'percussion 'plucked 'other)) "instrument-family"
var balanced (best-connection (orchestrate-granular db (orchestra (list 'Hn 'Vn)) 4 (record (list 'balance (record (list 'brass -6)) 'density (list 6 6) 'register (list 4 4) 'dynamics 0.5))))
check (all? (map balanced (function (x) (if (equal? (get (last x) 'instr) "Hn") (near? (get (last x) 'gain) (* (level->gain 0.5) (pow 10 -0.3)) 1e-6) (near? (get (last x) 'gain) (level->gain 0.5) 1e-6)))) identity) "balance: the brass 6 dB under the strings at the same level"
# one orchestra shared by two calls ('continue): the second books only players the first left free
seed 4
var shared (orchestra (list 'Ob 'Hn 'Vn 'Vc))
var cp1 (record (list 'continue 1 'start 0 'density (list 4 4) 'duration (list 2 2) 'register (list 4 4)))
var vs (score "shared" 44100)
connect! vs 0 (orchestrate-granular db shared 4 cp1) (list 0)
var second (orchestrate-granular db shared 4 (put cp1 'start 1))
connect! vs 1 second (list 0)
check (all? (map shared (function (p) (has? p 'busy-until))) identity) "continue: the orchestra's own records carry the state"
var v1 (score-validate vs shared)
check (get v1 'ok) "score-validate: two overlapping granulators on one orchestra with 'continue never book a player twice"
var vs2 (score "unshared" 44100)
connect! vs2 0 (orchestrate-granular db shared 4 (record (list 'density (list 4 4) 'duration (list 2 2) 'register (list 4 4)))) (list 0)
connect! vs2 1 (orchestrate-granular db shared 4 (record (list 'density (list 4 4) 'duration (list 2 2) 'register (list 4 4)))) (list 0)
var v2 (score-validate vs2 shared)
check (and (not (get v2 'ok)) (> (length (get v2 'overbooked)) 0)) "score-validate: ... without it they overbook (the same people twice)"
check (get (score-validate vs2 (orchestra (list 'Ob 'Ob 'Hn 'Hn 'Vn 'Vn 'Vc 'Vc))) 'ok) "score-validate: an orchestra twice the size plays it"
seed 5
var ooo (orchestra (list 'Vn 'Vc))
var cpo (record (list 'continue 1 'density (list 2 2) 'duration (list 3 3) 'register (list 4 4)))
var vs4 (score "any order" 44100)
connect! vs4 4 (orchestrate-granular db ooo 4 (put cpo 'start 4)) (list 0)
connect! vs4 0 (orchestrate-granular db ooo 6 (put cpo 'start 0)) (list 0)
check (get (score-validate vs4 ooo) 'ok) "continue: calls in any order of time never overlap a player's bookings"
check (any? (score-events vs4) (function (e) (< (get e 'dur) 3))) "continue: ... a note is cut short where the player's next booking begins"
orchestra-rest! shared
check (all? (map shared (function (p) (and (== (get p 'busy-until) 0) (equal? (type (get p 'last-pitch)) "nil")))) identity) "orchestra-rest!: the state cleared"
var vs3 (score "ossia" 44100)
event vs3 0 1 (note db 'Vc "C4" 'mf 'ord)
event vs3 0 1 (note db 'Vc "D4" 'mf 'ord)
check (get (score-validate vs3 (orchestra (list "Vn|Vc" 'Vc))) 'ok) "score-validate: an ossia player counts for either instrument (a matching, not a count)"
check (not (get (score-validate vs3 (orchestra (list "Vn|Vc" 'Vn))) 'ok)) "score-validate: ... but one person: two cello notes need two who can play it"
event vs3 2 1 (note db 'Vn "A5" 'mf 'ord)
var v3 (score-validate vs3 (orchestra (list 'Vn 'Vc)))
check (and (not (get v3 'ok)) (== (length (get v3 'transposed)) 1) (== (get (get v3 'peak) "Vc") 2)) "score-validate: a note at a pitch the instrument was not recorded at is reported (shifted), the peak per instrument counted"
check (not (validation-print v3)) "validation-print: returns whether it is playable"
var cie (chordinterp-env (list "C4") (list "C5") 10 3)
check (equal? (map cie head) (list 0 5 10)) "chordinterp-env: breakpoints over the time"
check (equal? (env-at cie 5) (list (list 66))) "chordinterp-env: the chord halfway"
var merged (merge-continuations (list (list 0 1 (note db 'Vn "C4" 'mf 'ord)) (list 1 1 (note db 'Vn "C4" 'mf 'ord)) (list 2 1 (note db 'Vn "D4" 'mf 'ord))))
check (and (== (length merged) 2) (== (getidx (head merged) 1) 2)) "merge-continuations: a note continued becomes one longer note"

# --- morphological orchestration ---
seed 6
var tsc (score "target" 44100)
event tsc 0 3 (put (note db 'Vc "C4" 'mf 'ord) 'gain 2)
each (range 6) (function (k) (event tsc (* k 0.4) 0.2 (put (note db 'Ob (+ 62 (mod (* k 3) 6)) 'mf 'ord) 'gain 2)))
var tx (head (score-render tsc "mono"))
var tg (target-analyse tx 44100 (get db 'block) 1024)
check (equal? (get tg 'block) 2048) "target-analyse: the database's block"
check (== (length (get tg 'spectra)) (length (get tg 'density))) "target-analyse: one spectrum per curve sample"
check (== (length (get tg 'fine)) (length (get tg 'spectra))) "target-analyse: the fine spectra alongside"
check (and (has? tg 'attacks) (has? tg 'centroid) (has? tg 'spread) (has? tg 'low) (has? tg 'loudness)) "target-analyse: every curve"
check (near? (get tg 'seconds) 3.6 0.01) "target-analyse: the length"
check (and (>= (length (get tg (quote attacks))) 4) (<= (length (get tg (quote attacks))) 12)) "target-analyse: the oboe's attacks are the flux peaks"
check (> (max (get tg 'density)) 2) "target-analyse: the density counts them a second"
check (< (length (get (target-analyse-with tx 44100 (get db 'block) 1024 (record (list 'threshold 4))) 'attacks)) (length (get tg 'attacks))) "target-analyse-with: a higher threshold keeps fewer peaks"
check (== (length (target-at tg 'spectra 0)) 1024) "target-at: a curve's value at a time"
check (near? (target-level tg 0.5 40) 1 0.15) "target-level: the loudest point is 1"
var envs (target-envelopes tg (record (list 'step 0.25)))
check (equal? (sort-list (map (keys envs) str)) (list "density" "dynamics" "register")) "target-envelopes: density, register, dynamics"
check (numeric-range? (env-at (get envs 'register) 1)) "target-envelopes: the register as a range of octaves"
check (>= (env-at (get envs 'density) 1) 0.5) "target-envelopes: the density is at least min-density"
check (near? (env-at (get (target-envelopes tg (record (list 'step 0.25 'density-scale 2))) 'density) 1) (* 2 (env-at (get envs 'density) 1)) 1e-9) "target-envelopes: 'density-scale multiplies the target's density"
var tpk (target-peaks tg 1 12)
check (any? (vec->list tpk) (function (f) (< (abs (- (hz->midi f) 60)) 0.3))) "target-peaks: the cello's C4 among the peaks, within 30 cents"
var tdict (target-dictionary db tpk orch 0 127 'mf nil)
check (all? (map tdict (function (a) (any? (vec->list tpk) (function (f) (<= (abs (- (hz->midi f) (get (last a) 'midi))) 0.6))))) identity) "target-dictionary: every sound at a peak"
check (== (length (target-dictionary db tpk orch 0 127 'mf (list 'pizz))) 0) "target-dictionary: 'styles restricts the techniques"
check (near? (entry-f0 (get (note db 'Vn "C4" 'mf 'ord) 'entry) 44100 2048) 261.6 8) "entry-f0: the sound's own frequency, near its pitch"
var mo (orchestrate-morphological db orch tg (record (list 'solutions 2)))
check (equal? (get mo 'method) 'morphological) "orchestrate-morphological: a result"
check (== (length (solutions mo 0)) 2) "orchestrate-morphological: solutions"
var mf (best-connection mo)
check (> (length mf) 3) "orchestrate-morphological: events"
check (all? (map mf (function (x) (equal? (get (last x) 'kind) 'note))) identity) "orchestrate-morphological: notes"
check (any? mf (function (x) (== (get (last x) 'midi) 60))) "orchestrate-morphological: the cello's C4 is found by the pursuit"
check (all? (map mf (function (x) (has? (last x) 'cents))) identity) "orchestrate-morphological: cents on every note"
check (all? (map mf (function (x) (<= (abs (get (last x) 'cents)) 50))) identity) "orchestrate-morphological: cents within a quarter tone"
check (> (max-of (map mf (function (x) (getidx x 1)))) 1) "orchestrate-morphological: the held cello gets a long note (atom-persistence)"
check (any? mf (function (x) (< (getidx x 1) 0.5))) "orchestrate-morphological: the short oboe notes get short ones"
check (<= (max-of (map (vec->list (range 0 3.5 0.05)) (function (t) (active-at mf t)))) (orchestra-size orch)) "orchestrate-morphological: never more than the players"
var ap (atom-persistence tg (get (get (last (head mf)) 'entry) 'features) (head (head mf)) 0.5 8)
check (and (>= ap 0) (<= ap 8)) "atom-persistence: seconds, bounded"
check (and (technique-short? "pizz-lv") (technique-short? 'stac) (not (technique-short? "ord"))) "technique-short?"
var kts (score "known" 44100)
event kts 0 2 (note db 'Vc "C4" 'mf 'ord)
event kts 0 2 (note db 'Ob "E4" 'mf 'ord)
var ktg (target-analyse (* 0.02 (head (score-render kts "mono"))) 44100 (get db 'block) 1024)
var kf (best-connection (orchestrate-morphological db orch ktg (record (list))))
check (and (any? kf (function (x) (equal? (get (last x) 'pitch) "C4"))) (any? kf (function (x) (equal? (get (last x) 'pitch) "E4")))) "orchestrate-morphological: the target's pitches are found at any level"
check (all? (map kf (function (x) (<= (abs (get (last x) 'cents)) 25))) identity) "orchestrate-morphological: a target made of the sounds themselves is in tune"
var kd (best-connection (orchestrate-morphological db orch ktg (record (list 'duration (list 0.2 0.2)))))
check (all? (map kd (function (x) (near? (getidx x 1) 0.2 1e-9))) identity) "orchestrate-morphological: a 'duration of your own replaces the persistence"
var polyf (best-connection (orchestrate-granular db orch 4 (record (list 'density (list 20 20) 'duration (list 2 2) 'polyphony 2))))
check (<= (max-of (map (vec->list (range 0 4 0.1)) (function (t) (active-at polyf t)))) 2) "granular: 'polyphony caps the players sounding at once"

# --- several databases at once ---
var both (db-merge (list db db))
check (== (db-size both) 64) "db-merge: the entries of both"
check (has? (head (get both 'entries)) 'root) "db-merge: each entry keeps its own sounds' folder"
check (== (length (get both 'paths)) 2) "db-merge: remembers its files"
check (== (db-size (db-load (list "../examples/data/microsol/microsol.spectrum.db" "../examples/data/microsol/microsol.spectrum.db"))) 64) "db-load: a list of files merges"
check (== (get (note both 'Hn 'C4 'mf 'ord) 'shift) 0) "note: draws on a merged database"
var other (record (list 'path "x" 'root "." 'type "mfcc" 'block 2048 'hop 256 'ncoeff 13 'entries (list)))
check (contains? (error-of (function () (db-merge (list db other)))) "types differ") "db-merge: feature types must agree"
check (contains? (error-of (function () (db-merge (list)))) "no databases") "db-merge: empty"

# --- the roll of notes: staves ---
var roll2 (score-roll s2)
var rows2 (getidx (head (getidx roll2 1)) 1)
check (equal? (head (head rows2)) "Vn") "score-roll: a row per instrument"
check (equal? (last (head rows2)) "treble") "score-roll: a treble clef for a violin around C4-B4"
check (== (getidx (head (getidx (head (getidx roll2 1)) 2)) 8) 60) "score-roll: a note's bar carries its MIDI pitch"
check (equal? (getidx (head (getidx (head (getidx roll2 1)) 2)) 9) "mf") "score-roll: and its dynamics"
save-png roll2 "/tmp/musil_test_staff.png" 800 400
check (exists? "/tmp/musil_test_staff.png") "the staff draws"

# --- play, on the null device ---
check (equal? (get s2 'reverb) (list 0.7 0.3)) "score: in the hall by default"
score-reverb! s2 1 0
check (equal? (get s2 'reverb) (list 1 0)) "score-reverb!: dry"
check (equal? (type (play-score s2 0.8)) "nil") "play-score: streams the events through the engine, waits"
var end (score-play-now s2 1 0.5)
check (> end (audio-time)) "score-play-now: returns at once with the end time"
check (not (equal? (type score-player) "nil")) "score-play-now: a player is up"
check (> (get score-player 'cues) 0) "score-play-now: the events handed to the engine as cues"
check (head (cues-status)) "play-cues: the loader is active"
check (>= (bus-reverb-latency) 0) "bus-reverb-latency: seconds"
check (equal? bus-hall-state (list 44100 1 0)) "bus-hall!: the score's mix on the bus"
sleep 0.3
check (>= (getidx (cues-status) 3) 1) "the loader thread has read and queued the first sounds"
check (== (getidx (cues-status) 4) 0) "no cue failed"
check (near? (azimuth->pan 90) -1 1e-9) "azimuth->pan: left is -1"
event s2 3 0.2 (sine 44100 660 0.2)
check (head (playhead)) "score-play-now: the playhead is on"
check (== (last (playhead)) (register-score s2)) "score-play-now: the playhead names the score, so only its roll follows"
var hm (score-hall-mix s2 44100)
check (and (== (length hm) 2) (has? s2 'cache)) "score-hall-mix: the offline mix, cached on the score"
score-clear-cache! s2
check (equal? (type (opt s2 'cache nil)) "nil") "score-clear-cache!"
var nested (score "outer" 44100)
event nested 1 0 s2
check (all? (map (score-flatten nested 0) (function (e) (>= (get e 'at) 1))) identity) "score-flatten: the inner events at absolute times"
check (== (length (score-flatten nested 0)) (length (get s2 'events))) "score-flatten: every inner event"
stop-score
check (not (head (playhead))) "stop-score: the playhead is off"
check (== (length (head (render-buffer s2 "mono"))) (length (head (score-render s2 "mono")))) "render-buffer: dry when the score's wet is 0"
score-reverb! s2 0.7 0.3
var rb (render-buffer s2 "stereo")
check (and (== (length rb) 2) (> (length (head rb)) (length (head (score-render s2 "stereo"))))) "render-buffer: the channels in the hall, the tail included"
var rh (render-hall s2 "/tmp/musil_test_hall.wav")
check (== (length rh) 2) "render-hall: stereo in the hall"
check (<= (max-of (map rh (function (c) (max (abs c))))) 0.981) "render-hall: never above 0.98"
check (> (length (head rh)) (length (head (score-render s2 "stereo")))) "render-hall: the hall's tail follows"
check (== (head (read-wav "/tmp/musil_test_hall.wav")) 44100) "render-hall: the file"
check (== (length (roll-render (register-score s2) "/tmp/musil_test_hall2.wav")) 2) "roll-render: by the score's number"
var saved (score "to save" 44100)
event saved 0 1 (note-cents! (note db 'Vn "C4" 'mf 'ord) 12)
event saved 0 0.5 (note db 'Ob "E4" 'mf 'ord)
event saved 1.5 0.25 (note db 'Hn "G4" 'mf 'ord)
event saved 2 0.2 (sine 44100 440 0.2)
check (== (score-save saved "/tmp/musil_test_connection.txt") 3) "score-save: the notes written, the buffer left out"
var ctext (read "/tmp/musil_test_connection.txt")
check (starts-with? ctext "[ orchestra Vn Ob Hn ]") "score-save: the orchestra line"
check (contains? ctext "[ segment 1500") "score-save: segments by onset, in ms"
check (contains? ctext "[ note 1000 Vn ord C4 mf") "score-save: a note line as Orchidea writes it"
var loaded (score-load db "/tmp/musil_test_connection.txt" "back")
check (== (length (get loaded 'events)) 3) "score-load: the notes back"
check (equal? (map (get loaded 'events) (function (e) (get e 'source))) (map (take (get saved 'events) 3) (function (e) (get e 'source)))) "score-load: the same sounds, by file"
check (== (get (head (get loaded 'events)) 'cents) 12) "score-load: the cents"
check (near? (get (last (get loaded 'events)) 'at) 1.5 1e-6) "score-load: the onsets"
var sched (score-schedule s2 1 0.7)
check (== (length sched) 3) "score-schedule: synths, end, start (the live way)"
check (> (getidx sched 1) (getidx sched 2)) "score-schedule: ends after it starts"
each (head sched) free
check (== (length (score-render-at s2 "stereo" 22050)) 2) "score-render-at: another rate"
check (near? (length (head (score-render-at s2 "stereo" 22050))) (/ (length (head (score-render s2 "stereo"))) 2) 2) "score-render-at: half the samples at half the rate"
var ph (playhead)
check (== (length ph) 3) "playhead: (list on time owner)"
playhead! (audio-time) 2 (+ (audio-time) 10)
check (head (playhead)) "playhead!: on while a score plays"
check (near? (getidx (playhead) 1) 2 0.2) "playhead: the position from where it started"
playhead-off!
check (not (head (playhead))) "playhead-off!"
check (== (length (play (sine 44100 440 0.05) 44100)) 1) "play from live is not shadowed by music (a buffer still plays)"
function bad (gate) (pvoc-stretch gate 2)
var s4 (score "bad" 44100)
event s4 0 0.1 (instrument bad (list))
check (contains? (error-of (function () (score-schedule s4 1 0))) "does not stream") "score-schedule: a non-streamable instrument is refused (play-score renders it instead)"
audio-quit
report "test_music"
