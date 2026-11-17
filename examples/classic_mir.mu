; classic_mir: the classic analyses of music information retrieval on a song, Suzanne Vega's "Tom's Diner"
; (data/toms_diner.wav, mono). Every step is a feature computed over frames, a picture, and a decision read
; from it:
;   1. onset strength and tempo     the spectral flux frame by frame (onset-strength); its autocorrelation read at
;                                   the lags of 30-300 bpm, a prior on what a listener taps (tempo-estimate); the
;                                   tempogram: the same locally, bpm against time
;   2. beats                        the sequence of frames that is strong and keeps the period (beat-track, Ellis's
;                                   dynamic programming); a click on each, mixed with the song, to check by ear
;   3. chromagram and key           the spectrum folded onto the twelve pitch classes (chromagram); the song's
;                                   pitch-class profile against the Krumhansl-Kessler key profiles (key-scores)
;   4. structure                    Foote's self-similarity matrix: every beat compared with every other, on the
;                                   chroma (harmony) and on the MFCCs (timbre); a checkerboard kernel run along its
;                                   diagonal gives the novelty curve, whose peaks are the sections' boundaries
; Usage: musil classic_mir.mu
load "signals.mu"
load "plot.mu"
var w (read-wav "data/toms_diner.wav")
var sr (head w)
var x (head (getidx w 1))
var secs (/ (length x) sr)
print "Tom's Diner:" (fixed secs 1) "s at" sr "Hz"

; --- 1. onset strength, tempo, tempogram ---------------------------------------------------------------------------
var n 2048
var hop 512
var fps (/ sr hop)                                                 ; frames a second: 86 at this hop
var flux (onset-strength x n hop)                                  ; the rectified change of the magnitude spectrum, a frame every hop
var bpm (tempo-estimate flux fps)
print "tempo:" bpm "bpm (the autocorrelation of the onset strength at the lags of 30-300 bpm, a prior centred on 100)"
print "       without the prior:" (tempo-estimate-with flux fps 120 10) "bpm (the eighths or the half-bar are as periodic as the beat: the octave is a choice)"
var T (tempogram (gather flux (range 0 (length flux) 4)) (/ fps 4) 128)   ; locally, over 128 frames (6 s) at a quarter of the rate: rows 30..300 bpm
var fig-tempo (set-labels (add-image (figure "tempogram: autocorrelation of the onset strength, bpm 30-300 against time") T) "frame (quarter rate)" "bpm - 30")

; --- 2. beats, and a click track to hear them --------------------------------------------------------------------
var beat-frames (beat-track flux fps bpm 100)                      ; the frames of the beats: strong, and near the period
var beat-times (/ (+ (* beat-frames hop) (/ n 2)) sr)              ; a frame's time is the middle of its window; (beats x sr n hop) does all this
print "beats:" (length beat-times) "found, the first at" (fixed (head beat-times) 2) "s, then every" (fixed (mean (diff beat-times)) 3) "s"
var fig-flux (figure "onset strength and the beats found on it")
add-line fig-flux (/ (range (length flux)) fps) flux "flux"
add-scatter fig-flux beat-times (gather flux beat-frames) "beats"
set-labels fig-flux "time (s)" "flux"
var with-clicks (list (* 0.7 x) (+ (* 0.7 x) (* 0.5 (clicks beat-times sr secs))))    ; the song left, the song and the clicks right
write-wav "/tmp/musil_toms_beats.wav" sr with-clicks
print "wrote /tmp/musil_toms_beats.wav: the song, the clicks on the right channel"

; --- 3. chromagram and key ----------------------------------------------------------------------------------------
var C (chromagram x sr 4096 2048)                                  ; 12 x frames, rows C C# D ... B, a frame every 2048 samples
var cfps (/ sr 2048)
var profile (chroma-mean C)
var keys (key-scores profile)
print "pitch classes:" (map (zip key-names (vec->list profile)) (function (p) (concat (head p) " " (fixed (last p) 2))))
print "key:" (key-name (head keys)) "(r =" (fixed (last (head keys)) 2) "); then" (join (map (take (drop keys 1) 3) (function (k) (concat (key-name k) " " (fixed (last k) 2)))) ", ")
var fig-chroma (set-labels (add-image (figure "chromagram (C at the bottom, B at the top)") C) "frame" "pitch class")
var fig-profile (set-labels (add-bars (figure "pitch-class profile") (range 12) profile "") "pitch class (0 = C)" "mean chroma")

; --- 4. structure: self-similarity, novelty, boundaries ----------------------------------------------------------
; beat-synchronous features: one column per beat (216 of them), what the song does beat by beat
var Cb (beat-sync C beat-times cfps)
var Cc (map Cb (function (row) (- row (mean row))))                ; each pitch class centred: what a beat has beyond the song's average chord
var S-harmony (self-similarity Cc)                                 ; cosine similarity between every pair of beats, on the harmony
var M (mfcc-gram x sr 2048 1024 40 13)                             ; 13 MFCCs a frame: the timbre
var Mb (beat-sync (drop M 1) beat-times (/ sr 1024))               ; without coefficient 0 (the level)
var Mz (map Mb (function (row) (/ (- row (mean row)) (max 1e-9 (stdev row)))))   ; each coefficient standardised
var S-timbre (self-similarity Mz)
var novelty (foote-novelty S-timbre 32)                            ; a 32-beat checkerboard along the diagonal: 4 bars before against 4 bars after
var bounds (novelty-peaks novelty 1 2 31)                          ; its peaks above twice the running median, in beats
var bound-times (map (vec->list bounds) (function (k) (getidx beat-times (min k (- (length beat-times) 1)))))
print "sections (from the timbre's novelty):" (length bound-times) "boundaries at" (map bound-times (function (t) (fixed t 1))) "s"
var edges (concat-list (list 0) (concat-list bound-times (list secs)))
each (vec->list (range (- (length edges) 1))) (function (k) (print "   " (pad-left (str (fixed (getidx edges k) 1)) 6 " ") "-" (pad-left (str (fixed (getidx edges (+ k 1)) 1)) 6 " ") "s:" (fixed (/ (- (getidx edges (+ k 1)) (getidx edges k)) (* 4 (/ 60 bpm))) 1) "bars"))
var fig-harmony (set-labels (add-image (figure "self-similarity of the beats: harmony (chroma)") S-harmony) "beat" "beat")
var fig-timbre (set-labels (add-image (figure "self-similarity of the beats: timbre (MFCC)") S-timbre) "beat" "beat")
var fig-novelty (figure "novelty along the diagonal, and the boundaries")
add-line fig-novelty nil novelty "novelty"
add-scatter fig-novelty bounds (gather novelty bounds) "boundaries"
set-labels fig-novelty "beat" "novelty"

; --- the pictures -------------------------------------------------------------------------------------------------
var all (subplots "Tom's Diner: tempo, beats, chroma, structure" (list fig-flux fig-tempo fig-chroma fig-profile fig-harmony fig-timbre fig-novelty) 0 2)
save-png all "/tmp/musil_toms_mir.png" 1600 1200
show all 1400 900
show fig-harmony 700 700                                           ; the two matrices square, on their own
show fig-timbre 700 700
; to hear the beats: (play-file "/tmp/musil_toms_beats.wav")     (load "live.mu") (audio-init) first
