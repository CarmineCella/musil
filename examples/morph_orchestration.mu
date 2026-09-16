# morph_orchestration: morphological orchestration. A target sound is analysed into curves over
# time (how many events a second, how many voices at once, where it sits in pitch, how loud,
# how long its spectrum persists), the orchestral granulator follows those curves instead of
# envelopes written by hand, and at every event a matching pursuit over the free players'
# sounds chooses instruments, pitches and playing styles for the target's spectrum at that
# instant, less what already sounds. Nothing is segmented: a long sound under short ones comes
# out as a long note under short ones.
# Usage: musil morph_orchestration.mu [target.wav] [seed]
load "music.mu"
load "plot.mu"
seed (if (> (length args) 1) (num (getidx args 1)) 3)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
# var db (db-load "../datasets/FullSOL2020.spectrum.db")       # FullSOL: every instrument and technique
var path (if (> (length args) 0) (getidx args 0) "data/A_minor.wav")

# --- the target and its analysis: the same spectral space as the database's features ------------------------------
var w (read-wav path)
var x (take (head (getidx w 1)) (* 12 (head w)))                 # up to twelve seconds
var sr (head w)
var t0 (clock)
var target (target-analyse x sr (get db 'block) 1024)
print (filename path) ":" (fixed (get target 'seconds) 1) "s analysed in" (fixed (- (clock) t0) 2) "s;" (length (get target 'events)) "events found"
print "  rate (events/s) up to" (fixed (max (get target 'rate)) 1) "; polyphony up to" (max (get target 'polyphony)) "; register" (fixed (min (get target 'low)) 0) "-" (fixed (max (get target 'centroid)) 0) "(MIDI); loudness" (fixed (min (get target 'loudness)) 0) "to" (fixed (max (get target 'loudness)) 0) "dB; coherence up to" (fixed (max (get target 'coherence)) 1) "s"

var fig (figure "the target's morphology")
add-line fig (get target 'times) (get target 'rate) "events / s"
add-line fig (get target 'times) (get target 'polyphony) "polyphony"
add-line fig (get target 'times) (/ (- (get target 'centroid) 12) 12) "centroid (octave)"
add-line fig (get target 'times) (/ (- (get target 'low) 12) 12) "lowest partial (octave)"
add-line fig (get target 'times) (/ (+ (get target 'loudness) 60) 10) "loudness (dB/10 + 6)"
add-line fig (get target 'times) (get target 'coherence) "coherence (s)"
set-labels fig "time (s)" ""
show fig

# --- the orchestra: every instrument on disk, the strings doubled --------------------------------------------------
var here (db-instruments-available db)
var orch (orchestra (reduce here (function (acc i) (if (contains? (list "Vn" "Va" "Vc" "Cb") i) (concat-list acc (list i i)) (concat-list acc (list i)))) (list)))
print (orchestra-size orch) "players:" (orchestra-instruments orch)

# --- the orchestration: the curves as envelopes, a pursuit at every event; two realisations ----------------------------
set t0 (clock)
var r (orchestrate-morphological db orch target (record (list 'solutions 2 'follow 1)))
print "orchestrated in" (fixed (- (clock) t0) 2) "s:" (length (best-connection r)) "and" (length (solution r 0 1)) "events in the two solutions"
var s (score "morphological" 44100)
connect! s 0 r (list 0)
event-at s (+ (get target 'seconds) 1) 0 (fragment->score "the target" 44100 (list (list 0 (get target 'seconds) path))) 0 0   # the target itself after it, to compare
score-print s
render s "/tmp/musil_morph_orchestration.wav" "stereo"
print "wrote /tmp/musil_morph_orchestration.wav: the orchestration, then the target"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
