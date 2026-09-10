# scientific.mu — linear algebra, statistics and machine learning, Musil half.
#
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# Not loaded automatically: (load "scientific.mu"). Needs std.mu.
# A matrix is a list of row vectors: (list (vec 1 2) (vec 3 4)) is 2x2.
# The C++ half (scientific.h) provides mat-mul, transpose, det, rank, inv,
# solve, eig-sym, median-filter, kmeans, knn. Everything below is written in
# Musil as compositions of vector operations, which run at C++ speed.
load "std.mu"

# --- construction ------------------------------------------------------------
# A matrix literal is just (list (vec 1 2) (vec 3 4)).
# (list->mat L)           a matrix from a list of lists of numbers
function list->mat (L) (map L vec)
# (mat-fill r c v)         r x c matrix filled with v
function mat-fill (r c v) {
    var out (list)
    times r (function (k) (push out (+ (zeros c) v)))
    return out
}
# (mat-zeros r c) (mat-ones r c) matrices of zeros or ones
function mat-zeros (r c) (mat-fill r c 0)
# (mat-ones r c) a matrix of ones
function mat-ones (r c) (mat-fill r c 1)
# (mat-rand r c)           r x c matrix of values in [-1, 1]
function mat-rand (r c) {
    var out (list)
    times r (function (k) (push out (- (* 2 (rand c)) 1)))
    return out
}
# (eye n)                  identity; (diag v) diagonal matrix from a vector
function eye (n) (diag (ones n))
# (diag v) a square matrix with v on the diagonal
function diag (v) {
    var n (length v)
    var out (list)
    for (var k 0) (< k n) (var k (+ k 1)) {
        var row (zeros n)
        setidx row k (getidx v k)
        push out row
    }
    return out
}
# (mat-diag M)             the diagonal of a matrix as a vector
function mat-diag (M) {
    var n (min (nrows M) (ncols M))
    var out (zeros n)
    for (var k 0) (< k n) (var k (+ k 1)) { setidx out k (getidx (getidx M k) k) }
    return out
}

# --- shape and access ----------------------------------------------------------
# (nrows M) (ncols M) the number of rows or columns
function nrows (M) (length M)
# (ncols M) the number of columns
function ncols (M) (length (head M))
# (mat-shape M)            (list rows cols)
function mat-shape (M) (list (nrows M) (ncols M))
# (mat-get M i j)          element; (mat-set M i j v) in place
function mat-get (M i j) (getidx (getidx M i) j)
# (mat-set M i j v) set an element in place
function mat-set (M i j v) (setidx (getidx M i) j v)
# (mat-row M i)            row i as a vector; (mat-col M j) column j as a vector
function mat-row (M i) (getidx M i)
# (mat-col M j) column j as a vector
function mat-col (M j) (vec (map M (function (row) (getidx row j))))
# (get-rows M from n)      n rows starting at from, as a matrix
function get-rows (M from n) (slice M from n)
# (get-cols M from n)      n columns starting at from, as a matrix
function get-cols (M from n) (map M (function (row) (slice row from n)))

# --- products with vectors -------------------------------------------------------
# (mat-vec A x)            A x as a vector; (vec-mat x A) x A as a vector
function mat-vec (A x) (vec (map A (function (row) (dot row x))))
# (vec-mat x A) x A as a vector
function vec-mat (x A) (mat-vec (transpose A) x)
# (outer u v)              the matrix u v', rows u[i] * v
function outer (u v) (map (vec->list u) (function (ui) (* ui v)))
# (trace M)                sum of the diagonal
function trace (M) (sum (mat-diag M))

# --- elementwise arithmetic (rows are vectors, so each row broadcasts) ------------
# (mat-add A B) (mat-sub A B) (hadamard A B) elementwise sum, difference and product of two matrices
function mat-add (A B) (map (zip A B) (function (p) (+ (head p) (last p))))
# (mat-sub A B) elementwise difference
function mat-sub (A B) (map (zip A B) (function (p) (- (head p) (last p))))
# (hadamard A B) elementwise product
function hadamard (A B) (map (zip A B) (function (p) (* (head p) (last p))))
# (mat-scale M s) (mat-shift M s) every element times s, or plus s
function mat-scale (M s) (map M (function (row) (* row s)))
# (mat-shift M s) every element plus s
function mat-shift (M s) (map M (function (row) (+ row s)))
# (mat-map M f)            f applied to every element
function mat-map (M f) (map M (function (row) (map row f)))
# (mat-round M d)          every element rounded to d decimals (for printing)
function mat-round (M d) (map M (function (row) (fixed row d)))

