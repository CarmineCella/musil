;; --------------------------------
;; Musil reader tests
;; --------------------------------

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
;; basic list / quote reading
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; empty list
(test '() '())

;; simple list
(test '(quote (1 2 3)) (list 1 2 3))
(test '(list 1 2 3)    '(1 2 3))

;; nested lists
(test '(== '(1 (2 3) (4 (5))) '(1 (2 3) (4 (5)))) [1])

;; quoting symbols and lists with '
(test '(== 'x (quote x))              [1])
(test '(== '(1 2 3) (quote (1 2 3)))  [1])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; {} blocks and [] arrays – reader behavior
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; {} should read as a (begin ...) form
(test '(== '{ 1 2 3 } '(begin 1 2 3)) [1])

;; while evaluating, {} should behave like begin: last form is the value
(test '{ 1 2 3 } [3])

(test '(== '[1 2 3] '(array 1 2 3)) [1])
(test '(== [1 2 3] (array 1 2 3)) [1])

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

;; comment inside form should be ignored by the reader
(test '{ 
         1      ; this should be ignored by the reader
         2
       }
      [2])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; string reading and escapes
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; simple string
(test '(str 'length "hello") [5])

;; string with spaces
(test '(str 'length "hello world") [11])

;; newline escape \n
(test '(str 'length "a\nb") [3])

;; quote escape \" inside string
(test '(str 'length "a\"b") [3])

;; backslash escape \\ inside string
(test '(str 'length "a\\b") [3])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; mixture of data: list of strings, symbols, numbers
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test '(quote (foo "bar" 42))
      '(foo "bar" 42))
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; CSV read/write tests (list of lists)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Small pure-numeric table
(def CSV_TBL_NUM
  (list
    (list 1 2 3)
    (list 4 5 6)))

;; Numeric + string table
(def CSV_TBL_MIX
  (list
    (list 1     "foo"  3.5)
    (list 2     "bar"  7.0)
    (list -1.25 "baz"  0)))

;; Helper: write then read back
(def csv_roundtrip
  (lambda (fname table)
    {
      (writecsv fname table)
      (readcsv fname)
    }))

;; Pure numeric round-trip
(test (quote (csv_roundtrip "tmp_csv_num.csv" CSV_TBL_NUM))
      CSV_TBL_NUM)

;; Mixed round-trip
(test (quote (csv_roundtrip "tmp_csv_mix.csv" CSV_TBL_MIX))
      CSV_TBL_MIX)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; end report
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(report)

;; eof
