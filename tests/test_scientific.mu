# test_scientific.mu — self-checking test of the scientific library (scientific.h + scientific.mu).
#
# Every check is a (check ...) from test.mu. A passing run prints only the final line.
# Run with: musil tests/test_scientific.mu

load "test.mu"
load "scientific.mu"

function near? (a b eps) (< (max (abs (- a b))) eps)
function mat-near? (A B eps) (all? (zip A B) (function (p) (near? (head p) (last p) eps)))

var A (list (vec 1 2) (vec 3 4))
var B (list (vec 5 6) (vec 7 8))
var I2 (list (vec 1 0) (vec 0 1))

# --- construction and shape ---
check (equal? (list->mat (list (list 1 2) (list 3 4))) A) "list->mat"
check (equal? (mat-zeros 2 3) (list (vec 0 0 0) (vec 0 0 0))) "mat-zeros"
check (equal? (mat-ones 1 2) (list (vec 1 1))) "mat-ones"
check (equal? (mat-fill 2 2 7) (list (vec 7 7) (vec 7 7))) "mat-fill"
check (equal? (mat-shape (mat-rand 3 4)) (list 3 4)) "mat-rand: shape"
check (all? (flatten (map (mat-rand 3 4) (function (r) (map r identity)))) (function (x) (between? x -1 1))) "mat-rand: range"
check (equal? (eye 2) I2) "eye"
check (equal? (diag (vec 2 3)) (list (vec 2 0) (vec 0 3))) "diag"
check (equal? (mat-diag B) (vec 5 8)) "mat-diag"
check (== (nrows A) 2) "nrows"
check (== (ncols (list (vec 1 2 3))) 3) "ncols"
check (equal? (mat-shape A) (list 2 2)) "mat-shape"
check (== (mat-get B 1 0) 7) "mat-get"
var C (mat-zeros 2 2)
mat-set C 0 1 9
check (== (mat-get C 0 1) 9) "mat-set"
check (equal? (mat-row B 1) (vec 7 8)) "mat-row"
check (equal? (mat-col B 1) (vec 6 8)) "mat-col"
var M3 (list (vec 1 2 3) (vec 4 5 6) (vec 7 8 9))
check (equal? (get-rows M3 1 2) (list (vec 4 5 6) (vec 7 8 9))) "get-rows"
check (equal? (get-cols M3 1 2) (list (vec 2 3) (vec 5 6) (vec 8 9))) "get-cols"

# --- elementwise ---
check (equal? (mat-add A B) (list (vec 6 8) (vec 10 12))) "mat-add"
check (equal? (mat-sub B A) (list (vec 4 4) (vec 4 4))) "mat-sub"
check (equal? (hadamard A B) (list (vec 5 12) (vec 21 32))) "hadamard"
check (equal? (mat-scale A 2) (list (vec 2 4) (vec 6 8))) "mat-scale"
check (equal? (mat-shift A 1) (list (vec 2 3) (vec 4 5))) "mat-shift"
check (equal? (mat-map A (function (x) (* x x))) (list (vec 1 4) (vec 9 16))) "mat-map"
check (contains? (error-of (function () (mat-add A (list (vec 1 2 3) (vec 1 2 3))))) "size mismatch") "mat-add: shape mismatch"

# --- stacking ---
check (equal? (vstack A B) (list (vec 1 2) (vec 3 4) (vec 5 6) (vec 7 8))) "vstack"
check (equal? (hstack A B) (list (vec 1 2 5 6) (vec 3 4 7 8))) "hstack"
check (equal? (col-stack (list (vec 1 2 3) (vec 4 5 6))) (list (vec 1 4) (vec 2 5) (vec 3 6))) "col-stack"

# --- products (scientific.h) ---
check (equal? (mat-mul A B) (list (vec 19 22) (vec 43 50))) "mat-mul"
check (equal? (mat-mul A I2) A) "mat-mul: identity"
check (equal? (mat-mul A B I2) (mat-mul A B)) "mat-mul: variadic"
check (contains? (error-of (function () (mat-mul A (list (vec 1 2 3))))) "nonconformant") "mat-mul: nonconformant"
check (equal? (mat-vec A (vec 1 1)) (vec 3 7)) "mat-vec"
check (equal? (vec-mat (vec 1 1) A) (vec 4 6)) "vec-mat"
check (contains? (error-of (function () (mat-vec A (vec 1 2 3)))) "size mismatch") "mat-vec: length"
check (equal? (outer (vec 1 2) (vec 3 4)) (list (vec 3 4) (vec 6 8))) "outer"
check (== (trace M3) 15) "trace"
check (equal? (transpose (list (vec 1 2 3) (vec 4 5 6))) (list (vec 1 4) (vec 2 5) (vec 3 6))) "transpose"
check (equal? (transpose (transpose A)) A) "transpose twice"
check (contains? (error-of (function () (transpose (list (vec 1 2) (vec 1))))) "ragged") "ragged matrix is an error"
check (contains? (error-of (function () (transpose (list)))) "empty") "empty matrix is an error"
check (contains? (error-of (function () (transpose (list "a")))) "expected number") "rows must be vectors"
check (equal? (transpose (list 1 2)) (list (vec 1 2))) "a list of scalars is a column"

