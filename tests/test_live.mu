# test_live.mu — self-checking test of the live library (live.h + live.mu), on the null device.
#
# The null backend runs the audio callback from a timer at the real rate, so the clock, the
# queue, the voices and the scheduling are exercised; only the ears are missing. Commands
# reach the audio thread at the next block, so a short sleep follows each one before looking.
# Run with: musil tests/test_live.mu

load "test.mu"
load "live.mu"

var sr 44100
var tone (* 0.5 (sine sr 440 0.2))
var null-opts (list (list "device" "null"))

# --- before opening ---
check (== (opt (audio-status) "open" 1) 0) "audio-status: closed at first"
check (contains? (error-of (function () (play tone sr))) "no audio device open") "play: needs an open device"
check (equal? (type (audio-close)) "nil") "audio-close: harmless when closed"
check (equal? (type (audio-devices)) "list") "audio-devices: a list"

# --- opening ---
audio-open sr 256 2 null-opts
check (== (opt (audio-status) "open" 0) 1) "audio-open"
check (== (audio-sr) sr) "audio-sr"
check (== (audio-channels) 2) "audio-channels"
check (== (opt (audio-status) "block" 0) 256) "audio-status: block"
check (equal? (opt (audio-status) "device" "") "null") "audio-status: the null device"
check (== (audio-running?) 0) "not running until audio-start"
check (contains? (error-of (function () (audio-open sr 256 2 null-opts))) "already open") "audio-open: twice is an error"
check (== (audio-time) 0) "audio-time: zero before start"
audio-start
check (== (audio-running?) 1) "audio-start"
sleep 0.1
check (> (audio-time) 0.05) "the clock advances while running"

# --- voices ---
var v (play tone sr)
check (equal? (type v) "scalar") "play: returns a voice id"
sleep 0.05
check (contains? (voices) v) "voices: lists the playing voice"
check (> (opt (audio-status) "peak" 0) 0.1) "audio-status: peak shows the sound"
check (== (opt (audio-status) "voices" 0) 1) "audio-status: one voice"
wait-for v
check (not (contains? (voices) v)) "a voice ends when its buffer ends"
var lp (loop-buffer tone sr)
sleep 0.5
check (contains? (voices) lp) "loop-buffer: still playing after its length"
set-amp lp 0.1
set-pan lp -1
set-rate lp 2
check (contains? (error-of (function () (voice-set lp "nope" 1))) "amp, pan or rate") "voice-set: parameter names"
stop lp
sleep 0.05
check (not (contains? (voices) lp)) "stop"
var w (play-with tone sr (list (list "amp" 0.2) (list "pan" 0.5) (list "rate" 0.5)))
sleep 0.05
check (contains? (voices) w) "play-with"
stop-all
sleep 0.05
check (equal? (voices) (list)) "stop-all"
check (equal? (type (play (list tone tone) sr)) "scalar") "play: a stereo buffer"
check (contains? (error-of (function () (play (list tone (take tone 10)) sr))) "same length") "play: channel lengths"
check (contains? (error-of (function () (play (vec) sr))) "empty") "play: empty buffer"
check (contains? (error-of (function () (play tone 0))) "> 0") "play: sample rate"
stop-all

# --- scheduling on the clock ---
var t0 (audio-time)
var s (play-at (+ t0 0.3) tone sr)
sleep 0.1
check (contains? (voices) s) "play-at: the voice exists before its start"
check (< (opt (audio-status) "peak" 1) 0.01) "play-at: silent before its start"
wait-until (+ t0 0.35)
check (> (opt (audio-status) "peak" 0) 0.1) "play-at: sounding after its start"
wait-for s
var seq (sequence (list (list (+ (audio-time) 0.1) tone sr) (list (+ (audio-time) 0.2) tone sr)))
check (== (length seq) 2) "sequence: one voice per step"
stop-all

# --- files ---
write-wav "/tmp/musil_live_test.wav" 8000 (* 0.3 (sine 8000 300 0.1))
var fv (play-file "/tmp/musil_live_test.wav")
sleep 0.05
check (contains? (voices) fv) "play-file (resampled from 8 kHz by the voice)"
wait-for fv
check (equal? (type (play-file-with "/tmp/musil_live_test.wav" (list (list "amp" 0.5)))) "scalar") "play-file-with"
remove "/tmp/musil_live_test.wav"
stop-all

# --- stopping and closing ---
master-gain 0.5
audio-stop
check (== (audio-running?) 0) "audio-stop"
var frozen (audio-time)
sleep 0.05
check (== (audio-time) frozen) "the clock stops with the device"
audio-quit
check (== (opt (audio-status) "open" 1) 0) "audio-quit closes"
check (contains? (error-of (function () (audio-time))) "no audio device") "audio-time: needs a device"

report "test_live"
