# ─────────────────────────────────────────────────────────────────────────────
# music theory demo
# ─────────────────────────────────────────────────────────────────────────────

load("stdlib.mu")

# ── Equal temperament ─────────────────────────────────────────────────────────
# MIDI note 69 = A4 = 440 Hz

proc midi_to_hz (n) {
    return 440 * pow(2, (n - 69) / 12)
}

proc hz_to_midi (f) {
    return 69 + 12 * log(f / 440) / LN2
}

proc semitones_between (f1, f2) {
    return round(hz_to_midi(f2) - hz_to_midi(f1))
}

print "── Equal temperament ──"
print "A4  (midi 69)  = " fmt_fixed(midi_to_hz(69), 3) " Hz"
print "A5  (midi 81)  = " fmt_fixed(midi_to_hz(81), 3) " Hz"
print "C4  (midi 60)  = " fmt_fixed(midi_to_hz(60), 3) " Hz"
print "C#4 (midi 61)  = " fmt_fixed(midi_to_hz(61), 3) " Hz"

# ── Interval ratios ───────────────────────────────────────────────────────────

proc just_fifth ()  { return 3.0 / 2.0 }
proc just_fourth () { return 4.0 / 3.0 }
proc just_major3 () { return 5.0 / 4.0 }
proc just_minor3 () { return 6.0 / 5.0 }

proc et_ratio (semitones) { return pow(2, semitones / 12) }

print ""
print "── Just vs ET ratios ──"
print "fifth  just=" fmt_fixed(just_fifth(), 6) "  ET=" fmt_fixed(et_ratio(7), 6)
print "fourth just=" fmt_fixed(just_fourth(), 6) "  ET=" fmt_fixed(et_ratio(5), 6)
print "M3     just=" fmt_fixed(just_major3(), 6) "  ET=" fmt_fixed(et_ratio(4), 6)
print "m3     just=" fmt_fixed(just_minor3(), 6) "  ET=" fmt_fixed(et_ratio(3), 6)

# ── Scale construction ────────────────────────────────────────────────────────
# Each proc returns an array of semitone offsets from the root.
# Array literals replace the old arr()+push() chains.

proc scale_major ()      { return [0, 2, 4, 5, 7, 9, 11] }
proc scale_minor_nat ()  { return [0, 2, 3, 5, 7, 8, 10] }
proc scale_dorian ()     { return [0, 2, 3, 5, 7, 9, 10] }
proc scale_pentatonic () { return [0, 2, 4, 7, 9] }
proc scale_whole_tone () { return [0, 2, 4, 6, 8, 10] }

# Print MIDI notes for a scale starting at root.
# intervals[i] returns a NumVal directly — no num() conversion needed.
proc print_scale (name, root, intervals) {
    var n = len(intervals)
    var out = name + " (root=" + str(root) + "): "
    var i = 0
    while (i < n) {
        out = out + str(root + intervals[i])
        if (i < n - 1) { out = out + " " }
        i = i + 1
    }
    print out
}

print ""
print "── Scales from C4 (midi 60) ──"
print_scale("major",      60, scale_major())
print_scale("nat. minor", 60, scale_minor_nat())
print_scale("dorian",     60, scale_dorian())
print_scale("pentatonic", 60, scale_pentatonic())
print_scale("whole tone", 60, scale_whole_tone())

# ── Hexachord operations ──────────────────────────────────────────────────────
# Relevant to 6-Z4 / 6-Z33 work.
# h[i] returns a NumVal — arithmetic works directly, no num() wrapper.

proc hexachord_invert (h) {
    var n = len(h)
    var out = []
    var i = 0
    while (i < n) {
        push(out, mod(12 - h[i], 12))    # h[i] is a number — use directly
        i = i + 1
    }
    return sorted(out)
}

proc hexachord_transpose (h, t) {
    var n = len(h)
    var out = []
    var i = 0
    while (i < n) {
        push(out, mod(h[i] + t, 12))     # h[i] is a number — use directly
        i = i + 1
    }
    return sorted(out)
}

proc print_hex (name, h) {
    print name ": [" join(h, " ") "]"
}

print ""
print "── Hexachord operations (6-Z4 on F#=6) ──"
var h6z4 = [6, 7, 8, 9, 10, 11]    # F# G G# A A# B — array literal

print_hex("6-Z4 original  ", h6z4)
print_hex("6-Z4 inverted  ", hexachord_invert(h6z4))
print_hex("6-Z4 T5        ", hexachord_transpose(h6z4, 5))
print_hex("6-Z4 T5 invert ", hexachord_invert(hexachord_transpose(h6z4, 5)))

# ── Rhythm: subdivisions and durations ───────────────────────────────────────

proc duration_ms (bpm, subdivisions) {
    return 60000 / (bpm * subdivisions)
}

print ""
print "── Durations at 120 bpm ──"
print "quarter note  = " fmt_fixed(duration_ms(120, 1), 1) " ms"
print "eighth note   = " fmt_fixed(duration_ms(120, 2), 1) " ms"
print "triplet       = " fmt_fixed(duration_ms(120, 3), 1) " ms"
print "sixteenth     = " fmt_fixed(duration_ms(120, 4), 1) " ms"
print "32nd          = " fmt_fixed(duration_ms(120, 8), 1) " ms"

# ── Export a frequency table ──────────────────────────────────────────────────

var outfile = "/tmp/musil_freqs.txt"
write(outfile, "MIDI\tHz\tNote\n")
var names = split("C,C#,D,D#,E,F,F#,G,G#,A,A#,B", ",")
var midi = 48   # C3
while (midi <= 72) {
    var note_name = names[mod(midi, 12)]   # direct indexing — no get() needed
    var octave = floor(midi / 12) - 1
    var line = str(midi) + "\t" + fmt_fixed(midi_to_hz(midi), 2) + "\t" + note_name + str(octave) + "\n"
    append(outfile, line)
    midi = midi + 1
}
print ""
print "── Frequency table written to " outfile " ──"
print read(outfile)
