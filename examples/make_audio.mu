# make_audio.mu — generates the small synthetic sounds the signal examples use.
# Run from the examples directory: musil data/make_audio.mu
# (The files are committed; this only documents how they were made.)
load "system.mu"
load "signals.mu"
seed 2026
var sr 8000

# a decaying harmonic tone at 220 Hz, 1.5 s
var t (/ (range (* sr 1.5)) sr)
var harmonics (vec 1 0.5 0.25 0.125 0.06)
var pluck (reduce (zip (vec->list harmonics) (vec->list (range 1 6)))
                  (function (acc p) (+ acc (* (head p) (sin (* tau 220 (last p) t)) (exp (* -3 (last p) t))))) (zeros (length t)))
write-wav "/tmp/musil_pluck.wav" sr (normalize-peak pluck)

# a vowel-like sound with a slow vibrato, 1.2 s (a formant-filtered pulse train)
var t2 (/ (range (* sr 1.2)) sr)
var f0 (+ 150 (* 6 (sin (* tau 5 t2))))
var pulses (osc sr f0 (gen 512 (ones 12)))
var vowel (+ (bandpass pulses sr 700 4) (* 0.6 (bandpass pulses sr 1200 5)) (* 0.3 (bandpass pulses sr 2600 6)))
write-wav "/tmp/musil_voice.wav" sr (normalize-peak (fade-out (fade-in vowel 400) 800))

# four noise bursts with different decays, 1.6 s (for transient/steady separation)
var drums (zeros (* sr 1.6))
each (list 0 0.4 0.8 1.2) (function (at) {
    var burst (* (noise 2400) (exp (* -12 (/ (range 2400) sr))))
    set drums (add-at drums (floor (* at sr)) (lowpass burst sr 2000 0.7))
})
var pad (* 0.3 (sin (* tau 110 (/ (range (length drums)) sr))))
write-wav "/tmp/musil_drums.wav" sr (normalize-peak (+ drums pad))

# a synthetic stereo room impulse response, 1.5 s of decaying noise with early reflections
var n (* sr 1.5)
function ir-channel (seed-offset) {
    var tail (* (noise n) (exp (* -9 (/ (range n) sr))))
    var early (mix (list (list 0 (vec 1)) (list 180 (vec 0.5)) (list 330 (vec 0.35)) (list 470 (vec 0.25))))
    return (normalize-peak (add-at (* 0.25 tail) 0 early))
}
write-wav "/tmp/musil_ir.wav" sr (list (ir-channel 0) (ir-channel 1))
print "wrote pluck.wav voice.wav drums.wav ir.wav in /tmp/"
