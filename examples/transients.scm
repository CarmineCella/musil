;; transients.scm
;;
;; Example: transient separation using STFT + k-means

(load "stdlib.scm")

(def N     4096)
(def hop   (/ N 4))
(def SR    44100)
(def frame (/ N SR))

;; read mono signal (first channel)
(def snd (readwav "data/Gambale_cut.wav"))
(def sr  (lindex snd 0))
(def sig (car (lindex snd 1)))

;; STFT: list of complex spectra (arrays of interleaved re/im)
(def spec-list (stft sig N hop))

;; mag/phase decomposition per frame
(def magphi    (map car2pol spec-list))
(def magphi_d  (map deinterleave magphi))

;; amps, phis: lists of arrays
(def amps (map (lambda (p) (car p))    magphi_d))
(def phis (map (lambda (p) (second p)) magphi_d))

;; k-means clustering on amplitude spectra (2 clusters)
;; clusters = (list labels centroids ...)
(def clusters (kmeans amps 2))
(def labels   (array2list (lindex clusters 0)))  ;; scalar labels 0/1

;; separate transient (label = 0) vs stable (label = 1)
(def nframes (llength amps))
(def j 0)
(def tamps ())
(def samps ())

(while (< j nframes)
  {
    (def a   (lindex amps j))    ;; spectrum mag (array)
    (def lab (lindex labels j))  ;; 0 or 1

    ;; transient = a * (1 - lab), stable = a * lab
    (def t (* a (- 1 lab)))
    (def s (* a lab))

    (lappend tamps t)
    (lappend samps s)

    (= j (+ j 1))
  })

;; ---------- reconstruct first component (transient) ----------
(def magphi_t
  (map2 (lambda (a ph)
          (list a ph))
        tamps
        phis))

(def magphi_i_t (map interleave magphi_t))
(def imre_t     (map pol2car magphi_i_t))

(def sig_t (istft imre_t N hop))
(writewav "component_1.wav" sr (list sig_t))

;; ---------- reconstruct second component (stable) ----------
(def magphi_s
  (map2 (lambda (a ph)
          (list a ph))
        samps
        phis))

(def magphi_i_s (map interleave magphi_s))
(def imre_s     (map pol2car magphi_i_s))

(def sig_s (istft imre_s N hop))
(writewav "component_2.wav" sr (list sig_s))

;; eof
