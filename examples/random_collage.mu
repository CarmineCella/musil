# random_collage: a random piece from everything a score can hold, drawn from the bundled data:
# recordings placed and processed (stretched, reversed, filtered), notes from the database, a
# techno kick and a hoover from live.mu as synths, and functions rendering noise sweeps, all
# spatialised, rendered for headphones, shown as a roll, played.
# Usage: musil random_collage.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 11)
var sr 44100
var secs 24
#var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
var db (db-load "../datasets/FullSOL2020.spectrum.db")           # the full TinySOL, after ./fetch_tinysol.sh (or any *SOL set)

# --- material: recordings, and processed versions of them (the processing is a call, made when rendered) ---
var files (list "data/gong_c_sharp.wav" "data/archeos-bell.wav" "data/cage.wav" "data/Vox.wav")
function mono-of (path) (head (getidx (read-wav path) 1))
function stretched (path factor d) (take (pvoc-stretch (mono-of path) factor) (floor (* d sr)))       # slower, same pitch
function reversed (path d) (reverse (take (mono-of path) (floor (* d sr))))
function darkened (path cutoff d) (lowpass (take (mono-of path) (floor (* d sr))) sr cutoff 0.7)
function sweep (d) {
    var n (floor (* d sr))
    var half (floor (/ n 2))
    return (* 0.3 (highpass (noise n) sr 3000 0.7) (bpf 0 (list (list half 1) (list (- n half) 0))))
}

var s (score "collage" sr)
function at-random (lo hi) (+ lo (* (rand) (- hi lo)))
function pick (L) (getidx L (floor (* (rand) (length L))))

# --- 1. recordings: six, some plain, some processed ---------------------------------------------------------
each (range 6) (function (k) {
    var f (pick files)
    var d (at-random 1.5 5)
    var what (pick (list f (call stretched (list f (at-random 1.5 3) 'dur)) (call reversed (list f 'dur)) (call darkened (list f 600 'dur))))
    var e (event-at s (at-random 0 (- secs 5)) d what (at-random -150 150) (at-random -20 60))
    event-gain! e (at-random 0.4 0.9)
})
# --- 2. notes: a handful from whichever instruments have sounds on disk -----------------------------------
var here (db-instruments-available db)
each (range 10) (function (k) {
    var instr (pick here)
    var range (db-range db instr)
    var midi (+ (head range) (floor (* (rand) (+ 1 (- (last range) (head range))))))
    var e (event-at s (at-random 0 (- secs 3)) (at-random 0.5 2.5) (note db instr (midi->pitch midi) (pick (list 'pp 'mf 'ff)) 'ord) (at-random -90 90) 0)
    event-gain! e 0.7
})
# --- 3. synths from live.mu: a kick pattern and a hoover chord, as instruments ------------------------------
each (range 16) (function (k) (event-at s (+ 10 (* k 0.5)) 0.3 (instrument tkick (list)) 0 0))
each (list 45 52 57) (function (m) (event-at s 12 6 (instrument hoover (list (list 'freq (midi->hz m)) (list 'cutoff 900) (list 'glide 0.05))) (- (* 40 (rand)) 20) 10))
# --- 4. functions: noise sweeps, called with their durations ---------------------------------------------
each (range 4) (function (k) (event-at s (at-random 2 (- secs 2)) (at-random 0.5 1.5) sweep (at-random -180 180) (at-random 0 80)))

score-print s
render s "/tmp/musil_collage.wav" "binaural"
render s "/tmp/musil_collage_stereo.wav" "stereo"
print "wrote /tmp/musil_collage.wav (binaural: headphones) and /tmp/musil_collage_stereo.wav"
display s
if (interactive?) { play-score s 0.7 }
