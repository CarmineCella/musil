# atmospheres: a simulation of György Ligeti's "Atmosphères" (1961) with the orchestral granulator, after
# the register diagram of the piece (the bands each section occupies, C0 to C7, INTRO and letters A to T)
# and the measured loudness of a recording of it (data/atmospheres_curves.csv), which gives the dynamics; the
# recording's register curve also placed the extreme high cluster and the fall to the basses. The piece is a sequence of
# chromatic clusters whose only variables are the band, how full it is, the dynamics and the colour: that
# is exactly the granulator's parameter set. Every section is one (orchestrate-granular) with 'method
# 'random inside a register band (a wedge in the diagram is a register envelope), a density and durations
# that make the band a sustained cluster or a moving one, the dynamics read from a loudness envelope
# transcribed from the waveform, and styles for the colour (ord, tremolo, harmonics, col legno, pizzicato).
# The orchestra is Ligeti's halved and seated as a real one (violins, violas, cellos, basses, the winds and brass
# by name, the flutes doubling piccolo), one set of players for the whole piece: the sections overlap and book
# the same people ('continue), so nobody plays two notes at once, and (score-validate) checks it at the end. The
# clusters are drawn with 'method 'cluster (every pitch of the band once before any is repeated: chromatic
# saturation) and the micropolyphonic canons with 'leap 2 (each player by steps from its last pitch), a band
# full of moving lines. At the end the double basses sink to the bottom and the piano strings are brushed (a
# filtered noise).
# Usage: musil atmospheres.mu [timescale] [seed]     (timescale 0.25: a two-minute preview; 1: the piece)
load "music.mu"
var k (if (> (length args) 0) (num (getidx args 0)) 1)          # the timeline: 1 is the piece, 0.25 four times faster
seed (if (> (length args) 1) (num (getidx args 1)) 11)
var db (db-load "data/microsol/microsol.spectrum.db")         # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4 (a sketch only)
# var db (db-load "../datasets/FullSOL2020.spectrum.db")         # FullSOL: every instrument and technique
var sr 44100

