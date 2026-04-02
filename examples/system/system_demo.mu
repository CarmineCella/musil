print "=== Musil system demo ===\n"

print "** clock **"
var start = clock()
var acc = 0
var i = 0
while (i < 100000) {
    acc = acc + i
    i = i + 1
}
var stop = clock()
print "start clock = " start
print "stop  clock = " stop
print "elapsed (clock ticks) = " (stop - start) "\n"

print "** dirlist **"
var files = dirlist(".")
print "files in current directory:"
print files "\n"

print "** filestat **"
if (len(files) == 0) {
    print "no files to stat here\n"
} else {
    var first = files[0]
    print "first entry: " first
    var st = filestat(first)
    print "filestat " first " => " st "\n"
}

print "** getvar (environment variables) **"
print "PATH = " getvar("PATH") "\n"

print "** UDP primitives (usage demonstration) **\n"
print "To receive once on localhost:9000:"
print "  udprecv(\"127.0.0.1\", 9000)\n"
print "To send a message to localhost:9000:"
print "  udpsend(\"127.0.0.1\", 9000, \"hello\")\n"
print "For OSC-style sending (address string encoded as OSC):"
print "  udpsend(\"127.0.0.1\", 9000, \"/test\", 1)\n"

print "=== end of system_demo.mu ==="
