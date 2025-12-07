;; filters.scm
;;
;; Example: basic audio filters
;;
;; (c) www.carminecella.com
;;

(load "signals.scm")

;; read first channel from cage.wav
(def wav-info (readwav "data/cage.wav"))
(def sr       (lindex wav-info (array 0)))  ;; sample rate (array scalar)
(def chans    (lindex wav-info (array 1)))  ;; list of channel arrays
(def w        (lindex chans    (array 0)))  ;; first channel

;; --------------------------------
;; lowpass
;; --------------------------------
(print "lowpass:\n")

(def cutoff-lp (array 200))     ;; 200 Hz cutoff
(def Q-lp      (array 0.707))   ;; Butterworth-ish

(def w-lp (lowpass w sr cutoff-lp Q-lp))

;; write mono low-passed file
(writewav "lp.wav" sr (list w-lp))

;; --------------------------------
;; “bandpass” via HP + LP cascade
;; --------------------------------
(print "bandpass (highpass + lowpass cascade):\n")

(def cutoff-hp  (array 2000))   ;; highpass at 2 kHz
(def cutoff-bp  (array 2500))   ;; lowpass at 2.5 kHz
(def Q-bp       (array 0.707))

;; first highpass, then lowpass → narrow-ish band
(def w-hp (highpass w sr cutoff-hp Q-bp))
(def w-bp (lowpass  w-hp sr cutoff-bp Q-bp))

(writewav "bp.wav" sr (list w-bp))

;; eof
