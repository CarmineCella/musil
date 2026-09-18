# atmospheres: a simulation of György Ligeti's "Atmosphères" (1961) with the orchestral granulator, after
# the register diagram of the piece (the bands each section occupies, C0 to C7, INTRO and letters A to T)
# and the waveform of a recording (about 8'50"), which gives the dynamics. The piece is a sequence of
# chromatic clusters whose only variables are the band, how full it is, the dynamics and the colour: that
# is exactly the granulator's parameter set. Every section is one (orchestrate-granular) with 'method
# 'random inside a register band (a wedge in the diagram is a register envelope), a density and durations
# that make the band a sustained cluster or a moving one, the dynamics read from a loudness envelope
# transcribed from the waveform, and styles for the colour (ord, tremolo, harmonics, col legno, pizzicato).
# The players are ossia (a string player is "Vn|Va|Vc|Cb": the band decides the instrument), so a low band
# goes to the basses and a high one to the violins, as it does in the score. The micropolyphonic canons
# come out as their statistical result, a band full of moving notes. At the end the double basses sink to
# the bottom and the piano strings are brushed (a filtered noise).
# Usage: musil atmospheres.mu [timescale] [seed]     (timescale 0.25: a two-minute preview; 1: the piece)
load "music.mu"
var k (if (> (length args) 0) (num (getidx args 0)) 1)          # the timeline: 1 is the piece, 0.25 four times faster
seed (if (> (length args) 1) (num (getidx args 1)) 11)
#var db (db-load "data/microsol/microsol.spectrum.db")         # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4 (a sketch only)
var db (db-load "../datasets/FullSOL2020.spectrum.db")         # FullSOL: every instrument and technique
var sr 44100

# --- the orchestra: ossia players, one per family, so that the band decides the instrument -----------------------
var here (db-instruments-available db)
function have (L) (filter L (function (i) (contains? here (str i))))
function ossia-of (L) (join (map (have L) str) "|")
var string-player (ossia-of (list 'Vn 'Va 'Vc 'Cb))
var wind-player (ossia-of (list 'Picc 'Fl 'Ob 'ClBb 'Bn 'Cbn))
var brass-player (ossia-of (list 'TpC 'Hn 'Tbn 'BTb))
var low-player (ossia-of (list 'Cb 'Vc 'Cbn 'Bn 'BTb 'Tbn))
function players (spec n) (map (vec->list (range n)) (function (j) spec))
function band (L) (orchestra (filter L (function (p) (not (equal? p "")))))
var strings (band (players string-player 26))                    # Ligeti's 56 strings, halved
var winds (band (players wind-player 10))
var brass (band (players brass-player 9))
var tutti (band (concat-list (players string-player 26) (concat-list (players wind-player 10) (players brass-player 9))))
var low (band (players low-player 8))
var high (band (concat-list (players wind-player 8) (players (ossia-of (list 'Vn 'Va)) 12)))
print (orchestra-size tutti) "players in the tutti;" (orchestra-instruments tutti)

