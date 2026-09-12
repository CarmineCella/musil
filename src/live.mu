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
