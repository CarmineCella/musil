;; stdlib_test.scm
;; Tests for the Musil standard library (stdlib.scm)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Load stdlib
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(load "stdlib.scm")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Test framework (same pattern as core_test.scm)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def total  [0])
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

(def report
  (lambda ()
    {
      (print "Total tests: " total ", failed: " failed "\n")
      (if (== failed [0])
          (print "ALL TESTS PASSED\n")
          (print "SOME TESTS FAILED\n"))
    }))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Macro tests: function, let, when, unless, schedule
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; ---------------------------------------------------------------------------
;; function
;; ---------------------------------------------------------------------------

(function add2 (x)
  (+ x 2))

(test '(add2 [3]) [5])

(function addxy (x y)
  (+ x y))

(test '(addxy [2] [3]) [5])

;; partial application: (addxy 2) returns a closure
(def add2b (addxy [2]))
(test '(add2b [5]) [7])

;; typeof for function macro itself (it should be a macro)
(test '(info 'typeof function) '(macro))

;; ---------------------------------------------------------------------------
;; let
;; ---------------------------------------------------------------------------

;; simple let
(test '(let ((x [1])
             (y [2]))
         (+ x y))
      [3])

;; let with shadowing of outer variable
(def x [10])
(test '(let ((x [5]))
         (+ x [1]))
      [6])

;; let with more bindings
(test '(let ((a [1])
             (b [2])
             (c [3]))
         (+ a (+ b c)))
      [6])

;; typeof let (macro)
(test '(info 'typeof let) '(macro))

;; ---------------------------------------------------------------------------
;; when
;; ---------------------------------------------------------------------------

(def w1 [0])
(when (> [1] [0])
  {
    (= w1 [1])
  })
(test 'w1 [1])

(def w2 [0])
(when (< [1] [0])
  {
    (= w2 [1])
  })
(test 'w2 [0])

;; typeof when (macro)
(test '(info 'typeof when) '(macro))

;; ---------------------------------------------------------------------------
;; unless
;; ---------------------------------------------------------------------------

(def u1 [0])
(unless (< [1] [0])
  {
    (= u1 [1])
  })
(test 'u1 [1])

(def u2 [0])
(unless (> [1] [0])
  {
    (= u2 [1])
  })
(test 'u2 [0])

;; typeof unless (macro)
(test '(info 'typeof unless) '(macro))

;; ---------------------------------------------------------------------------
;; schedule
;; ---------------------------------------------------------------------------

;; We only test that 'schedule' is a macro here; timing/async semantics are
;; better tested manually or with a more elaborate harness.
(test '(info 'typeof schedule) '(macro))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Booleans & logic
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test 'true  1)
(test 'false 0)

(test '(not 0) 1)
(test '(not 1) 0)

(test '(and 1 1) 1)
(test '(and 1 0) 0)
(test '(and 0 1) 0)

(test '(or 1 0) 1)
(test '(or 0 1) 1)
(test '(or 0 0) 0)

(test '(<> 1 2) 1)
(test '(<> 1 1) 0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; List primitives
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(car '(1 2 3))            1)
(test '(cdr '(1 2 3))            '(2 3))

(test '(second '(10 20 30))      20)
(test '(third '(10 20 30))       30)
(test '(fourth '(1 2 3 4))       4)

(test '(lhead '(1 2 3))          '(1))
(test '(llast '(1 2 3))          3)
(test '(ltail '(1 2 3))          '(3))

(test '(ltake '(1 2 3 4) 2)      '(1 2))
(test '(ldrop '(1 2 3 4) 2)      '(3 4))

(test '(lindex (lsplit '(1 2 3 4) 2) 0) '(1 2))
(test '(lindex (lsplit '(1 2 3 4) 2) 1) '(3 4))

(test '(lreverse '(1 2 3 4))     '(4 3 2 1))

(test '(match 2 '(1 2 3 2 4))    '(1 3))

(test '(elem 2 '(1 2 3))         1)
(test '(elem 5 '(1 2 3))         0)

(test '(zip '(1 2) '(10 20))     '((1 10) (2 20)))
(test '(dup 3 'x)                '(x x x))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Higher-order operators
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(map (lambda (x) (* x x)) '(1 2 3)) '(1 4 9))

(test '(map2 + '(1 2) '(10 20))  '(11 22))

(test '(filter (lambda (x) (> x 0)) '(-1 0 2 3))
      '(2 3))

(test '(foldl + 0 '(1 2 3 4))    10)

(test '(flip - 3 10)             7)       ; (- 10 3)

(test '(comp square succ 3)      16)      ; square(succ(3)) = 4^2 = 16

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Arithmetic helpers (arrays/scalars)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; getval / setval
(def v [10 20 30])
(test '(getval v 1)              [20])
(setval v [99] 1)
(test 'v                         [10 99 30])

(test '(succ 3)                  4)
(test '(pred 3)                  2)

(test '(quotient 7 2)            3)
(test '(remainder 7 2)           1)
(test '(mod 7 2)                 1)

(test '(twice 4)                 8)
(test '(square 3)                9)

(test '(round 3.2)               3)
(test '(round 3.7)               4)

;; mean / stddev / standard / normal
(test '(mean [1 2 3])            2)
(test '(stddev [1 2 3])          1)

(test '(standard [1 2 3])        [-1 0 1])

(test '(normal [1 2 3])
      [0.3333333333 0.6666666667 1])

;; dot / ortho / norm / diff
(test '(dot [1 2 3] [4 5 6])     32)

(test '(ortho [1 -1] [1 1])      1)

(test '(norm [3 4])              5)

(test '(diff [1 2 4])            [1 2])

;; factorial, fib, ack (small values)
(test '(fac 5)                   120)

(test '(fib 1)                   1)
(test '(fib 2)                   1)
(test '(fib 5)                   5)

(test '(ack 0 0)                 1)
(test '(ack 1 0)                 2)
(test '(ack 1 1)                 3)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; List-based numeric operators: sign, compare, dot-operator (.)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(sign '(-2 0 3))          '(-1 1 1))

(test '(compare < '(3 1 2))      1)   ; min
(test '(compare > '(3 1 2))      3)   ; max

(test '(. + '(1 2) '(10 20))     '(11 22))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Constants
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test 'TWOPI                     6.2831853072)
(test 'SQRT2                     1.4142135624)
(test 'LOG2                      0.3010299957)
(test 'E                         2.7182818284)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Final report
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(report)
