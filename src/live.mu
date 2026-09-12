# live.mu — real-time sound, Musil half.
#
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# Not loaded automatically: (load "live.mu"). Needs system.mu (files) and signals.mu.
# The C++ half (live.h) is the device, the command queue, the clock, the sampler voices and
# the streaming graph: audio-open, audio-close, audio-start, audio-stop, audio-status,
# audio-devices, audio-time, play-buffer, stop, stop-all, master-gain, voice-set, voices,
# synth, set-param, free, synth-params, synths, streamable, synth-render. This file is the
# comfortable surface over them.
#
# Two kinds of sound. A voice plays a buffer: (play buffer sr) returns a voice id that stop,
# voice-set and voices refer to. A synth is an ordinary Musil function of its parameters
# whose body uses streamable builtins (osc, adsr, lowpass, delay, comb, conv, pan, + * ...):
# called normally it returns a buffer; (synth f) compiles the same body into a graph the
# audio thread runs, and the parameters become hot: (set-param id 'freq 440) changes the
# running sound, (set-param id 'cutoff 200 0.5) ramps it over half a second. By convention
# a parameter named gate drives the envelope: note-on and note-off set it.
#
# Offline and streamed agree to float precision where both are defined (synth-render checks
# it), with one difference to know: control arguments of filters and envelopes (a cutoff, an
# attack time) are single numbers offline (biquad computes its coefficients once) and
# per-block controls when streaming, so a moving cutoff is a streaming feature; oscillator
# frequencies and gates are signals in both.
load "system.mu"
load "signals.mu"

# --- opening and closing ---------------------------------------------------------
# (audio-init)             open the default device at 44100 Hz, 256 samples, stereo, and start it
function audio-init () (audio-init-with 44100 256 2)
# (audio-init-with sr block channels)   the same with your numbers. With the environment variable
#                          MUSIL_NULL_AUDIO set (the test suite does), the silent null device is used.
#                          A device left open by a stopped program is reused (or reopened if the
#                          numbers differ), so running one live example after another just works.
function audio-init-with (sr block channels) {
    if (equal? (type (getenv "MUSIL_NULL_AUDIO")) "nil") { audio-open sr block channels } { audio-open sr block channels (list (list "device" "null")) }
    audio-start
}
# (audio-quit)             stop everything and close the device
function audio-quit () {
    stop-all
    audio-close
}
# (audio-sr) (audio-channels) (audio-running?)   from audio-status
function audio-sr () (opt (audio-status) "sr" 0)
function audio-channels () (opt (audio-status) "channels" 0)
function audio-running? () (opt (audio-status) "running" 0)
# (audio-report)           print the status, one line per field
function audio-report () (each (audio-status) (function (p) (print (pad-right (head p) 10 " ") (last p))))

# --- playing ---------------------------------------------------------------------
# (play buffer sr)         play a buffer (a vector, or (list left right ...)) recorded at sr; => voice id
function play (buffer sr) (play-buffer buffer sr 1 0 1 0 0)
# (play-with buffer sr opts)   the same with options: (list (list "amp" 0.5) (list "pan" -1) (list "rate" 2)
#                          (list "loop" 1) (list "at" 0.5)); at is a time in seconds on the audio clock
function play-with (buffer sr opts) (play-buffer buffer sr (opt opts "amp" 1) (opt opts "pan" 0) (opt opts "rate" 1) (opt opts "loop" 0) (opt opts "at" 0))
# (play-file path)         play a WAV file (any channels) at its own sample rate; => voice id
function play-file (path) {
    var w (read-wav path)
    return (play (getidx w 1) (head w))
}
# (play-file-with path opts)
function play-file-with (path opts) {
    var w (read-wav path)
    return (play-with (getidx w 1) (head w) opts)
}
# (play-at t buffer sr)    play at time t (seconds on the audio clock): sample-accurate however
#                          late the program is, as long as t is still ahead
function play-at (t buffer sr) (play-buffer buffer sr 1 0 1 0 t)
# (loop-buffer buffer sr)  play looping; stop it with (stop voice)
function loop-buffer (buffer sr) (play-buffer buffer sr 1 0 1 1 0)
# (set-amp voice a) (set-pan voice p) (set-rate voice r)   change a playing voice, ramped
function set-amp (voice a) (voice-set voice "amp" a)
function set-pan (voice p) (voice-set voice "pan" p)
function set-rate (voice r) (voice-set voice "rate" r)
# (wait-for voice)         return when the voice has finished (polls the engine)
function wait-for (voice) {
    while (contains? (voices) voice) { sleep 0.02 }
    return nil
}
# (wait-until t)           return when the audio clock has passed t seconds
function wait-until (t) {
    while (< (audio-time) t) { sleep 0.01 }
    return nil
}
# (sequence steps)         play (list (list time buffer sr) ...) at their times, all scheduled at once
function sequence (steps) (map steps (function (s) (play-at (head s) (getidx s 1) (last s))))

