# morph_orchestration: morphological orchestration. A target sound is analysed into curves (target-analyse) and
# the curves drive the orchestral granulator: the peaks of the spectral flux, counted a second, give the density
# of events; the spectral centroid and spread give the register; the loudness gives the dynamics. At every event a
# matching pursuit between the target's spectrum at that instant (less what already sounds) and the sounds of the
# free players at the target's spectral peaks picks instrument, pitch and technique, and each note lasts as long
# as the target keeps its sound (atom-persistence). Nothing is segmented: a long sound under short ones comes out
# as a long note under short ones.
# Usage: musil morph_orchestration.mu -- [target.wav] [seed]
load "music.mu"
load "plot.mu"
seed (if (> (length args) 1) (num (getidx args 1)) 3)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
# var db (db-load "../datasets/FullSOL2020.spectrum.db")       # FullSOL: every instrument and technique (short ones too)
var path (if (> (length args) 0) (getidx args 0) "data/A_minor.wav")

# --- the target and its analysis: the same spectral space as the database's features ------------------------------
var w (read-wav path)
var x (take (head (getidx w 1)) (* 12 (head w)))                 # up to twelve seconds
var sr (head w)
var t0 (clock)
var target (target-analyse x sr (get db 'block) 1024)
print (filename path) ":" (fixed (get target 'seconds) 1) "s analysed in" (fixed (- (clock) t0) 2) "s;" (length (get target 'attacks)) "flux peaks"

var fig (figure "the target's morphology")
add-line fig (get target 'times) (get target 'density) "density (flux peaks / s)"
add-line fig (get target 'times) (/ (- (get target 'centroid) 12) 12) "centroid (octave)"
add-line fig (get target 'times) (get target 'spread) "spread (octaves)"
add-line fig (get target 'times) (/ (- (get target 'low) 12) 12) "lowest partial (octave)"
add-line fig (get target 'times) (/ (+ (get target 'loudness) 60) 10) "loudness (dB/10 + 6)"
set-labels fig "time (s)" ""
show fig

# --- the orchestra: every instrument on disk, the strings doubled --------------------------------------------------
var here (db-instruments-available db)
var orch (orchestra (reduce here (function (acc i) (if (contains? (list "Vn" "Va" "Vc" "Cb") i) (concat-list acc (list i i)) (concat-list acc (list i)))) (list)))
print (orchestra-size orch) "players:" (orchestra-instruments orch)

# --- the orchestration ---------------------------------------------------------------------------------------------
set t0 (clock)
var r (orchestrate-morphological db orch target (record (list 'solutions 1)))
var f (best-connection r)
print "orchestration in" (fixed (- (clock) t0) 2) "s:" (length f) "notes; durations from" (fixed (min-of (map f (function (e) (getidx e 1)))) 2) "to" (fixed (max-of (map f (function (e) (getidx e 1)))) 2) "s"
granulation-report r                                             # the players' shares, the events the pursuit left unplayed
var s (score "morphological" 44100)
connect! s 0 r (list 0)
event-at s (+ (get target 'seconds) 1) 0 (fragment->score "the target" 44100 (list (list 0 (get target 'seconds) path))) 0 0   # the target itself after it, to compare
score-print s
render s "/tmp/musil_morph_orchestration.wav" "stereo"
print "wrote /tmp/musil_morph_orchestration.wav: the orchestration, then the target; (musil morph_orchestration.mu -- data/coque.wav) for the other target"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
# more of the target's activity: (orchestrate-morphological db orch (target-analyse-with x sr (get db 'block) 1024 (record (list 'threshold 1.5))) (record (list 'voices 3)))
