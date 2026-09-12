# musil — library reference: live (live.h + live.mu)
#
# Real-time sound: an audio device, a sample-accurate clock, and voices that play buffers.
# live.h is the device, the command queue and the voices; live.mu the surface over them.
# The interpreter never runs on the audio thread: it posts commands, the callback plays.
# Run with: musil reference_live.mu
#
# This reference opens the silent "null" device so it runs anywhere (and as a test);
# replace the option list with nothing to hear it: (audio-open 44100 256 2).

load "live.mu"
print ""
print "================================================================"
print "  musil: library reference (live)"
print "================================================================"

# --- 1. The device ---------------------------------------------------------------
print ""
print "--- device ---"
print "audio-devices    :" (equal? (type (audio-devices)) "list") "(a list of the playback devices' names)"
audio-open 44100 256 2 (list (list "device" "null"))    # sr, block size, channels; the null device here
print "audio-open       : opened;" (audio-sr) "Hz, block" (opt (audio-status) "block" 0) "," (audio-channels) "channels, device" (opt (audio-status) "device" "")
audio-start
print "audio-start      : running" (audio-running?)
sleep 0.05
print "audio-time       :" (> (audio-time) 0) "(the clock advances, in seconds)"
print "audio-report     : prints every field of audio-status (open, running, sr, block, channels, device, time, voices, load, peak)"

# --- 2. Voices ----------------------------------------------------------------------
print ""
print "--- voices ---"
var sr 44100
var tone (* 0.4 (sine sr 440 0.3))
var v (play tone sr)
sleep 0.05
print "play             : voice" v "; playing:" (contains? (voices) v)
print "play-with        : amp, pan, rate, loop, at:" (play-with tone sr (list (list "amp" 0.3) (list "pan" -0.5) (list "rate" 1.5)))
print "stereo           :" (play (list tone (* 0.5 tone)) sr) "(a list of channels)"
var lp (loop-buffer tone sr)
set-amp lp 0.2
set-pan lp 1
set-rate lp 0.5
print "loop-buffer, set-amp/pan/rate: voice" lp
stop lp
stop-all
sleep 0.05
print "stop, stop-all   : voices left" (length (voices))
wait-for v
print "wait-for         : waited for voice" v "to end"

# --- 3. Time -----------------------------------------------------------------------
print ""
print "--- time: sample-accurate scheduling on the clock ---"
var t0 (audio-time)
var s (play-at (+ t0 0.2) tone sr)
print "play-at t+0.2    : voice" s "waits for its time"
wait-until (+ t0 0.25)
print "wait-until       : now" (>= (audio-time) (+ t0 0.25))
var scheduled (sequence (list (list (+ (audio-time) 0.05) tone sr) (list (+ (audio-time) 0.15) tone sr)))
print "sequence         :" (length scheduled) "voices scheduled at once"
stop-all

# --- 4. Files ---------------------------------------------------------------------------
print ""
print "--- files ---"
write-wav "/tmp/musil_live_ref.wav" 8000 (* 0.3 (sine 8000 300 0.1))
print "play-file        : voice" (play-file "/tmp/musil_live_ref.wav") "(any sample rate: the voice resamples)"
print "play-file-with   : voice" (play-file-with "/tmp/musil_live_ref.wav" (list (list "rate" 2)))
remove "/tmp/musil_live_ref.wav"

