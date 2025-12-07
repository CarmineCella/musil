;; --------------------------------
;; Iris KNN classification (Musil)
;; --------------------------------

(load "stdlib.scm")
(load "scientific.scm")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 1) Load CSV and build dataset
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Assumption: (readcsv path) exists and returns a LIST of ROWS,
;; each ROW is a LIST of fields already parsed to numbers/strings.
;; iris.data.txt has 5 columns: 4 numeric + 1 label string.

(def IRIS_RAW (readcsv "data/iris.data.txt"))

;; Filter out empty rows (some Iris files have a blank line at end)
(def nonempty_rows
  (lambda (rows)
    (lfilter (lambda (row)
              (> (llength row) 0))
            rows)))

(def IRIS_ROWS (nonempty_rows IRIS_RAW))

;; Convert each row to (features label)
;; row = [f1 f2 f3 f4 label]
(def row2sample
  (lambda (row)
    {
      (def f1 (lindex row (array 0)))
      (def f2 (lindex row (array 1)))
      (def f3 (lindex row (array 2)))
      (def f4 (lindex row (array 3)))
      (def label (lindex row (array 4)))
      (list (array f1 f2 f3 f4) label)
    }))

(def IRIS_DATA (map row2sample IRIS_ROWS))

;; Optionally shuffle
(def DATA (lshuffle IRIS_DATA))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 2) Dataset info
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def samples  (llength DATA))
(def features (size (car (car DATA))))

(print samples " samples - " features " features\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 3) Train/Test split (80% / 20%)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def trainsz (floor (* 80 (/ samples 100))))
(def testsz  (- samples trainsz))

;; lrange(list start count)
(def trainset (lrange DATA 0      trainsz))
(def testset  (lrange DATA trainsz testsz))

(print "split: "
       (llength trainset) " train samples, "
       (llength testset)  " test samples\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 4) Train KNN
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "training...\n")
(def K 3)
(def model (knntrain trainset K))
(print "done\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 5) Test KNN
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(print "testing...\n")
(def classes (knntest model testset))
(print "done\n\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 6) Accuracy
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def acc (accuracy classes testset))

(print "accuracy = " acc "\n")
(print "accuracy (percent) = " (* 100 acc) " %\n")

;; eof
