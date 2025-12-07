;; conv_reverb.scm
;;
;; Example: reverberation by convolution
;

(load "stdlib.scm")

; wet / dry controls
(def scale (array 0.1))   ; scales the convolved IR
(def mixc  (array 0.8))   ; wet/dry mix: 0 = dry, 1 = fully wet
(def one   (array 1))
(def dry_gain (- one mixc))          ; 1 - mix
(def wet_gain (* scale mixc))        ; scale * mix

; read input and IR
(def sig_info (readwav "data/anechoic1.wav"))
(def ir_info  (readwav "data/Concertgebouw-s.wav"))

(def sr      (car sig_info))          ; sampling rate (array)
(def sig_chs (second sig_info))       ; list of arrays (channels)
(def ir_chs  (second ir_info))        ; list of arrays (channels)

; assume mono input, stereo IR
(def sigL (lindex sig_chs (array 0)))
(def irL  (lindex ir_chs  (array 0)))
(def irR  (lindex ir_chs  (array 1)))

(print "input length = " (size sigL)
       ", IR L length = " (size irL)
       ", IR R length = " (size irR) "\n")

(print "applying convolution...\n")

; convolve (IR * dry signal)
(def wetL (conv irL sigL))
(def wetR (conv irR sigL))

; apply wet/dry gains
(def dryL_scaled (* sigL dry_gain))
(def wetL_scaled (* wetL wet_gain))
(def wetR_scaled (* wetR wet_gain))

; mix dry + wet (mix handles different lengths)
(def outsigL (mix (array 0) wetL_scaled (array 0) dryL_scaled))
(def outsigR (mix (array 0) wetR_scaled (array 0) dryL_scaled))

(print "done. output L length = " (size outsigL)
       ", R length = " (size outsigR) "\n")

; write stereo file
(def out_chs (list outsigL outsigR))
(writewav "reverb.wav" sr out_chs)

(print "reverb.wav written\n")

; eof
