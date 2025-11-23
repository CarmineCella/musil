;; stdlib_test.scm
;; Tests for the Musil standard library (stdlib.scm)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Load stdlib
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(load "stdlib.scm")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Test framework (same pattern as core_test.scm)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def total  (array 0))
(def failed (array 0))

(def test
  (lambda (expr expected)
    (begin
      (= total (+ total (array 1)))
      (def value (eval expr))
      (def ok (== value expected))
      (if (== ok (array 1))
          (print "PASS: " expr "\n")
          (begin
            (= failed (+ failed (array 1)))
            (print "FAIL: " expr " => " value ", expected " expected "\n"))))))

(def report
  (lambda ()
    (begin
      (print "Total tests: " total ", failed: " failed "\n")
      (if (== failed (array 0))
          (print "ALL READER TESTS PASSED\n")
          (print "SOME READER TESTS FAILED\n")))))


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
(def v (array 10 20 30))
(test '(getval v 1)              (array 20))
(setval v (array 99) 1)
(test 'v                         (array 10 99 30))

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
(test '(mean (array 1 2 3))      2)
(test '(stddev (array 1 2 3))    1)

(test '(standard (array 1 2 3))  (array -1 0 1))

(test '(normal (array 1 2 3))
      (array 0.3333333333 0.6666666667 1))

;; dot / ortho / norm / diff
(test '(dot (array 1 2 3) (array 4 5 6)) 32)

(test '(ortho (array 1 -1) (array 1 1))  1)

(test '(norm (array 3 4))                5)

(test '(diff (array 1 2 4))              (array 1 2))

;; factorial, fib, ack (small values)
(test '(fac 5)                           120)

(test '(fib 1)                           1)
(test '(fib 2)                           1)
(test '(fib 5)                           5)

(test '(ack 0 0)                         1)
(test '(ack 1 0)                         2)
(test '(ack 1 1)                         3)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; List-based numeric operators: sign, compare, dot-operator (.)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(sign '(-2 0 3))                  '(-1 1 1))

(test '(compare < '(3 1 2))              1)   ; min
(test '(compare > '(3 1 2))              3)   ; max

(test '(. + '(1 2) '(10 20))             '(11 22))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Constants
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test 'TWOPI                           6.2831853072)
(test 'SQRT2                           1.4142135624)
(test 'LOG2                            0.3010299957)
(test 'E                               2.7182818284)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Final report
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(report)
