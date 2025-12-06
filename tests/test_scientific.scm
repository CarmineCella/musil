;; --------------------------------
;; Musil scientific tests
;; --------------------------------

(load "stdlib.scm")

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

;; Approximate test *for scalars only*:
;; (test_approx '(expr) expected eps)
(def test_approx
  (lambda (expr expected eps)
    {
      (= total (+ total [1]))
      (def value (eval expr))
      (def diff (abs (- value expected)))
      (def ok (< diff eps))
      (if (== ok [1])
          (print "PASS≈: " expr "\n")
          {
            (= failed (+ failed [1]))
            (print "FAIL≈: " expr " => " value ", expected " expected " with eps " eps "\n")
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

;; Scalar epsilon for approx tests
(def EPS 1e-06)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Test data
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; 2x2 test matrix:
;; [1 2]
;; [3 4]
(def M2
  (list (array 1 2)
        (array 3 4)))

;; Another 2x2 matrix for binary ops:
;; [5 6]
;; [7 8]
(def M2b
  (list (array 5 6)
        (array 7 8)))

;; Alias for simple 2x2 examples
(def Mtest M2)

;; 3x2 matrix for PCA and other tests:
;; [1 2]
;; [3 4]
;; [5 6]
(def M3x2
  (list (array 1 2)
        (array 3 4)
        (array 5 6)))

;; K-means data: 6 points in 2D, roughly 2 clusters
(def KM_DATA
  (list (array 0.0  0.0)
        (array 0.1  0.1)
        (array -0.1 -0.1)
        (array 5.0  5.0)
        (array 5.1  5.1)
        (array 4.9  4.9)))

;; KNN training set: (features label)
(def KNN_TRAIN
  (list
    (list (array 0.0  0.0)  "A")
    (list (array 0.2  0.1)  "A")
    (list (array 5.0  5.0)  "B")
    (list (array 5.1  4.9)  "B")))

;; KNN query points
(def KNN_QUERY
  (list
    (array 0.05 0.05)
    (array 5.05 5.00)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Basic matrix shape / transpose / row / col
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test (quote (nrows M2)) 2)
(test (quote (ncols M2)) 2)

(test (quote (transpose M2))
      (list (array 1 3)
            (array 2 4)))

;; row / col helpers
(test (quote (row M2 0))
      (list (array 1 2)))

(test (quote (row M2 1))
      (list (array 3 4)))

(test (quote (col M2 0))
      (list (array 1)
            (array 3)))

(test (quote (col M2 1))
      (list (array 2)
            (array 4)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; matmul / matadd / matsub / hadamard / matdisp
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; M2 * M2b =
;; [1 2; 3 4] * [5 6; 7 8] =
;; [19 22; 43 50]
(test (quote (matmul M2 M2b))
      (list (array 19 22)
            (array 43 50)))

;; M2 * I = M2
(test (quote (matmul M2 (eye (array 2))))
      M2)

;; matadd
;; [1 2; 3 4] + [5 6; 7 8] = [6 8; 10 12]
(test (quote (matadd M2 M2b))
      (list (array 6 8)
            (array 10 12)))

;; matsub
;; [1 2; 3 4] - [5 6; 7 8] = [-4 -4; -4 -4]
(test (quote (matsub M2 M2b))
      (list (array -4 -4)
            (array -4 -4)))

;; hadamard (element-wise)
;; [1*5 2*6; 3*7 4*8] = [5 12; 21 32]
(test (quote (hadamard M2 M2b))
      (list (array 5 12)
            (array 21 32)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; matsum / getrows / getcols
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; matsum axis 0: sum over rows -> (3 7) as a LIST of scalars
(test (quote (matsum M2 (array 0)))
      (list 3 7))

;; matsum axis 1: sum over cols -> ([4 6]) as LIST containing one ARRAY
(test (quote (matsum M2 (array 1)))
      (list (array 4 6)))

;; getrows: row 0 only
(test (quote (getrows M2 (array 0) (array 0)))
      (list (array 1 2)))

;; getcols: column 1 only -> [[2],[4]]
(test (quote (getcols M2 (array 1) (array 1)))
      (list (array 2)
            (array 4)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; eye / zeros / ones / det / inv / diag / rank / solve
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; eye
(test (quote (eye (array 3)))
      (list (array 1 0 0)
            (array 0 1 0)
            (array 0 0 1)))

;; zeros / ones 1D
(test (quote (zeros (array 4)))
      (array 0 0 0 0))

(test (quote (ones (array 3)))
      (array 1 1 1))

;; zeros / ones 2D: rows x len
(def Z2D_EXPECT
  (list (array 0 0 0)
        (array 0 0 0)))

(def O2D_EXPECT
  (list (array 1 1 1)
        (array 1 1 1)))

(test (quote (zeros (array 3) (array 2))) Z2D_EXPECT)
(test (quote (ones  (array 3) (array 2))) O2D_EXPECT)

;; Scheme wrappers zerosvec / zerosmat / onesvec / onesmat
(test (quote (zerosvec (array 4)))
      (array 0 0 0 0))

(test (quote (onesvec (array 2)))
      (array 1 1))

(test (quote (zerosmat (array 2) (array 2)))
      (list (array 0 0)
            (array 0 0)))

(test (quote (onesmat (array 2) (array 3)))
      (list (array 1 1 1)
            (array 1 1 1)))

;; det [1 2; 3 4] = -2
(test (quote (det M2)) -2)

;; Property test: det(M2) * det(inv(M2)) ≈ 1
(test_approx (quote (* (det M2) (det (inv M2))))
             1
             EPS)

;; diag on ARRAY -> diagonal matrix
(test (quote (diag (array 1 2 3)))
      (list (array 1 0 0)
            (array 0 2 0)
            (array 0 0 3)))

;; diag on matrix -> ARRAY of diagonal entries
(test (quote (diag M2))
      (array 1 4))

;; rank of linearly dependent rows -> 1
(test (quote (rank (list (array 1 2)
                         (array 2 4))))
      1)

;; Solve M2 * x = [5 11]^T   -> x = [1 2]^T
(def SOL (solve M2 (array 5 11)))

;; Here SOL is an ARRAY, so compare whole array directly
(test (quote SOL)
      (array 1 2))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Statistics: median / linefit / norm / dist / matmean / matstd / cov / zscore
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; median: just check it returns one array inside a list in this construction
(test (quote (llength (list (median (array 1 100 3 4) (array 3)))))
      1)

;; Explicit median result: [50.5 3 4 3.5]
(def MED (median (array 1 100 3 4) (array 3)))
(test (quote MED)
      (array 50.5 3 4 3.5))

;; linefit: y = 2x + 1
(def LF (linefit (array 0 1 2 3)
                 (array 1 3 5 7)))

;; LF = [slope intercept] = [2 1]
(test (quote LF)
      (array 2 1))

;; norm tests
(def NV (array 3 4))
(test (quote (norm NV))
      5)            ;; L2

(test (quote (norm NV (array 1)))
      7)            ;; L1 = 3+4

(test (quote (norm (array 0 0 0)))
      0)

;; dist tests
(test (quote (dist (array 1 2 3) (array 1 2 3)))
      0)

(test (quote (dist (array 0 0) (array 3 4)))
      5)           ;; L2 distance

(test (quote (dist (array 1 2) (array 3 0) (array 1)))
      4)           ;; L1 distance: |1-3|+|2-0| = 4

;; matmean along columns (axis 0): [2 3]
(test (quote (matmean M2 (array 0)))
      (array 2 3))

;; matmean along rows (axis 1): [1.5 3.5]
(test (quote (matmean M2 (array 1)))
      (array 1.5 3.5))

;; matstd along columns (axis 0): [1 1]
(test (quote (matstd M2 (array 0)))
      (array 1 1))

;; matstd along rows (axis 1): [0.5 0.5]
(test (quote (matstd M2 (array 1)))
      (array 0.5 0.5))

;; cov, rows as observations, cols as variables:
;; for M2 -> [[2 2],[2 2]]
(test (quote (cov M2))
      (list (array 2 2)
            (array 2 2)))

;; zscore(M2) with column means [2 3] and std [1 1] -> [[-1 -1],[1 1]]
(test (quote (zscore M2))
      (list (array -1 -1)
            (array  1  1)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; corr (structural properties)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; diag(corr(M2)) = [1 1]
(test (quote (diag (corr M2)))
      (array 1 1))

;; corr(M2) is symmetric: corr = transpose(corr)
(test (quote (corr M2))
      (transpose (corr M2)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; stack2 / hstack / vstack / matcol
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; matcol tests
(def MC_M
  (list
    (array 1 2 3)
    (array 4 5 6)
    (array 7 8 9)))

(test (quote (matcol MC_M (array 0))) (array 1 4 7))
(test (quote (matcol MC_M (array 1))) (array 2 5 8))
(test (quote (matcol MC_M (array 2))) (array 3 6 9))

;; stack2 tests
(def ST_X (array 10 20 30))
(def ST_Y (array  1  2  3))

(def ST_EXPECT
  (list
    (array 10 1)
    (array 20 2)
    (array 30 3)))

(test (quote (stack2 ST_X ST_Y)) ST_EXPECT)

;; hstack and vstack tests
(def HS_A
  (list
    (array 1 2)
    (array 3 4)))

(def HS_B
  (list
    (array 5 6)
    (array 7 8)))

(def HS_EXPECT
  (list
    (array 1 2 5 6)
    (array 3 4 7 8)))

(def VS_EXPECT
  (list
    (array 1 2)
    (array 3 4)
    (array 5 6)
    (array 7 8)))

(test (quote (hstack HS_A HS_B)) HS_EXPECT)
(test (quote (vstack HS_A HS_B)) VS_EXPECT)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PCA core + helpers
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; pca(M3x2) -> cols x (cols+1) = 2 x 3 matrix
(test (quote (nrows (pca M3x2))) 2)
(test (quote (ncols (pca M3x2))) 3)

;; PCA helpers: shapes only
(test (quote (nrows (pca_eigvecs M3x2))) 2)
(test (quote (ncols (pca_eigvecs M3x2))) 2)

;; eigvals: 2 x 1 matrix
(test (quote (nrows (pca_eigvals M3x2))) 2)
(test (quote (ncols (pca_eigvals M3x2))) 1)

;; pca_scores: for M3x2 and k=1 -> 3 x 1
(test (quote (nrows (pca_scores M3x2 (array 1)))) 3)
(test (quote (ncols (pca_scores M3x2 (array 1)))) 1)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Linear regression (Scheme helpers)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Linear regression (Scheme helpers) – shapes + correctness
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Simple linear regression: y = 2x, no intercept
;; X = [[1],[2],[3]], Y = [[2],[4],[6]]

(def LR_X
  (list (array 1)
        (array 2)
        (array 3)))

(def LR_Y
  (list (array 2)
        (array 4)
        (array 6)))

;; Fit
(def LR_BETA (linreg_fit LR_X LR_Y))

;; Shape tests: beta is 1x1 matrix
(test (quote (nrows LR_BETA)) 1)
(test (quote (ncols LR_BETA)) 1)

;; Numeric correctness: beta = 2
;; Represented as a 1x1 matrix => (list (array 2))
(test (quote LR_BETA)
      (list (array 2)))

;; Predictions on training data: X * beta = Y
(test (quote (linreg_predict LR_X LR_BETA))
      LR_Y)

;; New points: X_new = [[4],[5]]
(def LR_X_NEW
  (list (array 4)
        (array 5)))

;; Shape tests for predictions
(test (quote (nrows (linreg_predict LR_X_NEW LR_BETA))) 2)
(test (quote (ncols (linreg_predict LR_X_NEW LR_BETA))) 1)

;; Numeric correctness: y_new = [8, 10]^T
(test (quote (linreg_predict LR_X_NEW LR_BETA))
      (list (array 8)
            (array 10)))

;; Residuals on training data: Y - X*beta = 0
(def LR_RES
  (linreg_residuals LR_X LR_Y LR_BETA))

;; Shape
(test (quote (nrows LR_RES)) 3)
(test (quote (ncols LR_RES)) 1)

;; Numeric correctness: all zeros
(test (quote LR_RES)
      (list (array 0)
            (array 0)
            (array 0)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; K-means + helpers
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def KM_RES (kmeans KM_DATA (array 2)))

;; KM_RES = (labels centroids)
;; centroids matrix: K x m = 2 x 2
(test (quote (nrows (lindex KM_RES (array 1)))) 2)
(test (quote (ncols (lindex KM_RES (array 1)))) 2)

;; labels array: length = number of points = 6
(test (quote (size (lindex KM_RES (array 0)))) 6)

;; helper accessors
(test (quote (size (kmeans_labels (kmeans KM_DATA (array 2)))))
      6)

(test (quote (nrows (kmeans_centroids (kmeans KM_DATA (array 2)))))
      2)

(test (quote (ncols (kmeans_centroids (kmeans KM_DATA (array 2)))))
      2)

;; run_* helpers
(test (quote (size (kmeans_run_labels KM_DATA (array 2))))
      6)

(test (quote (nrows (kmeans_run_centroids KM_DATA (array 2))))
      2)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; KNN + helpers
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def KNN_RES (knn KNN_TRAIN (array 1) KNN_QUERY))

;; knn returns one label per query
(test (quote (llength KNN_RES)) 2)

;; helper knn_predict
(test (quote (llength (knn_predict KNN_TRAIN 1 KNN_QUERY)))
      2)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Random constructors: randvec / randmat (shape only)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; randvec: length n
(def RV (randvec (array 5)))
(test (quote (size RV)) 5)

;; randmat: rows x cols
(def RM (randmat (array 3) (array 2)))
(test (quote (nrows RM)) 3)
(test (quote (ncols RM)) 2)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; bpf tests
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Single segment: 0 → 1 in 4 steps
(def BPF1_EXPECT (array 0 0.25 0.5 0.75))
(test (quote (bpf (array 0) (array 4) (array 1))) BPF1_EXPECT)

;; Two segments:
;;  0 → 1, len=4:   0, 0.25, 0.5, 0.75
;;  1 → 0, len=4:   1, 0.75, 0.5, 0.25
(def BPF2_EXPECT (array 0 0.25 0.5 0.75 1 0.75 0.5 0.25))

(test (quote (bpf (array 0) (array 4) (array 1)
                  (array 4) (array 0)))
      BPF2_EXPECT)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; standardize / cov_from_zscore wrappers
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(test (quote (standardize M2))
      (zscore M2))

(test (quote (cov_from_zscore M2))
      (cov (zscore M2)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Report
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(report)
