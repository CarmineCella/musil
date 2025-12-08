;; features.scm
;; Example: low-level audio features (array-based)
;;

(load "stdlib.scm")

(def N     4096)
(def hop   (/ N 4))
(def SR    44100)
(def frame (/ N SR))

;; use only positive-frequency half of spectrum for descriptors
(def Nspec (/ N 2))

;; read mono signal (first channel)
(def sig  (car (second (readwav "data/Vox.wav"))))

;; STFT returns a list-of-spectra directly
(def data    (stft sig N hop))
(def nframes (llength data))

;; frequencies for 0 .. SR/2 over Nspec bins
(def freqs (bpf 0 Nspec (/ SR 2.0)))

;; previous magnitude spectrum (for flux)
(def oamps (zeros Nspec))

;; pre-allocate feature arrays (one value per frame)
(def centr  (zeros nframes))
(def spread (zeros nframes))
(def skew   (zeros nframes))
(def kurt   (zeros nframes))
(def flux   (zeros nframes))
(def irr    (zeros nframes))
(def decr   (zeros nframes))
(def f0b    (zeros nframes))
(def nrg    (zeros nframes))
(def zx     (zeros nframes))

(def i 0)

(while (< i nframes)
  {
    ;; STFT frame in mag/phase
    (def magphi   (car2pol (lindex data i)))
    (def magphi_d (deinterleave magphi))

    ;; full magnitude (all bins), but we only use the first Nspec
    (def amps-full (car    magphi_d))
    (def phis      (second magphi_d))
    (def amps      (slice amps-full 0 Nspec))

    ;; ---------- frequency-domain features (positive freqs only) ----------
    (def c (speccent  amps freqs))
    (def s (specspread amps freqs c))

    (def sk (specskew  amps freqs c s))
    (def ku (speckurt  amps freqs c s))
    (def fl (specflux  amps oamps))
    (def ir (specirr   amps))
    (def de (specdecr  amps))

    ;; store into feature arrays at index i
    (assign centr  (array c)  i 1)
    (assign spread (array s)  i 1)
    (assign skew   (array sk) i 1)
    (assign kurt   (array ku) i 1)
    (assign flux   (array fl) i 1)
    (assign irr    (array ir) i 1)
    (assign decr   (array de) i 1)

    ;; update previous magnitude for spectral flux
    (assign oamps amps 0 Nspec)

    ;; ---------- time-domain features ----------
    (def seg (slice sig (* i hop) N))

    (def f0 (acorrf0 seg SR))
    (def en (energy  seg))
    (def zc (zcr     seg))

    (assign f0b (array f0) i 1)
    (assign nrg (array en) i 1)
    (assign zx  (array zc) i 1)

    (= i (+ i 1))
  })

(print (size centr) " " (size f0b) "\n")

;; ---------- plot spectral features ----------
(def spec_svg
  (plot "Spectral features"
        centr  "centroid"
        spread "spread"
        skew   "skewness"
        kurt   "kurtosis"
        flux   "flux"
        irr    "irregularity"
        decr   "decrease"
        "*"))

;; approximate ZCR in Hz (for comparison with f0)
(def zx_hz (* zx (/ SR 2.0)))

;; ---------- plot f0 and zero-crossing ----------
;; median smoothing shrinks f0b, so we must match lengths
(= f0b (median f0b 5))
(def len_f0      (size f0b))
(def zx_hz_plot  (slice zx_hz 0 len_f0))

(def f0_svg
  (plot "Fundamental frequency"
        f0b        "autocorr f0"
        zx_hz_plot "zero-crossing (Hz approx.)"
        "*"))

;; ---------- plot energy ----------
(def energy_svg
  (plot "Energy"
        nrg "energy"
        "*"))

;; ---------- resynthesis from f0 and energy ----------
;; For BPF envelopes we still need lists, so convert once here
(def f0bl (array2list f0b))
(def nrgl (array2list nrg))

(def f0s
  (array
    (map2 (lambda (x y)
            (bpf x hop y))
          f0bl
          (ldrop f0bl 1))))

(def amps-env
  (array
    (map2 (lambda (x y)
            (bpf x hop y))
          nrgl
          (ldrop nrgl 1))))

(def t1    (gen 4096 1 0.73 0.6 0.20))
(def synth (* amps-env (osc SR f0s t1)))

(writewav "f0_synth.wav" SR (list synth))

;; eof
