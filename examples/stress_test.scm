(def loop (lambda (n)
  (if (> n 0) (loop (- n 1)))))


(def tic (clock))
(loop 1000000)
(def toc (clock))
(print (- toc tic) "\n")