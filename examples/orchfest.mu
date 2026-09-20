# orchfest: the three orchestrators in one score, one after the other, on the same orchestra and the same target:
# the first eight seconds of the A minor piano phrase (data/A_minor.wav), a dynamic target with a chord at every
# onset.
#   0-8 s      granular: a process described by envelopes; here the pitches follow the phrase (a 'chords envelope
#              made of the pitches Orchidea reads in each segment, at the segment's time), the register its centroid
#              (target-analyse), the density its onsets: the phrase as a cloud that keeps its harmony but not its notes
#   10-18 s    morphological: the phrase's morphology drives the granulator (the flux peaks give the density, the
#              centroid the register, the loudness the dynamics) and a matching pursuit at each event picks the
#              sounds at the phrase's partials; nothing is segmented
#   20-28 s    mimetic (Orchidea): the phrase cut at its onsets and each chord matched as a whole by a genetic search
#              over the orchestra's sounds at the chord's pitches; the solutions connected by the shortest melodic
#              path ('path) with dovetailing; a dashed line at every segment in the roll, a menu of its solutions
#   30 s       the piano itself, to compare
# Usage: musil orchfest.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 2)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4 (a sketch)
var db (db-load "../datasets/TinySOL.spectrum.db")           # TinySOL, after ./fetch_tinysol.sh; or FullSOL2020
var sr 44100
var path "data/A_minor.wav"
var w (read-wav path)
var fsr (head w)
var secs 8
var x (take (head (getidx w 1)) (floor (* secs fsr)))            # the first eight seconds

# --- one orchestra for the three --------------------------------------------------------------------------------------
var here (db-instruments-available db)
var orch (orchestra (filter (list 'Fl 'Ob 'ClBb 'Bn 'Hn 'TpC 'Tbn 'Vn 'Vn 'Va 'Vc 'Cb) (function (i) (contains? here (str i)))))
var small (< (length (get db 'entries)) 100)                    # MicroSOL: no pitch filter (C4-G4 only)
print (orchestra-size orch) "players:" (orchestra-instruments orch)
var s (score "orchfest" sr)

# --- the target read once by Orchidea's analysis: the segments at the onsets, the pitches of each ---------------------
var target (mimetic-print (mimetic-target db x fsr (record (list 'threshold 0.1 'timegate 0.1 'partials 0.3))))
var segs (get target 'segments)

# --- 1. granular: the phrase's harmony as a chord envelope, its onsets as the density ------------------------------------
var chord-env (env (reduce (map segs (function (g) (list (get g 'at) (list (map (get g 'notes) head))))) concat-list (list)))
var morph-target (target-analyse (head (to-rate (list x) fsr sr)) sr (get db 'block) 1024)
var reach (orchestra-octaves db orch)
var gran (orchestrate-granular db orch secs (record (list
    'method 'random  'coupling 0  'chords chord-env  'chord-weight 1
    'density (list 6 10)  'duration (list 0.2 0.6)
    'register (list (head reach) (last reach))  'dynamics 0.5)))
connect! s 0 gran (list 0)
print "granular:" (length (best-connection gran)) "notes"

# --- 2. morphological: the phrase's curves drive the granulator, a pursuit at every event -----------------------------------
var morph (orchestrate-morphological db orch morph-target (record (list 'solutions 1 'density-scale 1.5)))
connect! s 10 morph (list 0)
print "morphological:" (length (best-connection morph)) "notes"

# --- 3. mimetic: Orchidea, segment by segment, the solutions connected by the shortest melodic path ----------------------
var mparams (record (list 'population 100 'epochs 100 'sparsity 0.01 'threshold 0.1 'timegate 0.1 'partials (if small 0 0.3)
                          'solutions 6 'connection 'path 'movement 1 'dovetail 0.5 'hold 4))
var mimetic (orchestrate-mimetic db orch (if small x target) fsr mparams)
connect! s 20 mimetic (get mimetic 'choices)
solution-print mimetic 0

# --- the piano itself, then the whole ------------------------------------------------------------------------------------
event s 30 0 (fragment->score "the piano" sr (list (list 0 secs path)))
score-print s
render s "/tmp/musil_orchfest.wav" "stereo"
print "wrote /tmp/musil_orchfest.wav"
display s
# in the roll the mimetic segments (20-28 s) each have a menu of solutions: click in one and choose
