# live.mu — real-time sound, Musil half.
#
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# Not loaded automatically: (load "live.mu"). Needs system.mu (files) and signals.mu.
# The C++ half (live.h) is the device, the command queue, the clock and the sampler voices:
# audio-open, audio-close, audio-start, audio-stop, audio-status, audio-devices, audio-time,
# play-buffer, stop, stop-all, master-gain, voice-set, voices. This file is the comfortable
# surface over them. Sounds are played by voices: (play buffer sr) returns a voice id that
# stop, voice-set and voices refer to.
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
