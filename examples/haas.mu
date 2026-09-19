# haas: a simulation of the processes of Georg Friedrich Haas with the orchestral granulator, after "in vain"
# (2000, for 24 players). Haas's material is not a theme but a tuning: the music moves between the equal
# temperament of the piano and the just intonation of the overtone series, and the passage from one to the other
# is the drama. The processes here:
#   the cascades     every player runs down a chromatic scale, fast, and starts again at the top when it reaches
#                    the bottom of its band, all out of step: a waterfall of tempered scales (a signed 'leap, the
#                    restarts at the top with 'tilt), swelling and ebbing in waves
#   the overtone chord   the light: a chord of partials of a low fundamental ('method 'harmonic), tuned justly
#                    ('just 1: the 7th partial 31 cents low, the 11th 49 low, the 13th 41 high; the roll shows the
#                    cents), held in long notes that swell one after another, the brass low in it; its harmonicity
#                    rises from a cloud around the partials to the pure spectrum
#   the beating      the strings on one pitch, then a few, a quarter-tone and a sixth-tone apart (cents set on the
#                    notes after the draw), the beats slow and fast: a chord that shimmers instead of standing still
#   the return       the cascades come back and slow down until each run is a few long notes and the waterfall
#                    freezes; under it the last overtone chord, a fifth lower, fades out
# One orchestra for the piece ('continue), the doublings seated; the score validated at the end.
# Meant for a full database: with the bundled MicroSOL (Ob, Hn, Vn, Vc, C4-G4) the bands are clipped to a fifth.
# Usage: musil haas.mu [timescale] [seed]     (timescale 0.25: a preview; 1: five and a half minutes)
load "music.mu"
var k (if (> (length args) 0) (num (getidx args 0)) 1)
seed (if (> (length args) 1) (num (getidx args 1)) 13)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
# var db (db-load "../datasets/FullSOL2020.spectrum.db")       # FullSOL: every instrument over its whole range
var sr 44100

