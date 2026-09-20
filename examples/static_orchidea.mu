# static_orchidea: assisted orchestration of a bell (Orchidea, static). The target is one segment (the onset
# threshold 2 lets no onset through), analysed into the database's features and into the pitches of its partials
# (the partials above 0.3 of the strongest); the search space is the sounds of the orchestra at those pitches, and a
# genetic search of 300 individuals over 300 epochs looks for the combination of one sound per player (or a rest,
# with sparsity 0.001) whose summed spectrum is nearest the bell's. The ten best solutions are kept: the score takes
# the first; in the roll, click in the background and pick another from the menu to hear it in its place.
# Usage: musil static_orchidea.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 1)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4 (a sketch)
var db (db-load "../datasets/TinySOL.spectrum.db")           # TinySOL, after ./fetch_tinysol.sh; or FullSOL2020
var sr 44100

# --- the target: the bell, as a signal -----------------------------------------------------------------------------
var path "data/archeos-bell.wav"
var w (read-wav path)
var x (head (getidx w 1))
var fsr (head w)

# --- the orchestra: Orchidea's default, of what the database has (a symbol is a player; "Fl|Picc" would be a doubling) --
var here (db-instruments-available db)
var orch (orchestra (filter (list 'Fl 'Fl 'Ob 'Ob 'ClBb 'ClBb 'Bn 'Bn 'Hn 'Hn 'TpC 'TpC 'Tbn 'Tbn 'BTb 'Vn 'Vn 'Va 'Va 'Vc 'Vc 'Cb 'Cb) (function (i) (contains? here (str i)))))
print (orchestra-size orch) "players:" (orchestra-instruments orch)

# --- the parameters (Orchidea's names in the comments) --------------------------------------------------------------
var small (< (length (get db 'entries)) 100)                    # MicroSOL has no sound at the bell's pitches: no pitch filter then
var params (record (list
    'population 300            # pop_size
    'epochs     300            # max_epochs
    'sparsity   0.001          # how often a player is dropped from a solution
    'threshold  2              # onsets_threshold: above 1, one segment (static)
    'timegate   0.1            # onsets_timegate, seconds
    'partials   (if small 0 0.3)   # partials_filtering: the partials above 0.3 of the strongest give the pitches
    'solutions  10))           # the best kept per segment
if small { print "MicroSOL: the pitch filter is off (its four instruments have no sound at the bell's pitches); every sound may serve" }

# --- the target analysed, then orchestrated ------------------------------------------------------------------------
var target (mimetic-print (mimetic-target db x fsr params))
var r (orchestrate-mimetic db orch target fsr params)
solution-print r 0

# --- the score: the best solution, then the bell itself to compare --------------------------------------------------
var s (score "static orchidea" sr)
connect! s 0 r (get r 'choices)
event s (+ (get target 'seconds) 1) 0 (fragment->score "the bell" sr (list (list 0 (get target 'seconds) path)))
score-print s
render s "/tmp/musil_static_orchidea.wav" "stereo"
print "wrote /tmp/musil_static_orchidea.wav (the orchestration, then the bell)"
display s
# the roll: click in the background, then choose a solution from the menu; Play to hear it; Save... writes the connection
# (score-save s "bell.txt") writes Orchidea's connection file; (score-choose! s 0 0 1) puts the second solution in place
