# orchfest: the three orchestrators in one score, one after the other, on the same orchestra and the same sound.
#   0-20 s     granular: a process described by envelopes (a cloud that gathers into the bell's pitches: 'chords
#              taken from the bell's partials, 'chord-weight rising, the density falling, the notes lengthening)
#   21-33 s    morphological: the bell's morphology drives the granulator (the flux peaks give the density, the
#              centroid the register, the loudness the dynamics) and a matching pursuit at each event picks the
#              sounds; nothing is segmented
#   34-41 s    mimetic (Orchidea): the bell's spectrum matched as a whole by a genetic search over the orchestra's
#              sounds at the bell's pitches; a static target, one solution of ten (the roll's menu has the others)
#   42 s       the bell itself, to compare
# Usage: musil orchfest.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 2)
#var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4 (a sketch)
var db (db-load "../datasets/TinySOL.spectrum.db")           # TinySOL, after ./fetch_tinysol.sh; or FullSOL2020
var sr 44100
var path "data/archeos-bell.wav"
var w (read-wav path)
var x (head (getidx w 1))
var fsr (head w)
var secs (/ (length x) fsr)

# --- one orchestra for the three --------------------------------------------------------------------------------------
var here (db-instruments-available db)
var orch (orchestra (filter (list 'Fl 'Ob 'ClBb 'Bn 'Hn 'TpC 'Tbn 'Vn 'Vn 'Va 'Vc 'Cb) (function (i) (contains? here (str i)))))
var small (< (length (get db 'entries)) 100)
print (orchestra-size orch) "players:" (orchestra-instruments orch)
var s (score "orchfest" sr)

# --- 1. granular: a cloud that settles on the bell's pitches -------------------------------------------------------------
var target (mimetic-target db x fsr (record (list 'threshold 2 'partials 0.3)))     # the bell's partials, as pitches (its one segment)
var bell-pitches (map (get (head (get target 'segments)) 'notes) head)
print "the bell's partials:" bell-pitches
var reach (orchestra-octaves db orch)
var gran (orchestrate-granular db orch 20 (record (list
    'method 'random  'coupling 0
    'chords (if (== (length bell-pitches) 0) nil (list bell-pitches))  'chord-weight (env (list 0 0 8 0 20 1))   # free pitches at first, the bell's chord at the end
    'density (env (list 0 (list 8 12) 20 (list 1 2)))  'duration (env (list 0 (list 0.1 0.3) 20 (list 3 5)))
    'register (list (head reach) (last reach))  'dynamics (env (list 0 0.2 20 0.6)))))
connect! s 0 gran (list 0)
print "granular:" (length (best-connection gran)) "notes"

# --- 2. morphological: the bell's morphology drives the granulator ------------------------------------------------------
var morph-target (target-analyse (head (to-rate (list x) fsr sr)) sr (get db 'block) 1024)
var morph (orchestrate-morphological db orch morph-target (record (list 'solutions 1 'density-scale 2)))
connect! s 21 morph (list 0)
print "morphological:" (length (best-connection morph)) "notes"

# --- 3. mimetic: Orchidea's search on the bell's spectrum ---------------------------------------------------------------
var mparams (record (list 'population 100 'epochs 100 'sparsity 0.01 'threshold 2 'partials (if small 0 0.3) 'solutions 10))   # MicroSOL: no pitch filter (nothing at the bell's pitches)
var mimetic (orchestrate-mimetic db orch x fsr mparams)
connect! s 34 mimetic (get mimetic 'choices)
solution-print mimetic 0

# --- the bell itself, then the whole ------------------------------------------------------------------------------------
event s 42 0 (fragment->score "the bell" sr (list (list 0 secs path)))
score-print s
render s "/tmp/musil_orchfest.wav" "stereo"
print "wrote /tmp/musil_orchfest.wav"
display s
# in the roll the mimetic segment (34 s) has a menu of solutions: click in it and choose one
