# music theory demo

# --- Equal temperament: MIDI note 69 = A4 = 440 Hz ---
load "system.mu"
function midi-to-hz (n) (* 440 (pow 2 (/ (- n 69) 12)))
function hz-to-midi (f) (+ 69 (* 12 (log2 (/ f 440))))
function semitones-between (f1 f2) (round (- (hz-to-midi f2) (hz-to-midi f1)))

print "-- Equal temperament --"
print "A4  (midi 69)  =" (fixed (midi-to-hz 69) 3) "Hz"
print "A5  (midi 81)  =" (fixed (midi-to-hz 81) 3) "Hz"
print "C4  (midi 60)  =" (fixed (midi-to-hz 60) 3) "Hz"
print "C#4 (midi 61)  =" (fixed (midi-to-hz 61) 3) "Hz"
# midi-to-hz is elementwise, so a whole scale converts at once
print "C major from C4:" (fixed (midi-to-hz (+ 60 (vec 0 2 4 5 7 9 11))) 2)

# --- Interval ratios ---
function et-ratio (semitones) (pow 2 (/ semitones 12))
print ""
print "-- Just vs ET ratios --"
print "fifth  just=" (fixed (/ 3 2) 6) " ET=" (fixed (et-ratio 7) 6)
print "fourth just=" (fixed (/ 4 3) 6) " ET=" (fixed (et-ratio 5) 6)
print "M3     just=" (fixed (/ 5 4) 6) " ET=" (fixed (et-ratio 4) 6)
print "m3     just=" (fixed (/ 6 5) 6) " ET=" (fixed (et-ratio 3) 6)

# --- Scales: vectors of semitone offsets from the root ---
var scale-major      (vec 0 2 4 5 7 9 11)
var scale-minor-nat  (vec 0 2 3 5 7 8 10)
var scale-dorian     (vec 0 2 3 5 7 9 10)
var scale-pentatonic (vec 0 2 4 7 9)
var scale-whole-tone (vec 0 2 4 6 8 10)

function print-scale (name root intervals) (print (format "{} (root={}): {}" name root (+ root intervals)))

print ""
print "-- Scales from C4 (midi 60) --"
print-scale "major"      60 scale-major
print-scale "nat. minor" 60 scale-minor-nat
print-scale "dorian"     60 scale-dorian
print-scale "pentatonic" 60 scale-pentatonic
print-scale "whole tone" 60 scale-whole-tone

# --- Hexachord operations (pitch-class sets) ---
function pc-invert (h) (sort (mod (- 12 h) 12))
function pc-transpose (h t) (sort (mod (+ h t) 12))

print ""
print "-- Hexachord operations (6-Z4 on F#=6) --"
var h6z4 (vec 6 7 8 9 10 11)    # F# G G# A A# B
print "6-Z4 original  :" h6z4
print "6-Z4 inverted  :" (pc-invert h6z4)
print "6-Z4 T5        :" (pc-transpose h6z4 5)
print "6-Z4 T5 invert :" (pc-invert (pc-transpose h6z4 5))

# --- Rhythm: subdivisions and durations ---
function duration-ms (bpm subdivisions) (/ 60000 (* bpm subdivisions))
print ""
print "-- Durations at 120 bpm --"
print "quarter, eighth, triplet, sixteenth, 32nd (ms):" (fixed (duration-ms 120 (vec 1 2 3 4 8)) 1)

# --- Export a frequency table ---
var outfile "/tmp/musil_freqs.txt"
var names (split "C,C#,D,D#,E,F,F#,G,G#,A,A#,B" ",")
var lines (list "MIDI\tHz\tNote")
each (range 48 73) (function (midi) {
    var note-name (getidx names (mod midi 12))
    var octave (- (floor (/ midi 12)) 1)
    push lines (format "{}\t{}\t{}{}" midi (fixed (midi-to-hz midi) 2) note-name octave)
})
write-lines outfile lines
print ""
print "-- Frequency table written to" outfile "--"
print (read outfile)
remove outfile

# --- And finally a sound: one second of A4 with a decaying envelope, to a WAV file ---
var sr 44100
var t (/ (range sr) sr)
var tone (* (sin (* 2 pi 440 t)) (exp (* -3 t)))
write-wav "/tmp/musil_a4.wav" sr tone
var back (read-wav "/tmp/musil_a4.wav")
print "wrote /tmp/musil_a4.wav:" (wav-duration back) "s at" (head back) "Hz"
remove "/tmp/musil_a4.wav"
