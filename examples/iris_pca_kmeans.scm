;; iris_pca_kmeans.scm
;;
;; Example: use CSV + scientific + plotting on the Iris dataset.
;;

(load "stdlib.scm")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 1. Read raw CSV as list-of-rows
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def IRIS_RAW (readcsv "data/iris.data.txt"))

(print "Loaded Iris CSV, number of rows: "
       (llength IRIS_RAW) "\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 2. Build numeric feature matrix (4 columns) using list2mat
;;
;; Each raw row is:
;;   (list sepal_len sepal_wid petal_len petal_wid label)
;; We take the first 4 items (numeric), then convert to matrix.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def IRIS_NUM_ROWS
  (map (lambda (row)
         (ltake row 4))   ;; keep first 4 numeric columns
       IRIS_RAW))

(def IRIS_X (list2mat IRIS_NUM_ROWS))   ;; matrix = list-of-arrays

(def IRIS_ROWS (nrows IRIS_X))
(def IRIS_COLS (ncols IRIS_X))

(print "Iris numeric matrix: " IRIS_ROWS " samples, "
       IRIS_COLS " features\n")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 3. PCA on the 4D features
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def IRIS_PCA (pca IRIS_X))
;; pca returns matrix (cols x (cols+1)), last col = eigenvalues
;; For Iris, cols = 4, so we take column index 4.
(def IRIS_EIG (matcol IRIS_PCA (array 4)))

(plot "Iris PCA eigenvalues"
      IRIS_EIG "eigenvalues"
      "*")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 4. k-means on petal features (length, width) + plotting
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Extract petal length (col 2) and petal width (col 3) as arrays
(def PETAL_LEN (matcol IRIS_X (array 2)))
(def PETAL_WID (matcol IRIS_X (array 3)))

;; Build a [N x 2] matrix with (length, width)
(def PETAL_MAT (stack2 PETAL_LEN PETAL_WID))

;; K-means with K = 3 (three Iris species)
(def K (array 3))
(def KM_RES (kmeans PETAL_MAT K))
(def KM_LABELS    (lindex KM_RES (array 0)))
(def KM_CENTROIDS (lindex KM_RES (array 1)))

;; 4a. Scatter all points as one cloud
(scatter "Iris: petal length vs width"
         PETAL_LEN PETAL_WID "samples"
         ".")

;; 4b. Scatter the centroids as a separate series
(def CENTS   KM_CENTROIDS)           ;; 3×2 matrix
(def CENTS_X (matcol CENTS (array 0)))
(def CENTS_Y (matcol CENTS (array 1)))

(scatter "Iris k-means centroids"
         PETAL_LEN PETAL_WID "samples"
         CENTS_X    CENTS_Y  "centroids"
         ".")

(print "plots saved as SVG\n")

;; eof

