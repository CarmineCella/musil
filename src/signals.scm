; signals.scm

(load "core.scm")

; ---------------------------------------------------------
; Small helpers: arrays + lists
; ---------------------------------------------------------

; array nth: returns a 1-element array [a[i]]
(def arr-nth
  (lambda (a i)
    (slice a (array i) (array 1))))

; array length: returns a 1-element array [N]
(def arr-len
  (lambda (a)
    (size a)))

; list nth: list element at index i
(def list-nth
  (lambda (lst i)
    (lindex lst (array i))))

; list length: returns a 1-element array [N]
(def list-len
  (lambda (lst)
    (llength lst)))


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
      (def b (list-nth coeffs 0))
      (def a (list-nth coeffs 1))
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
