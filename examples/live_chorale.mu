# live_chorale: a four-voice chorale from an offline pluck, scheduled on the audio clock
# Usage: musil live_chorale.mu
#
# Every note is a buffer made by an ordinary Musil function (pluck), and `sequence` hands
# all of them to the engine at once as (list time buffer sr): the audio thread plays each
# at its exact sample, however long the program took to prepare them. This is the pattern
# for anything rhythmic: compute ahead, schedule ahead.
load "live.mu"

audio-init
var sr (audio-sr)

# --- an instrument: a plucked string, offline (Karplus-Strong-like: noise into a comb, lowpassed)
function pluck (freq dur amp) {
    var n (floor (* dur sr))
    var excitation (* (noise (floor (/ sr freq))) (ones (floor (/ sr freq))))
    var string (comb (vec excitation (zeros (- n (length excitation)))) (floor (/ sr freq)) 0.996)
    var tone (lowpass string sr (* 4 freq) 0.7)
    return (* amp (normalize-peak tone) (fade-out (ones n) (floor (* 0.1 n))))
}
# --- pitch names: (hz "A4") from a note name with octave, # and b allowed
var names (list "C" "C#" "D" "D#" "E" "F" "F#" "G" "G#" "A" "A#" "B")
function hz (name) {
    var flat (contains? name "b")
    var letter (concat (getidx name 0) (if (contains? name "#") "#" ""))
    var octave (num (last name))
    var pc (- (find names letter) (if flat 1 0))
    return (* 440 (pow 2 (/ (- (+ pc (* 12 (+ octave 1))) 69) 12)))
}

# --- the chorale: four voices, one chord per beat (soprano alto tenor bass), plus durations in beats
var chords (list
    (list "E4" "C4" "G3" "C3")
    (list "D4" "B3" "G3" "G2")
    (list "E4" "C4" "G3" "C3")
    (list "F4" "C4" "A3" "F3")
    (list "G4" "D4" "B3" "G2")
    (list "E4" "C4" "G3" "C3")
    (list "D4" "B3" "G3" "G2")
    (list "C4" "C4" "E3" "C3"))
var beats (list 1 1 1 1 1 1 1 2)
var bpm 72
var beat (/ 60 bpm)
var start (+ (audio-time) 0.2)                # a little ahead: the notes are computed first, then all scheduled

# --- build every note as a buffer, with its time; the voices get different levels and pans
var steps (list)
var t start
each (zip chords beats) (function (p) {
    var chord (head p)
    var dur (* (last p) beat)
    each (zip chord (list 0.5 0.35 0.35 0.6)) (function (q) {
        push steps (list t (pluck (hz (head q)) (* dur 1.1) (last q)) sr)
    })
    set t (+ t dur)
})
print (length steps) "notes prepared in" (fixed (- (audio-time) start -0.2) 2) "s; playing" (fixed (- t start) 1) "s of music"
var voices-playing (sequence steps)           # everything scheduled at once, sample-accurate
# the same steps make an offline mix: what the engine plays is what the file holds
write-wav "/tmp/musil_chorale.wav" sr (normalize-peak (mix (map steps (function (s) (list (floor (* (- (head s) start) sr)) (getidx s 1))))))
wait-until (+ t 1.5)
audio-quit
print "done:" (length voices-playing) "voices were used; /tmp/musil_chorale.wav has the same music"
