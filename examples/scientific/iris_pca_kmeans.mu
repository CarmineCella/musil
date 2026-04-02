# ─────────────────────────────────────────────────────────────────────────────
# iris_pca_kmeans.mu  —  PCA and k-means on the Iris dataset
#
# Requires: stdlib.mu, scientific.mu loaded; scientific C++ builtins registered.
# Plotting calls require the plotting library to be loaded and its builtins
# registered — those are stubbed here until the plotting library is converted.
#
# Data: data/iris.data.txt — 150 rows, 5 comma-separated fields:
#         sepal_len, sepal_wid, petal_len, petal_wid, species_label
# ─────────────────────────────────────────────────────────────────────────────

load("stdlib.mu")
load("scientific.mu")
# load("plotting.mu")    # uncomment once the plotting library is converted

# ── 1. Read CSV ───────────────────────────────────────────────────────────────

proc readcsv (path) {
    var content = read(path)
    var lines   = split(content, "\n")
    var result  = []
    for (var line in lines) {
        var line = trim(line)
        if (len(line) > 0) { push(result, split(line, ",")) }
    }
    return result
}

var IRIS_RAW = readcsv("../../data/iris.data.txt")
print "loaded Iris CSV, rows: " len(IRIS_RAW)

# ── 2. Build numeric feature matrix (4 columns) ───────────────────────────────
# Keep only the 4 numeric fields from each row; discard the label.
# list2mat converts an Array of numeric Arrays into a matrix (Array of Vectors).

var IRIS_NUM_ROWS = map(IRIS_RAW, proc (row) {
    return [num(row[0]), num(row[1]), num(row[2]), num(row[3])]
})

var IRIS_X    = list2mat(IRIS_NUM_ROWS)
var IRIS_ROWS = nrows(IRIS_X)
var IRIS_COLS = ncols(IRIS_X)

print "Iris numeric matrix: " IRIS_ROWS " samples,  " IRIS_COLS " features"

# ── 3. PCA on the 4D features ─────────────────────────────────────────────────
# pca() returns a d x (d+1) matrix; the last column holds eigenvalues.
# For Iris: d = 4, so the eigenvalue column is index 4.

var IRIS_PCA = pca(IRIS_X)
var IRIS_EIG = matcol(IRIS_PCA, 4)     # eigenvalues as a NumVal

print ""
print "── PCA eigenvalues ──"
var ei = 0
while (ei < len(IRIS_EIG)) {
    print "  PC" (ei+1) ":  " fmt_fixed(IRIS_EIG[ei], 6)
    ei = ei + 1
}

# Plotting: uncomment when the plotting library is available
# plot("Iris PCA eigenvalues", IRIS_EIG, "eigenvalues", "*")

# ── 4. K-means on petal features (length, width) ─────────────────────────────
# Extract petal length (col 2) and petal width (col 3) as NumVals.

var PETAL_LEN = matcol(IRIS_X, 2)
var PETAL_WID = matcol(IRIS_X, 3)

# Build N x 2 matrix with (length, width)
var PETAL_MAT = stack2(PETAL_LEN, PETAL_WID)

# K-means with K = 3 (three Iris species)
var KM_RES    = kmeans(PETAL_MAT, 3)
var KM_LABELS     = kmeans_labels(KM_RES)       # NumVal of cluster indices (0,1,2)
var KM_CENTROIDS  = kmeans_centroids(KM_RES)    # 3 x 2 matrix

print ""
print "── K-means centroids (petal length, petal width) ──"
var ci = 0
while (ci < nrows(KM_CENTROIDS)) {
    var c = KM_CENTROIDS[ci]
    print "  cluster " ci ":  len=" fmt_fixed(c[0], 3) "  wid=" fmt_fixed(c[1], 3)
    ci = ci + 1
}

# Extract centroid columns for plotting
var CENTS_X = matcol(KM_CENTROIDS, 0)
var CENTS_Y = matcol(KM_CENTROIDS, 1)

# Print a compact label summary (how many points in each cluster)
var cluster_counts = arr(3, 0)
for (var lbl in to_arr(KM_LABELS)) {
    cluster_counts[lbl] = cluster_counts[lbl] + 1
}
print ""
print "── Cluster sizes ──"
var si = 0
while (si < 3) {
    print "  cluster " si ": " cluster_counts[si] " points"
    si = si + 1
}

# Plotting (uncomment when plotting library is available):
# scatter("Iris: petal length vs width",
#         PETAL_LEN, PETAL_WID, "samples", ".")
# scatter("Iris k-means centroids",
#         PETAL_LEN, PETAL_WID, "samples",
#         CENTS_X,  CENTS_Y,   "centroids", ".")

print ""
print "done  (scatter plots stubbed — load plotting.mu to enable)"
