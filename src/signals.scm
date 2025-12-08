; signals.scm

(load "core.scm")

; ---------------------------------------------------------
; RMS, normalization
; ---------------------------------------------------------

; NOTE: your C++ energy() behaves like RMS:
;   energy([1 2 3]) ≈ sqrt((1^2+2^2+3^2)/3) ≈ 2.16025
; So rms is simply an alias.
(def rms
  (lambda (sig)
    (energy sig)))

; normalize(sig, target_rms)
(def normalize
  (lambda (sig target)
    {
      (def r (rms sig))
      (if (== r (array 0))
          sig
          (* sig (array (/ target r))))
    }))


; ---------------------------------------------------------
; Filter design + application
; ---------------------------------------------------------

; Design a biquad (returns list: (b a))
; type: 'lowpass, 'highpass, 'notch, 'peak, 'lowshelf, 'highshelf
(def design-biquad
  (lambda (type Fs f0 Q gain_db)
    (filtdesign type
                (array Fs)
                (array f0)
                (array Q)
                (array gain_db))))

; Apply a single biquad described by (b a)
(def apply-biquad
  (lambda (sig coeffs)
    {
      (def b (lindex coeffs 0))
      (def a (lindex coeffs 1))
      (filter sig b a)
    }))

; Simple chain of two biquads
(def apply-biquad-chain2
  (lambda (sig coeffs1 coeffs2)
    {
      (def y1 (apply-biquad sig coeffs1))
      (apply-biquad y1 coeffs2)
    }))


; ---------------------------------------------------------
; Named EQ / filter helpers
; ---------------------------------------------------------

