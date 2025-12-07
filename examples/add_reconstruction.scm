;; add_reconstruction.scm
;;
;; Sound analysis and reconstruction (fixed, no list-nth/etc.)

(load "signals.scm")

;; ------------------------------------------------------------------
;; Parameters
;; ------------------------------------------------------------------
(def sr    (array 44100))
(def dur   (array 1.2))
(def samps (* dur sr))

(def nyquist (/ sr (array 2)))
(def nwin    (array 4096))
(def offset  (array 128))

(def tab1 (gen nwin (array 1)))  ;; simple harmonic wavetable (guard point included)

;; ------------------------------------------------------------------
;; Read input sound (first channel)
;; readwav -> (sr (list ch0 ch1 ...))
;; ------------------------------------------------------------------
(print "analysing......")

(def wav-info (readwav "data/gong_c_sharp.wav"))
(def chans    (lindex wav-info (array 1)))
(def input    (lindex chans (array 0)))      ;; first channel

;; ------------------------------------------------------------------
;; Spectral analysis
;; ------------------------------------------------------------------
(def spec  (fft input))       ;; complex: length = 2 * Nfft
(def polar (car2pol spec))    ;; [mag0 phase0 mag1 phase1 ...]

;; Nfft = number of complex bins
(def nfft (/ (size polar) (array 2)))

;; magnitudes for all bins 0..Nfft-1 (we’ll then select a band)
(def mag0 (slice polar (array 0) nfft (array 2)))

;; focus on a band [offset, offset + nwin)
(def mag  (slice mag0 offset nwin))

;; frequency axis:
;; we want bin step ≈ sr / Nfft, so build BPF from 0 to sr
(def fftfreqs0 (bpf (array 0) nfft (* (array 2) nyquist)))  ;; 2*nyquist = sr
(def fftfreqs  (slice fftfreqs0 offset (size mag)))

;; ------------------------------------------------------------------
;; Build time-varying envelopes for each partial
;; amplitudes and frequencies are made constant over 'samps'
;; using a simple 2-point BPF (start=end=v)
;;
;; amplitude normalization:
;;   For a real sinusoid of amplitude A:
;;       |X[k]| ≈ Nfft * A / 2
;;   so we use A ≈ 2 * |X[k]| / Nfft
;; ------------------------------------------------------------------
(def amps (list))
(def i    (array 0))
(while (< i (size mag))
  {
    (def v    (slice mag i (array 1)))             ;; |X[k]|
    (def vsc  (* v (/ (array 2) nfft)))           ;; ≈ amplitude A
    (def env  (bpf vsc samps vsc))                ;; constant amp over duration
    (= amps (lappend amps env))
    (= i (+ i (array 1)))
  })

(def freqs (list))
(= i (array 0))
(while (< i (size fftfreqs))
  {
    (def v   (slice fftfreqs i (array 1)))        ;; frequency in Hz
    (def env (bpf v samps v))                    ;; constant freq envelope
    (= freqs (lappend freqs env))
    (= i (+ i (array 1)))
  })

(print "done\nsynthesising...")

;; ------------------------------------------------------------------
;; Resynthesis via oscillator bank
;; ------------------------------------------------------------------
(def out (oscbank sr amps freqs tab1))

;; gentle global scaling + fade-out
(def norm (array 0.8))
(def fade (bpf norm samps (array 0.0)))
(def out-faded (* fade out))

(print "done\n")

;; ------------------------------------------------------------------
;; Write result as mono WAV
;; writewav: filename sr (list channels)
;; ------------------------------------------------------------------
(writewav "reconstruction.wav" sr (list out-faded))

;; eof
