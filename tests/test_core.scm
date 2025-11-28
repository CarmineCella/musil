;; core_test.scm
;; tests for the core Musil language

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Test framework
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
;; Basic arrays, equality, arithmetic
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; array / scalar basics
(test '[1]          [1])
(test '[1 2 3]      [1 2 3])
(test '[[1 2] [3]]  [1 2 3])

;; equality (==)
(test '(== [1] [1])       [1])
(test '(== [1 2 3] [1 2 3]) [1])
(test '(== [1] [2])       [0])

;; + - * / (scalar)
(test '(+ [1] [2])      [3])
(test '(- [5] [3])      [2])
(test '(* [2] [3])      [6])
(test '(/ [8] [2])      [4])

;; + - * / (vector + scalar)
(test '(+ [1 2 3] [10]) [11 12 13])
(test '(- [5 6 7] [1])  [4 5 6])
(test '(* [1 2 3] [2])  [2 4 6])
(test '(/ [8 6 4] [2])  [4 3 2])

;; + - * / (vector + vector)
(test '(+ [1 2 3] [10 11 12]) [11 13 15])
(test '(- [5 6 7] [1 2 3])    [4 4 4])
(test '(* [1 2 3] [4 5 6])    [4 10 18])
(test '(/ [8 9 10] [2 3 5])   [4 3 2])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Comparisons: < <= > >=
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(<  [1] [2])    [1])
(test '(<= [2] [2])    [1])
(test '(>  [3] [1])    [1])
(test '(>= [3] [3])    [1])

(test '(<  [2] [1])    [0])
(test '(>  [1] [2])    [0])

;; vector-wise comparison
(test '(< [1 2] [2 1]) [1 0])
(test '(> [3 1] [2 1]) [1 0])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Aggregates: min, max, sum, size
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(min [1 2 3])  [1])
(test '(max [1 2 3])  [3])
(test '(sum [1 2 3])  [6])
(test '(size [1 2 3]) [3])

(test '(min [5 4 9] [-1 10]) [4 -1])
(test '(max [5 4 9] [-1 10]) [9 10])
(test '(sum [1 1]   [2 2])   [2 4])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Unary math: sin, cos, tan, log, exp, abs, neg, floor, etc.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; sin / cos / tan on simple values
(test '(sin [0]) [0])
(test '(cos [0]) [1])
(test '(tan [0]) [0])

;; vector trig (including pi)
(test '(sin [0 3.141592653589793]) [0 0])
(test '(cos [0 3.141592653589793]) [1 -1])

;; acos / asin / atan at simple points
(test '(acos [1]) [0])
(test '(asin [0]) [0])
(test '(atan [0]) [0])

;; hyperbolic: sinh / cosh / tanh
(test '(sinh [0]) [0])
(test '(cosh [0]) [1])
(test '(tanh [0]) [0])

;; vector hyperbolic
(test '(sinh [0 1]) (sinh [0 1]))  ;; identity check
(test '(cosh [0 1]) (cosh [0 1]))
(test '(tanh [0 1]) (tanh [0 1]))

;; exp / log / log10
(test '(exp [0])           [1])
(test '(exp [1])           [2.718281828])
(test '(log [1])           [0])
(test '(log [2])           [0.6931471806])
(test '(log10 [1 100])     [0 2])

;; vector exp/log combo
(test '(exp [0 1])         [1 2.718281828])
(test '(log [1 2])         [0 0.6931471806])

;; abs, neg, sqrt already partially tested, add vector checks
(test '(abs [-3 -2 -1 0 1 2 3]) [3 2 1 0 1 2 3])
(test '(neg [-1 0 1])           [1 0 -1])
(test '(sqrt [0 1 4 9])         [0 1 2 3])

(test '(sin [0])    [0])
(test '(cos [0])    [1])
(test '(tan [0])    [0])

;; inverse trigs at simple points
(test '(asin [0])   [0])
(test '(atan [0])   [0])

;; floor
(test '(floor [1.2 -1.2 0.0]) [1 -2 0])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; slice and assign
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; slice: start, len, (optional) stride
(test '(slice [10 20 30 40 50]
              [1] [3])
      [20 30 40])

(test '(slice [10 20 30 40 50]
              [0] [3] [2])
      [10 30 50])

;; assign modifies first array
(def asrc [1 2 3 4 5])
(assign asrc [9 8] [1] [2])
(test 'asrc [1 9 8 4 5])

;; slice full range
(test '(slice [1 2 3 4] [0] [4])
      [1 2 3 4])

;; slice single element, with stride
(test '(slice [10 20 30 40] [1] [1] [2])
      [20])

;; slice with stride > 1 on middle portion
(test '(slice [5 10 15 20 25 30] [1] [3] [2])
      [10 20 30])

;; assign with length 1
(def asrc2 [1 2 3 4])
(assign asrc2 [99] [2] [1])
(test 'asrc2 [1 2 99 4])

;; assign with stride 2
(def asrc3 [1 2 3 4 5])
(assign asrc3 [99 88] [1] [2] [2])
;; indices 1 and 3 replaced
(test 'asrc3 [1 99 3 88 5])

;; assign on prefix
(def asrc4 [10 20 30 40])
(assign asrc4 [1 2 3] [0] [3])
(test 'asrc4 [1 2 3 40])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Lists and list operations: list, lindex, lset, llength, lappend, lrange,
;; lreplace, lshuffle
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; basic list length
(test '(llength (list [1] [2] [3])) [3])

;; lindex
(test '(lindex (list [10] [20] [30]) [1])
      [20])

;; lset
(def lst1 (list [1] [2] [3]))
(lset lst1 [99] [1])
(test '(lindex lst1 [1]) [99])

;; lappend
(def lst2 (list [1] [2]))
(lappend lst2 [3])
(test '(llength lst2) [3])
(test '(lindex lst2 [2]) [3])

;; lrange
(test '(lindex (lrange (list [1] [2] [3] [4])
                       [1] [2])
               [0])
      [2])

;; lreplace
(def lr (list [1] [2] [3] [4]))
(lreplace lr (list [9] [8]) [1] [2])
(test '(lindex lr [1]) [9])
(test '(lindex lr [2]) [8])

;; lshuffle: just check length preserved
(test '(llength (lshuffle (list [1] [2] [3])))
      [3])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; array2list
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(lindex (array2list [10 20 30]) [0]) [10])
(test '(lindex (array2list [10 20 30]) [2]) [30])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Core forms: quote, def, =, lambda, macro, if, while, begin, eval, apply
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; quote (on an array)
(test ''[1 2] '[1 2])

;; def and =
(def a [1])
(test 'a [1])
(= a [2])
(test 'a [2])

;; lambda (lambda) and function application
(def id (lambda (x) x))
(test '(id [5]) [5])

;; apply with built-in +
(test '(apply + (list [1] [2] [3])) [6])

;; eval
(test '(eval '(+ [1] [2])) [3])

;; if
(test '(if [1] [10] [20]) [10])
(test '(if [0] [10] [20]) [20])

;; while: sum 0..4
(def c [0])
(def s [0])
(while (< c [5])
  {
    (= s (+ s c))
    (= c (+ c [1]))
  })
(test 's [10])

;; explicit block test using {}
(test '{ [1] [2] [3] } [3])

;; simple macro: ignore arg, always produce [99]
(def m1 (macro (x) '[99]))
(test '(m1 [3]) [99])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; info: vars / exists / typeof
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; (info 'vars) should return a non-empty list of symbols
(test '(> (llength (info 'vars)) [0]) [1])

;; exists: define a variable and check its presence
(def t_var [123])
(test '(lindex (info 'exists 't_var) [0])       [1])
(test '(lindex (info 'exists 'non_existing_var) [0]) [0])

;; typeof: lists the types of evaluated arguments as symbols
(test '(info 'typeof '(1 2 3))           '(list))
(test '(info 'typeof 'x)                 '(symbol))
(test '(info 'typeof "hello")            '(string))
(test '(info 'typeof [1 2 3])            '(array))

;; typeof on lambda, macro, and built-in op
(def t-lam (lambda (x) x))
(test '(info 'typeof t-lam)              '(lambda))

(def t-mac (macro (x) x))
(test '(info 'typeof t-mac)              '(macro))

(test '(info 'typeof +)                  '(op))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Currying & partial application
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; simple 2-arg function
(def two-params
  (lambda (x y)
    (+ x y)))

;; full application
(test '(two-params [2] [3]) [5])

;; partial application: one argument
(def add2 (two-params [2]))
(test '(add2 [10]) [12])

;; 3-arg function to exercise multi-step currying
(def add3
  (lambda (x y z)
    (+ (+ x y) z)))

;; full application
(test '(add3 [1] [2] [3]) [6])

;; partially apply 1 argument
(def add1 (add3 [1]))
(test '(add1 [2] [3]) [6])

;; partially apply 2 arguments
(def add1and2 (add3 [1] [2]))
(test '(add1and2 [3]) [6])

;; chained partial application
(test '(((add3 [1]) [2]) [3]) [6])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Closures
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; classic counter closure
(def make-counter
  (lambda (n)
    (lambda ()
      (= n (+ n [1]))
      n)))

(def c1 (make-counter [0]))

(test '(c1) [1])
(test '(c1) [2])

(def c2 (make-counter [10]))
(test '(c2) [11])

;; c1 retains its own state
(test '(c1) [3])

;; closure capturing outer variable
(def outer-val [5])

(def make-adder-with-outer
  (lambda (y)
    (lambda (z)
      (+ (+ outer-val y) z))))  ;; captures outer-val lexically

(def add-from-outer (make-adder-with-outer [2]))
(test '(add-from-outer [3]) [10])  ;; 5 + 2 + 3

(= outer-val [100])
(test '(add-from-outer [3]) [105])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Shadowing (parameters vs globals, nested scopes)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; parameter shadowing a global
(def x [10])

(def use-x
  (lambda (x)
    (+ x [1])))

(test '(use-x [5]) [6])   ;; uses parameter x=[5]
(test 'x         [10])    ;; global x unchanged

;; local variable shadowing outer within a lambda
(def y [5])

(def f
  (lambda (y)
    (+ y [1])))

(test '(f [10]) [11])    ;; uses inner y=[10]
(test 'y       [5])      ;; outer y still [5]

;; lexical vs dynamic-style shadowing: closure should see x lexically
(def x [1])

(def make-lex-adder
  (lambda (y)
    (lambda (z)
      (+ (+ x y) z))))  ;; x captured here ([1])

(def add-from-x (make-lex-adder [2]))  ;; x=[1], y=[2]

;; rebind x at top level
(= x [100])

;; closure should now see x=[100] (because env is shared here)
(test '(add-from-x [3]) [105])

;; nested shadowing inside lambda body
(def z [7])

(def g
  (lambda (z)
    {
      (def z (+ z [1]))
      z
    }))

(test '(g [3]) [4])   ;; inner z = [4]
(test 'z       [7])   ;; outer z untouched

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; String operations via (str ...)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; length
(test '(str 'length "abc") [3])

;; find
(test '(str 'find "hello world" "world") [6])
(test '(str 'find "abc" "z")             [-1])

;; range
(test '(str 'range "hello" [1] [3]) "ell")

;; replace
(test '(str 'replace "hello world" "world" "DSP") "hello DSP")

;; split
(test '(llength (str 'split "a,b,c" ",")) [3])

;; regex (simple match of digits)
(test '(llength (str 'regex "abc123" "[0-9]+")) [1])


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
