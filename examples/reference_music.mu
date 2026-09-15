# reference_music: every function of the music library, once
load "music.mu"
seed 1

# --- 1. A score and its events -----------------------------------------------------------------
print "--- the score ---"
var s (score "demo" 8000)
print "score           :" (get s 'name) "at" (score-sr s) "Hz," (length (score-events s)) "events"
var tone (sine 8000 440 0.5)
var e1 (event s 0 0.4 tone)
print "event (buffer)  :" (get e1 'kind) (get e1 'label) "at" (get e1 'at) "for" (get e1 'dur)
var e2 (event s 0.5 0.3 "data/drums.wav")
print "event (file)    :" (get e2 'kind) (get e2 'label)
function blip (d) (* (sine 8000 880 d) 0.3)
var e3 (event s 1 0.2 (call blip (list 'dur)))
var e4 (event s 1.3 0.2 blip)
print "call            :" (get e3 'label) "; a bare function is called with the duration:" (get e4 'args)
var sr 8000
function ping (gate freq) (* (adsr sr gate 0.01 0.1 0.5 0.1) (osc sr (sig gate freq) sine-table))
var e5 (event s 1.6 0.3 (instrument ping (list (list 'freq 660))))
print "instrument      :" (get e5 'label) (get e5 'params)
var e6 (event-at s 2 0.3 (list tone tone) 45 10)
print "event-at        : a stereo buffer at azimuth" (get e6 'az) "elevation" (get e6 'el)
event-gain! e1 0.8
event-move! e4 1.4
event-length! e4 0.25
event-place! e4 -30 0
print "event-gain! ... : gain" (get e1 'gain) "; moved to" (get e4 'at) "for" (get e4 'dur) "at" (get e4 'az)
print "score-duration  :" (score-duration s) "s; event-end of the last:" (event-end e6)
score-print s

# --- 2. Rendering ------------------------------------------------------------------------------
print ""
print "--- rendering ---"
var r (render-event e1 8000)
print "render-event    :" (length r) "channel of" (length (head r)) "samples (cut to the duration, faded at the end)"
print "fit-duration    :" (length (fit-duration (ones 100) 1 8000)) "(shorter sounds are left alone)"
print "layout-channels : mono" (layout-channels "mono") "stereo" (layout-channels "stereo") "binaural" (layout-channels "binaural") "ambi 2" (layout-channels "ambi 2") "ring of 6" (layout-channels (speaker-ring 6)) "8 channels" (layout-channels 8)
print "place           : a mono sound at the left, stereo levels" (fixed (vec (map (place (list tone) "stereo" 90 0 8000) rms)) 3)
var mix (score-render s "stereo")
print "score-render    :" (length mix) "channels," (length (head mix)) "samples"
render s "/tmp/musil_reference_score.wav" "stereo"
print "render          : written to /tmp/musil_reference_score.wav"

# --- 3. The roll -------------------------------------------------------------------------------
print ""
print "--- the roll ---"
print "score-rows      :" (map (score-rows s) head)
var roll (score-roll s)
print "score-roll      : a figure with" (length (getidx (head (getidx roll 1)) 2)) "bars in" (length (getidx (head (getidx roll 1)) 1)) "rows"
print "event-lanes     :" (event-lanes (list (record (list 'at 0 'dur 2)) (record (list 'at 1 'dur 1)) (record (list 'at 3 'dur 1)))) "(overlapping events get lanes)"
print "display         : (display s) shows the roll: a staff per instrument, note heads and duration lines, the cursor follows play-score"

# --- 4. Transformations ------------------------------------------------------------------------
print ""
print "--- transformations ---"
score-shift! s 1
print "score-shift!    : the first event now at" (get e1 'at)
score-scale! s 0.5
print "score-scale!    : ... and at" (get e1 'at) "for" (get e1 'dur)
print "score-select    :" (length (score-select s (function (e) (equal? (get e 'kind) 'call)))) "calls"
score-sort! s
print "score-sort!     : first at" (get (head (score-events s)) 'at)
score-remove! s e6
print "score-remove!   :" (length (score-events s)) "events left"
print "score-merge     :" (length (score-events (score-merge s s))) "events in the merged score"

# --- 5. A database and notes ---------------------------------------------------------------------
print ""
print "--- database ---"
var db (db-load "data/microsol/microsol.spectrum.db")
print "db-load         :" (db-size db) "entries;" (get db 'type) "features," (get db 'ncoeff) "coefficients; root" (filename (get db 'root))
print "db-instruments  :" (db-instruments db)
print "db-techniques   :" (db-techniques db) " db-dynamics:" (db-dynamics db) " db-pitches:" (length (db-pitches db))
print "db-query        :" (length (db-query db 'Vn nil 'mf nil)) "mf violin sounds;" (length (db-query db 'Vn "C#4" 'mf 'ord)) "exact"
print "db-range        : violin" (db-range db 'Vn) "= " (midi->pitch (head (db-range db 'Vn))) "to" (midi->pitch (last (db-range db 'Vn)))
var e (head (db-query db 'Vn "C#4" 'mf 'ord))
print "an entry        :" (get e 'instr) (get e 'tech) (get e 'pitch) (get e 'dyn) (get e 'other) "midi" (get e 'midi)
print "db-features     :" (length (db-features e)) "values"
print "db-path         :" (filename (db-path db e)) "; db-available?" (db-available? db e)
print "pitch->midi     :" (pitch->midi "C4") (pitch->midi "D#5") (pitch->midi "Bb3") " midi->pitch:" (midi->pitch 75)
var n1 (note db 'Vn 'C4 'mf 'ord)
var n2 (note db 'Vn 'B4 'mf 'ord)
print "note            :" (get n1 'label) "shift" (get n1 'shift) ";" (get n2 'label) "uses" (get (get n2 'entry) 'pitch) "shifted" (get n2 'shift)
print "note-path       :" (filename (note-path n1)) " note-duration:" (fixed (note-duration n1) 2) "s"
var s2 (score "notes" 44100)
event s2 0 1 n1
event s2 0.5 1 n2
event s2 1 1 (note db 'Vc 'C4 'ff 'ord)
event s2 1 1 (note db 'Ob 'E4 'mf 'ord)
print "score-rows      : notes by instrument, orchestral order:" (map (score-rows s2) head)

# --- 6. More queries, making a database, playing ------------------------------------------------------
print ""
print "--- queries, db-gen, playing ---"
print "db-query (lists):" (length (db-query db (list 'Vn 'Vc) nil (list 'mf 'ff) nil)) "string sounds, mf or ff"
print "db-grep         :" (length (db-grep db "Vn-ord-C.?4-mf")) "violin C4 and C#4 (a regular expression on the file name)"
print "db-find         :" (length (db-find db "Vc-ord-C")) "cello C's (a substring)"
print "db-between      :" (length (db-between db 'Vn "C4" "E4")) "violin sounds between C4 and E4"
print "db-nearest      :" (map (db-nearest db e 3) (function (x) (filename (get x 'file)))) "(closest features)"
print "db-available    :" (length (db-available db)) "sounds on disk; db-instruments-available:" (db-instruments-available db)
print "db-gen          :" (db-gen (get db 'root) "/tmp/musil_reference_gen.db" "mfcc" 2048 256 13) "sounds analysed into /tmp/musil_reference_gen.db (mfcc, 13); db-make generates and loads"
print "play-score      : (play-score s gain) through live, ahead of the clock; (play-score-from s gain from) from a time"
print "score-schedule  : schedules and returns (the synths to free, the end and the start clock times); playhead reads the cursor"
print "db-merge        :" (db-size (db-merge (list db db))) "entries out of two databases (or (db-load (list \"a.db\" \"b.db\")))"