# --- synths ----------------------------------------------------------------------
# (note-on id) (note-off id)   set the synth's gate parameter to 1 or 0
function note-on (id) (set-param id 'gate 1)
function note-off (id) (set-param id 'gate 0)
# (note id dur)            gate on now, off after dur seconds (sample-accurate, scheduled)
function note (id dur) {
    set-param id 'gate 1
    set-param id 'gate 0 0 (+ (audio-time) dur)
    return nil
}
# (note-at t id dur)       the same at time t on the clock
function note-at (t id dur) {
    set-param id 'gate 1 0 t
    set-param id 'gate 0 0 (+ t dur)
    return nil
}
# (set-params id pairs)    several parameters at once: (list (list 'freq 440) (list 'cutoff 800))
function set-params (id pairs) {
    each pairs (function (p) (set-param id (head p) (last p)))
    return nil
}
# (play-synth f pairs)     compile f, set its parameters, gate it on; => id
function play-synth (f pairs) {
    var id (synth f)
    set-params id pairs
    note-on id
    return id
}
# (free-all)               free every synth
function free-all () (each (synths) free)
# (synth-render-file f pairs seconds path)   render a synth offline through its graph and write a WAV
function synth-render-file (f pairs seconds path) (write-wav path (audio-sr-or 44100) (synth-render f pairs seconds))
function audio-sr-or (default) (if (opt (audio-status) "open" 0) (audio-sr) default)
# Tables for osc, made once: sine, saw, square, triangle (bandlimited by their harmonic count)
var sine-table (gen 1024 (vec 1))
var saw-table (gen 1024 (/ 1 (range 1 40)))
var square-table (gen 1024 (* (/ 1 (range 1 40)) (mod (range 1 40) 2)))
var triangle-table (gen 1024 (* (/ 1 (* (range 1 40) (range 1 40))) (mod (range 1 40) 2)))

# --- patterns and loops ----------------------------------------------------------------
# A loop is a function of the cycle number returning events; an event is (list beat dur thunk)
# where thunk is called with the absolute time (seconds on the clock) and the duration, and
# schedules what it wants. (live-loop 'bass 4) calls the function named bass every 4 beats;
# redefine bass while it runs and the next cycle uses the new one: that is the live coding.
# (ev beat dur thunk)      an event
function ev (beat dur thunk) (list beat dur thunk)
# (synth-ev beat dur id pairs)   a note on a synth: sets the parameters and gates it for dur
function synth-ev (beat dur id pairs) (ev beat dur (function (t d) {
    each pairs (function (p) (set-param id (head p) (last p) 0 t))
    note-at t id d
}))
# (play-ev beat buffer sr)     a buffer at a beat; (play-ev-with beat buffer sr opts) with play-with's options
function play-ev (beat buffer sr) (ev beat 0 (function (t d) (play-at t buffer sr)))
function play-ev-with (beat buffer sr opts) (ev beat 0 (function (t d) (play-buffer buffer sr (opt opts "amp" 1) (opt opts "pan" 0) (opt opts "rate" 1) (opt opts "loop" 0) t)))
# (ev-beat e) (ev-dur e) (ev-thunk e)
function ev-beat (e) (head e)
function ev-dur (e) (getidx e 1)
function ev-thunk (e) (last e)
# --- transformations of event lists (they return new lists) ---
# (shift events beats)     move every event later by beats
function shift (events beats) (map events (function (e) (ev (+ (ev-beat e) beats) (ev-dur e) (ev-thunk e))))
# (fast events factor)     squeeze the events in time by factor (2 = twice as fast); (slow events factor)
function fast (events factor) (map events (function (e) (ev (/ (ev-beat e) factor) (/ (ev-dur e) factor) (ev-thunk e))))
function slow (events factor) (fast events (/ 1 factor))
# (rev events length)      play the cycle backwards (length = the loop's beats)
function rev (events length) (map events (function (e) (ev (- length (ev-beat e) (ev-dur e)) (ev-dur e) (ev-thunk e))))
# (every n cycle f events)    apply f to the events on every n-th cycle (cycle is the loop's cycle number)
function every (n cycle f events) (if (== (mod cycle n) 0) (f events) events)
# (degrade events p)       keep each event with probability p
function degrade (events p) (filter events (function (e) (< (rand) p)))
# (cat lists length)       several cycles' worth of events one after another, each of length beats
function cat (lists length) {
    var out (list)
    var offset 0
    each lists (function (l) {
        each (shift l offset) (function (e) (push out e))
        set offset (+ offset length)
    })
    return out
}
# (stack lists)            several event lists at once
function stack (lists) (flatten lists)
# (steps beats-per-step items thunk-of-item)   one event per item, evenly spaced; nil items are rests
function steps (beats-per-step items thunk-of-item) {
    var out (list)
    var k 0
    each items (function (item) {
        if (not (equal? (type item) "nil")) { push out (ev (* k beats-per-step) beats-per-step (thunk-of-item item)) }
        set k (+ k 1)
    })
    return out
}

# --- pitch names --------------------------------------------------------------------
var note-names (list "C" "C#" "D" "D#" "E" "F" "F#" "G" "G#" "A" "A#" "B")
# (midi->hz n) (hz->midi f)   MIDI note numbers and Hz (69 = A4 = 440)
function midi->hz (n) (* 440 (pow 2 (/ (- n 69) 12)))
function hz->midi (f) (+ 69 (* 12 (log2 (/ f 440))))
# (note->midi "C#4")       a note name with octave (# or b) to a MIDI number; (hz "A4") straight to Hz
function note->midi (name) {
    var s (str name)
    var letter (upper (getidx s 0))
    var acc (if (> (length s) 2) (getidx s 1) "")
    var sharp (equal? acc "#")
    var flat (equal? acc "b")
    var octave (num (slice s (if (or sharp flat) 2 1) 4))
    return (+ (find note-names letter) (if sharp 1 0) (if flat -1 0) (* 12 (+ octave 1)))
}
function hz (name) (midi->hz (note->midi name))
# (chord root kind)        the MIDI numbers of a chord: kind is 'maj 'min 'dom7 'min7 'maj7 'sus4 'dim; root a note name
function chord (root kind) {
    var r (note->midi root)
    var iv (opt (list (list "maj" (list 0 4 7)) (list "min" (list 0 3 7)) (list "dom7" (list 0 4 7 10)) (list "min7" (list 0 3 7 10))
                      (list "maj7" (list 0 4 7 11)) (list "sus4" (list 0 5 7)) (list "dim" (list 0 3 6)) (list "min9" (list 0 3 7 10 14))) (str kind) nil)
    if (equal? (type iv) "nil") { error "chord: unknown kind " kind }
    return (map iv (function (i) (+ r i)))
}

# --- a compact notation for steps -------------------------------------------------------
# (pat "bd ~ sn ~" beats)   one string per cycle: tokens separated by spaces are steps of equal
#                          length, ~ is a rest, [a b] subdivides a step, a*n repeats a token n times;
#                          => (list (list beat dur token) ...) spread over `beats`. It is a notation
#                          for steps, nothing more: (pat "x ~ x ~" 4) equals (steps 1 (list "x" nil "x" nil) ...)
function pat-tokens (s) {
    var out (list)
    var cur ""
    var depth 0
    each (split s "") (function (c) {
        if (equal? c "[") { set depth (+ depth 1) }
        if (equal? c "]") { set depth (- depth 1) }
        if (and (equal? c " ") (== depth 0)) {
            if (> (length cur) 0) { push out cur }
            set cur ""
        } {
            set cur (concat cur c)
        }
    })
    if (> (length cur) 0) { push out cur }
    return out
}
function pat-expand (tokens start span) {
    var out (list)
    if (== (length tokens) 0) { return out }
    var step (/ span (length tokens))
    var k 0
    each tokens (function (tok) {
        var t (+ start (* k step))
        if (and (starts-with? tok "[") (ends-with? tok "]")) {
            each (pat-expand (pat-tokens (slice tok 1 (- (length tok) 2))) t step) (function (e) (push out e))
        } {
            var star (find tok "*")
            if (>= star 0) {
                var n (num (slice tok (+ star 1) 8))
                var name (slice tok 0 star)
                each (range n) (function (j) (if (not (equal? name "~")) { push out (list (+ t (* j (/ step n))) (/ step n) name) }))
            } {
                if (not (equal? tok "~")) { push out (list t step tok) }
            }
        }
        set k (+ k 1)
    })
    return out
}
function pat (s beats) (pat-expand (pat-tokens s) 0 beats)
# (pat-events pattern maker)   events from a pattern: maker gets the token and returns a thunk (t d)
function pat-events (pattern maker) (map pattern (function (p) (ev (head p) (getidx p 1) (maker (last p)))))

# --- rhythm tools --------------------------------------------------------------------------
# (euclid hits steps)      the Euclidean rhythm as a list of 1 / nil, for steps: (euclid 3 8) = x ~ ~ x ~ ~ x ~
function euclid (hits steps) {
    var out (list)
    var bucket (- steps hits)
    each (range steps) (function (k) {
        set bucket (+ bucket hits)
        if (>= bucket steps) { set bucket (- bucket steps)
                               push out 1 } { push out nil }
    })
    return out
}
# (swing events amount)    push the off-beat eighths later by amount (0 none, 0.33 heavy), in beats
function swing (events amount) (map events (function (e) {
    var b (ev-beat e)
    var off (mod (* b 2) 2)
    return (ev (if (near? off 1 0.01) (+ b (* amount 0.5)) b) (ev-dur e) (ev-thunk e))
}))
function near? (a b eps) (< (abs (- a b)) eps)
# (humanize events beats)  move every event by a random amount up to +/- beats
function humanize (events beats) (map events (function (e) (ev (max 0 (+ (ev-beat e) (* beats (- (* 2 (rand)) 1)))) (ev-dur e) (ev-thunk e))))
# (sometimes p f events)   apply f to the events with probability p, per cycle
function sometimes (p f events) (if (< (rand) p) (f events) events)
# (chance p events)        keep each event with probability p (an alias of degrade with the argument order of sometimes)
function chance (p events) (degrade events p)

# --- polyphony: several instances of one function, and chords on them ----------------------
# (poly f n)               n instances of the synth function f; => (list id ...)
function poly (f n) (map (range n) (function (k) (synth f)))
# (chord-ev beat dur ids midis pairs)   a chord: each instance gets a pitch (as freq) and the pairs, and is gated for dur
function chord-ev (beat dur ids midis pairs) (ev beat dur (function (t d) {
    each (zip ids midis) (function (p) {
        set-param (head p) 'freq (midi->hz (last p)) 0 t
        each pairs (function (q) (set-param (head p) (head q) (last q) 0 t))
        note-at t (head p) d
    })
}))

# (sig gate v)             the constant v as a signal as long as gate: (osc sr (sig gate 190) table) is a fixed
#                          190 Hz both offline (a vector) and streamed (a node); plain 190 would be one sample offline
function sig (gate v) (+ v (* 0 gate))
# --- a house kit: instruments as functions (call them for a buffer, synth them to play) -------------
# (kick gate)              a sine with a pitch drop and a click, saturated
function kick (gate) (tanh (* 1.5 (adsr sr gate 0.001 0.28 0 0.05) (osc sr (+ 48 (* 140 (adsr sr gate 0 0.045 0 0.01))) sine-table)))
# (snare gate)             bright noise and a short 190 Hz tone
function snare (gate) (+ (* 0.6 (adsr sr gate 0.001 0.13 0 0.05) (highpass (noise gate) sr 1800 1)) (* 0.5 (adsr sr gate 0.001 0.07 0 0.03) (osc sr (sig gate 190) sine-table)))
# (clap gate)              band-limited noise with a spread attack
function clap (gate) (* 0.8 (+ (adsr sr gate 0.001 0.02 0 0.01) (adsr sr gate 0.012 0.15 0 0.08)) (bandpass (noise gate) sr 1400 1.2))
# (hat gate) (ohat gate)   closed and open hi-hats: highpassed noise, short and long
function hat (gate) (* 0.35 (adsr sr gate 0.001 0.035 0 0.02) (highpass (noise gate) sr 8000 0.8))
function ohat (gate) (* 0.3 (adsr sr gate 0.001 0.25 0 0.1) (highpass (noise gate) sr 7000 0.8))
# (acid gate freq cutoff res)   a sawtooth into a resonant lowpass whose cutoff follows an envelope, then drive
#                          (a moving cutoff is a streaming feature: use synth-render for a buffer of it)
function acid (gate freq cutoff res) (tanh (* 1.8 (adsr sr gate 0.004 0.18 0.35 0.1) (lowpass (osc sr freq saw-table) sr (* cutoff (+ 0.25 (adsr sr gate 0.003 0.14 0.15 0.1))) res)))
# (stab gate freq cutoff)  the chord voice: two detuned saws and a sub octave, lowpassed
function stab (gate freq cutoff) (* 0.3 (adsr sr gate 0.004 0.14 0.25 0.12) (lowpass (+ (osc sr freq saw-table) (osc sr (* freq 1.006) saw-table) (* 0.6 (osc sr (* 0.5 freq) square-table))) sr cutoff 1.5))
# (pad gate freq cutoff)   slow detuned saws for the breakdown
function pad (gate freq cutoff) (* 0.22 (adsr sr gate 0.9 0.5 0.8 1.5) (lowpass (+ (osc sr freq saw-table) (osc sr (* freq 1.004) saw-table) (osc sr (* freq 0.996) saw-table)) sr cutoff 0.9))
# (house-kit)              synths for kick snare clap hat ohat as (list (list 'kick id) ...); (kit-get kit 'name)
function house-kit () (list (list "kick" (synth kick)) (list "snare" (synth snare)) (list "clap" (synth clap)) (list "hat" (synth hat)) (list "ohat" (synth ohat)))
function kit-get (kit name) (opt kit (str name) nil)
# (drums kit pattern-string beats)   events for a kit from a pattern whose tokens are bd sn cp hh oh
function drums (kit s beats) (pat-events (pat s beats) (function (tok) {
    var id (kit-get kit (opt (list (list "bd" "kick") (list "sn" "snare") (list "cp" "clap") (list "hh" "hat") (list "oh" "ohat") (list "rm" "rumble") (list "pc" "perc")) tok tok))
    return (function (t d) (if (not (equal? (type id) "nil")) { note-at t id 0.05 }))
}))
# (melody id pattern-string beats pairs)   notes for a synth from a pattern of note names (a2 c3 ...) and rests
function melody (id s beats pairs) (pat-events (pat s beats) (function (tok) (function (t d) {
    set-param id 'freq (hz tok) 0 t
    each pairs (function (q) (set-param id (head q) (last q) 0 t))
    note-at t id (* d 0.8)
})))

# --- a techno kit: harder, darker, with tails and ducking ---------------------------------
# (tkick gate)             a hard kick: deep pitch drop, click transient, saturated
function tkick (gate) (tanh (* 2.2 (+ (* (adsr sr gate 0.0005 0.4 0 0.06) (osc sr (+ 42 (* 220 (adsr sr gate 0 0.03 0 0.005))) sine-table)) (* 0.5 (adsr sr gate 0.0005 0.006 0 0.003) (noise gate)))))
# (rumble gate)            the kick's tail: a low sine with a long decay through a comb, lowpassed
function rumble (gate) (* 0.5 (adsr sr gate 0.01 0.9 0 0.2) (lowpass (comb (osc sr (sig gate 44) sine-table) 337 0.6) sr 120 0.7))
# (that gate) (tohat gate) metallic hats: ring-modulated squares through a highpass
function that (gate) (* 0.3 (adsr sr gate 0.0005 0.03 0 0.015) (highpass (* (osc sr (sig gate 3371) square-table) (osc sr (sig gate 5713) square-table)) sr 7500 0.7))
function tohat (gate) (* 0.28 (adsr sr gate 0.0005 0.3 0 0.1) (highpass (* (osc sr (sig gate 3371) square-table) (osc sr (sig gate 5713) square-table)) sr 6500 0.7))
# (tclap gate)             a wide clap: noise through a resonant bandpass with two attacks
function tclap (gate) (* 0.9 (+ (adsr sr gate 0.001 0.015 0 0.01) (adsr sr gate 0.015 0.2 0 0.15)) (bandpass (noise gate) sr 1100 2))
# (perc gate freq)         a tuned percussion: noise burst plus a pitch-dropping tone
function perc (gate freq) (* 0.7 (+ (* (adsr sr gate 0.001 0.1 0 0.05) (osc sr (* freq (+ 1 (* 2 (adsr sr gate 0 0.02 0 0.01)))) sine-table)) (* 0.3 (adsr sr gate 0.001 0.03 0 0.01) (bandpass (noise gate) sr (* 4 freq) 3))))
# (sub gate freq)          a sub bass: a sine an octave under freq, with a hint of the fundamental
function sub (gate freq) (* 0.8 (adsr sr gate 0.005 0.1 0.8 0.08) (+ (osc sr (sig gate (* 0.5 freq)) sine-table) (* 0.2 (osc sr (sig gate freq) sine-table))))
# (hoover gate freq cutoff glide)   detuned saws with portamento (the frequency is lagged by glide seconds), a dark lowpass
function hoover (gate freq cutoff glide) (* 0.25 (adsr sr gate 0.02 0.3 0.5 0.3) (lowpass (+ (osc sr (lag freq sr glide) saw-table) (osc sr (* 1.01 (lag freq sr glide)) saw-table) (osc sr (* 0.99 (lag freq sr glide)) saw-table) (* 0.7 (osc sr (* 0.5 (lag freq sr glide)) square-table))) sr cutoff 1.2))
# (tstab gate freq cutoff) a dark stab: squares through a lowpass with a fast decay
function tstab (gate freq cutoff) (* 0.3 (adsr sr gate 0.003 0.12 0.15 0.1) (lowpass (+ (osc sr freq square-table) (osc sr (* 1.007 freq) square-table)) sr cutoff 2))
# (tpad gate freq cutoff amp)   a pad with an amp parameter, for ducking (duck)
function tpad (gate freq cutoff amp) (* 0.22 amp (adsr sr gate 0.8 0.5 0.8 1.2) (lowpass (+ (osc sr freq saw-table) (osc sr (* freq 1.003) saw-table) (osc sr (* freq 0.5) saw-table)) sr cutoff 0.8))
# (zap gate freq)          a fast downward sweep: saw with a pitch envelope, for accents
function zap (gate freq) (* 0.4 (adsr sr gate 0.001 0.12 0 0.05) (osc sr (* freq (+ 0.2 (* 6 (adsr sr gate 0 0.05 0 0.01)))) saw-table))
# (techno-kit)             synths for kick rumble hat ohat clap perc, as (list (list 'kick id) ...); tokens for drums:
#                          bd rm hh oh cp pc
function techno-kit () (list (list "kick" (synth tkick)) (list "rumble" (synth rumble)) (list "hat" (synth that)) (list "ohat" (synth tohat)) (list "clap" (synth tclap)) (list "perc" (synth perc)))
# (drums kit s beats) also understands rm (rumble) and pc (perc) tokens
# (duck ids beats depth)   ducking events on the beats: every beat the amp parameter of each id drops to depth
#                          and comes back in a quarter beat (the pad's "pumping" under the kick)
function duck (ids beats depth) (map (vec->list (range beats)) (function (b) (ev b 0.25 (function (t d) {
    each ids (function (id) {
        set-param id 'amp depth 0.005 t
        set-param id 'amp 1 (* d 1.2) (+ t 0.01)
    })
}))))
# (sweep id name from to beats)   ramp a parameter from one value to another over beats (returns at once)
function sweep (id name from to beats) {
    set-param id name from
    set-param id name to (* beats (/ 60 (bpm)))
    return nil
}
# (accents events amount)  push every event a little later except the ones on the beat (a lazy feel)
function accents (events amount) (map events (function (e) (ev (if (near? (mod (ev-beat e) 1) 0 0.01) (ev-beat e) (+ (ev-beat e) amount)) (ev-dur e) (ev-thunk e))))
