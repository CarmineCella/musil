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
check (== (length bar) 11) "score-roll: a bar is row start dur label tip group lane lanes midi dyn event-id"
check (== (getidx bar 8) -1) "score-roll: an unpitched event has no pitch"
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
check (equal? (type (play-score s2 0.8)) "nil") "play-score: renders, puts in the hall, plays, waits"
var end (score-play-now s2 1 0.5)
check (> end (audio-time)) "score-play-now: returns at once with the end time"
check (head (playhead)) "score-play-now: the playhead is on"
stop-score
check (not (head (playhead))) "stop-score: the playhead is off"
var rh (render-hall s2 "/tmp/musil_test_hall.wav")
check (== (length rh) 2) "render-hall: stereo in the hall"
check (<= (max-of (map rh (function (c) (max (abs c))))) 0.981) "render-hall: never above 0.98"
check (> (length (head rh)) (length (head (score-render s2 "stereo")))) "render-hall: the hall's tail follows"
check (== (head (read-wav "/tmp/musil_test_hall.wav")) 44100) "render-hall: the file"
check (== (length (roll-render (register-score s2) "/tmp/musil_test_hall2.wav")) 2) "roll-render: by the score's number"
var sched (score-schedule s2 1 0.7)
check (== (length sched) 3) "score-schedule: synths, end, start (the live way)"
check (> (getidx sched 1) (getidx sched 2)) "score-schedule: ends after it starts"
each (head sched) free
check (== (length (score-render-at s2 "stereo" 22050)) 2) "score-render-at: another rate"
check (near? (length (head (score-render-at s2 "stereo" 22050))) (/ (length (head (score-render s2 "stereo"))) 2) 2) "score-render-at: half the samples at half the rate"
var ph (playhead)
check (== (length ph) 2) "playhead: (list on time)"
playhead! (audio-time) 2 (+ (audio-time) 10)
check (head (playhead)) "playhead!: on while a score plays"
check (near? (last (playhead)) 2 0.2) "playhead: the position from where it started"
playhead-off!
check (not (head (playhead))) "playhead-off!"
check (== (length (play (sine 44100 440 0.05) 44100)) 1) "play from live is not shadowed by music (a buffer still plays)"
function bad (gate) (pvoc-stretch gate 2)
var s4 (score "bad" 44100)
event s4 0 0.1 (instrument bad (list))
check (contains? (error-of (function () (score-schedule s4 1 0))) "does not stream") "score-schedule: a non-streamable instrument is refused (play-score renders it instead)"
audio-quit
report "test_music"