# --- decompositions ---
check (== (det A) -2) "det 2x2"
check (== (det M3) 0) "det: singular"
check (== (det (eye 4)) 1) "det: identity"
check (near? (det (list (vec 2 0 1) (vec 1 3 2) (vec 1 1 2))) 6 1e-12) "det 3x3"
check (contains? (error-of (function () (det (list (vec 1 2 3))))) "square") "det: not square"
check (== (rank M3) 2) "rank: deficient"
check (== (rank (eye 3)) 3) "rank: full"
check (== (rank (list (vec 0 0) (vec 0 0))) 0) "rank: zero"
check (mat-near? (inv A) (list (vec -2 1) (vec 1.5 -0.5)) 1e-12) "inv"
check (mat-near? (mat-mul A (inv A)) I2 1e-12) "inv: A inv(A) = I"
check (contains? (error-of (function () (inv M3))) "singular") "inv: singular"
check (near? (solve A (vec 5 11)) (vec 1 2) 1e-12) "solve"
check (contains? (error-of (function () (solve M3 (vec 1 2 3)))) "singular") "solve: singular"
check (contains? (error-of (function () (solve A (vec 1 2 3)))) "elements") "solve: b length"

# --- statistics ---
check (equal? (median-filter (vec 1 1 9 1 1 9 1 1) 3) (ones 8)) "median-filter removes spikes"
check (contains? (error-of (function () (median-filter (vec 1 2) 0))) ">= 1") "median-filter: order"
check (near? (linefit (vec 0 1 2 3) (vec 1 3 5 7)) (vec 2 1) 1e-12) "linefit"
check (contains? (error-of (function () (linefit (vec 1 1 1) (vec 1 2 3)))) "degenerate") "linefit: vertical"
check (== (lp-norm (vec 3 4) 2) 5) "lp-norm 2"
check (== (lp-norm (vec 3 -4) 1) 7) "lp-norm 1"
check (== (dist (vec 0 0) (vec 3 4)) 5) "dist"
check (== (dist-p (vec 0 0) (vec 3 4) 1) 7) "dist-p: L1"
check (contains? (error-of (function () (dist (vec 1) (vec 1 2)))) "size mismatch") "dist: size"
check (contains? (error-of (function () (lp-norm (vec 1) 0.5))) ">= 1") "lp-norm: p"
check (equal? (dist-matrix (list (vec 0 0) (vec 3 4))) (list (vec 0 5) (vec 5 0))) "dist-matrix"
check (== (nearest (list (vec 0 0) (vec 10 10) (vec 5 5)) (vec 6 6)) 2) "nearest"
check (equal? (center (list (vec 1 10) (vec 3 20))) (list (vec -1 -5) (vec 1 5))) "center"
check (equal? (mat-sum M3 0) (vec 12 15 18)) "mat-sum 0"
check (equal? (mat-sum M3 1) (vec 6 15 24)) "mat-sum 1"
check (equal? (mat-mean M3 0) (vec 4 5 6)) "mat-mean 0"
check (equal? (mat-mean M3 1) (vec 2 5 8)) "mat-mean 1"
check (near? (mat-std (list (vec 2 4) (vec 4 6) (vec 6 8)) 0) (vec 1.633 1.633) 1e-3) "mat-std 0"
var Z (zscore (list (vec 1 5) (vec 3 5) (vec 5 5)))
check (near? (mat-mean Z 0) (vec 0 0) 1e-12) "zscore: zero mean"
check (near? (mat-col Z 0) (vec -1.2247 0 1.2247) 1e-3) "zscore: unit std"
check (equal? (mat-col Z 1) (vec 0 0 0)) "zscore: constant column becomes zeros"
var D (list (vec 1 2) (vec 2 4) (vec 3 6))
check (mat-near? (cov D) (list (vec 1 2) (vec 2 4)) 1e-12) "cov"
check (mat-near? (corr D) (list (vec 1 1) (vec 1 1)) 1e-12) "corr: perfectly correlated"
check (contains? (error-of (function () (cov (list (vec 1 2))))) "at least 2") "cov: one observation"

