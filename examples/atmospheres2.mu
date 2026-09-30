# atmospheres2: Ligeti's "Atmosphères" simulated from a recording of it. Where atmospheres.mu transcribes the
# register diagram and the waveform by eye, this one reads the curves of the recording itself (data/
# atmospheres_curves.csv: one line a second with the loudness in dB, the lowest strong partial and the spectral
# centroid in octaves, the spread in octaves, the flux peaks a second and the coherence time in seconds, measured
# with the descriptors of signals) and turns them into the granulator's envelopes:
#   register   from the lowest partial up to the centroid plus the spread (the band the sound fills)
#   dynamics   from the loudness, -60 dB (ppp) to -20 dB (fff, the recording's loudest point)
#   polyphony  from the loudness too: a quiet passage is a few players, a climax everyone
#   density    from the flux peaks (a held cluster has none: a floor re-attacks it as the samples run out)
#   duration   from the coherence time (how long the sound keeps its spectrum): long where it holds, short where it moves
#   styles     ordinario; tremolo and flatterzunge where the flux is high; harmonics where the band is very high
# One granulator over the whole piece, the orchestra all ossia players (the band decides the instrument, and a
# player keeps the one it took for half a minute at least: 'seat). The analysis is not an orchestration of the
# recording (that is morph_orchestration.mu): the notes fill the band chromatically ('method 'cluster), as in the
# score they are clusters; only the shape is the recording's. The report and the validation at the end say how
# busy the players were and that every note has a player of its own.
# Usage: musil atmospheres2.mu [timescale] [seed] [recording.wav]   (a recording given: its curves are measured
#        and written to data/atmospheres_curves.csv first; timescale 0.25 for a two-minute preview)
load "music.mu"
var k (if (> (length args) 0) (num (getidx args 0)) 1)
seed (if (> (length args) 1) (num (getidx args 1)) 11)
#var db (db-load "data/microsol/microsol.spectrum.db")         # the bundled MicroSOL (a sketch: four instruments, C4-G4)
var db (db-load "../datasets/FullSOL2020.spectrum.db")         # FullSOL: every instrument and technique
var sr 44100
var curves-path "data/atmospheres_curves.csv"

# --- the analysis of a recording, when one is given: the curves, one line a second -------------------------------
function analyse-recording (path) {
    var w (read-wav path)
    var fsr (head w)
    var x (if (== (length (getidx w 1)) 1) (head (getidx w 1)) (* 0.5 (+ (head (getidx w 1)) (getidx (getidx w 1) 1))))
    var secs (/ (length x) fsr)
    var hop (round (/ fsr 4))
    var spectra (frame-spectra x 4096 hop 2048)
    var reg (register-curve spectra fsr 4096)
    var loud (loudness-curve x fsr hop)
    var coh (coherence-time spectra hop fsr 0.75)
    var dens (peak-density (flux-peaks x fsr 1024 1024 2 21) secs 2 (/ hop fsr))
    var n (min (length spectra) (min (length loud) (length dens)))
    var rows (list)
    var j 0
    while (< j n) {
        var m (min 4 (- n j))
        var sl (function (v) (mean (slice v j m)))
        push rows (list (round (* j (/ hop fsr))) (sl loud) (/ (- (sl (last reg)) 12) 12) (/ (- (sl (head reg)) 12) 12) (sl (getidx reg 1)) (sl dens) (sl coh))
        set j (+ j 4)
    }
    write-csv curves-path rows
    print "wrote" curves-path ":" (length rows) "seconds"
}
if (> (length args) 2) { analyse-recording (getidx args 2) }
var curves (read-csv curves-path)
var secs (+ 1 (head (last curves)))
print "curves:" (length curves) "seconds of the recording"

