# gran_orchestration: orchestration as granular synthesis. A process is described by envelopes
# over time (density of events, register, playing styles, dynamics, chords, harmonicity) and
# realised by the players of an orchestra, never more at once than there are players. The
# orchestra and the playing styles are taken from whatever database is loaded: every
# instrument it has, twice for the strings, and every technique it was recorded with.
# Two processes: a unison E4, slow and long, ppp, that accelerates to ten events a second, very
# short, in every style, over octaves 2 to 6 at fff; then, from there, a slowing down at fff
# that turns towards the harmonic series of E2.
# Usage: musil gran_orchestration.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 4)
#var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4, ordinario only
# var db (db-load "../datasets/TinySOL.spectrum.db")           # the full TinySOL, after ./fetch_tinysol.sh
var db (db-load "../datasets/FullSOL2020.spectrum.db")       # FullSOL: every technique
var sr 44100

# --- the orchestra from the database: every instrument on disk, the strings doubled ----------------------------
var here (db-instruments-available db)
var spec (reduce here (function (acc i) (if (contains? (list "Vn" "Va" "Vc" "Cb") i) (concat-list acc (list i i)) (concat-list acc (list i)))) (list))
var orch (orchestra (map spec (function (i) i)))
# the techniques: everything any instrument was recorded with (a note falls back to the instrument's own when a
# style is not among its recordings)
var styles (sort-list (unique (reduce (map here (function (i) (db-techniques-of db i))) concat-list (list))))
var dyns (list 'ppp 'pp 'p 'mp 'mf 'f 'ff 'fff)
print (orchestra-size orch) "players:" (orchestra-instruments orch)
print (length styles) "playing styles:" styles

# --- 1. unison E4, slow and long, ppp ... ten events a second, very short, every style, octaves 2-6, fff (50 s) -------
var p1 (record (list
    'density  (env (list 0 (list 0.3 0.5) 15 (list 0.5 1) 35 (list 4 6) 50 (list 10 10)))    # events per second
    'duration (env (list 0 (list 3 6) 20 (list 1 2) 50 (list 0.05 0.12)))                 # seconds
    'chords   (list (list "E4"))                                                          # the unison ...
    'chord-weight (env (list 0 1 15 1 45 0))                                              # ... dissolving into free pitches from 15 s
    'register (env (list 0 (list 4 4) 15 (list 4 4) 50 (list 2 6)))                       # octaves, widening
    'styles   (env (list 0 (list 'ord) 20 (list 'ord) 50 styles))                        # every style at the end, crossfaded
    'dynamics (env (list 0 0 15 0.1 50 1))                                                # a level: ppp (0) to fff (1), continuous
    'method   'random
    'solutions 2))
var r1 (orchestrate-granular db orch 50 p1)
print "1: unison to storm:" (length (best-connection r1)) "events"

# --- 2. from there: fff kept, slowing down, the pitches turning to the harmonic series of E2 (50 s) --------------------
var p2 (record (list
    'density  (env (list 0 (list 10 10) 20 (list 3 4) 50 (list 0.3 0.5)))
    'duration (env (list 0 (list 0.05 0.12) 25 (list 0.5 1.5) 50 (list 3 8)))
    'register (list 2 6)
    'styles   (env (list 0 styles 35 (list 'ord)))
    'dynamics 1                                                                           # fff throughout
    'method   'harmonic
    'fundamental "E2"
    'harmonicity (env (list 0 0 15 0.3 50 1))                                          # nothing to everything on the series
    'solutions 2))
var r2 (orchestrate-granular db orch 50 p2)
print "2: storm to the E2 spectrum:" (length (best-connection r2)) "events"

# --- the score: the two processes end to end; the other solutions are one index away ------------------------------
var s (score "granular" sr)
connect! s 0 r1 (list 0)
connect! s 50 r2 (list 0)                                        # (list 1) for the other realisation
score-print s
granulation-report r2                                            # events due and played, the saturation, every player's share
validation-print (score-validate s orch)                         # every note has a player of its own, at a recorded pitch
render s "/tmp/musil_gran_orchestration.wav" "stereo"
print "wrote /tmp/musil_gran_orchestration.wav (in the hall, as the roll plays it)"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
