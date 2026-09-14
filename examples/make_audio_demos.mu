# make_audio_demos: every synthetic sound the examples use, made here, so that how each was
# built is on record and can be changed. Run from the examples directory:
#     musil make_audio_demos.mu            makes them all
#     musil make_audio_demos.mu small      only the small 8 kHz test sounds (to /tmp)
#     musil make_audio_demos.mu hpss       data/hpss_demo.wav   (chords under drums)
#     musil make_audio_demos.mu stems      data/stems_demo.wav  (drums, bass, chords, melody: four stems,
#                                          and each stem alone as data/stems_<name>.wav)
# The data/ files are committed; running this rewrites them.
load "live.mu"
load "plot.mu"
seed 2026
var what (if (> (length args) 0) (getidx args 0) "all")
function wanted? (name) (or (equal? what "all") (equal? what name))

# =====================================================================================
# 1. small 8 kHz sounds: a pluck, a vowel, four noise bursts, a stereo room response (to /tmp)
# =====================================================================================
if (wanted? "small") {
    var sr 8000
    # a decaying harmonic tone at 220 Hz, 1.5 s
    var t (/ (range (* sr 1.5)) sr)
    var harmonics (vec 1 0.5 0.25 0.125 0.06)
    var pluck-s (reduce (zip (vec->list harmonics) (vec->list (range 1 6)))
                        (function (acc p) (+ acc (* (head p) (sin (* tau 220 (last p) t)) (exp (* -3 (last p) t))))) (zeros (length t)))
    write-wav "/tmp/musil_pluck.wav" sr (normalize-peak pluck-s)
    # a vowel-like sound with a slow vibrato, 1.2 s (a formant-filtered pulse train)
    var t2 (/ (range (* sr 1.2)) sr)
    var f0 (+ 150 (* 6 (sin (* tau 5 t2))))
    var pulses (osc sr f0 (gen 512 (ones 12)))
    var vowel (+ (bandpass pulses sr 700 4) (* 0.6 (bandpass pulses sr 1200 5)) (* 0.3 (bandpass pulses sr 2600 6)))
    write-wav "/tmp/musil_voice.wav" sr (normalize-peak (fade-out (fade-in vowel 400) 800))
    # four noise bursts with different decays over a pad, 1.6 s (transient/steady separation)
    var bursts (zeros (* sr 1.6))
    each (list 0 0.4 0.8 1.2) (function (at) {
        var burst (* (noise 2400) (exp (* -12 (/ (range 2400) sr))))
        set bursts (add-at bursts (floor (* at sr)) (lowpass burst sr 2000 0.7))
    })
    var pad-s (* 0.3 (sin (* tau 110 (/ (range (length bursts)) sr))))
    write-wav "/tmp/musil_drums.wav" sr (normalize-peak (+ bursts pad-s))
    # a synthetic stereo room impulse response, 1.5 s of decaying noise with early reflections
    var n (* sr 1.5)
    function ir-channel (k) {
        var tail (* (noise n) (exp (* -9 (/ (range n) sr))))
        var early (mix (list (list 0 (vec 1)) (list 180 (vec 0.5)) (list 330 (vec 0.35)) (list 470 (vec 0.25))))
        return (normalize-peak (add-at (* 0.25 tail) 0 early))
    }
    write-wav "/tmp/musil_ir.wav" sr (list (ir-channel 0) (ir-channel 1))
    print "small: wrote musil_pluck.wav musil_voice.wav musil_drums.wav musil_ir.wav in /tmp/"
}

# =====================================================================================
# shared: chords as detuned partials with a slow envelope; drums from the house kit, offline
# =====================================================================================
var sr 44100
function pad-note (midi start dur) {
    var f (midi->hz midi)
    var m (floor (* dur sr))
    var tt (/ (range m) sr)
    var env (bpf 0 (list (list (floor (* 0.4 sr)) 1) (list (- m (floor (* 0.8 sr))) 1) (list (floor (* 0.4 sr)) 0)))
    var s (* env (+ (sin (* tau f tt)) (sin (* tau (* f 1.004) tt)) (sin (* tau (* f 0.996) tt)) (* 0.5 (sin (* tau (* 2 f) tt))) (* 0.3 (sin (* tau (* 3 f) tt)))))
    return (list (floor (* start sr)) (* 0.08 s))
}
var g (vec (ones 400) (zeros (* 0.3 sr)))
var kick-s (kick g)
var snare-s (snare g)
var hat-s (hat g)
function drum-track (bpm bars) {                 # kick on every beat, snare on 2 and 4, hats on eighths
    var beat (/ 60 bpm)
    var hits (list)
    each (range (* 4 bars)) (function (b) {
        push hits (list (floor (* b beat sr)) kick-s)
        if (odd? b) { push hits (list (floor (* b beat sr)) snare-s) }
        push hits (list (floor (* b beat sr)) hat-s)
        push hits (list (floor (* (+ b 0.5) beat sr)) hat-s)
    })
    return (mix hits)
}
function corr (a b) (/ (dot a b) (* (norm a) (norm b)))

