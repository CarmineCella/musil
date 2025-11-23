;; core_test.scm
;; tests for the core Musil language

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Test framework
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def total  (array 0))
(def failed (array 0))

(def test
  (lambda (expr expected)
    (begin
      (= total (+ total (array 1)))
      (def value (eval expr))
      (if (== value expected)
          (array 1)
          (begin
            (= failed (+ failed (array 1)))
            (print "FAIL: " expr " => " value ", expected " expected "\n"))))))

(def report
  (lambda ()
    (begin
      (print "Total tests: " total ", failed: " failed "\n")
      (if (== failed (array 0))
          (print "ALL TESTS PASSED\n")
          (print "SOME TESTS FAILED\n")))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Basic arrays, equality, arithmetic
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; array / scalar basics
(test '(array 1)          (array 1))
(test '(array 1 2 3)      (array 1 2 3))
(test '(array (array 1 2) (array 3)) (array 1 2 3))

;; equality (==)
(test '(== (array 1) (array 1)) (array 1))
(test '(== (array 1 2 3) (array 1 2 3)) (array 1))
(test '(== (array 1) (array 2)) (array 0))

;; + - * / (scalar)
(test '(+ (array 1) (array 2))      (array 3))
(test '(- (array 5) (array 3))      (array 2))
(test '(* (array 2) (array 3))      (array 6))
(test '(/ (array 8) (array 2))      (array 4))

;; + - * / (vector + scalar)
(test '(+ (array 1 2 3) (array 10)) (array 11 12 13))
(test '(- (array 5 6 7) (array 1))  (array 4 5 6))
(test '(* (array 1 2 3) (array 2))  (array 2 4 6))
(test '(/ (array 8 6 4) (array 2))  (array 4 3 2))

;; + - * / (vector + vector)
(test '(+ (array 1 2 3) (array 10 11 12)) (array 11 13 15))
(test '(- (array 5 6 7) (array 1 2 3))    (array 4 4 4))
(test '(* (array 1 2 3) (array 4 5 6))    (array 4 10 18))
(test '(/ (array 8 9 10) (array 2 3 5))   (array 4 3 2))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Comparisons: < <= > >=
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(<  (array 1) (array 2))    (array 1))
(test '(<= (array 2) (array 2))    (array 1))
(test '(>  (array 3) (array 1))    (array 1))
(test '(>= (array 3) (array 3))    (array 1))

(test '(<  (array 2) (array 1))    (array 0))
(test '(>  (array 1) (array 2))    (array 0))

;; vector-wise comparison
(test '(< (array 1 2) (array 2 1)) (array 1 0))
(test '(> (array 3 1) (array 2 1)) (array 1 0))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Aggregates: min, max, sum, size
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(min (array 1 2 3))  (array 1))
(test '(max (array 1 2 3))  (array 3))
(test '(sum (array 1 2 3))  (array 6))
(test '(size (array 1 2 3)) (array 3))

(test '(min (array 5 4 9) (array -1 10)) (array 4  -1))
(test '(max (array 5 4 9) (array -1 10)) (array 9  10))
(test '(sum (array 1 1)   (array 2 2))   (array 2  4))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Unary math: sin, cos, tan, log, exp, abs, neg, floor, etc.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; sin / cos / tan on simple values
(test '(sin (array 0))                 (array 0))
(test '(cos (array 0))                 (array 1))
(test '(tan (array 0))                 (array 0))

;; vector trig (including pi)
(test '(sin (array 0 3.141592653589793)) (array 0 0))
(test '(cos (array 0 3.141592653589793)) (array 1 -1))

;; acos / asin / atan at simple points
(test '(acos (array 1))                (array 0))
(test '(asin (array 0))                (array 0))
(test '(atan (array 0))                (array 0))

;; hyperbolic: sinh / cosh / tanh
(test '(sinh (array 0))                (array 0))
(test '(cosh (array 0))                (array 1))
(test '(tanh (array 0))                (array 0))

;; vector hyperbolic
(test '(sinh (array 0 1))              (sinh (array 0 1)))  ;; identity check
(test '(cosh (array 0 1))              (cosh (array 0 1)))
(test '(tanh (array 0 1))              (tanh (array 0 1)))

;; exp / log / log10
(test '(exp (array 0))                 (array 1))
(test '(exp (array 1))                 (array 2.718281828))
(test '(log (array 1))                 (array 0))
(test '(log (array 2))                 (array 0.6931471806))
(test '(log10 (array 1 100))           (array 0 2))

;; vector exp/log combo
(test '(exp (array 0 1))               (array 1 2.718281828))
(test '(log (array 1 2))               (array 0 0.6931471806))

;; abs, neg, sqrt already partially tested, add vector checks
(test '(abs (array -3 -2 -1 0 1 2 3))  (array 3 2 1 0 1 2 3))
(test '(neg (array -1 0 1))            (array 1 0 -1))
(test '(sqrt (array 0 1 4 9))          (array 0 1 2 3))

(test '(sin (array 0))    (array 0))
(test '(cos (array 0))    (array 1))
(test '(tan (array 0))    (array 0))

;; inverse trigs at simple points
(test '(asin (array 0))   (array 0))
(test '(atan (array 0))   (array 0))

;; floor
(test '(floor (array 1.2 -1.2 0.0)) (array 1 -2 0))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; slice and assign
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; slice: start, len, (optional) stride
(test '(slice (array 10 20 30 40 50)
              (array 1) (array 3))
      (array 20 30 40))

(test '(slice (array 10 20 30 40 50)
              (array 0) (array 3) (array 2))
      (array 10 30 50))

;; assign modifies first array
(def asrc (array 1 2 3 4 5))
(assign asrc (array 9 8) (array 1) (array 2))
(test 'asrc (array 1 9 8 4 5))

;; slice full range
(test '(slice (array 1 2 3 4) (array 0) (array 4))
      (array 1 2 3 4))

;; slice single element, with stride
(test '(slice (array 10 20 30 40) (array 1) (array 1) (array 2))
      (array 20))

;; slice with stride > 1 on middle portion
(test '(slice (array 5 10 15 20 25 30) (array 1) (array 3) (array 2))
      (array 10 20 30))

;; assign with length 1
(def asrc2 (array 1 2 3 4))
(assign asrc2 (array 99) (array 2) (array 1))
(test 'asrc2 (array 1 2 99 4))

;; assign with stride 2
(def asrc3 (array 1 2 3 4 5))
(assign asrc3 (array 99 88) (array 1) (array 2) (array 2))
;; indices 1 and 3 replaced
(test 'asrc3 (array 1 99 3 88 5))

;; assign on prefix
(def asrc4 (array 10 20 30 40))
(assign asrc4 (array 1 2 3) (array 0) (array 3))
(test 'asrc4 (array 1 2 3 40))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Lists and list operations: list, lindex, lset, llength, lappend, lrange,
;; lreplace, lshuffle
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; basic list length
(test '(llength (list (array 1) (array 2) (array 3))) (array 3))

;; lindex
(test '(lindex (list (array 10) (array 20) (array 30)) (array 1))
      (array 20))

;; lset
(def lst1 (list (array 1) (array 2) (array 3)))
(lset lst1 (array 99) (array 1))
(test '(lindex lst1 (array 1)) (array 99))

;; lappend
(def lst2 (list (array 1) (array 2)))
(lappend lst2 (array 3))
(test '(llength lst2) (array 3))
(test '(lindex lst2 (array 2)) (array 3))

;; lrange
(test '(lindex (lrange (list (array 1) (array 2) (array 3) (array 4))
                       (array 1) (array 2))
               (array 0))
      (array 2))

;; lreplace
(def lr (list (array 1) (array 2) (array 3) (array 4)))
(lreplace lr (list (array 9) (array 8)) (array 1) (array 2))
(test '(lindex lr (array 1)) (array 9))
(test '(lindex lr (array 2)) (array 8))

;; lshuffle: just check length preserved
(test '(llength (lshuffle (list (array 1) (array 2) (array 3))))
      (array 3))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; array2list
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(lindex (array2list (array 10 20 30)) (array 0)) (array 10))
(test '(lindex (array2list (array 10 20 30)) (array 2)) (array 30))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Core forms: quote, def, =, lambda, macro, if, while, begin, eval, apply
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; quote
(test '(quote (array 1 2)) '(array 1 2))

;; def and =
(def a (array 1))
(test 'a (array 1))
(= a (array 2))
(test 'a (array 2))

;; lambda (lambda) and function application
(def id (lambda (x) x))
(test '(id (array 5)) (array 5))

;; apply with built-in +
(test '(apply + (list (array 1) (array 2) (array 3))) (array 6))

;; eval
(test '(eval '(+ (array 1) (array 2))) (array 3))

;; if
(test '(if (array 1) (array 10) (array 20)) (array 10))
(test '(if (array 0) (array 10) (array 20)) (array 20))

;; while: sum 0..4
(def c (array 0))
(def s (array 0))
(while (< c (array 5))
  (begin
    (= s (+ s c))
    (= c (+ c (array 1)))))
(test 's (array 10))

;; begin (already used, but add a simple explicit test)
(test '(begin (array 1) (array 2) (array 3)) (array 3))

;; simple macro: ignore arg, always produce (array 99)
(def m1 (macro (x) '(array 99)))
(test '(m1 (array 3)) (array 99))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; info: vars / exists / typeof
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; (info 'vars) should return a non-empty list of symbols
(test '(> (llength (info 'vars)) (array 0)) (array 1))

;; exists: define a variable and check its presence
(def t_var (array 123))
(test '(lindex (info 'exists 't_var) (array 0))       (array 1))
(test '(lindex (info 'exists 'non_existing_var) (array 0)) (array 0))

;; typeof: lists the types of evaluated arguments as symbols
(test '(info 'typeof '(1 2 3))           '(list))
(test '(info 'typeof 'x)                 '(symbol))
(test '(info 'typeof "hello")            '(string))
(test '(info 'typeof (array 1 2 3))      '(array))

;; typeof on lambda, macro, and built-in op
(def t-lam (lambda (x) x))
(test '(info 'typeof t-lam)              '(lambda))

(def t-mac (macro (x) x))
(test '(info 'typeof t-mac)              '(macro))

(test '(info 'typeof +)                  '(op))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; String operations via (str ...)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; length
(test '(str 'length "abc") (array 3))

;; find
(test '(str 'find "hello world" "world") (array 6))
(test '(str 'find "abc" "z")             (array -1))

;; range
(test '(str 'range "hello" (array 1) (array 3)) "ell")

;; replace
(test '(str 'replace "hello world" "world" "DSP") "hello DSP")

;; split
(test '(llength (str 'split "a,b,c" ",")) (array 3))

;; regex (simple match of digits)
(test '(llength (str 'regex "abc123" "[0-9]+")) (array 1))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Final report
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(report)
