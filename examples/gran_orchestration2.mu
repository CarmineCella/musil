# gran_orchestration2: the orchestral granulator, one parameter at a time. A single process of two minutes,
# every player of the orchestra in one group so that they can strike together:
#   0-20 s    a regular pulse, everyone together (coupling 1), two strokes a second, short and equal (0.15 s),
#             piano, in the narrowest band: octave 3
#   20-60 s   the pulse quickens (to eight a second), the notes lengthen (to 1.5 s), the dynamics rise (to
#             forte), and the band opens from octave 3 to octaves 2-6, the players still together
#   60-90 s   the players come apart (coupling 1 -> 0: each on its own time), and the band climbs to octave 6
#             alone, very high and very loud (fff), fast and short
#   90-120 s  the end: long, loud sounds in octave 2, struck together again
# Every change is an envelope on one parameter: density, duration, dynamics, register, coupling; the pitches are
# random within the band ('method 'random). The roll shows the process; compare it with the description above.
# The report after the run says how many events were due and played and how busy each player was; the validation
# checks the score against the orchestra (gran_orchestration3.mu goes on from here: the players as people).
# Meant for a full database: with the bundled MicroSOL (four instruments, C4-G4 only) the octaves 2, 3 and 6 are
# beyond every instrument and the granulator says so (unplayable, nothing written) rather than inventing notes.
# Usage: musil gran_orchestration2.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 5)
#var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4 (octaves 2, 3 and 6 are beyond it)
var db (db-load "../datasets/FullSOL2020.spectrum.db")       # FullSOL: every instrument over its whole range
var sr 44100

# --- the orchestra: ossia players (the band decides the instrument), all in one group so they strike together -------
var here (db-instruments-available db)
function have (L) (filter L (function (i) (contains? here (str i))))
function ossia-of (L) (join (map (have L) str) "|")
var string-player (ossia-of (list 'Vn 'Va 'Vc 'Cb))
var wind-player (ossia-of (list 'Picc 'Fl 'Ob 'ClBb 'Bn))
var brass-player (ossia-of (list 'TpC 'Hn 'Tbn 'BTb))
function players (spec n) (filter (map (vec->list (range n)) (function (j) spec)) (function (p) (not (equal? p ""))))
var group (concat-list (players string-player 12) (concat-list (players wind-player 6) (players brass-player 4)))
var orch (orchestra (list group))                              # one list: one paired group
print (orchestra-size orch) "players in one group:" (orchestra-instruments orch)

# --- the process: five envelopes over 120 s ---------------------------------------------------------------------
var p (record (list
    'method   'random
    'coupling (env (list 0 1  60 1  90 0  90.01 1  120 1))                     # together, apart from 60 to 90, together again
    'density  (env (list 0 (list 2 2)  20 (list 2 2)  60 (list 8 8)  90 (list 12 12)  90.01 (list 0.8 0.8)  120 (list 0.5 0.5)))   # strokes a second (a fixed value: a regular pulse)
    'duration (env (list 0 (list 0.15 0.15)  20 (list 0.15 0.15)  60 (list 1.5 1.5)  90 (list 0.3 0.3)  90.01 (list 5 8)  120 (list 6 9)))
    'dynamics (env (list 0 0.3  20 0.3  60 0.75  90 1  120 1))                 # a level: p (0.3) to fff (1)
    'register (env (list 0 (list 3 3)  20 (list 3 3)  60 (list 2 6)  90 (list 6 6)  90.01 (list 2 2)  120 (list 2 2)))
    'spread   0.02                                                             # a strike together is scattered by up to 20 ms: an orchestra's attack
    'styles   (list 'ord)))
var r (orchestrate-granular db orch 120 p)
var f (best-connection r)
print (length f) "notes"
granulation-report r                                             # events due and played, the saturation, every player's share
var s (score "granular process" sr)
connect! s 0 r (list 0)
score-print s
validation-print (score-validate s orch)                         # every note has a player of its own, at a recorded pitch
render s "/tmp/musil_gran_orchestration2.wav" "stereo"
print "wrote /tmp/musil_gran_orchestration2.wav (in the hall)"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
