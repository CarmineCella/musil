print "=== udp_sender.mu ==="
print "Sending a UDP message to 127.0.0.1:9000\n"

var ok = udpsend("127.0.0.1", 9000, "hello from Musil!")

if (ok) {
    print "send OK"
} else {
    print "send FAILED"
}

print "Done."
