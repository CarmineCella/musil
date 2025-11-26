;; basic.scm

(def a [1 2 3 4])

(def add2 (lambda (x) {
    (= x (+ x 2))
    (print "-> " x "\n")    
}))


(add2 a)


(if 1 {
    2
} (3))