print "=== udp_receiver.mu ==="
print "Waiting for one UDP message on 127.0.0.1:9000..."
print "Use udp_sender.mu in another process to send.\n"

var msg = udprecv("127.0.0.1", 9000)

print "Received UDP message: " msg
print "Done."