# --- stacking ---------------------------------------------------------------
# (vstack A B)             rows of A then rows of B
function vstack (A B) (concat-list A B)
# (hstack A B)             each row of A followed by the row of B
function hstack (A B) (map (zip A B) (function (p) (vec (head p) (last p))))
# (col-stack vectors)      a matrix whose columns are the given vectors: (col-stack (list x y))
function col-stack (vectors) (transpose vectors)

# --- norms and distances ----------------------------------------------------------
# (lp-norm v p)            (sum |v|^p)^(1/p); p = 2 is the euclidean norm
function lp-norm (v p) {
    if (< p 1) { error "lp-norm: p must be >= 1" }
    return (pow (sum (pow (abs v) p)) (/ 1 p))
}
# (dist x y)               euclidean distance; (dist-p x y p) with another p
# (dist-p x y p) distance with another p: 1 for the city-block distance
function dist-p (x y p) {
    if (!= (length x) (length y)) { error "dist: size mismatch (" (length x) " vs " (length y) ")" }
    return (lp-norm (- x y) p)
}
function dist (x y) (dist-p x y 2)
# (dist-matrix M)          pairwise euclidean distances between the rows of M
function dist-matrix (M) (map M (function (a) (vec (map M (function (b) (dist a b))))))
# (nearest M x)            index of the row of M closest to x
function nearest (M x) (argmin (vec (map M (function (row) (dist row x)))))

# --- reductions and statistics along an axis: 0 = down the columns, 1 = along the rows ---
# (mat-sum M axis)
function mat-sum (M axis) {
    if (== axis 0) { return (reduce M + (zeros (ncols M))) }
    return (vec (map M sum))
}
# (mat-mean M axis)
function mat-mean (M axis) {
    if (== axis 0) { return (/ (mat-sum M 0) (nrows M)) }
    return (vec (map M mean))
}
# (mat-std M axis)         population standard deviation
function mat-std (M axis) {
    if (== axis 0) { return (mat-std (transpose M) 1) }
    return (vec (map M stdev))
}
# (zscore M)               columns standardised to mean 0, std 1 (constant columns become 0)
function zscore (M) {
    var mu (mat-mean M 0)
    var sd (mat-std M 0)
    var safe (+ sd (== sd 0))            # avoid dividing by zero; those columns are 0 anyway
    return (map M (function (row) (* (/ (- row mu) safe) (!= sd 0))))
}
# (standardize M) the same as zscore
function standardize (M) (zscore M)
# (center M)               each column minus its mean
function center (M) {
    var mu (mat-mean M 0)
    return (map M (function (row) (- row mu)))
}
# (cov M)                  sample covariance of the columns (rows are observations)
function cov (M) {
    var n (nrows M)
    if (< n 2) { error "cov: need at least 2 observations" }
    var C (center M)
    return (mat-scale (mat-mul (transpose C) C) (/ 1 (- n 1)))
}
# (corr M)                 correlation of the columns; constant columns give 0
function corr (M) {
    var C (cov M)
    var sd (sqrt (mat-diag C))
    var safe (+ sd (== sd 0))
    var R (map (zip C (vec->list sd)) (function (p) (/ (head p) (* safe (last p)))))
    return (map (zip R (vec->list sd)) (function (p) (* (head p) (!= sd 0) (!= (last p) 0))))
}
# (linefit x y)            (slope intercept) of the least-squares line through the points
function linefit (x y) {
    var n (length x)
    if (or (!= n (length y)) (== n 0)) { error "linefit: x and y must have the same nonzero length" }
    var mx (mean x)
    var my (mean y)
    var den (- (dot x x) (* n mx mx))
    if (< (abs den) 1e-12) { error "linefit: degenerate fit (vertical line)" }
    var slope (/ (- (dot x y) (* n mx my)) den)
    return (vec slope (- my (* slope mx)))
}

# --- display -------------------------------------------------------------
# (mat-str M d)            one line per row, d decimals, columns aligned
function mat-str (M d) {
    var cells (map M (function (row) (map (vec->list row) (function (x) (fmt-fixed x d)))))
    var width (max-of (map (flatten cells) length))
    var lines (map cells (function (row) (join (map row (function (s) (pad-left s width " "))) "  ")))
    return (join lines "\n")
}
# (mat-print M)            print with 3 decimals; (mat-print-with M d) chooses the decimals
function mat-print (M) (print (mat-str M 3))
# (mat-print-with M d) print with d decimals
function mat-print-with (M d) (print (mat-str M d))

