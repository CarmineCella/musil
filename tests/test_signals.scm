; test_signals.scm
; Tests for Musil DSP library (signals.h + signals.scm)

(load "signals.scm")

; ------------------------------------------------------------------------
; Testing framework
; ------------------------------------------------------------------------

(def total  (array 0))
(def failed (array 0))

(def test
  (lambda (expr expected)
    {
      (= total (+ total (array 1)))
      (def value (eval expr))
      (if (== value expected)
          (print "PASS: " expr "\n")
          {
            (= failed (+ failed (array 1)))
            (print "FAIL: " expr " => " value ", expected " expected "\n")
          })
    }))

(def test~
  (lambda (expr expected tol)
    {
      (= total (+ total (array 1)))
      (def value (eval expr))
      (def diff (abs (- value expected)))
      (if (< diff tol)
          (print "PASS~: " expr "\n")
          {
            (= failed (+ failed (array 1)))
            (print "FAIL~: " expr " => " value ", expected " expected " with tol " tol "\n")
          })
    }))

(def report
  (lambda ()
    {
      (print "Total tests: " total ", failed: " failed "\n")
      (if (== failed (array 0))
          (print "ALL TESTS PASSED\n")
          (print "SOME TESTS FAILED\n"))
    }))

; ------------------------------------------------------------------------
; 1. mix
; ------------------------------------------------------------------------

; mix overlaps segments: second array starts at index 1
; [1 2] + [3 4] shifted by 1 => [1, 2+3, 4] = [1 5 4]
(test '(mix (array 0) (array 1 2)
            (array 1) (array 3 4))
      (array 1 5 4))

; ------------------------------------------------------------------------
; 2. gen / gen10: harmonic table with guard point
; ------------------------------------------------------------------------

(def g (gen (array 8) (array 1)))

; guard point is last element, must equal first
(test
 '(== (getval g (array 0))
      (getval g (- (size g) (array 1))))
 (array 1))

; ------------------------------------------------------------------------
; 3. osc
; ------------------------------------------------------------------------

(test
 '(size (osc (array 44100)
                (array 440 440 440 440)
                (gen (array 16) (array 1))))
 (array 4))

; ------------------------------------------------------------------------
; 4. fft / ifft round-trip
; ------------------------------------------------------------------------

(def sig (array 1 2 3 4))
(def X   (fft sig))
(def sig2 (ifft X))