# --- the ensemble: 22 players after in vain's, the flutes doubling piccolo (a seated ossia) ------------------------
var here (db-instruments-available db)
function have (L) (filter L (function (i) (contains? here (str i))))
function ossia-of (L) (join (map (have L) str) "|")
function players (spec n) (filter (map (vec->list (range n)) (function (j) spec)) (function (p) (not (equal? p ""))))
function some (L) (reduce (map L (function (p) (players (ossia-of (head p)) (last p)))) concat-list (list))
var wind-spec (some (list (list (list 'Fl 'Picc) 2) (list (list 'Ob) 1) (list (list 'ClBb) 2) (list (list 'Bn) 1)))
var brass-spec (some (list (list (list 'Hn) 2) (list (list 'TpC) 1) (list (list 'Tbn) 1) (list (list 'BTb) 1)))
var string-spec (some (list (list (list 'Vn) 5) (list (list 'Va) 2) (list (list 'Vc) 2) (list (list 'Cb) 2)))
var orch (orchestra (concat-list wind-spec (concat-list brass-spec string-spec)))
function of (L) (filter orch (function (p) (any? (get p 'instrs) (function (i) (contains? (map L str) i)))))   # the same records
var winds (of (list 'Fl 'Picc 'Ob 'ClBb 'Bn))
var brass (of (list 'Hn 'TpC 'Tbn 'BTb))
var strings (of (list 'Vn 'Va 'Vc 'Cb))
var not-brass (concat-list winds strings)
var reach (orchestra-octaves db orch)
function band (lo hi) (list (max (head reach) (min (- (last reach) 0.5) lo)) (min (last reach) (max (+ (head reach) 0.5) hi)))
print (orchestra-size orch) "players:" (orchestra-instruments orch) "; they reach octaves" (fixed (head reach) 1) "to" (fixed (last reach) 1)

# --- waves: a dynamics envelope that swells and ebbs every period, its peaks following a curve ---------------------
# (waves secs period lo peak-of): breakpoints every half period, lo then the peak (peak-of u, u the position 0..1)
function waves (secs period lo peak-of) {
    var kv (list)
    var t 0
    var up 0
    while (<= t secs) { push kv t
                        push kv (if up (peak-of (/ t secs)) lo)
                        set up (not up)
                        set t (+ t (* 0.5 period)) }
    return (env kv)
}
function ramp (a b) (function (u) (+ a (* u (- b a))))

# --- a section: a granulator on some of the players, the state carried ('continue, 'start); => the fragment ------
var s (score "haas" sr)
function section (name start secs who params) {
    put! params 'continue 1
    put! params 'start (* k start)
    put! params 'seat (* k 60)
    var r (orchestrate-granular db who (* k secs) params)
    var rep (get r 'report)
    print name ":" (fixed (* k start) 0) "-" (fixed (* k (+ start secs)) 0) "s," (get rep 'notes) "notes;" (get rep 'skipped-busy) "events skipped, the players busy;" (get rep 'skipped-unplayable) "unplayable"
    return (best-connection r)
}
function lay (start f) (add! s (* k start) f)                     # the fragment into the score at a time of the piece
# densities in events a second and durations in seconds of the piece (the timescale applied here)
function per-sec (d) (list (/ (head d) k) (/ (last d) k))
function secs-of (d) (list (* k (head d)) (* k (last d)))

# 1. the cascades (0-75 s): everyone but the brass, each a descending chromatic run in its own time, restarting at
#    the top; waves of eight seconds, rising from p to ff
lay 0 (section "cascades" 0 75 not-brass (record (list
    'method 'random  'coupling 0  'leap (list -2 -1)  'tilt 0.8
    'density (per-sec (list 50 80))  'duration (secs-of (list 0.12 0.2))  'register (band 3 6.5)      # everyone running: ten to fifteen notes sounding at once
    'dynamics (waves (* k 75) (* k 8) 0.25 (ramp 0.5 0.9)))))

# 2. into the dark (65-95 s): the cascades thin out and slow down, the notes lengthen, the dynamics sink
lay 65 (section "into the dark" 65 30 not-brass (record (list
    'method 'random  'coupling 0  'leap (list -2 -1)  'tilt (env (list 0 0.8 (* k 30) 0))
    'density (env (list 0 (per-sec (list 60 60)) (* k 30) (per-sec (list 1 1))))
    'duration (env (list 0 (secs-of (list 0.15 0.2)) (* k 30) (secs-of (list 2 4))))
    'register (band 3 6.5)  'dynamics (env (list 0 0.6 (* k 30) 0.05)))))

# 3. the overtone chord (85-185 s): the light. Partials of C1 in just intonation, the brass low in the chord, long
#    notes swelling in waves of twelve seconds; the harmonicity rises from a cloud around the partials to the pure
#    spectrum, and the chord sinks from ff to ppp at the end
lay 85 (section "overtone chord" 85 100 orch (record (list
    'method 'harmonic  'fundamental "C1"  'just 1  'harmonicity (env (list 0 0.5 (* k 40) 1 (* k 100) 1))
    'coupling 0  'inertia 0.9  'tilt -0.4  'balance (record (list 'brass -4))
    'density (per-sec (list 1.5 3))  'duration (secs-of (list 6 12))  'register (band 1.5 5.5)
    'dynamics (waves (* k 100) (* k 12) 0.05 (function (u) (if (< u 0.8) (+ 0.5 (* 0.5 (sin (* 3.1416 u)))) (* 0.4 (- 1 u))))))))

# 4. the beating (180-245 s): the strings on one pitch, then a few, every other note a quarter-tone or a sixth-tone
#    away (cents set after the draw): the chord shimmers. Waves of six seconds
var chords (env (list 0 (list (list "E4")) (* k 25) (list (list "E4")) (* k 40) (list (list "E4" "F4")) (* k 65) (list (list "D4" "E4" "F4" "F#4"))))
var beating (section "beating" 180 65 strings (record (list
    'method 'random  'chords chords  'chord-weight 1  'coupling 0  'inertia 1
    'density (per-sec (list 2 4))  'duration (secs-of (list 4 8))  'register (band 3 5)
    'dynamics (waves (* k 65) (* k 6) 0.1 (ramp 0.4 0.7)))))
var detune (list 0 33 -33 50 0 -50 33 -33)                                     # sixth-tones and quarter-tones, by turns
each (zip beating (vec->list (range (length beating)))) (function (p) (note-cents! (last (head p)) (getidx detune (mod (last p) (length detune)))))
lay 180 beating

# 5. the return (240-300 s): the cascades again, slowing until each run is a few long notes and the waterfall
#    freezes; the brass join at the end
lay 240 (section "return" 240 60 orch (record (list
    'method 'random  'coupling 0  'leap (list -2 -1)  'tilt (env (list 0 0.8 (* k 60) 0))
    'density (env (list 0 (per-sec (list 60 60)) (* k 30) (per-sec (list 6 6)) (* k 60) (per-sec (list 0.5 0.5))))
    'duration (env (list 0 (secs-of (list 0.12 0.2)) (* k 30) (secs-of (list 0.5 1)) (* k 60) (secs-of (list 5 9))))
    'register (band 3 6.5)  'dynamics (waves (* k 60) (* k 8) 0.2 (ramp 0.8 0.1)))))

# 6. the last light (270-330 s): the overtone chord a fifth lower, pure, thinning and fading out
lay 270 (section "last light" 270 60 orch (record (list
    'method 'harmonic  'fundamental "F0"  'just 1  'harmonicity 1  'coupling 0  'inertia 1  'tilt -0.3
    'balance (record (list 'brass -4))
    'density (env (list 0 (per-sec (list 2 2)) (* k 60) (per-sec (list 0.3 0.3))))
    'duration (secs-of (list 8 14))  'register (band 1 5)
    'dynamics (env (list 0 0.3 (* k 30) 0.3 (* k 60) 0)))))

score-print s
validation-print (score-validate s orch)                          # every note has a player of its own, at a recorded pitch
render s "/tmp/musil_haas.wav" "stereo"
print "wrote /tmp/musil_haas.wav (in the hall);" (fixed (score-duration s) 0) "s"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