# --- the orchestra: ossia players, the band decides the instrument -------------------------------------------------
var here (db-instruments-available db)
function have (L) (filter L (function (i) (contains? here (str i))))
function ossia-of (L) (join (map (have L) str) "|")
function players (spec n) (filter (map (vec->list (range n)) (function (j) spec)) (function (p) (not (equal? p ""))))
var orch (orchestra (concat-list (players (ossia-of (list 'Vn 'Va 'Vc 'Cb)) 26) (concat-list (players (ossia-of (list 'Picc 'Fl 'Ob 'ClBb 'Bn 'Cbn)) 10) (players (ossia-of (list 'TpC 'Hn 'Tbn 'BTb)) 9))))
var n-players (orchestra-size orch)
print n-players "players:" (orchestra-instruments orch)
var styles (unique (reduce (map here (function (i) (db-techniques-of db i))) concat-list (list)))
function styled (L) { var got (filter L (function (t) (contains? styles (str t))))
                      return (if (== (length got) 0) (list 'ord) got) }
var ord (list 'ord)
var trem (styled (list 'trem 'flatt 'ord))
var harm (styled (list 'art-harm 'nat-harm 'ord))

# --- the curves as envelopes: a breakpoint every two seconds of the recording (scaled by the timescale) -----------
# a row: (t dB low centroid spread density coherence)
function level-of (row) (max 0 (min 1 (/ (+ (getidx row 1) 60) 40)))
function curve-env (f) {
    var kv (list)
    var j 0
    while (< j (length curves)) { push kv (* k (head (getidx curves j)))
                                  push kv (f (getidx curves j))
                                  set j (+ j 2) }
    return (env kv)
}
var reach (orchestra-octaves db orch)                           # what this orchestra can play: the band is kept inside it
print "the orchestra reaches octaves" (fixed (head reach) 1) "to" (fixed (last reach) 1)
var params (record (list
    'method    'cluster                                            # every pitch of the band once before any is repeated: the clusters
    'seat      (* k 30)                                            # an ossia player keeps the instrument it took for half a minute at least
    'inertia   0.8                                                 # and its bowing, most of the time
    'register  (curve-env (function (r) { var hi (min (last reach) (max (+ (head reach) 0.5) (+ (getidx r 3) (getidx r 4))))
                                         var lo (max (head reach) (min (- hi 0.5) (- (getidx r 2) 0.25)))
                                         return (list lo hi) }))
    'dynamics  (curve-env level-of)
    'polyphony (curve-env (function (r) (max 2 (round (* n-players (pow (level-of r) 0.7))))))
    'density   (curve-env (function (r) { var d (if (< (level-of r) 0.02) 0.05 (max (/ 0.8 k) (/ (* 2 (getidx r 5)) k)))
                                         return (list d d) }))
    'duration  (curve-env (function (r) { var c (max 0.5 (min 12 (getidx r 6)))
                                         return (list (* k 0.6 c) (* k 1.3 c)) }))
    'styles    (curve-env (function (r) (if (> (getidx r 5) 1.5) trem (if (> (getidx r 2) 5.5) harm ord))))))
var t0 (clock)
var r (orchestrate-granular db orch (* k secs) params)
var f (best-connection r)
print (length f) "notes in" (fixed (- (clock) t0) 1) "s"
var rep (get r 'report)                                          # (granulation-report r) prints every player's share too
print "granulation:" (get rep 'due) "events due," (get rep 'notes) "notes;" (get rep 'skipped-busy) "skipped with every able player busy (saturation" (fixed (* 100 (get rep 'saturation)) 0) "%);" (get rep 'skipped-unplayable) "unplayable"
var s (score "atmospheres (from the recording)" sr)
connect! s 0 r (list 0)
# the piano strings brushed at the end, ppppp: a filtered noise breathing in and out
var brush-secs (* k 18)
var brush (* 0.03 (lowpass (noise (floor (* brush-secs sr))) sr 700 0.7) (bpf 0 (list (list (floor (* 0.5 brush-secs sr)) 1) (list (floor (* 0.5 brush-secs sr)) 0))))
event s (* k (- secs 22)) brush-secs brush
score-print s
validation-print (score-validate s orch)                          # every note has a player of its own, at a recorded pitch
render s "/tmp/musil_atmospheres2.wav" "stereo"
print "wrote /tmp/musil_atmospheres2.wav (in the hall)"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