# --- the styles: what the database has of what the score asks for (a missing one falls back to the instrument's) ----
var styles (unique (reduce (map here (function (i) (db-techniques-of db i))) concat-list (list)))
function styled (L) { var got (filter L (function (t) (contains? styles (str t))))
                      return (if (== (length got) 0) (list 'ord) got) }
var ord (list 'ord)
var trem (styled (list 'ord 'trem 'flatt))
var harm (styled (list 'art-harm 'nat-harm 'sul-tasto 'ord))
var points (styled (list 'pizz-sec 'col-legno-batt 'stacc 'pizz-bartok 'ord))

# --- the dynamics: a level 0..1 (ppp to fff) transcribed from the waveform, in the seconds of the recording ---------
var loud (env (list 0 0.05  40 0.1  60 0.15  95 0.2  108 0.55  122 0.9  135 0.75  148 0.25  160 0.15  185 0.2
                    205 0.4  225 0.35  238 0.7  250 1  262 0.7  272 0.25  300 0.15  310 0.6  322 0.55  333 0.15
                    352 0.3  365 0.75  380 0.65  392 0.35  420 0.15  440 0.1  470 0.08  500 0.05  530 0))
# the loudness envelope of a section, in the section's own (scaled) time
function level-env (start end) {
    var kv (list)
    var secs (* k (- end start))
    var t 0
    while (< t secs) { push kv t
                       push kv (env-at loud (+ start (/ t k)))
                       set t (+ t (* 5 k)) }
    push kv secs
    push kv (env-at loud end)
    return (env kv)
}

# --- the sections: a band (octaves, C4 = 4) from its start to its end (a wedge when they differ), how it is filled ---
var s (score "atmospheres" sr)
var total 0
# (section name start end orch reg-from reg-to density duration styles): times in the recording's seconds; density
# in events a second and durations in seconds, both of the recording (the timescale is applied here)
function section (name start end orch reg-from reg-to density duration sty) {
    var secs (* k (- end start))
    var params (record (list
        'method   'random
        'register (env (list 0 reg-from secs reg-to))
        'density  (list (/ (head density) k) (/ (last density) k))
        'duration (list (* k (head duration)) (* k (last duration)))
        'styles   sty
        'dynamics (level-env start end)))
    var r (orchestrate-granular db orch secs params)
    connect! s (* k start) r (list 0)
    print name ":" (fixed (* k start) 0) "-" (fixed (* k end) 0) "s," (length (best-connection r)) "notes; band" reg-from "->" reg-to
    set total (max total (* k end))
}

# INTRO: the whole orchestra, a cluster over six octaves, ppp, held
section "intro"    0   52 tutti   (list 1 7)     (list 1 7)      (list 3 5)   (list 8 14)  ord
# A: the strings alone in the middle, a pedal of the basses under them
section "A"        52  90 strings (list 2.5 4.5) (list 2.5 4.5)  (list 2 3)   (list 6 12)  ord
section "A pedal"  55  90 low     (list 1 1.3)   (list 1 1.3)    (list 0.5 1) (list 10 15) ord
# B: the first big cluster, rising to the first climax; two lines at the bottom
section "B"        90  150 tutti  (list 2.5 6.5) (list 2.5 6.5)  (list 3 6)   (list 5 10)  trem
section "B bass"   95  150 low    (list 0.5 1.2) (list 0.5 1.2)  (list 0.5 1) (list 10 15) ord
# C: two bands, high and low, the middle empty
section "C high"   150 185 high   (list 4 6.5)   (list 4 6.5)    (list 2 4)   (list 5 10)  ord
section "C low"    150 182 low    (list 1.5 3)   (list 1.5 3)    (list 1 2)   (list 6 10)  ord
# D: the wedge that opens upwards to the piccolos, filling as it widens
section "D"        185 210 tutti  (list 3 4)     (list 3 7)      (list 4 8)   (list 3 6)   trem
# E: two thin layers, high winds and middle strings
section "E high"   210 225 winds  (list 5 6)     (list 5 6)      (list 1 2)   (list 5 8)   ord
section "E mid"    210 225 strings (list 2.5 3.5) (list 2.5 3.5) (list 1 2)   (list 5 8)   harm
# F: points: pizzicato and col legno scattered over the whole range
section "F"        225 240 strings (list 1 5.5)  (list 1 5.5)    (list 10 16) (list 0.1 0.3) points
# G: the lowest register alone, loud: the basses' cluster
section "G"        240 275 low    (list 0.5 1.5) (list 0.5 1.5)  (list 1 2)   (list 6 10)  trem
# H: the strings' micropolyphony, a wide band closing to a point
section "H"        275 300 strings (list 1 6)    (list 3.5 3.5)  (list 6 10)  (list 1 3)   ord
# I to M: a thin middle band held, a burst of brass in it
section "I-M"      300 355 strings (list 3 3.5)  (list 3 3.5)    (list 1 2)   (list 6 12)  harm
section "brass"    308 332 brass  (list 2.5 4.5) (list 2.5 4.5)  (list 2 4)   (list 2 4)   trem
# N, O: the second and largest cluster, up to C7
section "N"        355 378 tutti  (list 2 5.5)   (list 2 5.5)    (list 3 5)   (list 5 10)  ord
section "O"        378 418 tutti  (list 2.5 7)   (list 2.5 7)    (list 4 8)   (list 4 8)   trem
# P: the wedge closing, diminuendo
section "P"        418 432 tutti  (list 2.5 6)   (list 4 4.5)    (list 3 5)   (list 3 6)   ord
# Q, R: fragments, low and middle, quiet
section "QR low"   432 465 low    (list 0.5 1.5) (list 0.5 1.5)  (list 0.5 1) (list 6 10)  ord
section "QR mid"   436 462 strings (list 2 3)    (list 2 3)      (list 1 2)   (list 5 8)   harm
# S, T: the last wedge, the strings in harmonics closing on a point; the basses sink to the bottom
section "ST"       465 530 strings (list 1 5.5)  (list 3 3.2)    (list 2 4)   (list 6 10)  harm
section "end bass" 500 530 low    (list 0 1)     (list 0 1)      (list 0.5 1) (list 8 12)  ord
# the piano strings brushed, ppppp: a filtered noise breathing in and out
var brush-secs (* k 18)
var brush (* 0.03 (lowpass (noise (floor (* brush-secs sr))) sr 700 0.7) (bpf 0 (list (list (floor (* 0.5 brush-secs sr)) 1) (list (floor (* 0.5 brush-secs sr)) 0))))
event s (* k 512) brush-secs brush

score-print s
print (fixed total 0) "s in all"
render s "/tmp/musil_atmospheres.wav" "stereo"
print "wrote /tmp/musil_atmospheres.wav (dry); Render... in the roll writes it in the hall"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
