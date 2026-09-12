# live_session: open this file in the IDE and run it (Cmd-Shift-Enter); it opens the device,
# defines an instrument, starts a sound, and leaves everything running. Then type in the
# console line, one line at a time (Enter runs it; Up recalls the previous line), or put the
# cursor on a line below and press Cmd-Alt-Enter:
#
#   set-param a 'cutoff 300 2          the filter closes over two seconds
#   set-param a 'freq 165              a new pitch, at once
#   set-param a 'cutoff 3000 0.5       and opens again
#   note-off a                         release
#   note a 0.3                         a short note
#   set-param a 'res 8                 more resonance, then note a 0.3 again
#   var b (synth lead)                 a second instrument
#   set-params b (list (list 'freq 55) (list 'cutoff 600) (list 'res 2))
#   note-on b
#   audio-report                       voices, load, peak, live
#   free-all
#   audio-quit                         when you are done
#
# Change a number, select the lines and press Cmd-Enter: the change is heard at once.
load "live.mu"
if (opt (audio-status) "open" 0) { audio-quit }
audio-init
var sr (audio-sr)
function lead (gate freq cutoff res) \
    (pan (* (adsr sr gate 0.01 0.2 0.5 0.4) (lowpass (osc sr freq saw-table) sr cutoff res)) 0)
var a (synth lead)
set-params a (list (list 'freq 110) (list 'cutoff 1200) (list 'res 3))
note-on a
print "playing: a =" a "; now type set-param, note-off, note ... in the console (see the file's header)"
