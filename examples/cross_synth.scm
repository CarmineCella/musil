;; cross_synth.scm
;;

(load "stdlib.scm")

;; --------------------------------------------------------------------
;; Parameters
;; --------------------------------------------------------------------
(def hop (array 512))
(def sz  (array 2048))

;; --------------------------------------------------------------------
;; Read input signals
;; readwav returns: (sr (list ch1 ch2 ...))
;; --------------------------------------------------------------------
(def sig_data (readwav "data/Vox.wav"))
(def sr       (lindex sig_data (array 0)))
(def sig_chs  (lindex sig_data (array 1)))
(def sig1     (lindex sig_chs (array 0)))

(def ir_data  (readwav "data/Beethoven_Symph7.wav"))
(def ir_chs   (lindex ir_data (array 1)))
(def sig2     (lindex ir_chs (array 0)))

(def sig1_len (size sig1))
(def sig2_len (size sig2))

;; choose minimum length
(def min_len sig1_len)
(if (> sig1_len sig2_len)
    (= min_len sig2_len)
    min_len)

;; leave room for last window
(= min_len (- min_len sz))

(print "sig 1 len = " sig1_len
       ", sig 2 len = " sig2_len
       ", min len = " min_len "\n")

;; Hann window: window N  a0 a1 a2
;; Hann = 0.5 - 0.5 cos(2πn/(N-1))
(def bwin (window sz (array 0.5) (array 0.5) (array 0)))

;; output buffer initialized to zeros for [0 .. min_len + sz)
(def outsig (bpf (array 0) (+ sz min_len) (array 0)))

;; denoise threshold: constant 0.0001 over sz/2 bins
(def half_sz (/ sz (array 2)))
(def threshold (bpf (array 0.0001) half_sz (array 0.0001)))

;; --------------------------------------------------------------------
;; Main cross-synthesis loop
;; --------------------------------------------------------------------
(def i (array 0))
(while (< i min_len)
  {
    ;; --- analysis of signal 1 ---
    (def buff1 (slice sig1 i sz))
    (= buff1 (* bwin buff1))
    (def spec1   (fft buff1))
    (def spec1_p (car2pol spec1))
    (def magphi1 (deinterleave spec1_p))
    (def amps1   (lindex magphi1 (array 0)))
    (def phi1    (lindex magphi1 (array 1)))
    ;; noise gate on amps1
    (= amps1 (* (> amps1 threshold) amps1))

    ;; --- analysis of signal 2 ---
    (def buff2 (slice sig2 i sz))
    (= buff2 (* bwin buff2))
    (def spec2   (fft buff2))
    (def spec2_p (car2pol spec2))
    (def magphi2 (deinterleave spec2_p))
    (def amps2   (lindex magphi2 (array 0)))
    (def phi2    (lindex magphi2 (array 1)))

    ;; --- cross-synthesis: geometric mean of magnitudes, phase of sig2 ---
    (def outamps (sqrt (* amps1 amps2)))
    (def outphi  phi2)

    (def out_spec (interleave (list outamps outphi)))
    (def outbuff  (ifft (pol2car out_spec)))
    (= outbuff (* bwin outbuff))

    ;; overlap-add into outsig
    (def cur_seg (slice outsig i (size outbuff)))
    (def sum_seg (+ cur_seg outbuff))
    (= outsig (assign outsig sum_seg i (size outbuff)))

    ;; advance
    (= i (+ i hop))
  })

(print "min = " (min outsig) ", max = " (max outsig) "\n")

;; --------------------------------------------------------------------
;; Write output
;; --------------------------------------------------------------------
(writewav "xsynth.wav" sr (list outsig))

;; eof
