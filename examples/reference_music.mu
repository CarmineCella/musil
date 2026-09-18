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
print "display         : (display s) shows the roll: staves, note heads and duration lines; Play from the cursor, a double-click plays an event"

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
event s2 1 1 (note db 'Ob "E4" 'mf 'ord)
print "score-rows      : notes by instrument, orchestral order:" (map (score-rows s2) head)

# --- 5b. Musical elements: rhythms, chords, lines, textures ------------------------------------------------
print ""
print "--- elements ---"
seed 2
var r (rhythm (list 1 0.5 -0.5 1))
print "rhythm, beats   :" r "lasts" (rhythm-duration r) "s; at 120 bpm:" (beats 120 r)
print "chord           :" (get (chord db 'Vn 'mf 'ord (list "C4" "E4" "G4")) 'label)
var ln (notes db 'Ob 'mf 'ord (list "C4" "D4" "E4") (rhythm (list 0.25 0.5)))
print "notes           :" (map ln (function (x) (list (head x) (getidx x 1) (get (last x) 'pitch)))) "(the rhythm repeats under the line)"
print "chords          :" (length (chords db 'Hn 'mf 'ord (list (list "C4" "E4") (list "D4" "F4")) (rhythm (list 1 -0.5 1)))) "chord events"
print "texture         :" (length (texture db (list 'Vn 'Vc) 'mf 'ord (list "C4" "D4") (rhythm (list 0.5 0.5)) (list 1 2))) "notes: one line at two speeds, the faster repeating"
print "pivots          :" (map (pivots db 'Ob 'mf 'ord (list "C4" "G4") 3 (rhythm (list 0.5 0.5 0.5))) (function (x) (get (last x) 'pitch))) "(two lines wandering around C4 and G4)"
print "chordinterp     :" (map (chordinterp db 'Vn 'mf 'ord (list "C4" "E4" "G4") (list "D4" "F4" "A4") (rhythm (list 0.5 0.5 0.5))) (function (x) (map (get (last x) 'notes) (function (n) (get n 'pitch)))))
print "transpose, invert, scale-pitches:" (transpose-pitches (list "C4" nil "E4") 2) (invert (list 60 64) "C4") (scale-pitches "D4" 'dorian (list 0 1 2 3 4 5 6 7))
print "more elements   : score-tempo!/add-beats!, fragment-gain/dynamics/articulate/thin/density/place/snap, chord-voicing, pitch-field, rotate/retrograde/cycle/interleave, harmonic-series, pitches-from-spectrum, rhythm-from-pattern/augment/diminish, tuplet, polyrhythm, walk, texture-staggered, arpeggio, chordinterp-ease/-sets, texture-on-chords/-pivots, orchestrate-line, score-map!, score-instrument (algorithmic_comp.mu uses them)"
print "fragments       : duration" (fragment-duration ln) "; shifted" (map (fragment-shift ln 2) head) "; scaled" (map (fragment-scale ln 2) head) "; repeated" (length (fragment-repeat ln 3)) "; until 2.6 s:" (length (fragment-until ln 2.6))
var sf (score "with fragments" 44100)
print "add!            :" (length (add! sf 0 ln)) "events placed;" (length (add-placed! sf 1 ln 45 0)) "more at the left"
var inner (fragment->score "inner" 44100 ln)
print "fragment->score :" (get inner 'name) "with" (length (score-events inner)) "events"
var e-in (event sf 2 0 inner)
print "a score inside  : kind" (get e-in 'kind) "for" (fixed (get e-in 'dur) 2) "s (0 = its whole length); rendered" (length (head (score-render sf "stereo"))) "samples; a score cannot contain itself"

# --- 5c. MIDI files, envelopes, the orchestra, the granulator -------------------------------------------------
print ""
print "--- midi, granular orchestration ---"
var mid (midi-read "data/test.mid")
print "midi-read       :" (length (get mid 'notes)) "notes in" (get mid 'tracks) "," (get mid 'seconds) "s"
print "midi->fragment  :" (length (midi->fragment "data/test.mid" db 'Vn 'ord)) "events; midi->score: an instrument per channel; velocity->dynamics" (velocity->dynamics 100)
print "env, env-at     :" (env-at (env (list 0 (list 4 6) 10 (list 0.5 1))) 5) "(ranges interpolated; sets crossfaded by chance)"
var orch (orchestra (list 'Ob 'Hn "Vn|Vc" (list 'Vn 'Vc)))
print "orchestra       :" (orchestra-size orch) "players," (orchestra-instruments orch) "; an ossia Vn|Vc, a paired (Vn Vc)"
seed 8
var gr (orchestrate-granular db orch 6 (record (list 'density (env (list 0 (list 4 6) 6 (list 1 2))) 'register (list 3 4) 'dynamics (env (list 0 (list 'pp) 6 (list 'ff))) 'solutions 2)))
print "orchestrate-granular:" (length (get gr 'segments)) "segment," (length (solutions gr 0)) "solutions; the best connection has" (length (best-connection gr)) "events; connect! places one"
print "methods         : random, pivots (chords, interval), chordinterp (chordinterp-env), markov (markov-score), harmonic (fundamental, harmonicity); coupling: how often a group sounds together"
print "db-index!       :" (length (keys (db-index! db))) "instruments indexed once (what note draws from); db-candidates, db-range-available, db-techniques-of" (db-techniques-of db 'Vn)

# --- 5d. Morphological orchestration -------------------------------------------------------------------
print ""
print "--- morphological ---"
var tsc (score "target" 44100)
event tsc 0 2 (note db 'Vc "C4" 'mf 'ord)
event tsc 0.5 0.3 (note db 'Ob "E4" 'mf 'ord)
var tg (target-analyse (head (score-render tsc "mono")) 44100 (get db 'block) 1024)
print "target-analyse  :" (fixed (get tg 'seconds) 2) "s," (length (get tg 'spectra)) "frames; curves: density (flux peaks a second), centroid, spread, low, loudness;" (length (get tg 'attacks)) "flux peaks"
print "target-envelopes: the granulator's parameters from the curves:" (sort-list (map (keys (target-envelopes tg (record (list)))) str))
print "target-peaks    : the pitches the notes may take, at 1 s:" (map (vec->list (target-peaks tg 1 6)) (function (f) (midi->pitch (round (hz->midi f)))))
seed 3
var mo (orchestrate-morphological db orch tg (record (list 'solutions 1)))
print "orchestrate-morphological:" (length (best-connection mo)) "notes with cents, e.g." (get (last (head (best-connection mo))) 'label) (get (last (head (best-connection mo))) 'cents) "cents; a matching pursuit at every event (target-notes), each note as long as the target keeps its sound (atom-persistence), no segmentation"

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
print "play-score      : (play-score s gain) renders the score, puts it in the concert hall (score-reverb!) and plays it; (play-score-from s gain from) from a time"
print "score-play-now  : starts and returns (the roll's Play); stop-score stops; playhead reads the cursor"
print "render-hall     :" (length (render-hall s2 "/tmp/musil_reference_hall.wav")) "channels in the hall, to a file (the roll's Render...)"
print "score-schedule  : the live way, every event on the clock (synths compiled, no hall); returns the synths to free"
print "db-merge        :" (db-size (db-merge (list db db))) "entries out of two databases (or (db-load (list \"a.db\" \"b.db\")))"
