; test_wav.scm
;
; Basic roundtrip tests for readwav / writewav with mono and stereo files.

(load "stdlib.scm")

; ----------------------------------------------------------------------
; Test framework
; ----------------------------------------------------------------------
(def total [0])
(def failed [0])

(def test
  (lambda (expr expected)
    {
      (= total (+ total [1]))
      (def value (eval expr))
      (def ok (== value expected))
      (if (== ok [1])
          (print "PASS: " expr "\n")
          {
            (= failed (+ failed [1]))
            (print "FAIL: " expr " => " value ", expected " expected "\n")
          })
    }))

(def test~
  (lambda (expr expected tol)
    {
      (= total (+ total [1]))
      (def value (eval expr))
      (def diff (- value expected))
      (def ok (< (abs diff) tol))
      (if (== ok [1])
          (print "PASS~: " expr "\n")
          {
            (= failed (+ failed [1]))
            (print "FAIL~: " expr " => " value ", expected " expected " with tol " tol "\n")
          })
    }))

(def report
  (lambda ()
    {
      (print "Total tests: " total ", failed: " failed "\n")
      (if (== failed [0])
          (print "ALL TESTS PASSED\n")
          (print "SOME TESTS FAILED\n"))
    }))


; ----------------------------------------------------------------------
; Test setup: create small stereo and mono buffers, write + read back
; ----------------------------------------------------------------------

; Common sample rate
(def sr (array 48000))

; Stereo test buffers (short, simple shapes)
(def chL (array -1 -0.5 0 0.5 1))
(def chR (array  1  0.5 0 -0.5 -1))
(def chs (list chL chR))
(def fname "test_wav_stereo.wav")

; Write stereo file
(def _ (writewav "test_wav_stereo.wav" sr chs))

; Read back
(def r (readwav "test_wav_stereo.wav"))

; r is expected to be: (sr channels)
(def r-sr  (lindex r (array 0)))
(def r-chs (lindex r (array 1)))

; ----------------------------------------------------------------------
; Stereo tests
; ----------------------------------------------------------------------

; SR should be scalar array, equal to original
(test '(size r-sr) (array 1))
(test '(== r-sr sr)   (array 1))

; Number of channels
(test '(llength r-chs) (array 2))

; Channel lengths should match originals
(test '(size (lindex r-chs (array 0))) (size chL))
(test '(size (lindex r-chs (array 1))) (size chR))

; Check a couple of samples with tolerance (quantization from 16-bit I/O)
(test~ '(getval (lindex r-chs (array 0)) (array 0))
       (getval chL (array 0))
       (array 1e-4))

(test~ '(getval (lindex r-chs (array 0)) (array 4))
       (getval chL (array 4))
       (array 1e-4))

(test~ '(getval (lindex r-chs (array 1)) (array 0))
       (getval chR (array 0))
       (array 1e-4))

(test~ '(getval (lindex r-chs (array 1)) (array 4))
       (getval chR (array 4))
       (array 1e-4))

; ----------------------------------------------------------------------
; Mono tests
; ----------------------------------------------------------------------

(def chM (array 0 0.25 0.5 0.75 1))
(def chsM (list chM))
(def fnameM "test_wav_mono.wav")

(def _m (writewav "test_wav_mono.wav" sr chsM))

(def rM   (readwav "test_wav_mono.wav"))
(def rM-sr  (lindex rM (array 0)))
(def rM-chs (lindex rM (array 1)))

; SR again
(test '(== rM-sr sr) (array 1))

; Mono => 1 channel
(test '(llength rM-chs) (array 1))

; Length should match
(test '(size (lindex rM-chs (array 0))) (size chM))

; Check a sample with tolerance
(test~ '(getval (lindex rM-chs (array 0)) (array 4))
       (getval chM (array 4))
       (array 1e-4))

; ----------------------------------------------------------------------
; Report
; ----------------------------------------------------------------------
(report)
