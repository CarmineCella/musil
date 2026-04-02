# ─────────────────────────────────────────────────────────────────────────────
# test_scientific.mu  —  Musil scientific library test suite
#
# Requires: stdlib.mu loaded, scientific C++ builtins registered,
#           and scientific.mu loaded.
#
# Run: ./musil test_scientific.mu
# ─────────────────────────────────────────────────────────────────────────────

load("stdlib.mu")
load("scientific.mu")

# ── Matrix / vector comparison helpers ───────────────────────────────────────
# These helpers compare matrices and vectors element-wise.

proc vec_eq (a, b) {
    if (len(a) != len(b)) { return 0 }
    return all(a == b)       # == on NumVals is element-wise; all() reduces to scalar
}

proc vec_eq_approx (a, b, eps) {
    if (len(a) != len(b)) { return 0 }
    return all(abs(a - b) < eps)
}

proc mat_eq (A, B) {
    if (len(A) != len(B)) { return 0 }
    var i = 0
    while (i < len(A)) {
        if (not vec_eq(A[i], B[i])) { return 0 }
        i = i + 1
    }
    return 1
}

proc mat_eq_approx (A, B, eps) {
    if (len(A) != len(B)) { return 0 }
    var i = 0
    while (i < len(A)) {
        if (not vec_eq_approx(A[i], B[i], eps)) { return 0 }
        i = i + 1
    }
    return 1
}

var EPS = 1e-6

# ── Test data ─────────────────────────────────────────────────────────────────
# M2:  [[1, 2],   M2b: [[5, 6],
#        [3, 4]]         [7, 8]]

var M2  = [vec(1,2), vec(3,4)]
var M2b = [vec(5,6), vec(7,8)]

# M3x2: [[1,2],[3,4],[5,6]]  (3 rows, 2 cols)
var M3x2 = [vec(1,2), vec(3,4), vec(5,6)]

# K-means: 6 points in 2D, clearly 2 clusters near (0,0) and (5,5)
var KM_DATA = [
    vec(0.0,  0.0),
    vec(0.1,  0.1),
    vec(-0.1, -0.1),
    vec(5.0,  5.0),
    vec(5.1,  5.1),
    vec(4.9,  4.9)
]

# KNN training set: each item is [features_vector, label]
var KNN_TRAIN = [
    [vec(0.0, 0.0), "A"],
    [vec(0.2, 0.1), "A"],
    [vec(5.0, 5.0), "B"],
    [vec(5.1, 4.9), "B"]
]

# KNN query points
var KNN_QUERY = [vec(0.05, 0.05), vec(5.05, 5.00)]

# ── Shape / transpose / row / col ────────────────────────────────────────────

assert_eq(nrows(M2), 2, "nrows M2")
assert_eq(ncols(M2), 2, "ncols M2")

assert(mat_eq(transpose(M2), [vec(1,3), vec(2,4)]),
       "transpose M2")

# row(M, i) returns a 1-row matrix (Array containing one vector)
assert(mat_eq(row(M2, 0), [vec(1,2)]), "row M2 0")
assert(mat_eq(row(M2, 1), [vec(3,4)]), "row M2 1")

# col(M, i) returns an m-row x 1-col matrix
assert(mat_eq(col(M2, 0), [vec(1), vec(3)]), "col M2 0")
assert(mat_eq(col(M2, 1), [vec(2), vec(4)]), "col M2 1")

# ── matmul / matadd / matsub / hadamard ──────────────────────────────────────
# [1 2; 3 4] * [5 6; 7 8] = [19 22; 43 50]

assert(mat_eq(matmul(M2, M2b), [vec(19,22), vec(43,50)]),
       "matmul M2 M2b")

assert(mat_eq(matmul(M2, eye(2)), M2),
       "matmul M2 eye = M2")

assert(mat_eq(matadd(M2, M2b), [vec(6,8), vec(10,12)]),
       "matadd M2 M2b")

assert(mat_eq(matsub(M2, M2b), [vec(-4,-4), vec(-4,-4)]),
       "matsub M2 M2b")

assert(mat_eq(hadamard(M2, M2b), [vec(5,12), vec(21,32)]),
       "hadamard M2 M2b")

