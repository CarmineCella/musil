# NB: this only works if Musil is compiled with realtime sound support

var s = readwav ("../../../data/Concertgebouw-s.wav")[1]
play (s)

play (s, 22050) # change SR
play ([s[0], s[1], s[0], s[1], s[0], s[1], s[0], s[1]]) # fake multi-channels
print "done playback"
