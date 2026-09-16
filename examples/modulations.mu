# modulations: a simulation of the processes of Gérard Grisey's "Modulations" (1976-77, for 33
# musicians) with the orchestral granulator, after the analyses of Baillet and Leibowitz. The
# piece is a slow drift whose only landmarks are the harmonic spectrum of E (41.2 Hz, the low E
# of the trombone) and periodic durations. Its sections have durations proportional to the
# intervals of that spectrum (A 225", B 131.5", C 167", D 110" 82" 65", E 210.6"); each is one
# process: A moves from inharmonicity to harmonicity while the range contracts, the rhythm goes
# from aperiodic to periodic, chords lengthen against rests and the dynamics fall from ffff to
# ppp; two chord-objects alternate, "A" (an F series in the bass under an inverted F series
# above) and "B" (its ring modulation); the orchestration goes from noisy, percussive and
# synchronised to quiet, harmonic and free. B, C and E repeat the inharmonic-to-harmonic motion
# with rising dynamics; D1-D3 go the other way. Here the timeline is compressed (one fifth), and
# the sound is whatever database is loaded; with FullSOL the styles are Grisey's.
# Usage: musil modulations.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 7)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL
# var db (db-load "../datasets/FullSOL2020.spectrum.db")       # FullSOL: every technique
var sr 44100
var k 0.2                                                     # the timeline, compressed

# --- the orchestra: what the database has, the strings doubled; groups that can be struck together ---------------
var here (db-instruments-available db)
function have (L) (filter L (function (i) (contains? here (str i))))
var winds (have (list 'Picc 'Fl 'Ob 'EH 'ClBb 'BCl 'Bn))
var brass (have (list 'Hn 'TpC 'Tbn 'BTb))
var strings (have (list 'Vn 'Va 'Vc 'Cb))
var spec (list)
if (> (length winds) 0) { push spec winds }                      # each family a paired group: it can strike its chord together
if (> (length brass) 0) { push spec brass }
each strings (function (i) (push spec (list i i)))               # the strings in pairs
var orch (orchestra spec)
print (orchestra-size orch) "players in" (length spec) "groups:" (orchestra-instruments orch)
var styles (sort-list (unique (reduce (map here (function (i) (db-techniques-of db i))) concat-list (list))))
var noisy (filter styles (function (t) (any? (list "trem" "flatt" "pizz" "col" "overpressure" "bartok" "harm" "sul" "aeolian" "slap" "key") (function (w) (contains? (str t) w)))))
if (== (length noisy) 0) { set noisy styles }
print "styles:" (length styles) "; noisy ones used for the inharmonic states:" (take noisy (min 8 (length noisy)))

# --- the spectra: E (41.2 Hz) as the reference; chord A (F series and its inversion), chord B (ring-modulated) ------
var e-spectrum (harmonic-series "E1" 16)                       # partials 1..16 of E1
var e-high (drop e-spectrum 4)                                 # partials 5..16, the audible chord
var f-series (harmonic-series "F1" 8)
var chord-a (unique (sort-list (concat-list (take f-series 6) (map (take f-series 6) (function (p) (- (* 2 (pitch->number "F4")) p))))))   # F series below, inverted above
function ring (chord) (unique (sort-list (filter (map (zip (take chord (- (length chord) 1)) (tail chord)) (function (p) (round (hz->midi (+ (midi->hz (head p)) (midi->hz (last p))))))) (function (m) (and (> m 28) (< m 100))))))
var chord-b (ring chord-a)
print "chord A:" (map chord-a midi->pitch)
print "chord B (ring modulated):" (map chord-b midi->pitch)

var s (score "modulations" sr)
var t 0
# a section: an inharmonic-to-harmonic (or the reverse) process on the E spectrum, with its own dynamics (levels
# 0..1, ppp to fff, interpolated) and density curves; `from` and `to` are harmonicity values, the chords alternate A and B while the harmonicity is low
function section (name secs h-from h-to dyn-from dyn-to dens-from dens-to dur-from dur-to sync-from sync-to reg-from reg-to) {
    var params (record (list
        'method    'harmonic
        'fundamental "E1"
        'harmonicity (env (list 0 h-from secs h-to))
        'chords    (env (list 0 (list chord-a chord-b) (* 0.5 secs) (list chord-a chord-b e-high) secs (list e-high)))
        'register  (env (list 0 reg-from secs reg-to))
        'density   (env (list 0 dens-from secs dens-to))
        'duration  (env (list 0 dur-from secs dur-to))
        'styles    (env (list 0 (if (< h-from h-to) noisy (list 'ord)) secs (if (< h-from h-to) (list 'ord) noisy)))
        'dynamics  (env (list 0 dyn-from secs dyn-to))
        'coupling  (env (list 0 sync-from secs sync-to))))
    var r (orchestrate-granular db orch secs params)
    connect! s t r (list 0)
    print name ":" (fixed secs 1) "s," (length (best-connection r)) "events; harmonicity" h-from "->" h-to ", coupling" sync-from "->" sync-to
    set t (+ t secs)
}
# A: inharmonic -> harmonic; ffff -> ppp; aperiodic (a wide density range) -> periodic (a fixed rate); chords lengthen
#    against the rests; struck together at first, free at the end; the range contracts from five octaves to two
section "A" (* 225 k) 0 1 1 0 (list 1 6) (list 2 2) (list 0.1 0.4) (list 2 4) 1 0 (list 1 6) (list 3 5)
# B: inharmonic -> harmonic again, quietly rising
section "B" (* 131.5 k) 0.1 0.9 0.15 0.6 (list 2 5) (list 3 3) (list 0.2 0.6) (list 1 2) 0.6 0.2 (list 2 6) (list 3 5)
# C: the long one, inharmonic -> harmonic, growing to fff
section "C" (* 167 k) 0 1 0.3 1 (list 1 3) (list 4 4) (list 0.3 1) (list 1.5 3) 0.3 0.8 (list 1 7) (list 2 6)
# D1, D2, D3: harmonic -> inharmonic, each shorter than the last, the dynamics easing
section "D1" (* 110 k) 1 0.3 0.85 0.6 (list 3 3) (list 2 6) (list 1 2) (list 0.2 0.5) 0.8 0.3 (list 2 6) (list 1 7)
section "D2" (* 82 k) 0.8 0.2 0.6 0.3 (list 3 3) (list 3 8) (list 0.8 1.5) (list 0.1 0.4) 0.5 0.2 (list 2 6) (list 1 7)
section "D3" (* 65 k) 0.6 0 0.3 0.15 (list 4 4) (list 4 10) (list 0.5 1) (list 0.05 0.2) 0.3 0 (list 2 6) (list 1 7)
# E: from an undefined harmony to the inharmonic end, ppp rising to fff, struck together again at the close
section "E" (* 210.6 k) 0.5 0 0 1 (list 2 4) (list 6 6) (list 1 3) (list 0.3 0.8) 0 1 (list 3 5) (list 1 7)

score-print s
render s "/tmp/musil_modulations.wav" "stereo"
print "wrote /tmp/musil_modulations.wav (dry); Render... in the roll writes it in the hall;" (fixed (score-duration s) 0) "s"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