# ── matsum / getrows / getcols ────────────────────────────────────────────────
# matsum(M, 0): sum each row → result is n_rows x 1 matrix
# M2 row sums: 1+2=3, 3+4=7  → [[3],[7]]
assert(mat_eq(matsum(M2, 0), [vec(3), vec(7)]),
       "matsum M2 axis=0 (row sums)")

# matsum(M, 1): sum each col → result is 1 x n_cols matrix
# M2 col sums: 1+3=4, 2+4=6  → [[4,6]]
assert(mat_eq(matsum(M2, 1), [vec(4,6)]),
       "matsum M2 axis=1 (col sums)")

assert(mat_eq(getrows(M2, 0, 0), [vec(1,2)]),
       "getrows M2 row 0")

assert(mat_eq(getcols(M2, 1, 1), [vec(2), vec(4)]),
       "getcols M2 col 1")

# ── eye / zeros / ones ───────────────────────────────────────────────────────

assert(mat_eq(eye(3), [vec(1,0,0), vec(0,1,0), vec(0,0,1)]),
       "eye(3)")

# zeros(n) → NumVal of zeros; zeros(cols, rows) → matrix
assert_eq(zeros(4),      vec(0,0,0,0), "zeros(4) is vector")
assert_eq(ones(3),       vec(1,1,1),   "ones(3)  is vector")

assert(mat_eq(zeros(3, 2), [vec(0,0,0), vec(0,0,0)]),
       "zeros(3,2) matrix")

assert(mat_eq(ones(3, 2), [vec(1,1,1), vec(1,1,1)]),
       "ones(3,2)  matrix")

# Wrappers from scientific.mu
assert_eq(zerosvec(4),  vec(0,0,0,0), "zerosvec(4)")
assert_eq(onesvec(2),   vec(1,1),     "onesvec(2)")

assert(mat_eq(zerosmat(2, 2), [vec(0,0), vec(0,0)]),
       "zerosmat(2,2)")

assert(mat_eq(onesmat(2, 3), [vec(1,1,1), vec(1,1,1)]),
       "onesmat(2,3)")

# ── det / inv / diag / rank / solve ──────────────────────────────────────────
# det([1,2;3,4]) = 1*4 - 2*3 = -2

assert_eq(det(M2), -2, "det M2 = -2")

# det(M) * det(inv(M)) = 1
assert_near(det(M2) * det(inv(M2)), 1, EPS,
            "det(M2) * det(inv(M2)) ≈ 1")

# diag: vector → diagonal matrix
assert(mat_eq(diag(vec(1,2,3)), [vec(1,0,0), vec(0,2,0), vec(0,0,3)]),
       "diag vec -> matrix")

# diag: matrix → diagonal vector
assert_eq(diag(M2), vec(1,4), "diag matrix -> vec")

# rank of linearly dependent matrix = 1
assert_eq(rank([vec(1,2), vec(2,4)]), 1,
          "rank of dependent matrix = 1")

# solve M2 * x = [5, 11]  →  x = [1, 2]
var SOL = solve(M2, vec(5, 11))
assert_eq(SOL, vec(1, 2), "solve M2 x = [5,11]")

# ── Statistics ────────────────────────────────────────────────────────────────

# median with window order 3 on [1, 100, 3, 4]
# Window results: [50.5, 3, 4, 3.5]
var MED = median(vec(1, 100, 3, 4), 3)
assert_eq(MED, vec(50.5, 3, 4, 3.5), "median window=3")

# linefit: y = 2x + 1  →  slope=2, intercept=1
var LF = linefit(vec(0,1,2,3), vec(1,3,5,7))
assert_eq(LF, vec(2, 1), "linefit slope=2 intercept=1")

# norm
var NV = vec(3, 4)
assert_eq(norm(NV),    5, "L2 norm [3,4] = 5")
assert_eq(norm(NV, 1), 7, "L1 norm [3,4] = 7")
assert_eq(norm(vec(0,0,0)), 0, "norm of zero vector = 0")

# dist
assert_eq(dist(vec(1,2,3), vec(1,2,3)), 0,   "dist equal points = 0")
assert_eq(dist(vec(0,0),   vec(3,4)),   5,   "L2 dist [0,0] to [3,4] = 5")
assert_eq(dist(vec(1,2),   vec(3,0), 1), 4,  "L1 dist = 4")

# matmean along axis 0 (per-column mean): M2 cols are [1,3] and [2,4]
# means = [2, 3]
assert_eq(matmean(M2, 0), vec(2, 3), "matmean axis=0")

