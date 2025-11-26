;; ---------------------------------------
;; Example: communication with Max via OSC
;; ---------------------------------------

(load "stdlib.scm")

(print "open the Max patch in this folder...")

;; Send a block of two OSC messages then wait 1000 ms

(udpsend "127.0.0.1" 10000 "/osc/dur 3" true)
(udpsend "127.0.0.1" 10000 "/osc/freq 440" true)
(sleep 1000)

;; Single timed OSC messages
(udpsend "127.0.0.1" 10000 "/osc/freq 330" true)
(sleep 2000)
(udpsend "127.0.0.1" 10000 "/osc/freq 220" true)
(sleep 5000)

;; Main loop: random frequencies around 220 Hz
(def i 0)

(udpsend "127.0.0.1" 10000 "/osc/dur 1" true)

(while (< i 50) 
  {
    (def freq (+ 220 (* i 10)))
    (def arg  (tostr "/osc/freq " freq))
    (print arg "\n")
    (def del 200)
    (if (> i 25)
        (= del 100))

    (udpsend "127.0.0.1" 10000 arg true)
    (udpsend "127.0.0.1" 10000 arg true)
    (sleep del)
    (print arg "\n")
    (= i (+ i 1))
  })

;; eof



