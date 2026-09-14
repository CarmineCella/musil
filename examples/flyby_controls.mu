# flyby_controls: the helicopter in your hands. Two sliders, azimuth and elevation, move it
# around your head in real time, rendered binaurally through the measured HRTFs; a third
# sets the distance. Headphones.
# Usage: musil flyby_controls.mu   (or open it in the IDE: the controls window appears)
#
# Inside the instrument, (binaural x az el sr) is a streaming node: when the direction changes
# it crossfades from the old pair of impulse responses to the new within a block, so moving a
# slider never clicks. Azimuth: 0 in front, 90 left, -90 right, 180 behind. Elevation: 90 above.
load "live.mu"
audio-init
var sr (audio-sr)

function heli (gate az el dist) (binaural (* (/ 1 dist) (lowpass (+ (* (noise gate) (+ 0.35 (* 0.65 (max (osc sr (sig gate 22) (gen 512 (vec 1 0.6 0.4 0.3 0.2))) 0)))) (* 0.4 (+ (osc sr (sig gate 55) sine-table) (* 0.5 (osc sr (sig gate 110) sine-table)) (* 0.3 (osc sr (sig gate 165) sine-table))))) sr (/ 16000 dist) 0.7)) az el sr)
var h (synth heli)
set-params h (list (list 'az 0) (list 'el 0) (list 'dist 2))
note-on h

control 'azimuth -180 180 0
control 'elevation -40 90 0
control 'distance 1 8 2
bind-control 'azimuth h 'az
bind-control 'elevation h 'el
bind-control 'distance h 'dist
controls
print "move the sliders (headphones): azimuth around you, elevation up, distance away; close the window to stop"
print "  try: azimuth 0 then 180 (front, behind); elevation 0 then 80 (ear height, above); azimuth 90 and -90 (left, right)"

while (controls-open?) { sleep 0.1 }
note-off h
sleep 0.5
free-all
audio-quit
