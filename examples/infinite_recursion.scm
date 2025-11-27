;; Example of infinite recursion

(def rec (lambda (x) { (print x "\n") (rec (+ x 1))})) 

(rec 1)

;; eof

