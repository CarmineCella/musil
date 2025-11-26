;; system_demo.scm
;;
;; Overview of the Musil system library primitives:
;;  - clock
;;  - dirlist
;;  - filestat
;;  - getvar
;;  - udpsend / udprecv (only usage example here)
;;  - schedule (macro from stdlib, expanding to %schedule)

(load "stdlib.scm")

(print "=== Musil system demo ===\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 1. clock
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** clock **\n")

(def start (clock))
;; do a tiny loop just to waste some time
(def acc 0)
(def i   0)
(while (< i 100000) {
    (= acc (+ acc i))
    (= i   (+ i 1))
})

(def stop (clock))

(print "start clock = " start "\n")
(print "stop  clock = " stop "\n")
(print "elapsed (clock ticks) = " (- stop start) "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 2. dirlist
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** dirlist **\n")

;; list current directory
(def files (dirlist "."))
(print "files in current directory:\n")
(print files "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 3. filestat
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** filestat **\n")

;; we pick first entry (if any) and inspect it
(if (eq (llength files) 0)
    (print "no files to stat here\n\n")
    {
      (def first (lindex files 0))
      (print "first entry: " first "\n")
      ;; filestat returns: (exists size nlink perms)
      (def st (filestat first))
      (print "filestat " first " => " st "\n\n")
    })

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 4. getvar (environment variable)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** getvar (environment variables) **\n")

(print "PATH = " (getvar "PATH") "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 5. schedule (macro over %schedule)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** schedule **\n")
(print "Scheduling a message in 1000 ms...\n")

(schedule
  (lambda () (print "[scheduled] one second has passed\n")) 1000)

(print "Wait a couple of secounds, you should see the message in a while...\n\n")
(sleep 2000)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 6. UDP usage examples (non-blocking explanation)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** UDP primitives (usage demonstration) **\n\n")

(print "To receive once on localhost:9000:\n")
(print "  (udprecv \"127.0.0.1\" (array 9000))\n\n")

(print "To send a message to localhost:9000:\n")
(print "  (udpsend \"127.0.0.1\" (array 9000) \"hello\")\n\n")

(print "For OSC-style sending (address string encoded as OSC):\n")
(print "  (udpsend \"127.0.0.1\" (array 9000) \"/test\" (array 1))\n\n")

(print "=== end of system_demo.scm ===\n")
