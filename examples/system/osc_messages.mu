print "open the Max patch in this folder..."

udpsend("127.0.0.1", 10000, "/osc/dur 3", 1)
udpsend("127.0.0.1", 10000, "/osc/freq 440", 1)
sleep(1000)

udpsend("127.0.0.1", 10000, "/osc/freq 330", 1)
sleep(2000)
udpsend("127.0.0.1", 10000, "/osc/freq 220", 1)
sleep(5000)

var i = 0
udpsend("127.0.0.1", 10000, "/osc/dur 1", 1)

while (i < 50) {
    var freq = 220 + i * 10
    var arg = "/osc/freq " + str(freq)
    print arg
    var del = 200
    if (i > 25) {
        del = 100
    }
    udpsend("127.0.0.1", 10000, arg, 1)
    udpsend("127.0.0.1", 10000, arg, 1)
    sleep(del)
    print arg
    i = i + 1
}
