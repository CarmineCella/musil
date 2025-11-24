;; ----------------------
;; Musil standard library
;; ----------------------

;; ------------------------------------------------------------
;; Macros layer
;; ------------------------------------------------------------

;; schedule macro:
;; (schedule (lambda () ...) delay)
;; expands to:
;;   (%schedule <that-lambda> delay)
(def schedule
  (macro (thunk delay)
    (list '%schedule thunk delay)))
          
;; function macro:
;; (function name (args...) body)
;; expands to:
;;   (def name (lambda (args...) body))
;;
;; body is a *single* expression; for multiple forms use (begin ...)
(def function
  (macro (name args body)
    (list 'def
          name
          (list 'lambda args body))))


;; when macro:
;; (when cond body)
;; expands to:
;;   (if cond body '())
;;
;; body is a single expression (often (begin ...)).
(def when
  (macro (cond body)
    (list 'if
          cond
          body
          (list 'quote '()))))


;; unless macro:
;; (unless cond body)
;; expands to:
;;   (if cond '() body)
(def unless
  (macro (cond body)
    (list 'if
          cond
          (list 'quote '())
          body)))

;; ------------------------------------------------------------
;; let macro
;; (let ((x 1) (y 2) ...) body)
;;   => ((lambda (x y ...) body) 1 2 ...)
;; body is a single expression (use (begin ...) for multiple)
;; ------------------------------------------------------------

(def let
  (macro (bindings body)
    (begin
      ;; collect variable names
      (def vars '())
      (def n (llength bindings))
      (def i (array 0))

      (while (< i n)
        (begin
          (def b (lindex bindings i))      ;; b = (x expr)
          (= vars (lappend vars (lindex b (array 0))))
          (= i (+ i (array 1)))))

      ;; build lambda: (lambda (vars...) body)
      (def lam (list 'lambda vars body))

      ;; start call as: ( (lambda ...) )
      (def call (list lam))

      ;; append each value expr to call
      (= i (array 0))
      (while (< i n)
        (begin
          (def b (lindex bindings i))          ;; b = (x expr)
          (= call (lappend call (lindex b (array 1))))
          (= i (+ i (array 1)))))

      call)))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Booleans
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def true  1)
(def false 0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Basic logic
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; eq: structural equality, wraps built-in ==
(function eq (a b)
  (== a b))

(function not (x)
  (if x false true))

(function and (x y)
  (if x
      (if y true false)
      false))

(function or (x y)
  (if x x (if y y false)))

(function <> (n1 n2)
  (not (eq n1 n2)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Lists
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(function car (l)
  (lindex l 0))

(function cdr (l)
  (lrange l 1 (- (llength l) 1)))

(function second (l)
  (lindex l 1))

(function third (l)
  (lindex l 2))

(function fourth (l)
  (lindex l 3))

(function lhead (l)
  (lappend '() (car l)))

(function llast (l)
  (lindex l (- (llength l) 1)))

(function ltail (l)
  (lappend '() (llast l)))

(function ltake (l n)
  (begin
    (def ltake-runner
      (lambda (acc l n)
        (if (<= n 0)
            acc
            (begin
              (if (eq (car l) '()) acc (lappend acc (car l)))
              (ltake-runner acc (cdr l) (- n 1))))))
    (ltake-runner '() l n)))

(function ldrop (l n)
  (if (<= n 0)
      l
      (ldrop (cdr l) (- n 1))))

(function lsplit (l n)
  (list (ltake l n) (ldrop l n)))

;; iterative reverse
(function lreverse (l)
  (begin
    (def res '())
    (def i (- (llength l) 1))
    (while (>= i 0)
      (begin
        (lappend res (lindex l i))
        (= i (- i 1))))
    res))

(function match (e l)
  (begin
    (def match-runner
      (lambda (acc n e l)
        (if (eq l '())
            acc
            (begin
              (if (eq (car l) e) (lappend acc n) acc)
              (match-runner acc (+ n 1) e (cdr l))))))
    (match-runner '() 0 e l)))

(function elem (x l)
  (if (eq (llength (match x l)) 0)
      false
      true))

(function zip (l1 l2)
  (map2 list l1 l2))

(function dup (n x)
  (begin
    (def dup-runner
      (lambda (acc n x)
        (if (<= n 0)
            acc
            (begin
              (lappend acc x)
              (dup-runner acc (- n 1) x)))))
    (dup-runner '() n x)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Higher-order operators
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(function map (f l)
  (begin
    (def map-runner
      (lambda (acc f l)
        (if (eq l '())
            acc
            (begin
              (lappend acc (f (car l)))
              (map-runner acc f (cdr l))))))
    (map-runner '() f l)))

(function map2 (f l1 l2)
  (begin
    (def map2-runner
      (lambda (acc f l1 l2)
        (if (or (eq l1 '()) (eq l2 '()))
            acc
            (begin
              (lappend acc (f (car l1) (car l2)))
              (map2-runner acc f (cdr l1) (cdr l2))))))
    (map2-runner '() f l1 l2)))

(function filter (f l)
  (begin
    (def filter-runner
      (lambda (acc f l)
        (if (eq l '())
            acc
            (begin
              (if (f (car l)) (lappend acc (car l)) acc)
              (filter-runner acc f (cdr l))))))
    (filter-runner '() f l)))

(function foldl (f z l)
  (if (eq l '())
      z
      (foldl f (f z (car l)) (cdr l))))

(function flip (f a b)
  (f b a))

(function comp (f g x)
  (f (g x)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Arithmetic helpers (on arrays/scalars)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(function getval (x n)
  (slice x n 1))

(function setval (x v n)
  (assign x v n 1))

(function succ (x)
  (+ x 1))

(function pred (x)
  (- x 1))

(function quotient (a b)
  (floor (/ a b)))

(function remainder (a b)
  (floor (- a (* b (quotient a b)))))

(function mod (a b)
  (floor (- a (* b (quotient a b)))))

(function twice (x)
  (+ x x))

(function square (x)
  (* x x))

(function round (x)
  (floor (+ x 0.5)))

;; mean of an array
(function mean (x)
  (/ (sum x) (size x)))

(function stddev (x)
  (begin
    (def mu (mean x))
    (def normal (/ 1. (- (size x) 1)))
    (sqrt (* (sum (square (- x mu))) normal))))

(function standard (x)
  (begin
    (def mu (mean x))
    (def s (stddev x))
    (/ (- x mu) s)))

(function normal (x)
  (/ x (max x)))

(function dot (a b)
  (sum (* a b)))

(function ortho (a b)
  (eq (dot a b) 0))

(function norm (x)
  (sqrt (sum (* x x))))

(function diff (x)
  (- (slice x 1 (size x)) x))

(function fac (x)
  (begin
    (def fact-worker
      (lambda (a product)
        (if (eq a 0)
            product
            (fact-worker (- a 1) (* product a)))))
    (fact-worker x 1)))

(function fib (n)
  (begin
    (def fib-worker
      (lambda (a b count)
        (if (<= count 1)
            b
            (fib-worker (+ a b) a (- count 1)))))
    (fib-worker 1 1 n)))

(function ack (m n)
  (if (eq m 0)
      (+ n 1)
      (if (eq n 0)
          (ack (- m 1) 1)
          (ack (- m 1) (ack m (- n 1))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; List-based numeric operators
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(function sign (l)
  (begin
    (def sign-runner
      (lambda (acc l)
        (if (eq l '())
            acc
            (begin
              (if (>= (car l) 0)
                  (lappend acc 1)
                  (lappend acc -1))
              (sign-runner acc (cdr l))))))
    (sign-runner '() l)))

(function compare (op l)
  (begin
    (def cmp-runner
      (lambda (n l)
        (if (eq l '())
            n
            (if (op (car l) n)
                (begin
                  (= n (car l))
                  (cmp-runner n (cdr l)))
                (cmp-runner n (cdr l))))))
    (cmp-runner (car l) l)))

(function . (f l1 l2)
  (map2 (lambda (x y) (f x y)) l1 l2))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Constants
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def TWOPI 6.2831853072)
(def SQRT2 1.4142135624)
(def LOG2  0.3010299957)
(def E     2.7182818284)

true

;; eof
