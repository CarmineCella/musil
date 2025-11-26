;; udp_receiver.scm
;;
;; Waits for a single UDP datagram on localhost:9000 and prints it.

(load "stdlib.scm")

(print "=== udp_receiver.scm ===\n")
(print "Waiting for one UDP message on 127.0.0.1:9000...\n")
(print "Use udp_sender.scm in another process to send.\n\n")

(def msg (udprecv "127.0.0.1" 9000))

(print "Received UDP message: " msg "\n")
(print "Done.\n")
