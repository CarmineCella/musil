;; udp_sender.scm
;;
;; Sends a UDP datagram to localhost:9000

(load "stdlib.scm")

(print "=== udp_sender.scm ===\n")
(print "Sending a UDP message to 127.0.0.1:9000\n\n")

(def ok (udpsend "127.0.0.1" 9000 "hello from Musil!"))

(if ok
    (print "send OK\n")
    (print "send FAILED\n"))

(print "Done.\n")
