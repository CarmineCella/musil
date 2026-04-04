# NB: this only works if Musil is compiled with realtime sound support

load ("stdlib.mu")
load ("signals.mu")

# play a file sync
var s = readwav ("../../../data/Concertgebouw-s.wav")[1]
play (s)
play (s, 22050) # change SR
play ([s[0], s[1], s[0], s[1], s[0], s[1], s[0], s[1]]) # fake multi-channels

# play a sine asycn	
var tab1 = gen(4096, 1)
play_async (osc (44100, linspace (220, 440, 441000), tab1)  * .2)
print "sound will play for 10 seconds, we will stop after 5..."
sleep (5000)
print dacinfo ()
dacstop ()
print "done playback"


proc playfile (f, async) {
	var s = readwav (f)[1]
	if (async == 1) { play_async (s) }
	else { play (s) }
}

playfile ("../../../data/Vox.wav", 1)
sleep (1000)
playfile ("../../../data/Beethoven_Symph7.wav", 1)
sleep (1000)
playfile ("../../../data/cage.wav", 1)
sleep (1000)
playfile ("../../../data/Gambale_cut.wav", 1)