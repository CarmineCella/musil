# test_live.mu — self-checking test of the live library (live.h + live.mu), on the null device.
#
# The null backend runs the audio callback from a timer at the real rate, so the clock, the
# queue, the voices and the scheduling are exercised; only the ears are missing. Commands
# reach the audio thread at the next block, so a short sleep follows each one before looking.
# Run with: musil tests/test_live.mu

load "test.mu"
load "live.mu"
function near? (a b eps) (< (max (abs (- a b))) eps)

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
audio-open sr 256 2 null-opts
check (== (opt (audio-status) "open" 0) 1) "audio-open: twice reuses the open device"
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

# --- synths: a function of parameters, streamed ---
var table (gen 1024 (vec 1 0.5))
function beep (gate freq cutoff) (pan (* (adsr sr gate 0.01 0.1 0.6 0.3) (lowpass (osc sr freq table) sr cutoff 0.7)) -0.3)
check (contains? (streamable) "osc") "streamable: lists the ugens"
check (contains? (streamable) "conv") "streamable: conv (partitioned)"
check (not (contains? (streamable) "pvoc")) "streamable: pvoc is not"
var s (synth beep)
check (equal? (type s) "scalar") "synth: returns an id"
check (equal? (synth-params s) (list "gate" "freq" "cutoff")) "synth-params: the function's parameters, in order"
sleep 0.05
check (contains? (synths) s) "synths: lists it"
set-params s (list (list 'freq 440) (list 'cutoff 2000))
sleep 0.05
check (< (opt (audio-status) "peak" 1) 0.001) "silent while the gate is 0"
note-on s
sleep 0.1
check (> (opt (audio-status) "peak" 0) 0.2) "note-on: sound"
set-param s 'cutoff 100 0.3
sleep 0.5
check (< (opt (audio-status) "peak" 1) 0.1) "set-param with a ramp: the filter closed"
note-off s
sleep 0.6
check (< (opt (audio-status) "peak" 1) 0.01) "note-off: released"
check (contains? (error-of (function () (set-param s 'nope 1))) "no parameter") "set-param: unknown parameter"
check (contains? (error-of (function () (set-param 999 'gate 1))) "no synth") "set-param: unknown synth"
free s
sleep 0.05
check (not (contains? (synths) s)) "free"
function bad (freq) (pvoc-stretch (osc sr freq table) 2)
check (contains? (error-of (function () (synth bad))) "not streamable: pvoc") "synth: refuses a non-streamable body, naming the builtin (pvoc-stretch is inlined down to pvoc)"
check (contains? (error-of (function () (synth sin))) "Musil function") "synth: needs a Musil function"
var f (synth (function (freq index) (osc sr (+ freq (* index freq (osc sr (* 2 freq) table))) table)))
set-params f (list (list 'freq 220) (list 'index 0.5))
sleep 0.1
check (> (opt (audio-status) "peak" 0) 0.5) "fm: a modulated frequency input"
free f
var ps (play-synth beep (list (list 'freq 330) (list 'cutoff 3000)))
sleep 0.05
check (contains? (synths) ps) "play-synth"
note ps 0.1
sleep 0.5
free-all
sleep 0.05
check (equal? (synths) (list)) "free-all"
# the streamed graph equals the function called offline, to float precision
var n 22050
var gate (vec (ones 11025) (zeros 11025))
var offline (beep gate (+ (zeros n) 220) 1500)
var streamed (synth-render beep (list (list 'gate gate) (list 'freq 220) (list 'cutoff 1500)) 0.5)
check (== (length streamed) 2) "synth-render: stereo out of pan"
check (near? (head streamed) (head offline) 1e-5) "synth-render equals the offline call (left)"
check (near? (last streamed) (last offline) 1e-5) "synth-render equals the offline call (right)"
function chain (freq) (lowpass (highpass (osc sr freq table) sr 100 0.7) sr 3000 0.7)
check (near? (synth-render chain (list (list 'freq 330)) 0.2) (chain (+ (zeros 8820) 330)) 1e-5) "synth-render: a filter chain equals its offline version"
function fx (amp) (* amp (comb (noise 0) 441 0.8))
check (== (length (synth-render fx (list (list 'amp 0.5)) 0.1)) 4410) "synth-render: noise and comb, mono"
function verb (amp) (* amp (conv (noise 0) (vec 1 (zeros 300) 0.5)))
check (> (rms (synth-render verb (list (list 'amp 0.5)) 0.2)) 0.1) "synth-render: partitioned conv streams"
check (contains? (error-of (function () (synth-render beep (list (list 'nope 1)) 0.1))) "no parameter") "synth-render: unknown parameter"

# --- the scheduler: loops in beats, redefined while running ---
tempo 120
check (== (bpm) 120) "tempo, bpm"
check (>= (beat) 0) "beat: counts from tempo"
check (near? (- (beat-time 2) (beat-time 0)) 1 1e-6) "beat-time: two beats at 120 bpm are one second"
var hits (list)
var p (synth (function (gate freq) (* (adsr sr gate 0.005 0.1 0.3 0.2) (osc sr freq saw-table))))
function bass (cycle) {
    push hits cycle
    return (list (synth-ev 0 0.5 p (list (list 'freq 55))) (synth-ev 1 0.5 p (list (list 'freq 82))))
}
live-loop 'bass 2
check (equal? (loops) (list "bass")) "live-loop: registered"
sleep 2.3
check (>= (length hits) 2) "live-loop: the function runs every cycle (from sleep's idle)"
check (equal? (take hits 2) (list 0 1)) "live-loop: cycle numbers"
function bass (cycle) (list (synth-ev 0 0.25 p (list (list 'freq 220))))
var before (length hits)
sleep 1.1
check (== (length hits) before) "redefining the function: the old one no longer runs"
stop-loop 'bass
sleep 0.1
check (equal? (loops) (list)) "stop-loop"
check (contains? (error-of (function () (live-loop 'nothing 4))) "no function named") "live-loop: needs a function of that name"
check (contains? (error-of (function () (live-loop 'bass 0))) "> 0") "live-loop: beats"
lookahead 0.5
check (contains? (error-of (function () (lookahead 0))) ">= 0.01") "lookahead: bounds"
lookahead 0.25
# event lists and their transformations
var e (list (ev 0 1 print) (ev 2 1 print))
check (equal? (map (fast e 2) ev-beat) (list 0 1)) "fast"
check (equal? (map (slow e 2) ev-beat) (list 0 4)) "slow"
check (equal? (map (shift e 1) ev-beat) (list 1 3)) "shift"
check (equal? (map (rev e 4) ev-beat) (list 3 1)) "rev"
check (equal? (map (every 2 0 (function (x) (fast x 2)) e) ev-beat) (list 0 1)) "every: applies on the cycle"
check (equal? (map (every 2 1 (function (x) (fast x 2)) e) ev-beat) (list 0 2)) "every: leaves the others"
check (== (length (degrade e 0)) 0) "degrade: p = 0 drops all"
check (== (length (degrade e 1)) 2) "degrade: p = 1 keeps all"
check (equal? (map (cat (list e e) 4) ev-beat) (list 0 2 4 6)) "cat"
check (== (length (stack (list e e))) 4) "stack"
check (equal? (map (steps 0.5 (list 60 nil 62) (function (n) (function (t d) t))) ev-beat) (list 0 1)) "steps: rests are nil"
check (== (ev-dur (head (steps 0.5 (list 1) (function (n) print)))) 0.5) "steps: duration"
var pe (play-ev 1 tone sr)
check (== (ev-beat pe) 1) "play-ev"
check (equal? (type (ev-thunk pe)) "function") "an event's thunk is a function"

# --- pitch names, chords, the step notation, rhythm tools, the kit ---
check (== (hz "A4") 440) "hz: A4"
check (near? (hz "C4") 261.63 0.01) "hz: C4"
check (near? (hz "Bb3") 233.08 0.01) "hz: flats"
check (== (note->midi "F#5") 78) "note->midi: sharps"
check (== (hz->midi 440) 69) "hz->midi"
check (equal? (chord "A3" 'min7) (list 57 60 64 67)) "chord: minor seventh"
check (contains? (error-of (function () (chord "C4" 'nope))) "unknown kind") "chord: unknown kind"
check (equal? (pat "bd ~ sn ~" 4) (list (list 0 1 "bd") (list 2 1 "sn"))) "pat: steps and rests"
check (equal? (map (pat "bd [hh hh] sn" 3) head) (list 0 1 1.5 2)) "pat: a subdivision"
check (equal? (map (pat "hh*4" 2) head) (list 0 0.5 1 1.5)) "pat: a repeat"
check (equal? (pat "" 4) (list)) "pat: empty"
check (equal? (map (pat-events (pat "x ~ x" 3) (function (tok) print)) ev-beat) (list 0 2)) "pat-events"
check (equal? (euclid 3 8) (list 1 nil nil 1 nil nil 1 nil)) "euclid 3 8"
check (equal? (euclid 5 8) (list 1 nil 1 nil 1 1 nil 1)) "euclid 5 8"
check (equal? (map (swing (list (ev 0 0.5 print) (ev 0.5 0.5 print) (ev 1 0.5 print)) 0.3) ev-beat) (list 0 0.65 1)) "swing: off-beat eighths move"
check (== (length (humanize e 0.05)) 2) "humanize: same count"
check (all? (map (humanize e 0.05) (function (x) (>= (ev-beat x) 0))) identity) "humanize: never before zero"
check (== (length (sometimes 0 (function (x) (list)) e)) 2) "sometimes: p = 0 never applies"
check (== (length (sometimes 1 (function (x) (list)) e)) 0) "sometimes: p = 1 always applies"
var g (vec (ones 200) (zeros 8000))
check (> (rms (kick g)) 0.05) "kick: renders offline"
check (> (rms (snare g)) 0.1) "snare: renders offline (its tone through sig)"
check (> (rms (clap g)) 0.01) "clap: renders offline (noise as long as the gate)"
check (> (rms (hat g)) 0.01) "hat"
check (> (rms (ohat g)) 0.01) "ohat"
check (contains? (error-of (function () (acid g (+ (zeros 8200) 55) 800 4))) "streaming feature") "acid: its cutoff envelope is a streaming feature (offline says so)"
check (> (rms (synth-render acid (list (list 'gate g) (list 'freq 55) (list 'cutoff 800) (list 'res 4)) 0.18)) 0.05) "acid: renders through the graph"
check (== (length (noise g)) (length g)) "noise: as long as a signal"
# the techno kit, and user functions inlined by the compiler
each (list (list "tkick" tkick) (list "rumble" rumble) (list "that" that) (list "tohat" tohat) (list "tclap" tclap)) (function (p) (check (> (rms ((last p) g)) 0.01) (concat (head p) ": renders offline")))
check (> (rms (perc g 220)) 0.05) "perc"
check (> (rms (sub g 110)) 0.05) "sub (a constant frequency through sig)"
check (== (length (sig g 5)) (length g)) "sig: a constant as long as a signal"
check (near? (synth-render sub (list (list 'gate g) (list 'freq 110)) (/ (length g) sr)) (sub g 110) 1e-5) "sub: the graph inlines sig and equals the call"
function voice (gate freq) (* (adsr sr gate 0.01 0.1 0.5 0.1) (lowpass (osc sr (sig gate freq) saw-table) sr 1200 1))
function layered (gate freq) (+ (voice gate freq) (* 0.5 (voice gate (* 2 freq))))
check (near? (synth-render layered (list (list 'gate g) (list 'freq 110)) (/ (length g) sr)) (layered g 110) 1e-5) "user helper functions are inlined into the graph"
function bad2 (gate) (bad2 gate)
check (contains? (error-of (function () (synth-render bad2 (list) 0.1))) "too deep") "recursion in a synth function is refused"
check (> (rms (synth-render hoover (list (list 'gate g) (list 'freq 110) (list 'cutoff 700) (list 'glide 0.05)) 0.2)) 0.02) "hoover: portamento through lag"
var tk (techno-kit)
check (== (length tk) 6) "techno-kit: six synths"
check (== (length (drums tk "[bd rm] hh pc cp" 4)) 5) "drums: rm and pc tokens"
check (== (length (duck (poly tpad 2) 4 0.2)) 4) "duck: one event per beat"
check (equal? (map (accents (list (ev 0 1 print) (ev 0.5 1 print)) 0.05) ev-beat) (list 0 0.55)) "accents"
var kit (house-kit)
check (== (length kit) 5) "house-kit: five synths"
check (equal? (type (kit-get kit 'kick)) "scalar") "kit-get"
check (== (length (drums kit "bd ~ sn ~ bd bd sn ~" 4)) 5) "drums: events from a pattern"
check (== (length (melody (synth acid) "a1 ~ a1 c2" 4 (list))) 3) "melody: notes from a pattern"
var voices3 (poly stab 3)
check (== (length voices3) 3) "poly"
check (== (ev-beat (chord-ev 1 0.5 voices3 (chord "A3" 'min) (list))) 1) "chord-ev"
free-all

# --- controls ---
clear-controls
control 'cutoff 100 5000 1200
toggle 'on 1
check (== (control-value 'cutoff) 1200) "control, control-value"
check (== (control-value 'on) 1) "toggle"
check (== (length (controls-list)) 2) "controls-list"
check (equal? (head (controls-list)) (list "cutoff" 100 5000 1200 0)) "controls-list: shape"
set-control 'cutoff 9999
check (== (control-value 'cutoff) 5000) "set-control: clamped to the range"
bind-control 'cutoff p 'freq
set-control 'cutoff 440
check (contains? (error-of (function () (bind-control 'nope p 'freq))) "no control") "bind-control: unknown control"
check (contains? (error-of (function () (control 'bad 1 1 1))) "hi must be > lo") "control: range"
unbind-control 'cutoff
clear-controls
check (equal? (controls-list) (list)) "clear-controls"

# --- OSC ---
control 'level 0 1 0.5
osc-listen 47131
osc-map "/level" 'level
check (== (osc-send "127.0.0.1" 47131 "/level" 0.25) 1) "osc-send"
sleep 0.1
check (near? (control-value 'level) 0.25 1e-6) "osc-map: an incoming message sets the control"
osc-send "127.0.0.1" 47131 "/level" 3
sleep 0.1
check (== (control-value 'level) 1) "osc-map: clamped"
check (contains? (error-of (function () (osc-map "/x" 'nope))) "no control") "osc-map: unknown control"
var msg (osc-encode "/synth/freq" 440 "hello")
check (== (mod (length msg) 4) 0) "osc-encode: padded to 4 bytes"
check (equal? (osc-decode msg) (list "/synth/freq" 440 "hello")) "osc-decode: round trip through the language"
check (equal? (type (osc-decode "nonsense")) "nil") "osc-decode: not OSC is nil"
osc-stop
clear-controls
free-all

# --- the evaluation port ---
serve 47770
check (== (serving) 47770) "serve, serving"
# the client runs in the background: the port is served from this thread's idle time (sleep idles)
exec "(printf 'var from-editor 41\\n(+ from-editor 1)\\n\\004' | nc 127.0.0.1 47770 > /tmp/musil_port_reply.txt 2>/dev/null &)"
sleep 0.5
check (equal? (trim (read "/tmp/musil_port_reply.txt")) "42") "the port evaluates what it receives and replies with the value"
check (== from-editor 41) "...in the global environment"
serve-stop
check (== (serving) 0) "serve-stop"

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
