# gran_orchestration3: the players as people. gran_orchestration2 taught the granulator's envelopes; this one
# teaches what makes its output a part a real player can sit down and play, one feature per section, and how to
# check it. One orchestra for the whole piece, its state carried from section to section ('continue), so that no
# player is ever booked twice, even where sections overlap; a report after each section; a validation at the end.
#   0-20 s    the seating: ossia players ("Vn|Vc": the band decides the instrument) take an instrument at their first
#             note and keep it ('seat): a person plays one instrument, not one per note
#   20-40 s   lines: each player moves by steps from its last pitch ('leap 2) and keeps its bowing ('inertia 0.9):
#             a micropolyphony of chromatic lines instead of a cloud of random points
#   40-60 s   the cluster: 'method 'cluster gives every pitch of the band once before any is repeated (the players
#             saturate the band chromatically, as in Ligeti), and 'weight moves the draws from the middle of the
#             band to its edges
#   60-80 s   the strike: the paired groups struck together ('coupling 1), each group at its own rate
#             ('group-density: strings, winds, brass), the attacks scattered by up to 60 ms ('spread), the
#             durations drawn log-uniformly ('duration-law 'log: as many short as long by ratio)
#   80-100 s  the dynamics as a constraint: 'dynamics-strict 1 makes the dynamics a filter on the samples like the
#             technique (with the bundled MicroSOL, recorded mf and one C4 ff of the cello, an ff can only be that
#             note: the report says what could not be played); 'balance keeps the brass 6 dB under the strings
#   90-120 s  two granulators at once on the same people: the tutti's tail and a low band, overlapping; since the
#             orchestra carries its bookings, the second finds only the players the first left free
# Then (score-validate) checks the whole score against the orchestra as a copyist would, and the same overlap
# written without 'continue is checked too, to show what goes wrong: the same people booked twice.
# Meant for a full database: with MicroSOL (Ob, Hn, Vn, Vc, C4-G4, mf only) the bands are clipped to what it has
# and every note at another level than mf counts as "at another dynamics" in the reports.
# Usage: musil gran_orchestration3.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 5)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
# var db (db-load "../datasets/FullSOL2020.spectrum.db")       # FullSOL: every instrument over its whole range
var sr 44100

