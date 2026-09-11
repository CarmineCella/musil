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
var steps (sequence (list (list (+ (audio-time) 0.05) tone sr) (list (+ (audio-time) 0.15) tone sr)))
print "sequence         :" (length steps) "voices scheduled at once"
stop-all

# --- 4. Files ---------------------------------------------------------------------------
print ""
print "--- files ---"
write-wav "/tmp/musil_live_ref.wav" 8000 (* 0.3 (sine 8000 300 0.1))
print "play-file        : voice" (play-file "/tmp/musil_live_ref.wav") "(any sample rate: the voice resamples)"
print "play-file-with   : voice" (play-file-with "/tmp/musil_live_ref.wav" (list (list "rate" 2)))
remove "/tmp/musil_live_ref.wav"

# --- 5. Closing --------------------------------------------------------------------------
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
