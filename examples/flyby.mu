# flyby: a helicopter passing over and around your head, for headphones
# Usage: musil flyby.mu
#
# The classic binaural demo. The sound is synthetic (a rotor: pulsed noise and a low drone,
# broadband, which is what the ears need for front/back and height cues), and its path is
# a curve of azimuth, elevation and distance: it comes from the front left, passes above,
# goes behind, circles to the right and away. Rendering: the measured KEMAR HRTFs (loaded
# by default; hrtf-loaded says so), direct binaural per block with a short crossfade,
# distance as gain and a touch of lowpass. Compare with (hrtf-unload), the spherical model:
# left and right stay, above and behind vanish.
load "system.mu"
load "signals.mu"
load "plot.mu"

var sr 44100
var secs 14
var n (* secs sr)
var t (/ (range n) sr)
print "HRTF directions loaded:" (hrtf-loaded) "(0 would be the spherical-head model)"

# --- 1. the helicopter: rotor pulses at 22 Hz on broadband noise, an engine drone, doppler-free ----
var pulse (osc sr (+ (zeros n) 22) (gen 512 (vec 1 0.6 0.4 0.3 0.2)))
var rotor (* (noise n) (+ 0.35 (* 0.65 (max pulse 0))))
var drone (* 0.4 (+ (sin (* tau 55 t)) (* 0.5 (sin (* tau 110 t))) (* 0.3 (sin (* tau 165 t)))))
var heli (normalize-peak (+ (lowpass rotor sr 6000 0.7) drone))

# --- 2. the path, one point per block: front-left, over the head, behind, right, away -------------
var blocks 1024
var nb (ceil (/ n blocks))
var u (/ (range nb) nb)                                 # 0 .. 1 along the path
var azs (- 60 (* 420 u))                                # 60 (front left) down through 0, -90 (right), -180 (behind), -300 = 60 again
var els (* 70 (sin (* pi (min 1 (* 2 u)))))             # rises to 70 above the head in the first half, back to the horizon
var dist (+ 1 (* 6 (abs (- u 0.4))))                    # closest at 40% of the way, far at both ends

# --- 3. rendering: each block binaural at its own direction, blocks overlapped with a short fade ---------
var fade 128
var left (zeros (+ n 512))
var right (zeros (+ n 512))
var win (vec (/ (range fade) fade) (ones (- blocks fade)))     # a fade-in; the previous block fades out with its own tail
each (range nb) (function (b) {
    var seg (slice heli (* b blocks) blocks)
    if (> (length seg) 0) {
        var d (getidx dist b)
        var g (/ 1 d)
        var dull (lowpass (* seg (take win (length seg)) g) sr (/ 16000 d) 0.7)
        var ears (binaural dull (princarg-deg (getidx azs b)) (getidx els b) sr)
        add-at! left (* b blocks) (take (head ears) (min (length (head ears)) (- (length left) (* b blocks))))
        add-at! right (* b blocks) (take (last ears) (min (length (last ears)) (- (length right) (* b blocks))))
    }
})
var out (normalize-peak-stereo (list (take left n) (take right n)))
write-wav "/tmp/musil_flyby.wav" sr out
print "wrote /tmp/musil_flyby.wav (" secs "s): front-left, over the head, behind, right, away"

# --- 4. the same path with the spherical-head model, to hear what measured HRTFs add ----------------------
hrtf-unload
var left2 (zeros (+ n 512))
var right2 (zeros (+ n 512))
each (range nb) (function (b) {
    var seg (slice heli (* b blocks) blocks)
    if (> (length seg) 0) {
        var d (getidx dist b)
        var ears (binaural (lowpass (* seg (take win (length seg)) (/ 1 d)) sr (/ 16000 d) 0.7) (princarg-deg (getidx azs b)) (getidx els b) sr)
        add-at! left2 (* b blocks) (take (head ears) (min (length (head ears)) (- (length left2) (* b blocks))))
        add-at! right2 (* b blocks) (take (last ears) (min (length (last ears)) (- (length right2) (* b blocks))))
    }
})
write-wav "/tmp/musil_flyby_model.wav" sr (normalize-peak-stereo (list (take left2 n) (take right2 n)))
hrtf-load "hrtf_kemar.csv"
print "wrote /tmp/musil_flyby_model.wav: the same path through the spherical-head model (no pinna: no above, no behind)"

# --- 5. the path -----------------------------------------------------------------------------------
var fig (figure "the helicopter's path")
add-line fig (* u secs) (princarg-deg azs) "azimuth (deg, + is left)"
add-line fig (* u secs) els "elevation (deg)"
add-line fig (* u secs) (* 10 dist) "distance x 10"
set-labels fig "time (s)" ""
show fig
