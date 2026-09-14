# flyby: a helicopter in front, over your head, behind, then around you, for headphones
# Usage: musil flyby.mu
#
# The path is deliberately slow and explicit: 3 s hovering in FRONT, 3 s climbing over the
# top to BEHIND, 3 s hovering behind, then one turn around at ear height through the right
# back to the front. The sound is broadband (a rotor of pulsed noise and a drone), which is
# what front/back and height need: those cues live above 4 kHz, in the pinna's filtering,
# and come from the measured KEMAR responses (hrtf-loaded says how many directions).
#
# Outputs: /tmp/musil_flyby.wav        binaural, direct (each block through its own HRIR)
#          /tmp/musil_flyby_model.wav  the same path through the spherical-head model (no pinna)
#          /tmp/musil_flyby_bformat.wav third-order B-format (16 channels, ACN/SN3D), for a
#                                     decoder of your own: in Reaper, the IEM BinauralDecoder
#                                     (which uses a better HRTF decoding than our ten virtual
#                                     speakers) or a loudspeaker decoder
#          /tmp/musil_flyby_ambi.wav   that B-format through our binaural-decode, to compare
load "system.mu"
load "signals.mu"
load "plot.mu"

var sr 44100
var secs 15
var n (* secs sr)
var t (/ (range n) sr)
print "HRTF directions loaded:" (hrtf-loaded) "(0 would be the spherical-head model)"

# --- 1. the helicopter ------------------------------------------------------------------------
var pulse (osc sr (+ (zeros n) 22) (gen 512 (vec 1 0.6 0.4 0.3 0.2)))
var rotor (* (noise n) (+ 0.35 (* 0.65 (max pulse 0))))
var drone (* 0.4 (+ (sin (* tau 55 t)) (* 0.5 (sin (* tau 110 t))) (* 0.3 (sin (* tau 165 t)))))
var heli (normalize-peak (+ (lowpass rotor sr 8000 0.7) drone))

# --- 2. the path: a table of (time azimuth elevation distance), interpolated per block ------------
#        azimuth 0 in front, positive to the left, 180 behind; elevation 0 at ear height, 90 above
var path (list (list 0 0 0 3) (list 3 0 0 2)                 # front, hovering, coming a little closer
               (list 4.5 0 90 1.5) (list 6 180 0 2)          # over the top (through 90 up) to behind
               (list 9 180 0 2)                              # behind, hovering
               (list 10.5 -90 0 2) (list 12 0 0 2)           # around through the right to the front
               (list 13.5 90 0 2) (list 15 180 0 6))         # on through the left to behind, and away
var blocks 1024
var nb (ceil (/ n blocks))
var tb (* (range nb) (/ blocks sr))
function column (k) (vec (map path (function (p) (getidx p k))))
# directions are interpolated as unit vectors (not as angles, which would take the short way through the front)
function dir-x (az el) (* (cos (deg->rad el)) (cos (deg->rad az)))
function dir-y (az el) (* (cos (deg->rad el)) (sin (deg->rad az)))
function dir-z (az el) (sin (deg->rad el))
var px (interp1 (column 0) (vec (map path (function (p) (dir-x (getidx p 1) (getidx p 2))))) tb)
var py (interp1 (column 0) (vec (map path (function (p) (dir-y (getidx p 1) (getidx p 2))))) tb)
var pz (interp1 (column 0) (vec (map path (function (p) (dir-z (getidx p 1) (getidx p 2))))) tb)
var azs (rad->deg (atan2 py px))
var els (rad->deg (asin (/ pz (max (sqrt (+ (* px px) (* py py) (* pz pz))) 1e-9))))
var dist (interp1 (column 0) (column 3) tb)
print "path: front 0-3 s, over the top 3-6 s, behind 6-9 s, around through the right 9-12 s, the left 12-15 s"

# --- 3. binaural, direct: each block through the HRIR of its direction, blocks faded in over 128 samples ---
var fade 128
var win (vec (/ (range fade) fade) (ones (- blocks fade)))
function render-binaural () {
    var left (zeros (+ n 512))
    var right (zeros (+ n 512))
    each (range nb) (function (b) {
        var seg (slice heli (* b blocks) blocks)
        if (> (length seg) 0) {
            var d (getidx dist b)
            var dull (lowpass (* seg (take win (length seg)) (/ 1 d)) sr (/ 16000 d) 0.7)
            var ears (binaural dull (getidx azs b) (getidx els b) sr)
            add-at! left (* b blocks) (take (head ears) (min (length (head ears)) (- (length left) (* b blocks))))
            add-at! right (* b blocks) (take (last ears) (min (length (last ears)) (- (length right) (* b blocks))))
        }
    })
    return (normalize-peak-stereo (list (take left n) (take right n)))
}
write-wav "/tmp/musil_flyby.wav" sr (render-binaural)
print "wrote /tmp/musil_flyby.wav (headphones)"

# --- 4. B-format, third order: the same path encoded block by block, for any decoder ---------------------
var shaped (zeros n)
each (range nb) (function (b) {
    var seg (slice heli (* b blocks) blocks)
    if (> (length seg) 0) {
        var d (getidx dist b)
        add-at! shaped (* b blocks) (lowpass (* seg (take win (length seg)) (/ 1 d)) sr (/ 16000 d) 0.7)
    }
})
var B (moving-source shaped 3 azs els blocks)
write-wav "/tmp/musil_flyby_bformat.wav" sr B
write-wav "/tmp/musil_flyby_ambi.wav" sr (normalize-peak-stereo (binaural-decode B sr))
print "wrote /tmp/musil_flyby_bformat.wav (16 ch, third order, AmbiX) and its binaural-decode /tmp/musil_flyby_ambi.wav"

# --- 5. the spherical-head model, for comparison --------------------------------------------------------
hrtf-unload
write-wav "/tmp/musil_flyby_model.wav" sr (render-binaural)
hrtf-load "hrtf_kemar.csv"
print "wrote /tmp/musil_flyby_model.wav: the same path through the spherical-head model (no pinna: no above, no behind)"

# --- 6. the path as a picture ---------------------------------------------------------------------------
var fig (figure "the helicopter's path")
add-line fig tb azs "azimuth (deg, + is left, 180 behind)"
add-line fig tb els "elevation (deg)"
add-line fig tb (* 10 dist) "distance x 10"
set-labels fig "time (s)" ""
show fig