# matmean along axis 1 (per-row mean): row means = [1.5, 3.5]
assert_eq(matmean(M2, 1), vec(1.5, 3.5), "matmean axis=1")

# matstd along axis 0: col std devs = [1, 1]
assert_eq(matstd(M2, 0), vec(1, 1), "matstd axis=0")

# matstd along axis 1: row std devs = [0.5, 0.5]
assert_eq(matstd(M2, 1), vec(0.5, 0.5), "matstd axis=1")

# cov(M2): 2x2 covariance  →  [[2,2],[2,2]]
assert(mat_eq(cov(M2), [vec(2,2), vec(2,2)]), "cov M2")

# zscore(M2): column z-scores  →  [[-1,-1],[1,1]]
assert(mat_eq(zscore(M2), [vec(-1,-1), vec(1,1)]), "zscore M2")

# ── corr (structural properties) ─────────────────────────────────────────────

# Diagonal of correlation matrix = [1, 1]
assert_eq(diag(corr(M2)), vec(1, 1), "diag(corr M2) = [1,1]")

# corr is symmetric: corr(M) = transpose(corr(M))
assert(mat_eq(corr(M2), transpose(corr(M2))), "corr M2 is symmetric")

# ── stack2 / hstack / vstack / matcol ────────────────────────────────────────

var MC_M = [vec(1,2,3), vec(4,5,6), vec(7,8,9)]
assert_eq(matcol(MC_M, 0), vec(1,4,7), "matcol col 0")
assert_eq(matcol(MC_M, 1), vec(2,5,8), "matcol col 1")
assert_eq(matcol(MC_M, 2), vec(3,6,9), "matcol col 2")

# stack2(x, y) → n x 2 matrix
assert(mat_eq(stack2(vec(10,20,30), vec(1,2,3)),
              [vec(10,1), vec(20,2), vec(30,3)]),
       "stack2")

var HS_A = [vec(1,2), vec(3,4)]
var HS_B = [vec(5,6), vec(7,8)]

assert(mat_eq(hstack(HS_A, HS_B), [vec(1,2,5,6), vec(3,4,7,8)]),
       "hstack")

assert(mat_eq(vstack(HS_A, HS_B), [vec(1,2), vec(3,4), vec(5,6), vec(7,8)]),
       "vstack")

# ── PCA: shape tests ──────────────────────────────────────────────────────────
# pca(M3x2) returns d x (d+1) = 2 x 3 matrix

assert_eq(nrows(pca(M3x2)), 2, "pca M3x2: nrows = 2")
assert_eq(ncols(pca(M3x2)), 3, "pca M3x2: ncols = 3")

assert_eq(nrows(pca_eigvecs(M3x2)), 2, "pca_eigvecs: nrows = 2")
assert_eq(ncols(pca_eigvecs(M3x2)), 2, "pca_eigvecs: ncols = 2")

assert_eq(nrows(pca_eigvals(M3x2)), 2, "pca_eigvals: nrows = 2")
assert_eq(ncols(pca_eigvals(M3x2)), 1, "pca_eigvals: ncols = 1")

# pca_scores(M3x2, k=1) → 3 x 1
assert_eq(nrows(pca_scores(M3x2, 1)), 3, "pca_scores k=1: nrows = 3")
assert_eq(ncols(pca_scores(M3x2, 1)), 1, "pca_scores k=1: ncols = 1")

# ── Linear regression ─────────────────────────────────────────────────────────
# X = [[1],[2],[3]], Y = [[2],[4],[6]] → beta = [[2]]  (slope = 2, no intercept)

var LR_X = [vec(1), vec(2), vec(3)]
var LR_Y = [vec(2), vec(4), vec(6)]

var LR_BETA = linreg_fit(LR_X, LR_Y)

# beta is a 1x1 matrix  [[2]]
assert_eq(nrows(LR_BETA), 1, "linreg_fit: beta nrows = 1")
assert_eq(ncols(LR_BETA), 1, "linreg_fit: beta ncols = 1")
assert(mat_eq(LR_BETA, [vec(2)]), "linreg_fit: beta = [[2]]")

# Predictions on training data: X * beta = Y
assert(mat_eq(linreg_predict(LR_X, LR_BETA), LR_Y),
       "linreg_predict on training data = Y")

