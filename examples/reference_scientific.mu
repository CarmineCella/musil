# musil — library reference: scientific (scientific.h + scientific.mu)
#
# Linear algebra, statistics and machine learning. A matrix is a list of row
# vectors: (list (vec 1 2) (vec 3 4)). Vectors keep their own arithmetic
# (+ - * / broadcast); matrices have mat-... functions. scientific.mu loads
# std.mu itself. Element loops (mat-mul, transpose, inv, eig-sym, kmeans, knn)
# are C++; everything else is Musil on top of vector operations.
# Run with: musil reference_scientific.mu

load "scientific.mu"
print ""
print "================================================================"
print "  musil: library reference (scientific)"
print "================================================================"

# --- 1. Building matrices --------------------------------------------------
print ""
print "--- construction ---"
var A (list (vec 1 2) (vec 3 4))
print "a literal       :" A
print "list->mat       :" (list->mat (list (list 1 2) (list 3 4)))
print "mat-zeros 2 3   :" (mat-zeros 2 3)
print "mat-ones 1 2    :" (mat-ones 1 2)
print "mat-fill 2 2 7  :" (mat-fill 2 2 7)
print "eye 3           :" (eye 3)
print "diag (vec 2 3)  :" (diag (vec 2 3))
seed 3
print "mat-rand 2 2    :" (mat-shape (mat-rand 2 2)) "(values in [-1, 1])"
print "col-stack       :" (col-stack (list (vec 1 2 3) (vec 4 5 6))) "(the vectors become columns)"

# --- 2. Shape and access -----------------------------------------------------
print ""
print "--- shape and access ---"
var M (list (vec 1 2 3) (vec 4 5 6) (vec 7 8 10))
print "nrows, ncols    :" (nrows M) (ncols M) "  mat-shape:" (mat-shape M)
print "mat-get M 1 2   :" (mat-get M 1 2)
print "mat-row M 0     :" (mat-row M 0)
print "mat-col M 2     :" (mat-col M 2)
print "mat-diag M      :" (mat-diag M)
print "get-rows M 1 2  :" (get-rows M 1 2) "(2 rows from row 1)"
print "get-cols M 0 2  :" (get-cols M 0 2) "(2 columns from column 0)"
var C (mat-zeros 2 2)
mat-set C 0 1 9
print "mat-set in place:" C

# --- 3. Elementwise arithmetic and stacking ----------------------------------
print ""
print "--- elementwise and stacking ---"
var B (list (vec 5 6) (vec 7 8))
print "mat-add A B     :" (mat-add A B)
print "mat-sub B A     :" (mat-sub B A)
print "hadamard A B    :" (hadamard A B)
print "mat-scale A 3   :" (mat-scale A 3)
print "mat-shift A 10  :" (mat-shift A 10)
print "mat-map A sqrt  :" (mat-map A sqrt)
print "vstack A B      :" (vstack A B)
print "hstack A B      :" (hstack A B)

# --- 4. Products, transpose, decompositions (scientific.h) --------------------
print ""
print "--- products and decompositions ---"
print "mat-mul A B     :" (mat-mul A B)
print "mat-mul A B A   :" (mat-mul A B A) "(left to right)"
print "mat-vec A (1 1) :" (mat-vec A (vec 1 1))
print "vec-mat (1 1) A :" (vec-mat (vec 1 1) A)
print "outer (1 2) (3 4):" (outer (vec 1 2) (vec 3 4))
print "trace M         :" (trace M)
print "transpose M     :" (transpose M)
print "det A, det M    :" (det A) (fixed (det M) 6)
print "rank M          :" (rank M) "  rank of (1 2; 2 4):" (rank (list (vec 1 2) (vec 2 4)))
print "inv A           :" (inv A)
print "A inv(A)        :" (mat-round (mat-mul A (inv A)) 12)
print "solve A (5 11)  :" (solve A (vec 5 11))
print "singular        :" (try (inv (list (vec 1 2) (vec 2 4))) catch e e)
print "not square      :" (try (det (list (vec 1 2 3))) catch e e)
print "ragged          :" (try (transpose (list (vec 1 2) (vec 3))) catch e e)
var S (list (vec 2 1) (vec 1 2))
print "eig-sym         :" (eig-sym S) "(eigenvalues, then eigenvectors as rows)"
print "A v = lambda v  :" (mat-vec S (head (last (eig-sym S)))) "=" (* 3 (head (last (eig-sym S))))