(def lowpass
  (lambda (sig Fs f0 Q)
    {
      (def fd (design-biquad 'lowpass Fs f0 Q 0.0))
      (apply-biquad sig fd)
    }))

(def highpass
  (lambda (sig Fs f0 Q)
    {
      (def fd (design-biquad 'highpass Fs f0 Q 0.0))
      (apply-biquad sig fd)
    }))

(def notch
  (lambda (sig Fs f0 Q)
    {
      (def fd (design-biquad 'notch Fs f0 Q 0.0))
      (apply-biquad sig fd)
    }))

(def peak-eq
  (lambda (sig Fs f0 Q gain_db)
    {
      (def fd (design-biquad 'peak Fs f0 Q gain_db))
      (apply-biquad sig fd)
    }))

(def lowshelf
  (lambda (sig Fs f0 Q gain_db)
    {
      (def fd (design-biquad 'lowshelf Fs f0 Q gain_db))
      (apply-biquad sig fd)
    }))

(def highshelf
  (lambda (sig Fs f0 Q gain_db)
    {
      (def fd (design-biquad 'highshelf Fs f0 Q gain_db))
      (apply-biquad sig fd)
    }))


; ---------------------------------------------------------
; Delay / comb / allpass helpers
; ---------------------------------------------------------

(def frac-delay
  (lambda (sig delay-samples)
    (delay sig (array delay-samples))))

(def comb-filter
  (lambda (sig delay-samples g)
    (comb sig (array delay-samples) (array g))))

(def allpass-filter
  (lambda (sig delay-samples g)
    (allpass sig (array delay-samples) (array g))))

; Toy Schroeder-style reverb network
(def schroeder-reverb
  (lambda (sig Fs)
    {
      (def d1 (* Fs 0.0297))
      (def d2 (* Fs 0.0371))
      (def d3 (* Fs 0.0411))
      (def d4 (* Fs 0.0437))

      (def c1 (comb-filter sig d1 0.773))
      (def c2 (comb-filter sig d2 0.802))
      (def c3 (comb-filter sig d3 0.753))
      (def c4 (comb-filter sig d4 0.733))

      (def mixc (+ c1 (+ c2 (+ c3 c4))))

      (def a1 (allpass-filter mixc (* Fs 0.005)  0.7))
      (def a2 (allpass-filter a1   (* Fs 0.0017) 0.7))

      a2
    }))


; ---------------------------------------------------------
; DC blocker / reson helpers
; ---------------------------------------------------------

(def dc-block
  (lambda (sig)
    (dcblock sig (array 0.995))))

(def dc-block-R
  (lambda (sig R)
    (dcblock sig (array R))))

(def resonator
  (lambda (sig sr freq tau)
    (reson sig (array sr) (array freq) (array tau))))


; ---------------------------------------------------------
; Resampling helpers
; ---------------------------------------------------------

(def resample-factor
  (lambda (sig factor)
    (resample sig (array factor))))

(def resample-to
  (lambda (sig Fs-in Fs-out)
    {
      (def factor (/ Fs-out Fs-in))
      (resample-factor sig factor)
    }))

;; ------------------------------------------------------------------
;; Oscillator bank
;; amps  : list of amplitude envelopes (arrays)
;; freqs : list of frequency envelopes (arrays)
;; sr    : sample rate (array scalar)
;; tab   : wavetable (array, guard point included)
;; ------------------------------------------------------------------
(def oscbank
  (lambda (sr amps freqs tab)
    {
      (def n (llength amps))
      (if (not (== n (llength freqs)))
          (error "[oscbank] amps and freqs must have same length"
                 (list n (llength freqs))))
      (def i   (array 0))
      (def out (array 0))            ;; overwritten at first partial
      (while (< i n)
        {
          (def a_i (lindex amps i))   ;; amplitude envelope (array)
          (def f_i (lindex freqs i))  ;; frequency envelope (array)
          (def sig (osc sr f_i tab))  ;; oscillator signal
          (def sigw (* a_i sig))      ;; weighted by amplitude envelope
          (if (== i (array 0))
              (= out sigw)
              (= out (+ out sigw)))
          (= i (+ i (array 1)))
        })
      out
    }))

;; STFT: (stft sig N hop) -> list of spectra
;;   sig : array
;;   N   : frame size
;;   hop : analysis hop

(def stft
  (lambda (sig N hop)
    {
      (def L    (size sig))
      (def pos  0)
      (def specs ())

      ;; analysis window (Hann-like)
      (def win (window N 0.5 0.5 0.0))

      (while (<= (+ pos N) L)
        {
          (def frame   (slice sig pos N))
          (def frame-w (* frame win))
          (def spec    (fft frame-w))
          (lappend specs spec)
          (= pos (+ pos hop))
        })

      specs
    }))

;; ISTFT: (istft specs N hop) -> array
;;   - FFT is unnormalized, IFFT divides by N in C++
;;   - We do overlap/add with a Hann window
;;   - We compute the constant C = sum(win^2) / hop
;;     and divide the whole output by C to correct the gain.

(def istft
  (lambda (spec-list N hop)
    {
      (def nframes (llength spec-list))
      (if (== nframes (array 0))
          (array 0)
          {
            ;; Output length: Lout = hop*(nframes-1) + N
            (def Lout (+ (* hop (- nframes 1)) N))
            (def out  (zeros Lout))

            ;; Hann analysis/synthesis window
            (def win  (window N 0.5 0.5 0.0))
            (def win2 (* win win))

            ;; Scalar normalization constant:
            ;; C = sum(win^2) / hop  ≈ 1.5 for Hann + N/4
            (def win-energy (sum win2))
            (def normconst  (/ win-energy hop))

            (def k 0)
            (while (< k nframes)
              {
                (def spec  (lindex spec-list k))
                (def frame (ifft spec))       ;; already /N in C++
                (def frame-w (* frame win))
                (def pos   (* k hop))

                ;; Overlap/add into out
                (def seg-out (slice out pos N))
                (def seg-new (+ seg-out frame-w))
                (assign out seg-new pos N)

                (= k (+ k 1))
              })

            ;; Final normalization by scalar normconst
            (def inv-norm (/ 1.0 normconst))      ;; scalar
            (def out-norm (* out (array inv-norm))) ;; scale array

            out-norm
          })
    }))


;; eof


