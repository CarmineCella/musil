;; --------------------------------
;; Musil overview (Scheme-like DSL)
;; --------------------------------

;; load standard library (defines macros: function, when, unless, let, schedule, ...)
(load "stdlib.scm")

(print "** hallo Musil!! **\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 1. Basics: variables, arrays, arithmetic
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** basics: variables, arrays, arithmetic **\n\n")

;; define variables
(def x 10)
(def y 3)

(print "x = " x ", y = " y "\n")
(print "x + y = " (+ x y) "\n")
(print "x * y = " (* x y) "\n")
(print "x - y = " (- x y) "\n")
(print "x / y = " (/ x y) "\n\n")

;; update variable with =
(= x (+ x 5))
(print "after (= x (+ x 5)), x = " x "\n\n")

;; arrays (valarrays of Real)
(def a (array 1 2 3))
(def b (array 10 20 30))

(print "a = " a "\n")
(print "b = " b "\n")
(print "a + b = " (+ a b) "\n")
(print "2 * a = " (* (array 2) a) "\n")
(print "size of a = " (size a) "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 2. Introspection (via info)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** introspection **\n\n")

(print "current variables: \n")
(print (info 'vars) "\n\n")

(print "variables starting with 'l': \n")
(print (info 'vars "l.*") "\n\n")

(print "typeof examples:\n")
(print "(info 'typeof '(a b c d)) = " (info 'typeof '(a b c d)) "\n")
(print "(info 'typeof 'a_symbol) = " (info 'typeof 'a_symbol) "\n")
(print "(info 'typeof \"a simple string\") = "
       (info 'typeof "a simple string") "\n")
(print "(info 'typeof 4) = " (info 'typeof 4) "\n")
(print "(info 'typeof a) = " (info 'typeof a) "\n")
(print "(info 'typeof map) = " (info 'typeof map) "\n")
(print "(info 'typeof +) = " (info 'typeof +) "\n\n")

(print "does 'alpha exist? " (info 'exists 'alpha) "\n\n")
(def alpha 10)
(print "does 'alpha exist now? " (info 'exists 'alpha) "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 3. Functions, lexical scope, and closures
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** functions & lexical scope **\n\n")

;; simple function (idiomatic: use function macro)
(function add3 (z)
  (+ z 3))

(print "(add3 10) = " (add3 10) "\n\n")

(print "** scopes are local to lambdas **\n")
(print "define alpha and beta inside it\n\n")

(def alpha
  (lambda (x)
    (def beta
      (lambda (x)
        (print "beta sees x = " x "\n")))
    (print "call beta from alpha: \n")
    (beta 4)
    (print "alpha returns x + 1 = " (+ x 1) "\n")
    (+ x 1)))

(print "(alpha 5) -> " (alpha 5) "\n")
(print "does beta exist here? "
       (info 'exists 'beta) "\n\n")

(print "** lexical closure example **\n\n")

(def outer-val 5)

(def foo
  (lambda ()
    (def inner (+ outer-val 5))
    inner))

(def bar
  (lambda ()
    (def outer-val 100)
    (foo)))   ;; foo still sees outer outer-val = 5 (lexical binding)

(print "outer-val = " outer-val "\n")
(print "foo() = " (foo) "\n")
(print "bar() = " (bar) "\n\n")

(print "** closures **\n\n")

(def count-down-from
  (lambda (n)
    (lambda ()
      (= n (- n 1))
      n)))  ;; return updated n

(def count-down-from-4 (count-down-from 4))

(print "count-down-from-4 stores its status:\n")
(print (count-down-from-4) "\n")
(print (count-down-from-4) "\n")
(print (count-down-from-4) "\n\n")

(def x 42)

(def make-printer
  (lambda ()
    (lambda ()
      (print "inner sees x = " x "\n"))))

(def p (make-printer))

(print "before changing x, calling p:\n")
(p)

(= x 99)
(print "after changing x globally, calling p again:\n")
(p)

(print "\nparameter shadowing example:\n")

(def x 10)

(def show-shadow
  (lambda (x)
    (print "inside show-shadow, x = " x "\n")
    (+ x 1)))

(print "global x = " x "\n")
(print "(show-shadow 5) = " (show-shadow 5) "\n")
(print "global x still = " x "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 4. Conditionals and loops
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** conditionals and loops **\n\n")

(def v 10)

(print "start from v = " v "\n")

;; simple if
(if (> v 5)
    (begin
      (= v (+ v 10))
      (print "v is now " v " (because it was > 5)\n"))
    (begin
      (print "this branch is not executed\n")))

(print "\nwhile loop with break at 5:\n")

(while (> v 0)
  (begin
    (= v (- v 1))
    (if (== v 5)
        (begin
          (print "here we stop at " v "\n")
          (break))  ;; uses BreakException internally
        (print v "\n"))))

(print "\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 5. Recursion
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** recursion **\n\n")

(function fact (n)
  (if (<= n 1)
      1
      (* n (fact (- n 1)))))

(print "fact(5) = " (fact 5) "\n\n")

(function bad_fib (x)
  (if (<= x 1)
      1
      (+ (bad_fib (- x 1))
         (bad_fib (- x 2)))))

(print "bad_fib(10) = " (bad_fib 10) "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 6. Lists & higher functions from stdlib
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** lists & higher functions (stdlib) **\n\n")

(def L '(1 2 3 4 5))

(print "L = " L "\n")
(print "car L = " (car L) "\n")
(print "cdr L = " (cdr L) "\n")
(print "second L = " (second L) "\n")
(print "third L = " (third L) "\n")
(print "fourth L = " (fourth L) "\n")
(print "lhead L = " (lhead L) "\n")
(print "llast L = " (llast L) "\n")
(print "ltail L = " (ltail L) "\n\n")

(print "ltake L 3 = " (ltake L 3) "\n")
(print "ldrop L 3 = " (ldrop L 3) "\n")
(print "lsplit L 2 = " (lsplit L 2) "\n")
(print "lreverse L = " (lreverse L) "\n\n")

(print "match 2 in '(1 2 3 2 4) = " (match 2 '(1 2 3 2 4)) "\n")
(print "elem 2 in L = " (elem 2 L) "\n")
(print "elem 99 in L = " (elem 99 L) "\n\n")

(print "zip '(1 2) '(10 20) = " (zip '(1 2) '(10 20)) "\n")
(print "dup 3 'x = " (dup 3 'x) "\n\n")

(print "map square over L: " (map square L) "\n")
(print "map2 + '(1 2) '(10 20) = " (map2 + '(1 2) '(10 20)) "\n")
(print "filter (> x 2) L = "
       (filter (lambda (x) (> x 2)) L) "\n")

(print "foldl + 0 L = " (foldl + 0 L) "\n")
(print "flip - 3 10 = " (flip - 3 10) "\n")
(print "comp square succ 3 = " (comp square succ 3) "\n\n")

(def L2 '(-2 0 3))
(print "L2 = " L2 "\n")
(print "sign L2 = " (sign L2) "\n")
(print "compare < '(3 1 2) = " (compare < '(3 1 2)) "   (min)\n")
(print "compare > '(3 1 2) = " (compare > '(3 1 2)) "   (max)\n")
(print "(. + '(1 2) '(10 20)) = " (. + '(1 2) '(10 20)) "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 7. Arrays and numeric utilities
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** arrays & numeric utilities **\n\n")

(def sig (array 1 2 3 4 5))

(print "sig = " sig "\n")
(print "mean(sig) = " (mean sig) "\n")
(print "stddev(sig) = " (stddev sig) "\n")
(print "standard(sig) = " (standard sig) "\n")
(print "normal(sig) = " (normal sig) "\n")
(print "dot(sig, sig) = " (dot sig sig) "\n")
(print "norm(sig) = " (norm sig) "\n")
(print "diff(sig) = " (diff sig) "\n\n")

(print "sin over (array 0 TWOPI) = " (sin (array 0 TWOPI)) "\n")
(print "cos over (array 0 TWOPI) = " (cos (array 0 TWOPI)) "\n")
(print "slice sig 1 3 = " (slice sig 1 3) "\n")

(def sig2 (array 0 0 0 0 0))
(assign sig2 (array 9 9) 2 2)
(print "assign (array 9 9) into sig2 at 2 len 2 => " sig2 "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 8. Macros: simple examples (inc!, function, when, unless, let)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** macros **\n\n")

;; local macro: increment a variable in-place
(def inc!
  (macro (var)
    (list '= var (list '+ var 1))))

(def m 0)
(print "m = " m "\n")
(inc! m)
(print "after (inc! m), m = " m "\n")
(inc! m)
(print "after (inc! m) again, m = " m "\n\n")

;; function macro (already in stdlib)
(function add2 (x)
  (+ x (array 2)))

(print "add2 10 => " (add2 (array 10)) "\n\n")

;; when / unless macros
(def x (array 5))

(when (> x (array 0))
  (begin
    (print "x is positive\n")
    (print "and we are in a when\n")))

(unless (> x (array 10))
  (print "x is not greater than 10\n"))

(print "\nlet examples:\n")

;; simple let
(print "(let ((x [1]) (y [2])) (+ x y)) => "
       (let ((x (array 1))
             (y (array 2)))
         (+ x y))
       "\n")

;; multiple forms via begin
(print "(let ((x [10])) ...) with begin => "
       (let ((x (array 10)))
         (begin
           (print "inside let, x = " x "\n")
           (+ x (array 5))))
       "\n")

;; nested let
(print "nested let => "
       (let ((x (array 1)))
         (let ((y (array 2)))
           (+ x y)))
       "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 9. Partial application (automatic currying)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "** partial application / currying **\n\n")

(function two-params (x y)
  (+ x y))

(print "(two-params 2 3) = " (two-params 2 3) "\n")

(def add2c (two-params 2))  ;; partially applied
(print "add2c is (two-params 2), (add2c 5) = " (add2c 5) "\n\n")

(function sum3 (a b c)
  (+ (+ a b) c))

(print "sum3 1 2 3 = " (sum3 1 2 3) "\n")

(def add1 (sum3 1))
(print "(add1 2 3) = " (add1 2 3) "\n")

(def add1and2 (sum3 1 2))
(print "(add1and2 3) = " (add1and2 3) "\n")

(print "(((sum3 1) 2) 3) = " (((sum3 1) 2) 3) "\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; End
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "that's all folks!\n")