# New points X_new = [[4],[5]], expected [[8],[10]]
var LR_X_NEW = [vec(4), vec(5)]
assert_eq(nrows(linreg_predict(LR_X_NEW, LR_BETA)), 2,
          "linreg_predict new: nrows = 2")
assert_eq(ncols(linreg_predict(LR_X_NEW, LR_BETA)), 1,
          "linreg_predict new: ncols = 1")
assert(mat_eq(linreg_predict(LR_X_NEW, LR_BETA), [vec(8), vec(10)]),
       "linreg_predict new points")

# Residuals on training data: Y - X*beta = 0
var LR_RES = linreg_residuals(LR_X, LR_Y, LR_BETA)
assert_eq(nrows(LR_RES), 3, "linreg_residuals: nrows = 3")
assert_eq(ncols(LR_RES), 1, "linreg_residuals: ncols = 1")
assert(mat_eq(LR_RES, [vec(0), vec(0), vec(0)]),
       "linreg_residuals on training data = 0")

# ── K-means ───────────────────────────────────────────────────────────────────

var KM_RES = kmeans(KM_DATA, 2)

# KM_RES = [labels_vector, centroids_matrix]
# centroids: K x m = 2 x 2
assert_eq(nrows(KM_RES[1]), 2, "kmeans centroids: nrows = 2")
assert_eq(ncols(KM_RES[1]), 2, "kmeans centroids: ncols = 2")

# labels: NumVal of length n = 6
assert_eq(len(KM_RES[0]),   6, "kmeans labels: length = 6")

# Helper accessors
assert_eq(len(kmeans_labels(kmeans(KM_DATA, 2))),   6, "kmeans_labels length = 6")
assert_eq(nrows(kmeans_centroids(kmeans(KM_DATA, 2))), 2, "kmeans_centroids nrows = 2")
assert_eq(ncols(kmeans_centroids(kmeans(KM_DATA, 2))), 2, "kmeans_centroids ncols = 2")

assert_eq(len(kmeans_run_labels(KM_DATA, 2)),        6, "kmeans_run_labels length = 6")
assert_eq(nrows(kmeans_run_centroids(KM_DATA, 2)),   2, "kmeans_run_centroids nrows = 2")

# ── KNN ───────────────────────────────────────────────────────────────────────

var KNN_RES = knn(KNN_TRAIN, 1, KNN_QUERY)
assert_eq(len(KNN_RES), 2, "knn: returns 2 labels")

# knn_predict wrapper
assert_eq(len(knn_predict(KNN_TRAIN, 1, KNN_QUERY)), 2, "knn_predict: 2 labels")

# knn_predict_one: single closest neighbour to (0.05,0.05) should be "A"
assert_eq(knn_predict_one(KNN_TRAIN, 1, vec(0.05,0.05)), "A",
          "knn_predict_one near origin = A")
assert_eq(knn_predict_one(KNN_TRAIN, 1, vec(5.05,5.00)), "B",
          "knn_predict_one near (5,5) = B")

# ── Random constructors (shape only) ─────────────────────────────────────────

var RV = randvec(5)
assert_eq(len(RV), 5, "randvec(5): length = 5")

var RM = randmat(3, 2)
assert_eq(nrows(RM), 3, "randmat(3,2): nrows = 3")
assert_eq(ncols(RM), 2, "randmat(3,2): ncols = 2")

# ── bpf (break-point function) ────────────────────────────────────────────────
# Single segment: 0 → 1 in 4 steps  →  <0, 0.25, 0.5, 0.75>

assert_eq(bpf(0, 4, 1), vec(0, 0.25, 0.5, 0.75),
          "bpf single segment 0→1 len=4")

# Two segments:  0→1 (len=4), 1→0 (len=4)
# Result: <0, 0.25, 0.5, 0.75, 1, 0.75, 0.5, 0.25>
assert_eq(bpf(0, 4, 1, 4, 0), vec(0, 0.25, 0.5, 0.75, 1, 0.75, 0.5, 0.25),
          "bpf two segments")

# ── Preprocessing wrappers ────────────────────────────────────────────────────

assert(mat_eq(standardize(M2), zscore(M2)),        "standardize = zscore")
assert(mat_eq(cov_from_zscore(M2), cov(zscore(M2))), "cov_from_zscore")

# ── Summary ───────────────────────────────────────────────────────────────────
test_summary()