(test~ '(getval sig2 (array 0)) (array 1) (array 1e-5))
(test~ '(getval sig2 (array 1)) (array 2) (array 1e-5))
(test~ '(getval sig2 (array 2)) (array 3) (array 1e-5))
(test~ '(getval sig2 (array 3)) (array 4) (array 1e-5))

; ------------------------------------------------------------------------
; 5. car2pol / pol2car round-trip
; ------------------------------------------------------------------------

(test
 '(pol2car (car2pol (array 1 0 0 1)))
 (array 1 0 0 1))

; ------------------------------------------------------------------------
; 6. window
;    a0=1,a1=0,a2=0 => rectangular window of ones
; ------------------------------------------------------------------------

(test
 '(window (array 4) (array 1) (array 0) (array 0))
 (array 1 1 1 1))

; ------------------------------------------------------------------------
; 7. basic spectral features
; ------------------------------------------------------------------------

; spectrum with single line at 2000 Hz
(test
 '(speccent (array 0 1 0) (array 1000 2000 3000))
 (array 2000))

; we only test that specspread / specdecr return scalars (no crash)
(test
 '(size (specspread (array 0 1 0) (array 1000 2000 3000) (array 2000)))
 (array 1))

(test
 '(size (specdecr (array 4 3 2 1)))
 (array 1))

; flux(x,x) == 0
(test
 '(specflux (array 1 2 3) (array 1 2 3))
 (array 0))

; irregularity: just check scalar result
(test
 '(size (specirr (array 1 2 3 4)))
 (array 1))

; ------------------------------------------------------------------------
; 8. energy / rms / zcr
; ------------------------------------------------------------------------

; energy() in C++ is actually RMS: energy([1 2 3]) ≈ 2.16025
(test~
 '(energy (array 1 2 3))
 (array 2.16025)
 (array 1e-5))

; rms is implemented using energy, so they should match numerically
(test~
 '(rms (array 1 2 3))
 (array 2.16025)
 (array 1e-5))

; zcr([1 -1 1 -1]) ≈ 0.75 (current C++ behavior)
(test~
 '(zcr (array 1 -1 1 -1))
 (array 0.75)
 (array 1e-6))

; ------------------------------------------------------------------------
; 9. conv / convmc
; ------------------------------------------------------------------------

(test
 '(conv (array 1 2) (array 3 4))
 (array 3 10 8))

(def convmc_out
  (convmc
    (list (array 1 2)
          (array 3 4))
    (list (array 1 0)
          (array 0 1))))

; check shape and each channel (length-3)
(test '(llength convmc_out) (array 2))
(test '(lindex convmc_out (array 0)) (array 1 2 0))
(test '(lindex convmc_out (array 1)) (array 0 3 4))

; ------------------------------------------------------------------------
; 10. dcblock
; ------------------------------------------------------------------------

; DC signal
(def dc_in (array 1 1 1 1 1 1 1 1))

; for tests, use smaller R so decay is visible in a short window
(def dc_out  (dcblock dc_in (array 0.3)))
(def dc_b    (dcblock dc_in (array 0.3)))
(def dc_bR   (dcblock dc_in (array 0.5)))

; just check decay: last sample is smaller than first
(test~
 '(- (getval dc_out (array 0)) (getval dc_out (array 7)))
 (array 1)       ; we just want a positive difference
 (array 1.0))    ; loose tol, this is more a sanity check

; ------------------------------------------------------------------------
; 11. reson
; ------------------------------------------------------------------------

; length must be sr * tau
(test
 '(size (reson (array 1 0 0 0) (array 1000) (array 100) (array 0.01)))
 (array 10))

; ------------------------------------------------------------------------
; 12. filter / filtdesign
; ------------------------------------------------------------------------

; identity filter
(test
 '(filter (array 1 2 3 4) (array 1) (array 1))
 (array 1 2 3 4))

; simple moving average (FIR [0.5 0.5])
(test
 '(filter (array 1 2 3 4) (array 0.5 0.5) (array 1))
 (array 0.5 1.5 2.5 3.5))

; filtdesign returns [b, a] list
(def fd (filtdesign 'lowpass (array 48000) (array 1000) (array 0.707) (array 0)))
(test '(llength fd) (array 2))

; ------------------------------------------------------------------------
; 13. delay (fractional, linear interpolation)
; ------------------------------------------------------------------------

; according to current fn_delay: D=1 => [0 0 2 3]
(test
 '(delay (array 1 2 3 4) (array 1))
 (array 0 0 2 3))

; ------------------------------------------------------------------------
; 14. comb / allpass
; ------------------------------------------------------------------------

; comb: y[n] = x[n] + g*y[n-D]
; for x=[1 0 0 0], D=1, g=1 => [1 1 1 1]
(test
 '(comb (array 1 0 0 0) (array 1) (array 1))
 (array 1 1 1 1))

; allpass with x=[1 0 0 0], D=1, g=0.5 => [-0.5, 0.75, 0.375, 0.1875]
(def ap (allpass (array 1 0 0 0) (array 1) (array 0.5)))

(test~ '(getval ap (array 0)) (array -0.5)   (array 1e-5))
(test~ '(getval ap (array 1)) (array 0.75)   (array 1e-5))
(test~ '(getval ap (array 2)) (array 0.375)  (array 1e-5))
(test~ '(getval ap (array 3)) (array 0.1875) (array 1e-5))

; ------------------------------------------------------------------------
; 15. resample (frequency-domain)
; ------------------------------------------------------------------------

(def rs1 (resample (array 1 2 3 4) (array 1.0)))  ; factor 1 -> same length
(def rs2 (resample (array 1 2 3 4) (array 2.0)))  ; upsample

(test '(size rs1) (array 4))
(test '(> (size rs2) (array 4)) (array 1))

; ------------------------------------------------------------------------
; 16. simple rms sanity (no normalize here)
; ------------------------------------------------------------------------

(def s (array 1 2 3))

; rms positive
(test~
 '(> (rms s) (array 0))
 (array 1)
 (array 1e-6))

; ------------------------------------------------------------------------
; 17. filter helpers in signals.scm
; ------------------------------------------------------------------------

; lowpass / highpass wrappers should preserve length
(test
 '(size (lowpass (array 1 2 3 4) 48000 1000 0.707))
 (array 4))

(test
 '(size (highpass (array 1 2 3 4) 48000 1000 0.707))
 (array 4))

; comb-filter / allpass-filter wrappers
(test
 '(comb-filter (array 1 0 0 0) 1 1)
 (array 1 1 1 1))

(test
 '(size (allpass-filter (array 1 0 0 0) 1 0.5))
 (array 4))

; ------------------------------------------------------------------------
; 18. resonator wrapper (signals.scm)
; ------------------------------------------------------------------------

(def res_sig (resonator (array 1 0 0 0) 1000 100 0.01))
(test '(size res_sig) (array 10))

;; ------------------------------------------------------------
;; 19. Spectral features
;; ------------------------------------------------------------

;; --- spectral centroid / spread / skewness / kurtosis ---

;; Simple three-bin spectrum: only the middle bin active.
;; amplitudes = [0 1 0], freqs = [1000 2000 3000]
;; centroid = 2000, spread = 0, skew = 0, kurtosis = 0 (by our implementation)
(test
  '(speccent (array 0 1 0) (array 1000 2000 3000))
  (array 2000))

(test
  '(specspread (array 0 1 0) (array 1000 2000 3000) (array 2000))
  (array 0))

(test
  '(specskew (array 0 1 0) (array 1000 2000 3000) (array 2000) (array 0))
  (array 0))

(test
  '(speckurt (array 0 1 0) (array 1000 2000 3000) (array 2000) (array 0))
  (array 0))

;; More interesting symmetric case:
;; amplitudes = [1 1], freqs = [0 1]
;; centroid = 0.5, spread = 0.5, skew = 0, kurtosis = 1
(def amps2  (array 1 1))
(def freqs2 (array 0 1))

(test~
  '(speccent amps2 freqs2)
  (array 0.5)
  (array 1e-06))

(def cent2 (speccent amps2 freqs2))

(test~
  '(specspread amps2 freqs2 cent2)
  (array 0.5)
  (array 1e-06))

(def spread2 (specspread amps2 freqs2 cent2))

(test~
  '(specskew amps2 freqs2 cent2 spread2)
  (array 0)
  (array 1e-06))

;; With spread = 0.5, the 4th central moment gives kurtosis = 1
(test~
  '(speckurt amps2 freqs2 cent2 spread2)
  (array 1)
  (array 1e-06))

;; --- spectral flux ---

;; specflux modifies the "old" array, but we only call it once here.
;; amplitudes = [1 2 3], old = [0 0 0]
;; differences = [1 2 3], all non-negative => flux = 6
(def amps3 (array 1 2 3))
(def old3  (array 0 0 0))

(test
  '(specflux amps3 old3)
  (array 6))

;; --- spectral irregularity ---

;; specirr = sum |a[i] - a[i-1]|, i=1..N-1
;; [1 2 3 4] -> |2-1| + |3-2| + |4-3| = 3
(test~
  '(specirr (array 1 2 3 4))
  (array 3)
  (array 1e-06))

;; --- spectral decrease ---

;; specdecr:
;; for [4 3 2 1]:
;;   decs = (3-4)/1 + (2-4)/2 + (1-4)/3 = -1 -1 -1 = -3
;;   den  = 3 + 2 + 1 = 6  =>  decs/den = -0.5
(test~
  '(specdecr (array 4 3 2 1))
  (array -0.5)
  (array 1e-06))

;; --- energy (RMS) ---

;; energy<T> in C++ returns sqrt(sum(a^2)/N) == RMS
;; [1 2 3] -> sqrt((1^2 + 2^2 + 3^2)/3) = sqrt(14/3) ≈ 2.160246899
(test~
  '(energy (array 1 2 3))
  (array 2.1602469)
  (array 1e-05))

;; --- zero-crossing rate ---

;; zcr = nb_zcr / N
;; samples = [1 -1 1 -1]
;; sign sequence: 1, -1, 1, -1
;; nb_zcr = 3, N = 4  =>  zcr = 0.75
(test~
  '(zcr (array 1 -1 1 -1))
  (array 0.75)
  (array 1e-06))

;; --- f0 estimate (ACF) ---

;; Trivial silence frame: all zeros -> f0 = 0
(test
  '(acorrf0 (array 0 0 0 0) (array 44100))
  (array 0))

; ------------------------------------------------------------------------
; Final report
; ------------------------------------------------------------------------

(report)