# =====================================================================================
# 2. hpss_demo.wav: sustained chords (Am F C G) under a drum pattern, 8 s at 120 bpm
# =====================================================================================
if (wanted? "hpss") {
    var secs 8
    var n (* secs sr)
    var prog (list (list 0 (chord "A3" 'min)) (list 2 (chord "F3" 'maj)) (list 4 (chord "C4" 'maj)) (list 6 (chord "G3" 'maj)))
    var layers (list)
    each prog (function (p) (each (last p) (function (m) (push layers (pad-note m (head p) 2)))))
    var harmonic-ref (take (vec (mix layers) (zeros n)) n)
    var drum-ref (take (vec (drum-track 120 4) (zeros n)) n)
    var mixed (+ harmonic-ref (* 0.5 drum-ref))
    write-wav "data/hpss_demo.wav" sr (* 0.9 (normalize-peak mixed))
    print "hpss: wrote data/hpss_demo.wav" secs "s"
    each (list 17 31) (function (k) {
        var parts (hpss mixed 2048 512 k)
        print "  kernel" k ": harmonic ~ chords" (fixed (corr (head parts) harmonic-ref) 3) "| percussive ~ drums" (fixed (corr (last parts) (* 0.5 drum-ref)) 3)
    })
}

# =====================================================================================
# 3. stems_demo.wav: four stems at 100 bpm, 8 bars: drums, a bass line, chords, a melody.
#    Each stem is written alone too (stems_drums.wav ...), so a separation can be scored.
# =====================================================================================
if (wanted? "stems") {
    var bpm 100
    var bars 8
    var beat (/ 60 bpm)
    var n (floor (* bars 4 beat sr))
    var prog (list (chord "D3" 'min7) (chord "G3" 'dom7) (chord "C4" 'maj7) (chord "A3" 'min7))   # one chord per two bars
    # drums
    var drums-t (take (vec (drum-track bpm bars) (zeros n)) n)
    # bass: the root of each chord, an octave down, on beats 1 and 3 and the "and" of 2, through sub
    var bass-hits (list)
    var gb (vec (ones (floor (* 0.4 beat sr))) (zeros (floor (* 0.3 beat sr))))
    each (range bars) (function (bar) {
        var root (- (head (getidx prog (mod (floor (/ bar 2)) 4))) 12)
        each (list 0 1.5 2 3.5) (function (off) (push bass-hits (list (floor (* (+ (* bar 4) off) beat sr)) (sub gb (midi->hz root)))))
    })
    var bass-t (take (vec (mix bass-hits) (zeros n)) n)
    # chords: the pad, one chord per two bars
    var layers (list)
    each (range 4) (function (k) (each (getidx prog k) (function (m) (push layers (pad-note m (* k 2 4 beat) (* 2 4 beat))))))
    var chords-t (take (vec (mix layers) (zeros n)) n)
    # melody: a pluck, eighth notes from the chord tones two octaves up, with a few passing notes
    var mel-hits (list)
    var gm (vec (ones (floor (* 0.3 beat sr))) (zeros (floor (* 0.4 beat sr))))
    var pattern (list 0 2 1 3 2 1 0 2)                      # indices into the chord's tones
    each (range bars) (function (bar) {
        var tones (getidx prog (mod (floor (/ bar 2)) 4))
        each (range 8) (function (k) {
            var m (+ 24 (getidx tones (mod (getidx pattern k) (length tones))))
            if (== (mod (+ bar k) 5) 4) { set m (+ m 2) }        # a passing note now and then
            push mel-hits (list (floor (* (+ (* bar 4) (* k 0.5)) beat sr)) (synth-render pluck (list (list 'gate gm) (list 'freq (midi->hz m)) (list 'cutoff 2500)) (/ (length gm) sr)))
        })
    })
    var melody-t (take (vec (mix mel-hits) (zeros n)) n)
    # the mix, and every stem alone at the level it has in the mix
    var stems (list (list "drums" (* 0.9 drums-t)) (list "bass" (* 0.9 bass-t)) (list "chords" (* 0.7 chords-t)) (list "melody" (* 0.5 melody-t)))
    var mixed (reduce stems (function (acc s) (+ acc (last s))) (zeros n))
    var peak (max (abs mixed))
    write-wav "data/stems_demo.wav" sr (* (/ 0.9 peak) mixed)
    each stems (function (s) (write-wav (concat "data/stems_" (head s) ".wav") sr (* (/ 0.9 peak) (last s))))
    print "stems: wrote data/stems_demo.wav (" (fixed (/ n sr) 1) "s at" bpm "bpm) and data/stems_drums.wav, _bass, _chords, _melody"
}