# --- 5. Synths: a function of parameters, streamed --------------------------------------
print ""
print "--- synths ---"
print "streamable       :" (streamable)
# an instrument is an ordinary function: called, it returns a buffer; given to synth, it streams
function beep (gate freq cutoff) (pan (* (adsr sr gate 0.01 0.1 0.6 0.3) (lowpass (osc sr freq sine-table) sr cutoff 0.7)) 0)
var offline (beep (vec (ones 4410) (zeros 4410)) (+ (zeros 8820) 440) 1500)
print "offline call     :" (length offline) "channels of" (length (head offline)) "samples"
var s (synth beep)
print "synth            : id" s ", parameters" (synth-params s)
set-params s (list (list 'freq 440) (list 'cutoff 1500))
note-on s
sleep 0.05
print "note-on          : sounding" (> (opt (audio-status) "peak" 0) 0.1)
set-param s 'cutoff 200 0.2
print "set-param        : cutoff ramped to 200 over 0.2 s"
note-off s
sleep 0.4
print "note-off         : released" (< (opt (audio-status) "peak" 1) 0.05)
note s 0.1
print "note             : gate on now, off in 0.1 s (scheduled)"
sleep 0.2
free s
print "free             : synths" (length (synths))
print "play-synth       : id" (play-synth beep (list (list 'freq 220) (list 'cutoff 800))) "(compile, set, gate on)"
free-all
var streamed (synth-render beep (list (list 'gate (vec (ones 4410) (zeros 4410))) (list 'freq 440) (list 'cutoff 1500)) 0.2)
print "synth-render     : the graph offline; equals the call within" (< (max (abs (- (head streamed) (head offline)))) 1e-5)
print "not streamable   :" (try (synth (function (freq) (pvoc-stretch (osc sr freq sine-table) 2))) catch e e)
print "tables           : sine-table saw-table square-table triangle-table, for osc"

# --- 6. Loops: patterns on the clock ---------------------------------------------------
print ""
print "--- loops ---"
tempo 120
print "tempo, bpm, beat :" (bpm) "bpm; beat" (>= (beat) 0) "; beat-time of beat 2 minus beat 0:" (fixed (- (beat-time 2) (beat-time 0)) 3) "s"
var p (synth beep)
function bassline (cycle) (list (synth-ev 0 0.5 p (list (list 'freq 55) (list 'cutoff 900))) (synth-ev 1 0.5 p (list (list 'freq 82) (list 'cutoff 900))))
live-loop 'bassline 2
print "live-loop        : loops" (loops) "; the function named bassline runs every 2 beats"
sleep 1.2
print "redefine         : function bassline ... again; the next cycle uses the new one"
stop-loop 'bassline
print "stop-loop        : loops" (loops)
var e (list (ev 0 1 print) (ev 2 1 print))
print "ev, ev-beat      :" (map e ev-beat) "(an event is (list beat dur thunk); the thunk gets the time and duration)"
print "fast, slow, shift:" (map (fast e 2) ev-beat) (map (slow e 2) ev-beat) (map (shift e 1) ev-beat)
print "rev, every       :" (map (rev e 4) ev-beat) (map (every 2 0 (function (x) (fast x 2)) e) ev-beat)
print "cat, stack       :" (map (cat (list e e) 4) ev-beat) (length (stack (list e e)))
print "steps            :" (map (steps 0.5 (list 60 nil 62 64) (function (n) (function (t d) t))) ev-beat) "(nil is a rest)"
print "synth-ev, play-ev: events on a synth (parameters + gate) or a buffer"
free p

# --- 7. Controls and OSC ------------------------------------------------------------------
print ""
print "--- controls and osc ---"
var q (synth beep)
control 'cutoff 100 5000 1200
toggle 'on 0
bind-control 'cutoff q 'cutoff
bind-control 'on q 'gate
print "control, toggle  :" (controls-list)
set-control 'cutoff 300
print "set-control      : cutoff now" (control-value 'cutoff) "(the bound synth followed, ramped)"
print "controls         : opens the controls window (sliders and check boxes) and returns; (controls-open?) tells if it is"
osc-listen 47131
osc-map "/cutoff" 'cutoff
osc-send "127.0.0.1" 47131 "/cutoff" 2000
sleep 0.1
print "osc-send/listen  : after a message to /cutoff, the control is" (control-value 'cutoff)
print "osc-encode/decode:" (osc-decode (osc-encode "/a" 1 "two")) "(OSC bytes for udp-send, and back)"
osc-stop
clear-controls
free q
print "serve            : (serve 7770) opens the evaluation port editors send code to; (serving) tells which"

# --- 8. Closing --------------------------------------------------------------------------
print ""
print "--- closing ---"
master-gain 0.8
audio-stop
print "audio-stop       : running" (audio-running?)
audio-quit
print "audio-quit       : open" (opt (audio-status) "open" 1)
print ""
print "================================================================"
print "  end of live reference"
print "================================================================"
