;; -------------------------
;; Musil scientific functions
;; --------------------------
;;

;; ------------------------------------------------------------
;; Basic vector/matrix helpers
;; ------------------------------------------------------------

;; row:
;;   Return a single row of matrix M as a 1 x n matrix.
;;   idx is a scalar row index (0-based).
;;
;; Example:
;;   (row M 0)  -> same as (getrows M (array 0) (array 0))
(def row
  (lambda (M idx)
    (getrows M (array idx) (array idx))))

;; col:
;;   Return a single column of matrix M as an m x 1 matrix.
;;   idx is a scalar column index (0-based).
;;
;; Example:
;;   (col M 1) -> same as (getcols M (array 1) (array 1))
(def col
  (lambda (M idx)
    (getcols M (array idx) (array idx))))

;; Random vector of length n
(def randvec
  (lambda (n)
    (rand n)))

;; Random matrix with given rows and cols
(def randmat
  (lambda (rows cols)
    (rand cols rows)))  ; or (rand cols rows) depending on your row/col convention

;; Matrix aliases (if you want a nicer API)
(def mat+
  (lambda (A B) (matadd A B)))

(def mat-
  (lambda (A B) (matsub A B)))

(def mat*
  (lambda (A B) (matmul A B)))

;; Convert a list-of-lists into a list-of-arrays (matrix)
(def list2mat
  (lambda (rows)
    (map (lambda (row)
           (array row))   ;; row is a list → array flattens it
         rows)))
         
;; ------------------------------------------------------------
;; Linear regression (normal equations)
;; ------------------------------------------------------------
;;
;; Closed-form linear regression:
;;   beta = (X^T X)^(-1) X^T Y
;;
;; We assume:
;;   - X is n x d matrix (rows = samples, cols = features)
;;   - Y is n x 1 matrix (column of targets)
;;   - beta is d x 1 matrix of coefficients
;;

(def linreg_fit
  (lambda (X Y)
    {
      (def Xt        (transpose X))
      (def XtX       (matmul Xt X))
      (def XtX_inv   (inv XtX))
      (def XtX_inv_Xt (matmul XtX_inv Xt))
      ;; d x 1
      (matmul XtX_inv_Xt Y)
    }))

;; linreg_predict:
;;   X_new: n_new x d matrix
;;   beta:  d x 1 matrix (from linreg_fit)
;; returns:
;;   n_new x 1 matrix of predictions
(def linreg_predict
  (lambda (X_new beta)
    (matmul X_new beta)))

;; linreg_residuals:
;;   X, Y as above, beta from linreg_fit
;; returns:
;;   n x 1 matrix of residuals: Y - X*beta
(def linreg_residuals
  (lambda (X Y beta)
    {
      (def Y_hat (linreg_predict X beta))
      (matsub Y Y_hat)
    }))

;; ------------------------------------------------------------
;; PCA helpers
;; ------------------------------------------------------------
;;
;; scientific.h's pca returns a matrix P of size:
;;   d x (d+1)
;; where:
;;   - first d columns are eigenvectors
;;   - last column holds eigenvalues
;;

;; pca_decompose:
;;   X: n x d data matrix (rows = observations)
;; returns:
;;   (list P eigvecs eigvals)
;; where:
;;   P       = raw PCA matrix as returned by (pca X)
;;   eigvecs = d x d matrix of eigenvectors
;;   eigvals = d x 1 matrix (last column of P)
(def pca_decompose
  (lambda (X)
    {
      (def P (pca X))
      (def total_cols (ncols P))
      (def last_col_idx (- total_cols 1))
      ;; eigenvectors: all columns except the last one
      (def eigvecs (getcols P (array 0) (array (- last_col_idx 1))))
      ;; eigenvalues: last column as a d x 1 matrix
      (def eigvals (getcols P (array last_col_idx) (array last_col_idx)))
      (list P eigvecs eigvals)
    }))

;; pca_eigvecs:
;;   Convenience accessor: return eigenvectors d x d.
(def pca_eigvecs
  (lambda (X)
    {
      (def dec (pca_decompose X))
      ;; index 1 in (list P eigvecs eigvals)
      (lindex dec (array 1))
    }))

;; pca_eigvals:
;;   Convenience accessor: return eigenvalues as d x 1 matrix.
(def pca_eigvals
  (lambda (X)
    {
      (def dec (pca_decompose X))
      ;; index 2 in (list P eigvecs eigvals)
      (lindex dec (array 2))
    }))

;; pca_scores:
;;   Project X onto the first k principal components.
;;   X: n x d data matrix
;;   k: scalar number of components (1 <= k <= d)
;; returns:
;;   n x k matrix of projected data
;;
;; We use:
;;   X (n x d) * V_k^T (d x k) = scores (n x k)
;;
;; where V_k is the d x k matrix of first k eigenvectors (as columns).
(def pca_scores
  (lambda (X k)
    {
      (def dec     (pca_decompose X))
      (def eigvecs (lindex dec (array 1)))          ;; d x d
      (def end_idx (- k 1))
      (def V_k (getcols eigvecs (array 0) (array end_idx))) ;; d x k
      (def V_k_T (transpose V_k))
      (matmul X V_k_T)
    }))

;; ------------------------------------------------------------
;; K-means helpers
;; ------------------------------------------------------------
;;
;; Primitive:
;;   (kmeans data (array K))
;; returns:
;;   (list labels centroids)
;; where:
;;   labels    : ARRAY of length n
;;   centroids : K x m matrix as LIST of (array ...)
;;

;; Extract labels from kmeans result
(def kmeans_labels
  (lambda (km_result)
    (lindex km_result (array 0))))

;; Extract centroids matrix from kmeans result
(def kmeans_centroids
  (lambda (km_result)
    (lindex km_result (array 1))))

;; Run kmeans and immediately return labels
(def kmeans_run_labels
  (lambda (data K)
    {
      (def R (kmeans data (array K)))
      (kmeans_labels R)
    }))

;; Run kmeans and immediately return centroids
(def kmeans_run_centroids
  (lambda (data K)
    {
      (def R (kmeans data (array K)))
      (kmeans_centroids R)
    }))

;; ------------------------------------------------------------
;; KNN helpers
;; ------------------------------------------------------------
;;
;; Primitive:
;;   (knn train (array K) queries)
;; returns:
;;   LIST of labels (strings)
;;

;; knn_predict:
;;   TRAIN   : training dataset as required by primitive knn
;;   K       : scalar
;;   QUERIES : list of ARRAY query points
;; returns:
;;   list of labels (one per query)
(def knn_predict
  (lambda (TRAIN K QUERIES)
    (knn TRAIN (array K) QUERIES)))

;; knn_predict_one:
;;   TRAIN : as above
;;   K     : scalar
;;   Q     : single ARRAY query point
;; returns:
;;   a single label (string)
(def knn_predict_one
  (lambda (TRAIN K Q)
    {
      (def labels (knn TRAIN (array K) (list Q)))
      (lindex labels (array 0))
    }))

;; ------------------------------------------------------------
;; Simple preprocessing wrappers
;; ------------------------------------------------------------

;; standardize:
;;   Alias for zscore: column-wise standardization of X.
(def standardize
  (lambda (X)
    (zscore X)))

;; cov_from_zscore:
;;   Covariance matrix after z-score normalization.
(def cov_from_zscore
  (lambda (X)
    (cov (zscore X))))

;; eof