# --- 5. Statistics -----------------------------------------------------------------
print ""
print "--- statistics ---"
print "median-filter 3 :" (median-filter (vec 1 1 9 1 1 9 1 1) 3) "(spikes removed; median itself is in std)"
print "linefit         :" (linefit (vec 0 1 2 3) (vec 1 3 5 7)) "(slope intercept)"
print "lp-norm 1, 2, 3 :" (lp-norm (vec 3 -4) 1) (lp-norm (vec 3 -4) 2) (fixed (lp-norm (vec 3 -4) 3) 4)
print "dist, dist-p 1  :" (dist (vec 0 0) (vec 3 4)) (dist-p (vec 0 0) (vec 3 4) 1)
print "dist-matrix     :" (dist-matrix (list (vec 0 0) (vec 3 4) (vec 6 8)))
print "nearest         :" (nearest (list (vec 0 0) (vec 3 4) (vec 6 8)) (vec 5 5))
print "center          :" (center (list (vec 1 10) (vec 3 20)))
print "axis 0 is down the columns, axis 1 along the rows:"
print "mat-sum  0, 1   :" (mat-sum M 0) (mat-sum M 1)
print "mat-mean 0, 1   :" (mat-mean M 0) (mat-mean M 1)
print "mat-std  0, 1   :" (fixed (mat-std M 0) 3) (fixed (mat-std M 1) 3)
print "zscore          :" (mat-round (zscore M) 3)
print "cov             :" (mat-round (cov M) 3)
print "corr            :" (mat-round (corr M) 3)

# --- 6. Regression -------------------------------------------------------------
print ""
print "--- regression ---"
var X (add-intercept (col-stack (list (vec 0 1 2 3))))
var y (vec 1 3 5 7)
print "design matrix   :" X
var b (linreg-fit X y)
print "linreg-fit      :" b "(intercept slope)"
print "linreg-predict  :" (linreg-predict X b)
print "residuals       :" (fixed (linreg-residuals X y b) 12)

# --- 7. PCA -----------------------------------------------------------------------
print ""
print "--- pca ---"
var cloud (list (vec 1 0.1) (vec 2 -0.1) (vec 3 0.1) (vec 4 -0.1) (vec 5 0))
var P (pca cloud)
print "pca             :" (mat-round P 4) "(each row: direction then its eigenvalue, largest first)"
print "pca-directions  :" (mat-round (pca-directions P) 4)
print "pca-eigenvalues :" (fixed (pca-eigenvalues P) 4)
print "pca-explained   :" (fixed (pca-explained P) 4)
print "pca-scores 1    :" (mat-round (pca-scores cloud 1) 4)

# --- 8. k-means -----------------------------------------------------------------
print ""
print "--- kmeans ---"
var pts (list (vec 0 0) (vec 0.1 0) (vec 0 0.1) (vec 10 10) (vec 10.1 10) (vec 10 10.1))
var km (kmeans pts 2)
print "kmeans-labels   :" (kmeans-labels km)
print "kmeans-centroids:" (mat-round (kmeans-centroids km) 3)
print "cluster-sizes   :" (cluster-sizes (kmeans-labels km) 2)

# --- 9. KNN classification ---------------------------------------------------------
print ""
print "--- knn ---"
var training (list (list (vec 0 0) "a") (list (vec 0 1) "a") (list (vec 10 10) "b") (list (vec 10 11) "b"))
print "knn             :" (knn training 1 (list (vec 0.2 0.2) (vec 9 9)))
var model (knn-model training 3)
print "knn-predict-one :" (knn-predict-one model (vec 10 12))
print "knn-test        :" (knn-test model training)
print "accuracy        :" (accuracy (knn-test model training) training)
seed 1
var split (train-test-split (shuffle training) 0.5)
print "train-test-split:" (map split length) "(after shuffle)"

# --- 10. Break-point functions ---------------------------------------------------
print ""
print "--- bpf ---"
print "one segment     :" (bpf 0 (list (list 4 1))) "(4 samples from 0 towards 1, end excluded)"
print "attack, decay   :" (bpf 0 (list (list 4 1) (list 4 0)))
print "an envelope     :" (fixed (bpf 0 (list (list 3 1) (list 5 0.5) (list 4 0))) 2)

# --- 11. Display ---------------------------------------------------------------
print ""
print "--- display ---"
print "mat-str M 2:"
print (mat-str M 2)
print "mat-print (inv A):"
mat-print (inv A)
print "mat-print-with (cov M) 1:"
mat-print-with (cov M) 1

print ""
print "================================================================"
print "  end of scientific reference"
print "================================================================"
