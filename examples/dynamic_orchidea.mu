# dynamic_orchidea: assisted orchestration of a piano phrase (Orchidea, dynamic). The target is cut at its onsets
# (the peaks of the spectral flux above 0.1 of the strongest, at least 0.1 s apart: some hundred and eighty segments
# of the A minor phrase), each segment analysed into the database's features and the pitches of its partials, and
# each orchestrated by the genetic search (300 individuals, 300 epochs). The connection then chooses, segment by
# segment, the solution nearest the previous one, and a note continued across a boundary on the same player and
# pitch becomes one longer note. Some minutes of search: the progress is printed.
# Usage: musil dynamic_orchidea.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 1)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4 (a sketch)
var db (db-load "../datasets/TinySOL.spectrum.db")           # TinySOL, after ./fetch_tinysol.sh; or FullSOL2020
var sr 44100

# --- the target: the piano phrase ----------------------------------------------------------------------------------
var path "data/A_minor.wav"
var w (read-wav path)
var x (head (getidx w 1))
var fsr (head w)

# --- the orchestra: Orchidea's default, of what the database has ----------------------------------------------------
var here (db-instruments-available db)
var orch (orchestra (filter (list 'Fl 'Fl 'Ob 'Ob 'ClBb 'ClBb 'Bn 'Bn 'Hn 'Hn 'TpC 'TpC 'Tbn 'Tbn 'BTb 'Vn 'Vn 'Va 'Va 'Vc 'Vc 'Cb 'Cb) (function (i) (contains? here (str i)))))
print (orchestra-size orch) "players:" (orchestra-instruments orch)

# --- the parameters --------------------------------------------------------------------------------------------------
var small (< (length (get db 'entries)) 100)                    # MicroSOL: no pitch filter (C4-G4 only), and a coarser segmentation
var params (record (list
    'population 300            # pop_size
    'epochs     300            # max_epochs
    'sparsity   0.001
    'threshold  0.1            # onsets_threshold: the flux peaks above a tenth of the strongest are onsets
    'timegate   0.1            # onsets_timegate: at least a tenth of a second apart
    'partials   (if small 0 0.3)
    'solutions  10
    'connection 'closest))     # each segment's solution the nearest to the previous segment (Orchidea's); 'best: the best of each
if small { print "MicroSOL: the pitch filter is off (its four instruments have no sound at most of the piano's pitches)" }

# --- the target analysed, then orchestrated ------------------------------------------------------------------------
var target (mimetic-print (mimetic-target db x fsr params))
var r (orchestrate-mimetic db orch target fsr params)

# --- the score: the connection, then the piano itself to compare -----------------------------------------------------
var s (score "dynamic orchidea" sr)
connect! s 0 r (get r 'choices)
event s (+ (get target 'seconds) 1) 0 (fragment->score "the piano" sr (list (list 0 (get target 'seconds) path)))
score-print s
render s "/tmp/musil_dynamic_orchidea.wav" "stereo"
print "wrote /tmp/musil_dynamic_orchidea.wav (the orchestration, then the piano)"
display s
# the roll: a dashed line at every segment; click in one and choose another of its solutions from the menu
