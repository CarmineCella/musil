;; --------------------------------
;; Musil reader tests
;; --------------------------------

;; simple test harness (independent from core_test.scm)
(def total (array 0))
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
          (print "ALL TESTS PASSED\n")
          (print "SOME TESTS FAILED\n")))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; basic list / quote reading
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; empty list
(test '() '())

;; simple list
(test '(quote (1 2 3)) (list 1 2 3))
(test '(list 1 2 3) '(1 2 3))

;; nested lists
(test '(== '(1 (2 3) (4 (5))) '(1 (2 3) (4 (5)))) (array 1))

;; quoting symbols and lists with '
(test '(== 'x (quote x)) (array 1))
(test '(== '(1 2 3) (quote (1 2 3))) (array 1))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; whitespace and newlines
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; extra spaces and newlines should not matter
(test '(list
         1
         
         2
         3)
      '(1 2 3))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; comments (; ...) and line counting
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; comment inside form should be ignored
(test '(begin
          1      ; this should be ignored by the reader
          2)
      (array 2))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; string reading and escapes
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; simple string
(test '(str 'length "hello") (array 5))

;; string with spaces
(test '(str 'length "hello world") (array 11))

;; newline escape \n
(test '(str 'length "a\nb") (array 3))

;; quote escape \" inside string
(test '(str 'length "a\"b") (array 3))

;; backslash escape \\ inside string
(test '(str 'length "a\\b") (array 3))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; mixture of data: list of strings, symbols, numbers
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(quote (foo "bar" 42))
      '(foo "bar" 42))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; end report
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(report)

;; eof