# --- the orchestra: three paired groups (strings, winds, brass), the string players ossia; the groups matter only
#     where they are struck together (the strike): elsewhere 'coupling 0 leaves every player on its own time ----------
var here (db-instruments-available db)
function have (L) (filter L (function (i) (contains? here (str i))))
function ossia-of (L) (join (map (have L) str) "|")
function players (spec n) (filter (map (vec->list (range n)) (function (j) spec)) (function (p) (not (equal? p ""))))
var string-spec (players (ossia-of (list 'Vn 'Va 'Vc 'Cb)) 8)
var wind-spec (players (ossia-of (list 'Fl 'Ob 'ClBb 'Bn)) 3)
var brass-spec (players (ossia-of (list 'Hn 'TpC 'Tbn)) 3)
var orch (orchestra (filter (list string-spec wind-spec brass-spec) (function (g) (> (length g) 0))))   # each family a paired group
var strings (take orch (length string-spec))                    # the same records: the sections below share the people
var low (concat-list (drop strings (- (length strings) 3)) (drop orch (- (length orch) 2)))   # the last three string players and the last two brass
var reach (orchestra-octaves db orch)
function band (lo hi) (list (max (head reach) (min (- (last reach) 0.5) lo)) (min (last reach) (max (+ (head reach) 0.5) hi)))
print (orchestra-size orch) "players in three groups:" (orchestra-instruments orch) "; they reach octaves" (fixed (head reach) 1) "to" (fixed (last reach) 1)
var styles (unique (reduce (map here (function (i) (db-techniques-of db i))) concat-list (list)))
function styled (L) { var got (filter L (function (t) (contains? styles (str t))))
                      return (if (== (length got) 0) (list 'ord) got) }

# --- a section: a granulator on some of the players, its state carried ('continue, 'start), its report printed ----
var s (score "the players as people" sr)
function section (name start secs who params) {
    put! params 'continue 1
    put! params 'start start
    var r (orchestrate-granular db who secs params)
    connect! s start r (list 0)
    var rep (get r 'report)
    print "--" name ":" start "-" (+ start secs) "s;" (get rep 'notes) "notes," (get rep 'skipped-busy) "events skipped with every able player busy (saturation" (fixed (* 100 (get rep 'saturation)) 0) "%)," (get rep 'skipped-unplayable) "unplayable," (get rep 'substituted) "at another dynamics"
    return r
}

# 1. the seating: the band spans the orchestra; an ossia player takes the instrument of its first note and keeps it
var r1 (section "seating" 0 20 orch (record (list
    'method 'random  'seat 1000  'coupling 0  'density (list 3 5)  'duration (list 1 3)  'register (band 2 6)  'dynamics 0.4)))
print "   seats:" (join (map (get (get r1 'report) 'players) (function (p) (concat (join (get p 'instrs) "|") (if (equal? (type (get p 'seat)) "nil") "" (concat "=" (str (get p 'seat))))))) " ")

# 2. lines: steps from the last pitch, the bowing kept
section "lines" 20 20 strings (record (list
    'method 'random  'seat 1000  'coupling 0  'leap 2  'inertia 0.9  'styles (styled (list 'ord 'trem 'sul-tasto))
    'density (list 6 8)  'duration (list 0.3 0.8)  'register (band 3 5)  'dynamics 0.3))

# 3. the cluster: every pitch of the band once before any is repeated; the middle first, then the edges
section "cluster" 40 20 orch (record (list
    'method 'cluster  'seat 1000  'coupling 0  'weight (env (list 0 1 20 -1))  'density (list 4 4)  'duration (list 2 4)
    'register (band 2 6)  'dynamics (env (list 0 0.2 20 0.7))))

# 4. the strike: the groups struck together, each at its own rate, the attacks scattered, the durations log-uniform
section "strike" 60 20 orch (record (list
    'method 'random  'seat 1000  'coupling 1  'spread (env (list 0 0 20 0.06))
    'group-density (list (list 2 3) (list 0.5 1) (list 0.3 0.5))          # strings, winds, brass: events a second
    'duration (list 0.05 2)  'duration-law 'log  'register (band 2 6)  'dynamics 0.8))

# 5. the dynamics as a constraint: a set of markings, strict, and the brass held back
section "dynamics" 80 20 orch (record (list
    'method 'random  'seat 1000  'coupling 0  'dynamics (env (list 0 (list 'mf) 20 (list 'mf 'ff)))  'dynamics-strict 1
    'balance (record (list 'brass -6 'winds -2))
    'density (list 4 6)  'duration (list 0.5 1.5)  'register (band 3 5)))

# 6. two at once on the same people: the tutti's tail and a low band, overlapping; the second finds only the
#    players the first left free (the strings' last three and the brass' last two are in both)
section "tutti tail" 100 20 orch (record (list
    'method 'cluster  'seat 1000  'coupling 0  'density (list 3 3)  'duration (list 4 8)  'register (band 3 6)  'dynamics (env (list 0 0.6 20 0.1))))
section "low band" 90 30 low (record (list
    'method 'random  'seat 1000  'coupling 0  'density (list 1 2)  'duration (list 3 6)  'register (band 1 3)  'dynamics 0.5))

# --- the checks: is the score playable by this orchestra? ---------------------------------------------------------
score-print s
print "the whole score against the orchestra:"
validation-print (score-validate s orch)
print "the same overlap without 'continue (each call starts with everyone free):"
var loose (score "no continue" sr)
connect! loose 100 (orchestrate-granular db orch 20 (record (list 'method 'cluster 'density (list 3 3) 'duration (list 4 8) 'register (band 3 6)))) (list 0)
connect! loose 90 (orchestrate-granular db low 30 (record (list 'method 'random 'density (list 1 2) 'duration (list 3 6) 'register (band 1 3)))) (list 0)
validation-print (score-validate loose orch)
render s "/tmp/musil_gran_orchestration3.wav" "stereo"
print "wrote /tmp/musil_gran_orchestration3.wav (in the hall)"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
