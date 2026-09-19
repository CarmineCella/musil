# algorithmic_comp: the musical elements of the library combined with the language, atonally: a
# row and its forms, chords voiced and interpolated, a Ligeti-like micropolyphony in four lines,
# spectral pivots around the harmonic series of a low fundamental (after Grisey), textures that
# interpolate between chords or wander around them, walks, arpeggios, polyrhythms, densities,
# crescendi, and map/each over all of it. Every element makes a fragment (a list of
# (at dur payload)) that add! places in the score; the list functions transform them.
# Usage: musil algorithmic_comp.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 9)
var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
# var db (db-load "../datasets/TinySOL.spectrum.db")           # the full TinySOL, after ./fetch_tinysol.sh (or any *SOL set)
var sr 44100
var s (score "algorithmic" sr)
score-tempo! s 108                                             # times below are in beats (add-beats!) at 108
var strings (list 'Vn 'Vc)
var winds (list 'Ob 'Hn)
var all (list 'Ob 'Hn 'Vn 'Vc)

# --- 1. a row and its four forms: prime, inversion, retrograde, retrograde inversion (pitches are data) --------------
var row (list "E4" "F4" "G#4" "D4" "C#4" "A4" "A#4" "G4" "B4" "C4" "D#4" "F#4")    # the 12 tones, in the register at hand
var r1 (rhythm (list 1 0.5 0.5 1 0.75 0.25 1 0.5 0.5 1 -0.5 1.5))
add-beats! s 0 (notes db 'Ob 'mf 'ord row r1)
add-beats! s 10 (notes db 'Ob 'mf 'ord (invert row "E4") r1)
add-beats! s 20 (notes db 'Ob 'mf 'ord (retrograde row) (retrograde r1))
add-beats! s 30 (notes db 'Ob 'mf 'ord (transpose-pitches (invert (retrograde row) "E4") 6) r1)
print "1: the row, P I R RI"

# --- 2. chords from the row, voiced three ways, then interpolated as pitch sets --------------------------------------------
var tetra (list (take row 4) (take (drop row 4) 4) (drop row 8))                    # three tetrachords
var voiced (list (chord-voicing (getidx tetra 0) 'close) (chord-voicing (getidx tetra 1) 'open) (chord-voicing (getidx tetra 2) 'spread))
add-beats! s 0 (fragment-place (chords db 'Vn 'p 'ord voiced (rhythm (list 4 4 4))) 30 0)
add-beats! s 12 (fragment-place (chordinterp-sets db 'Vn 'p 'ord (getidx tetra 0) (getidx tetra 2) (tuplet 8 12)) 30 0)     # in eight steps, each voice to the nearest pitch
add-beats! s 24 (fragment-place (chordinterp-ease db 'Vn 'p 'ord (getidx tetra 2) (getidx tetra 1) (tuplet 8 12) 2.5) 30 0)  # slow then fast
print "2: tetrachords voiced close, open, spread; interpolated as sets; with an easing"

# --- 3. micropolyphony after Ligeti (Kammerkonzert): one chromatic line, four instruments, speeds close together,
#        entering one after another, pp, staccato, thinned towards the end ---------------------------------------------------
var cluster (list "F4" "F#4" "E4" "G4" "D#4" "G#4" "E4" "F#4" "F4" "D4")
var fast (beats 108 (rhythm (list 0.25 0.25 0.25 0.5 0.25 0.25 0.75 0.25)))
var micro (fragment-until (texture-staggered db all 'pp 'ord cluster fast (list 1 1.07 1.13 1.21) (list 0 0.4 0.8 1.2)) 14)   # the web kept going for 14 s
var micro2 (fragment-density (fragment-articulate micro 0.9) (list 1 1 1 1 0.9 0.7 0.5 0.3))   # thinning out at its end
add! s (beat->sec s 40) (fragment-place (fragment-gain micro2 0.5 1) -20 0)
print "3: micropolyphony:" (length micro2) "notes in four lines over" (fixed (fragment-duration micro2) 1) "s"

# --- 4. spectral pivots after Grisey: the harmonic series of a low E as the chord, each partial a centre; the lines
#        orchestrated by register, the lowest voices barely moving, the upper ones wider -------------------------------------
var spectrum (harmonic-series "E1" 12)                                               # partials 1..12 of E1, rounded to the tempered scale
var upper (drop spectrum 4)                                                          # partials 5..12: the audible chord above the fundamentals
var registers (list (list 'Vc "C2" "B3") (list 'Hn "C3" "G4") (list 'Ob "C4" "C6") (list 'Vn "G3" "C7"))
var piv (pivots db (map upper (function (p) (head (find-first registers (function (g) (and (>= p (pitch->number (getidx g 1))) (<= p (pitch->number (last g)))))))))
               'mf 'ord upper 2 (beats 108 (rhythm (list 1 1 2 1 1 2 1 1 4))))
add! s (beat->sec s 54) (fragment-gain piv 0.4 1)
event-at s (beat->sec s 54) (beat->sec s 14) (note db 'Vc "E2" 'ff 'ord) 0 0        # the fundamental held under it (shifted down from the cello's range)
print "4: spectral pivots around" (map upper midi->pitch)

# --- 5. textures that move: a texture through interpolated chords, and a texture wandering around a chord ---------------
var target (list "C#4" "G4" "A#4" "D#5")
var path (map (chordinterp-sets db 'Ob 'p 'ord (getidx tetra 0) target (tuplet 6 6)) (function (x) (map (get (last x) 'notes) (function (n) (get n 'midi)))))   # six chords, as pitch lists
add-beats! s 68 (fragment-place (texture-on-chords db winds 'p 'ord path (rhythm (list 0.25 0.25 0.5)) (list 1 1.2)) 20 0)
add-beats! s 68 (fragment-place (texture-on-pivots db strings 'pp 'ord target 3 (rhythm (list 0.5 0.25 0.25)) (list 1 1.33 1.66)) -20 0)
print "5: a texture through six chords (winds), a texture pivoting around the last (strings)"

# --- 6. walks, arpeggios, a polyrhythm, an ostinato: the language over fragments -------------------------------------------
add-beats! s 82 (walk db (list 'Ob 'Hn 'Vc) 'mf 'ord (list "A4" "D#4" "G3") 3 (rhythm (list 0.5 0.5 0.5 1 0.5 0.5 1)))
each (list 0 2 4) (function (k) (add-beats! s (+ 88 k) (arpeggio db 'Vn 'p 'ord (transpose-pitches (list "C4" "F#4" "A#4" "E5") k) 0.12 (if (odd? k) 'down 'up) 1)))
var pr (polyrhythm (rhythm (list 1 1 1)) (rhythm (list 1 1)))                        # 3 against 2, on a common cycle
add-beats! s 94 (notes db 'Ob 'mf 'ord (list "F5" "E5" "F#5") (head pr))
add-beats! s 94 (notes db 'Hn 'mf 'ord (list "C#3" "G3") (last pr))
var ost (notes db 'Vc 'pp 'ord (list "B3" "F3") (rhythm-from-pattern "x..x.x.." 0.25))
add-beats! s 94 (fragment-repeat ost 3)
add-beats! s 100 (fragment-scale (fragment-repeat ost 3) 2)                          # the same, augmented
print "6: walks, arpeggios up and down, 3 against 2, an ostinato from a pattern and its augmentation"

score-print s
render s "/tmp/musil_algorithmic.wav" "stereo"
print "wrote /tmp/musil_algorithmic.wav (in the hall, as the roll plays it)"
display s
# play it: press Play in the roll, or (play-score s 0.8) here
