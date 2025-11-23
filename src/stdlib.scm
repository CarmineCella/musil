;; ----------------------
;; Musil standard library
;; ----------------------

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Booleans
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def true  1)
(def false 0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Basic logic
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; eq: structural equality, wraps built-in ==
(def eq
  (lambda (a b)
    (== a b)))

(def not
  (lambda (x)
    (if x false true)))

(def and
  (lambda (x y)
    (if x
        (if y true false)
        false)))

(def or
  (lambda (x y)
    (if x x (if y y false))))

(def <>
  (lambda (n1 n2)
    (not (eq n1 n2))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Lists
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def car
  (lambda (l)
    (lindex l 0)))

(def cdr
  (lambda (l)
    (lrange l 1 (- (llength l) 1))))

(def second
  (lambda (l)
    (lindex l 1)))

(def third
  (lambda (l)
    (lindex l 2)))

(def fourth
  (lambda (l)
    (lindex l 3)))

(def lhead
  (lambda (l)
    (lappend '() (car l))))

(def llast
  (lambda (l)
    (lindex l (- (llength l) 1))))

(def ltail
  (lambda (l)
    (lappend '() (llast l))))

(def ltake
  (lambda (l n)
    (def ltake-runner
      (lambda (acc l n)
        (if (<= n 0)
            acc
            (begin
              (if (eq (car l) '()) acc (lappend acc (car l)))
              (ltake-runner acc (cdr l) (- n 1))))))
    (ltake-runner '() l n)))

(def ldrop
  (lambda (l n)
    (if (<= n 0)
        l
        (ldrop (cdr l) (- n 1)))))

(def lsplit
  (lambda (l n)
    (list (ltake l n) (ldrop l n))))

;; iterative reverse
(def lreverse
  (lambda (l)
    (def res '())
    (def i (- (llength l) 1))
    (while (>= i 0)
      (begin
        (lappend res (lindex l i))
        (= i (- i 1))))
    res))

(def match
  (lambda (e l)
    (def match-runner
      (lambda (acc n e l)
        (if (eq l '())
            acc
            (begin
              (if (eq (car l) e) (lappend acc n) acc)
              (match-runner acc (+ n 1) e (cdr l))))))
    (match-runner '() 0 e l)))

(def elem
  (lambda (x l)
    (if (eq (llength (match x l)) 0)
        false
        true)))

(def zip
  (lambda (l1 l2)
    (map2 list l1 l2)))

(def dup
  (lambda (n x)
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

(def map
  (lambda (f l)
    (def map-runner
      (lambda (acc f l)
        (if (eq l '())
            acc
            (begin
              (lappend acc (f (car l)))
              (map-runner acc f (cdr l))))))
    (map-runner '() f l)))

(def map2
  (lambda (f l1 l2)
    (def map2-runner
      (lambda (acc f l1 l2)
        (if (or (eq l1 '()) (eq l2 '()))
            acc
            (begin
              (lappend acc (f (car l1) (car l2)))
              (map2-runner acc f (cdr l1) (cdr l2))))))
    (map2-runner '() f l1 l2)))

(def filter
  (lambda (f l)
    (def filter-runner
      (lambda (acc f l)
        (if (eq l '())
            acc
            (begin
              (if (f (car l)) (lappend acc (car l)) acc)
              (filter-runner acc f (cdr l))))))
    (filter-runner '() f l)))

(def foldl
  (lambda (f z l)
    (if (eq l '())
        z
        (foldl f (f z (car l)) (cdr l)))))

(def flip
  (lambda (f a b)
    (f b a)))

(def comp
  (lambda (f g x)
    (f (g x))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Arithmetic helpers (on arrays/scalars)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def getval
  (lambda (x n)
    (slice x n 1)))

(def setval
  (lambda (x v n)
    (assign x v n 1)))

(def succ
  (lambda (x)
    (+ x 1)))

(def pred
  (lambda (x)
    (- x 1)))

(def quotient
  (lambda (a b)
    (floor (/ a b))))

(def remainder
  (lambda (a b)
    (floor (- a (* b (quotient a b))))))

(def mod
  (lambda (a b)
    (floor (- a (* b (quotient a b))))))

(def twice
  (lambda (x)
    (+ x x)))

(def square
  (lambda (x)
    (* x x)))

(def round
  (lambda (x)
    (floor (+ x 0.5))))

;; mean of an array
(def mean
  (lambda (x)
    (/ (sum x) (size x))))

(def stddev
  (lambda (x)
    (def mu (mean x))
    (def normal (/ 1. (- (size x) 1)))
    (sqrt (* (sum (square (- x mu))) normal))))

(def standard
  (lambda (x)
    (def mu (mean x))
    (def s (stddev x))
    (/ (- x mu) s)))

(def normal
  (lambda (x)
    (/ x (max x))))

(def dot
  (lambda (a b)
    (sum (* a b))))

(def ortho
  (lambda (a b)
    (eq (dot a b) 0)))

(def norm
  (lambda (x)
    (sqrt (sum (* x x)))))

(def diff
  (lambda (x)
    (- (slice x 1 (size x)) x)))

(def fac
  (lambda (x)
    (def fact-worker
      (lambda (a product)
        (if (eq a 0)
            product
            (fact-worker (- a 1) (* product a)))))
    (fact-worker x 1)))

(def fib
  (lambda (n)
    (def fib-worker
      (lambda (a b count)
        (if (<= count 1)
            b
            (fib-worker (+ a b) a (- count 1)))))
    (fib-worker 1 1 n)))

(def ack
  (lambda (m n)
    (if (eq m 0)
        (+ n 1)
        (if (eq n 0)
            (ack (- m 1) 1)
            (ack (- m 1) (ack m (- n 1)))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; List-based numeric operators
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def sign
  (lambda (l)
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

(def compare
  (lambda (op l)
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

(def .
  (lambda (f l1 l2)
    (map2 (lambda (x y) (f x y)) l1 l2)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Constants
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def TWOPI 6.2831853072)
(def SQRT2 1.4142135624)
(def LOG2  0.3010299957)
(def E     2.7182818284)

true

;; eof