# --- linear regression -----------------------------------------------------
# (linreg-fit X y)         coefficients b with y ~ X b, by the normal equations; y is a vector
function linreg-fit (X y) {
    var Xt (transpose X)
    return (solve (mat-mul Xt X) (mat-vec Xt y))
}
# (linreg-predict X b)     X b as a vector
function linreg-predict (X b) (mat-vec X b)
# (linreg-residuals X y b)
function linreg-residuals (X y b) (- y (linreg-predict X b))
# (add-intercept X)        a column of ones in front of X, for a model with a constant term
function add-intercept (X) (map X (function (row) (vec 1 row)))

# --- PCA: (pca X) => d x (d+1) matrix, row k = k-th principal direction then its eigenvalue, largest first ---
# (pca X) => d x (d+1) matrix: row k is the k-th principal direction followed by its eigenvalue, largest first
function pca (X) {
    var e (eig-sym (cov X))
    var values (head e)
    var directions (last e)
    return (map (zip directions (vec->list values)) (function (p) (vec (head p) (last p))))
}
# (pca-directions P) the directions of a pca result as a d x d matrix
function pca-directions (P) (get-cols P 0 (- (ncols P) 1))
# (pca-eigenvalues P) its eigenvalues as a vector
function pca-eigenvalues (P) (mat-col P (- (ncols P) 1))
# (pca-scores X k)         X projected on the first k principal directions, n x k
function pca-scores (X k) {
    var V (get-rows (pca-directions (pca X)) 0 k)
    return (mat-mul X (transpose V))
}
# (pca-explained P)        fraction of variance per component
function pca-explained (P) {
    var e (pca-eigenvalues P)
    return (/ e (sum e))
}

# --- break-point functions ------------------------------------------------------
# (bpf start segments)     piecewise linear function as one vector; segments is a list of
#                          (list length end): (bpf 0 (list (list 4 1) (list 4 0))) rises then falls.
#                          Each segment's end value is excluded (it starts the next one).
function bpf (start segments) {
    var out (vec)
    var cur start
    each segments (function (seg) {
        var len (head seg)
        var end (last seg)
        if (< len 1) { error "bpf: segment length must be >= 1" }
        set out (vec out (+ cur (* (range len) (/ (- end cur) len))))
        set cur end
    })
    return out
}

# --- k-means helpers (kmeans returns (list labels centroids)) -----------------
# (kmeans-labels result) (kmeans-centroids result) the two parts of a kmeans result
function kmeans-labels (result) (head result)
# (kmeans-centroids result) the centroids of a kmeans result, k x d
function kmeans-centroids (result) (last result)
# (cluster-sizes labels k)   how many points fell in each of the k clusters
function cluster-sizes (labels k) (vec (map (range k) (function (c) (sum (== labels c)))))

# --- KNN helpers -------------------------------------------------------------
# A training set is a list of (list features label). A model is (list training k).
# (knn-model training k) a model: (list training k); training is a list of (list features label)
function knn-model (training k) (list training k)
# (knn-predict model queries)   labels for a list of feature vectors
function knn-predict (model queries) (knn (head model) (last model) queries)
# (knn-predict-one model q)
function knn-predict-one (model q) (head (knn-predict model (list q)))
# (knn-test model samples)      predictions for samples given as (list features label)
function knn-test (model samples) (knn-predict model (map samples head))
# (accuracy predictions samples)   fraction of predictions equal to the samples' labels
function accuracy (predictions samples) {
    if (== (length predictions) 0) { return 0 }
    var hits (count (zip predictions (map samples last)) (function (p) (equal? (head p) (str (last p)))))
    return (/ hits (length predictions))
}
# (train-test-split samples fraction)   (list training test), first part of the list
function train-test-split (samples fraction) {
    var n (floor (* fraction (length samples)))
    return (list (take samples n) (drop samples n))
}
# (shuffle L)              a new list in random order (Fisher-Yates)
function shuffle (L) {
    var out (copy L)
    for (var k (- (length out) 1)) (> k 0) (var k (- k 1)) {
        var j (floor (* (rand) (+ k 1)))
        var t (getidx out k)
        setidx out k (getidx out j)
        setidx out j t
    }
    return out
}
