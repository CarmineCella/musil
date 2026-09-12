# live_controls: hot parameters with a range, shown as sliders, driven by keys or OSC
# Usage: musil live_controls.mu     (a controls window opens; Esc closes it)
#        in the Listener the controls are a panel: Tab to it, arrows change them
load "live.mu"
audio-init
var sr (audio-sr)
function pad (gate freq cutoff res detune) \
    (pan (* 0.4 (adsr sr gate 0.5 0.5 0.7 1) (lowpass (+ (osc sr freq saw-table) (osc sr (* freq detune) saw-table)) sr cutoff res)) 0)
var p (synth pad)
set-params p (list (list 'freq 110) (list 'cutoff 800) (list 'res 1) (list 'detune 1.005))
note-on p

# controls: a name, a range, a value; bound to parameters (several bindings per control are fine)
control 'cutoff 100 6000 800
control 'resonance 0.5 8 1
control 'detune 1 1.05 1.005
toggle 'gate 1
bind-control 'cutoff p 'cutoff
bind-control 'resonance p 'res
bind-control 'detune p 'detune
bind-control 'gate p 'gate

# OSC: any controller or program can move them: send a float to /pad/cutoff on port 9000
osc-listen 9000
osc-map "/pad/cutoff" 'cutoff
osc-map "/pad/resonance" 'resonance
print "controls:" (map (controls-list) head) "; OSC on port 9000 at /pad/cutoff and /pad/resonance"
print "e.g. from another terminal:  musil -e '(osc-send \"127.0.0.1\" 9000 \"/pad/cutoff\" 2500)'"

# the window (the CLI) or the panel (the Listener); loops, OSC and the editor port keep running meanwhile
controls
# a control can also be read by code
print "cutoff is now" (control-value 'cutoff)
osc-stop
free-all
audio-quit
