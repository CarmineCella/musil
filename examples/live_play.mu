# live_play: sounds through the audio device, from buffers and files, scheduled on the clock
# Usage: musil live_play.mu [sound.wav]   (defaults to the bundled data/ file)
load "live.mu"

audio-init                                   # 44100 Hz, 256 samples, stereo, started
audio-report
var path (if (> (length args) 0) (getidx args 0) "data/drums.wav")
var w (read-wav path)
var sr (head w)
var x (head (getidx w 1))
print "loaded" path ":" (fixed (/ (length x) sr) 2) "s"

# 1. the file, then the same buffer an octave down and panned
var v (play x sr)
wait-for v
var slow (play-with x sr (list (list "rate" 0.5) (list "pan" -0.7) (list "amp" 0.8)))
wait-for slow

# 2. a synthesized sound: two seconds of a pluck, made offline, played now
var t (/ (range (* 2 sr)) sr)
var pluck (* (sin (* tau 220 t)) (exp (* -3 t)))
play pluck sr
wait-until (+ (audio-time) 2)

# 3. a rhythm scheduled on the clock: eight hits, sample-accurate, however slow the loop
var hit (take x (floor (* 0.25 sr)))
var start (+ (audio-time) 0.1)
each (range 8) (function (k) (play-with hit sr (list (list "at" (+ start (* k 0.25))) (list "pan" (- (* 0.25 (mod k 8)) 1)) (list "rate" (if (even? k) 1 1.5)))))
wait-until (+ start 2.5)

# 4. a loop you can change while it plays
var lp (loop-buffer (take x (floor (* 0.5 sr))) sr)
each (list 1 1.25 1.5 2) (function (r) {
    set-rate lp r
    sleep 0.6
})
stop lp
audio-quit
print "done"
