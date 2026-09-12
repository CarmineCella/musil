# make_hpss_demo: the file examples/data/hpss_demo.wav, made here: sustained chords (harmonic,
# horizontal lines in the spectrogram) under a drum pattern (percussive, vertical lines), from
# the house kit rendered offline at 120 bpm; then how well hpss separates it, per kernel.
# Usage: musil make_hpss_demo.mu
load "live.mu"
var sr 44100
var secs 8
var n (* secs sr)
var t (/ (range n) sr)
# chords: three detuned saws per note, Am then F then C then G, two seconds each, slow envelope
function pad-note (midi start dur) {
    var f (midi->hz midi)
    var m (floor (* dur sr))
    var tt (/ (range m) sr)
    var env (bpf 0 (list (list (floor (* 0.4 sr)) 1) (list (- m (floor (* 0.8 sr))) 1) (list (floor (* 0.4 sr)) 0)))
    var s (* env (+ (sin (* tau f tt)) (sin (* tau (* f 1.004) tt)) (sin (* tau (* f 0.996) tt)) (* 0.5 (sin (* tau (* 2 f) tt))) (* 0.3 (sin (* tau (* 3 f) tt)))))
    return (list (floor (* start sr)) (* 0.08 s))
}
var prog (list (list 0 (chord "A3" 'min)) (list 2 (chord "F3" 'maj)) (list 4 (chord "C4" 'maj)) (list 6 (chord "G3" 'maj)))
var layers (list)
each prog (function (p) (each (last p) (function (m) (push layers (pad-note m (head p) 2)))))
var harmonic-ref (take (vec (mix layers) (zeros n)) n)
# drums: kick on every beat, snare on 2 and 4, hats on eighths, from the kit's functions offline
var beat (/ 60 120)
var g (vec (ones 400) (zeros (* 0.3 sr)))
var kick-s (kick g)
var snare-s (snare g)
var hat-s (hat g)
var hits (list)
each (range 16) (function (b) {
    push hits (list (floor (* b beat sr)) kick-s)
    if (odd? b) { push hits (list (floor (* b beat sr)) snare-s) }
    push hits (list (floor (* b beat sr)) hat-s)
    push hits (list (floor (* (+ b 0.5) beat sr)) hat-s)
})
var drum-ref (take (vec (mix hits) (zeros n)) n)
var mixed (+ harmonic-ref (* 0.5 drum-ref))
write-wav "examples/data/hpss_demo.wav" sr (* 0.9 (normalize-peak mixed))
print "wrote examples/data/hpss_demo.wav" secs "s"
function corr (a b) (/ (dot a b) (* (norm a) (norm b)))
each (list 9 17 31) (function (k) {
    var parts (hpss mixed 2048 512 k)
    print "kernel" k ": harmonic ~ chords" (fixed (corr (head parts) harmonic-ref) 3) "~ drums" (fixed (corr (head parts) (* 0.5 drum-ref)) 3) "| percussive ~ drums" (fixed (corr (last parts) (* 0.5 drum-ref)) 3) "~ chords" (fixed (corr (last parts) harmonic-ref) 3)
})
var parts (hpss mixed 2048 512 17)
print "onsets: percussive part" (length (onsets (last parts) sr 1024 256 0.15)) "mix" (length (onsets mixed sr 1024 256 0.15)) "drums alone" (length (onsets (* 0.5 drum-ref) sr 1024 256 0.15)) "(32 hats + kicks)"