# --- the orchestra: Ligeti's, halved, seated as a real one (13 first and second violins, 5 violas, 5 cellos, 4 basses;
#     the flutes doubling piccolo and the bassoons contrabassoon are ossia players with a 'seat: a doubling, not a
#     change of instrument at every note); one set of player records for the whole piece, so that the sections,
#     which overlap, book the same people ('continue) and never one twice ------------------------------------------
var here (db-instruments-available db)
function have (L) (filter L (function (i) (contains? here (str i))))
function ossia-of (L) (join (map (have L) str) "|")
function players (spec n) (filter (map (vec->list (range n)) (function (j) spec)) (function (p) (not (equal? p ""))))
var string-spec (concat-list (players (ossia-of (list 'Vn)) 13) (concat-list (players (ossia-of (list 'Va)) 5) (concat-list (players (ossia-of (list 'Vc)) 5) (players (ossia-of (list 'Cb)) 4))))
var wind-spec (concat-list (players (ossia-of (list 'Fl 'Picc)) 2) (concat-list (players (ossia-of (list 'Ob)) 2) (concat-list (players (ossia-of (list 'ClBb)) 2) (players (ossia-of (list 'Bn 'Cbn)) 2))))
var brass-spec (concat-list (players (ossia-of (list 'Hn)) 3) (concat-list (players (ossia-of (list 'TpC)) 2) (concat-list (players (ossia-of (list 'Tbn)) 2) (players (ossia-of (list 'BTb)) 1))))
var tutti (orchestra (concat-list string-spec (concat-list wind-spec brass-spec)))
function of (L) (filter tutti (function (p) (any? (get p 'instrs) (function (i) (contains? (map L str) i)))))   # the players of these instruments (the same records)
var strings (of (list 'Vn 'Va 'Vc 'Cb))
var upper-strings (of (list 'Vn 'Va))
var winds (of (list 'Fl 'Picc 'Ob 'ClBb 'Bn 'Cbn))
var brass (of (list 'Hn 'TpC 'Tbn 'BTb))
var low (of (list 'Vc 'Cb 'Bn 'Cbn 'Tbn 'BTb))                    # the bass lines and pedals
var high (of (list 'Fl 'Picc 'Ob 'ClBb 'Vn 'Va))                 # the high clusters
var upper (filter tutti (function (p) (not (any? low (function (q) (same? p q))))))   # everyone but the low players: a cluster over a bass line
print (orchestra-size tutti) "players in the tutti;" (orchestra-instruments tutti)

# --- the styles: what the database has of what the score asks for (a missing one falls back to the instrument's) ----
var styles (unique (reduce (map here (function (i) (db-techniques-of db i))) concat-list (list)))
function styled (L) { var got (filter L (function (t) (contains? styles (str t))))
                      return (if (== (length got) 0) (list 'ord) got) }
var ord (list 'ord)
var trem (styled (list 'ord 'trem 'flatt))
var harm (styled (list 'art-harm 'nat-harm 'sul-tasto 'ord))
var points (styled (list 'pizz-sec 'col-legno-batt 'stacc 'pizz-bartok 'ord))

# --- the dynamics: from the loudness of a recording, measured (data/atmospheres_curves.csv, one line a second, made
#     by atmospheres2.mu; the curve transcribed by eye from the waveform is the fallback). The piece is soft nearly
#     throughout and loud in a few places, so the loudness is mapped with a curve that keeps the middle soft: -62 dB
#     is ppp, -20 dB (the recording's loudest) fff, and the typical -45 dB comes out around p
var curves (try (read-csv "data/atmospheres_curves.csv") catch err nil)
var loud-by-eye (env (list 0 0.05  40 0.1  60 0.15  95 0.2  108 0.55  122 0.9  135 0.75  148 0.25  160 0.15  185 0.2
                           205 0.4  225 0.35  238 0.7  250 1  262 0.7  272 0.25  300 0.15  310 0.6  322 0.55  333 0.15
                           352 0.3  365 0.75  380 0.65  392 0.35  420 0.15  440 0.1  470 0.08  500 0.05  530 0))
function db->level (d) (pow (max 0 (min 1 (/ (+ d 62) 42))) 1.6)
function level-at (t) {                                                   # the recording's level at a second, from the curves (interpolated)
    if (equal? (type curves) "nil") { return (env-at loud-by-eye t) }
    var j (max 0 (min (- (length curves) 2) (floor t)))
    var a (db->level (getidx (getidx curves j) 1))
    var b (db->level (getidx (getidx curves (+ j 1)) 1))
    return (+ a (* (- t j) (- b a)))
}
print (if (equal? (type curves) "nil") "dynamics: the curve transcribed by eye" "dynamics: the recording's loudness curve")
# the loudness envelope of a section, in the section's own (scaled) time, sampled every two seconds of the recording
function level-env (start end) {
    var kv (list)
    var secs (* k (- end start))
    var t 0
    while (< t secs) { push kv t
                       push kv (level-at (+ start (/ t k)))
                       set t (+ t (* 2 k)) }
    push kv secs
    push kv (level-at end)
    return (env kv)
}

# --- the sections: a band (octaves, C4 = 4) from its start to its end (a wedge when they differ), how it is filled ---
var s (score "atmospheres" sr)
var total 0
# (section name start end orch reg-from reg-to density duration styles how): times in the recording's seconds; density
# in events a second and durations in seconds, both of the recording (the timescale is applied here); how the
# pitches are drawn: 'cluster (every pitch of the band once before any is repeated: a chromatic cluster), 'lines
# (each player by steps from its last pitch: the micropolyphony), or 'random (points)
function section (name start end orch reg-from reg-to density duration sty how) {
    var secs (* k (- end start))
    var method (if (equal? how 'lines) 'random how)
    var reach (orchestra-octaves db orch)                       # the band kept within what the section's orchestra can play
    var clip (function (b) { var hi (min (last reach) (max (+ (head reach) 0.5) (last b)))
                             return (list (max (head reach) (min (- hi 0.5) (head b))) hi) })
    var params (record (list
        'method   method
        'seat     (* k 30)                                             # a doubling (Fl|Picc, Bn|Cbn) kept at least half a minute
        'leap     (if (equal? how 'lines) 2 nil)                       # a micropolyphony: each player by steps from its last pitch
        'inertia  0.8                                                  # a player keeps its bowing most of the time
        'continue 1                                                    # one orchestra for the piece: the sections book the same people
        'start    (* k start)
        'register (env (list 0 (clip reg-from) secs (clip reg-to)))
        'density  (list (/ (head density) k) (/ (last density) k))
        'duration (list (* k (head duration)) (* k (last duration)))
        'styles   sty
        'dynamics (level-env start end)))
    var r (orchestrate-granular db orch secs params)
    connect! s (* k start) r (list 0)
    var rep (get r 'report)
    print name ":" (fixed (* k start) 0) "-" (fixed (* k end) 0) "s," (length (best-connection r)) "notes; band" reg-from "->" reg-to ";" (get rep 'skipped-busy) "events skipped, the players busy;" (get rep 'skipped-unplayable) "unplayable"
    set total (max total (* k end))
}

# INTRO: the whole orchestra, a cluster over six octaves, ppp, held
section "intro"    0    52   tutti         (list 1 7)     (list 1 7)      (list 3 5)   (list 8 14)  ord 'cluster
# A: the strings alone in the middle, a pedal of the basses under them
section "A"        52   90   upper-strings (list 2.5 4.5) (list 2.5 4.5)  (list 2 3)   (list 6 12)  ord 'cluster
section "A pedal"  55   90   low           (list 1 1.3)   (list 1 1.3)    (list 0.5 1) (list 10 15) ord 'random
# B: the first big cluster, rising to the first climax; two lines at the bottom
section "B"        90   150  upper         (list 2.5 6.5) (list 2.5 6.5)  (list 3 6)   (list 5 10)  trem 'cluster
section "B bass"   95   150  low           (list 0.5 1.2) (list 0.5 1.2)  (list 0.5 1) (list 10 15) ord 'lines
# C: two bands, high and low, the middle empty
section "C high"   150  185  high          (list 4 6.5)   (list 4 6.5)    (list 2 4)   (list 5 10)  ord 'cluster
section "C low"    150  182  low           (list 1.5 3)   (list 1.5 3)    (list 1 2)   (list 6 10)  ord 'cluster
# D: the wedge that opens upwards to the piccolos, filling as it widens
section "D"        185  210  tutti         (list 3 4)     (list 3 7)      (list 4 8)   (list 3 6)   trem 'cluster
# E: two thin layers, high winds and middle strings
section "E high"   210  225  winds         (list 5 6)     (list 5 6)      (list 1 2)   (list 5 8)   ord 'random
section "E mid"    210  225  strings       (list 2.5 3.5) (list 2.5 3.5) (list 1 2)   (list 5 8)   harm 'random
# F: the climb into the extreme high register: violins in harmonics and the piccolos, a band that rises from
#    octave 5 to the top of the orchestra (the recording's lowest partial reaches octave 7.5 at 4'20"), loud
section "F"        225  266  high          (list 5 6.5)   (list 7 8)      (list 3 5)   (list 4 8)   harm 'cluster
# G: the fall: the lowest register alone, the loudest point of the piece, the basses' cluster
section "G"        266  292  low           (list 1.5 2.5) (list 1.5 2.5)  (list 1.5 3) (list 6 10)  trem 'cluster
# H: the strings' micropolyphony, a wide band closing to a point
section "H"        292  320  strings       (list 1 6)    (list 3.5 3.5)  (list 6 10)  (list 1 3)   ord 'lines
# I to M: points (pizzicato, col legno) scattered over the range, then a thin middle band held with a burst of brass
section "points"   320  352  strings       (list 1 5.5)  (list 1 5.5)    (list 8 14)  (list 0.1 0.3) points 'random
section "I-M"      330  355  upper-strings (list 3 3.5)  (list 3 3.5)    (list 1 2)   (list 6 12)  harm 'random
section "brass"    336  352  brass         (list 2.5 4.5) (list 2.5 4.5)  (list 2 4)   (list 2 4)   trem 'random
# N, O: the second and largest cluster, up to C7
section "N"        355  378  tutti         (list 2 5.5)   (list 2 5.5)    (list 3 5)   (list 5 10)  ord 'cluster
section "O"        378  418  tutti         (list 2.5 7)   (list 2.5 7)    (list 4 8)   (list 4 8)   trem 'cluster
# P: the wedge closing, diminuendo
section "P"        418  432  tutti         (list 2.5 6)   (list 4 4.5)    (list 3 5)   (list 3 6)   ord 'cluster
# Q, R: fragments, low and middle, quiet
section "QR low"   432  465  low           (list 0.5 1.5) (list 0.5 1.5)  (list 0.5 1) (list 6 10)  ord 'random
section "QR mid"   436  462  upper-strings (list 2 3)    (list 2 3)      (list 1 2)   (list 5 8)   harm 'lines
# S, T: the last wedge, the strings in harmonics closing on a point; the basses sink to the bottom
section "ST"       465  530  strings       (list 1 5.5)  (list 3 3.2)    (list 2 4)   (list 6 10)  harm 'lines
section "end bass" 500  530  low           (list 0 1)     (list 0 1)      (list 0.5 1) (list 8 12)  ord 'random
# the piano strings brushed, ppppp: a filtered noise breathing in and out
var brush-secs (* k 18)
var brush (* 0.03 (lowpass (noise (floor (* brush-secs sr))) sr 700 0.7) (bpf 0 (list (list (floor (* 0.5 brush-secs sr)) 1) (list (floor (* 0.5 brush-secs sr)) 0))))
event s (* k 512) brush-secs brush

score-print s
print (fixed total 0) "s in all"
validation-print (score-validate s tutti)                          # every note has a player of its own, at a recorded pitch
render s "/tmp/musil_atmospheres.wav" "stereo"
print "wrote /tmp/musil_atmospheres.wav (in the hall, as the roll plays it)"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