# --- eigen decomposition ---
var e (eig-sym (list (vec 2 1) (vec 1 2)))
check (near? (head e) (vec 3 1) 1e-12) "eig-sym: eigenvalues descending"
check (near? (head (last e)) (vec 0.7071 0.7071) 1e-4) "eig-sym: first eigenvector"
check (near? (mat-vec (list (vec 2 1) (vec 1 2)) (head (last e))) (* 3 (head (last e))) 1e-12) "eig-sym: A v = lambda v"
check (near? (head (eig-sym (diag (vec 1 5 3)))) (vec 5 3 1) 1e-12) "eig-sym: diagonal"
check (contains? (error-of (function () (eig-sym (list (vec 1 2) (vec 3 4))))) "not symmetric") "eig-sym: not symmetric"
check (contains? (error-of (function () (eig-sym (list (vec 1 2 3))))) "square") "eig-sym: not square"

# --- machine learning ---
# points along the x axis: the first direction is (1 0) up to sign
var line (list (vec 1 0.1) (vec 2 -0.1) (vec 3 0.1) (vec 4 -0.1) (vec 5 0))
var P (pca line)
check (equal? (mat-shape P) (list 2 3)) "pca: d x (d+1)"
check (> (abs (head (head (pca-directions P)))) 0.99) "pca: first direction"
check (> (head (pca-eigenvalues P)) (last (pca-eigenvalues P))) "pca: eigenvalues descending"
check (near? (sum (pca-explained P)) 1 1e-12) "pca-explained sums to 1"
check (equal? (mat-shape (pca-scores line 1)) (list 5 1)) "pca-scores shape"
var two (list (vec 0 0) (vec 0.1 0) (vec 0 0.1) (vec 10 10) (vec 10.1 10) (vec 10 10.1))
var km (kmeans two 2)
check (== (length (kmeans-labels km)) 6) "kmeans: one label per point"
check (equal? (mat-shape (kmeans-centroids km)) (list 2 2)) "kmeans: k x d centroids"
check (equal? (slice (kmeans-labels km) 0 3) (+ (zeros 3) (head (kmeans-labels km)))) "kmeans: first three together"
check (!= (head (kmeans-labels km)) (last (kmeans-labels km))) "kmeans: groups separated"
check (equal? (cluster-sizes (kmeans-labels km) 2) (vec 3 3)) "cluster-sizes"
check (contains? (error-of (function () (kmeans two 7))) "between 1") "kmeans: k too large"
var training (list (list (vec 0 0) "a") (list (vec 0 1) "a") (list (vec 10 10) "b") (list (vec 10 11) "b"))
check (equal? (knn training 1 (list (vec 0.2 0.2) (vec 9 9))) (list "a" "b")) "knn"
var model (knn-model training 3)
check (equal? (knn-predict-one model (vec 10 12)) "b") "knn-predict-one"
var preds (knn-test model training)
check (== (accuracy preds training) 1) "accuracy: perfect"
check (== (accuracy (list "a" "a" "a" "a") training) 0.5) "accuracy: half"
check (contains? (error-of (function () (knn training 1 (list (vec 1 2 3))))) "features") "knn: query dimension"
check (contains? (error-of (function () (knn (list) 1 (list (vec 1))))) "empty") "knn: empty training set"
seed 1
var s (shuffle (list 1 2 3 4 5 6 7 8))
check (equal? (sort-list s) (list 1 2 3 4 5 6 7 8)) "shuffle keeps the elements"
check (equal? (map (train-test-split (list 1 2 3 4 5) 0.6) length) (list 3 2)) "train-test-split"

# --- regression ---
var X (list (vec 1 0) (vec 1 1) (vec 1 2) (vec 1 3))
var y (vec 1 3 5 7)
var b (linreg-fit X y)
check (near? b (vec 1 2) 1e-12) "linreg-fit"
check (near? (linreg-predict X b) y 1e-12) "linreg-predict"
check (near? (linreg-residuals X y b) (zeros 4) 1e-12) "linreg-residuals"
check (equal? (add-intercept (list (vec 5) (vec 6))) (list (vec 1 5) (vec 1 6))) "add-intercept"

# --- display ---
check (equal? (mat-round (list (vec 1.234 5.678)) 1) (list (vec 1.2 5.7))) "mat-round"
check (equal? (mat-str A 1) "1.0  2.0\n3.0  4.0") "mat-str"
check (equal? (mat-str (list (vec 1 -10)) 0) "  1  -10") "mat-str: alignment"

report "test_scientific"
