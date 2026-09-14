# spatial: sounds placed and moved in 3D, for headphones (binaural) and for speakers (ambisonics)
# Usage: musil spatial.mu [sound.wav]     (defaults to data/Vox.wav)
#
# Conventions (AmbiX): azimuth in degrees, 0 in front, positive to the left; elevation up.
# ambi-encode puts a mono sound at a direction as B-format ((order+1)^2 channels); ambi-rotate
# turns a whole field; ambi-decode feeds a ring of speakers; binaural-decode renders B-format
# for headphones through the measured KEMAR HRTFs (loaded by default: front/back and height
# are pinna cues, which only measured responses carry; hrtf-unload gives the spherical-head
# model, left/right only); pan-azimuth is plain stereo. flyby.mu is the helicopter demo.
#
# Listening: the *_turn, _left, _above, _field and flyby files are for headphones, as they are.
# The B-format file needs a decoder (in Reaper: the free IEM plug-in suite, BinauralDecoder
# on a 16-channel track); the ring-of-8 file needs eight speakers, or a DAW routing each
# channel to a speaker.
load "system.mu"
load "signals.mu"
load "plot.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/Vox.wav"))
var sr (head w)
var x (take (normalize-peak (head (getidx w 1))) (* 8 sr))
var secs (/ (length x) sr)
print (fixed secs 2) "s at" sr "Hz"

# --- 1. one turn around the head, binaural: the azimuth goes 0 -> 360 (left first) over the sound ------
var blocks 2048
var nb (ceil (/ (length x) blocks))
var azs (* 360 (/ (range nb) nb))
var turning (moving-source x 1 azs (zeros nb) blocks)          # B-format of a source that turns
var bin (binaural-decode turning sr)
write-wav "/tmp/musil_spatial_turn.wav" sr (normalize-peak-stereo bin)
print "one turn around the head          -> /tmp/musil_spatial_turn.wav   (headphones)"

# --- 2. a static source at the left, and one above: what elevation does ---------------------------
write-wav "/tmp/musil_spatial_left.wav" sr (normalize-peak-stereo (binaural x 90 0 sr))
write-wav "/tmp/musil_spatial_above.wav" sr (normalize-peak-stereo (binaural x 0 60 sr))
print "at the left / above the head       -> /tmp/musil_spatial_left.wav, _above.wav"

# --- 3. a field of three sources, rotated as a whole (the listener turning) ----------------------------
var d (read-wav "data/drums.wav")
var drums (take (vec (resample-to (head (getidx d 1)) (head d) sr) (zeros (length x))) (length x))
var field (ambi-render (list (list x 30 0) (list (* 0.6 drums) -90 0) (list (* 0.3 (sine sr 220 secs)) 180 20)) 3)   # third order
var yaws (* 360 (/ (range nb) nb))
var turned (map (vec->list (range (length field))) (function (k) (zeros (length x))))
each (range nb) (function (b) {
    var seg (map field (function (ch) (slice ch (* b blocks) blocks)))
    var rot (ambi-rotate seg (getidx yaws b))
    each (zip turned rot) (function (p) (add-at! (head p) (* b blocks) (last p)))
})
write-wav "/tmp/musil_spatial_field.wav" sr (normalize-peak-stereo (binaural-decode turned sr))
print "a field of three, the listener turning -> /tmp/musil_spatial_field.wav"

# --- 4. the same field for speakers: B-format as a file, and a ring of eight -------------------------
write-wav "/tmp/musil_spatial_bformat.wav" sr field                      # 16 channels, ACN/SN3D: any ambisonic decoder can take it
write-wav "/tmp/musil_spatial_ring8.wav" sr (ambi-decode field (speaker-ring 8))
write-wav "/tmp/musil_spatial_stereo.wav" sr (pan-azimuth x 45)
print "B-format (16 ch), a ring of 8, plain stereo pan -> /tmp/musil_spatial_bformat.wav, _ring8.wav, _stereo.wav"

# --- 5. picture: the two ears' impulse responses for a source at the left, and the turn's channel levels ----
var h (hrir 90 0 sr)
var fig (figure "spherical-head HRIR, source at the left")
add-line fig (* 1000 (/ (range (length (head h))) sr)) (head h) "left ear"
add-line fig (* 1000 (/ (range (length (last h))) sr)) (last h) "right ear (later, quieter)"
set-labels fig "time (ms)" "amplitude"
var fig2 (figure "binaural turn: rms of the two ears per block")
var el (envelope-follow (head bin) blocks)
var er (envelope-follow (last bin) blocks)
add-line fig2 (* (range (length el)) (/ blocks sr)) el "left"
add-line fig2 (* (range (length er)) (/ blocks sr)) er "right"
set-labels fig2 "time (s)" "rms"
show (subplots "spatial" (list fig fig2) 2 1)
