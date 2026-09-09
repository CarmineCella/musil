# iris: PCA, k-means and KNN classification on the Iris data set
#
# data/iris.data.txt has 150 rows: sepal length, sepal width, petal length,
# petal width, species. Run from the examples directory.

load "system.mu"
load "scientific.mu"

var rows (read-csv "data/iris.data.txt")
print "loaded" (length rows) "rows"

# features as a 150 x 4 matrix; labels as a list of strings
var X (map rows (function (r) (vec (take r 4))))
var labels (map rows last)
print "matrix:" (mat-shape X) "  species:" (unique labels)

# --- PCA ------------------------------------------------------------------
var P (pca X)
print ""
print "-- PCA --"
print "eigenvalues      :" (fixed (pca-eigenvalues P) 4)
print "explained (%)    :" (fixed (* 100 (pca-explained P)) 1)
print "first direction  :" (fixed (head (pca-directions P)) 3)
var scores (pca-scores X 2)
print "scores of the first 3 samples on PC1, PC2:"
mat-print (get-rows scores 0 3)

# --- k-means on petal length and width ---------------------------------------
var petals (get-cols X 2 2)
var km (kmeans petals 3)
var lab (kmeans-labels km)
print ""
print "-- k-means, k = 3, on (petal length, petal width) --"
print "centroids:"
mat-print-with (kmeans-centroids km) 3
print "cluster sizes:" (cluster-sizes lab 3)
# how well do the clusters follow the species? count the majority species per cluster
each (range 3) (function (c) {
    var members (filter (zip labels lab) (function (p) (== (last p) c)))
    var species (unique (map members head))
    print "  cluster" c ":" (map species (function (s) (list s (count members (function (p) (equal? (head p) s))))))
})

# --- KNN classification ----------------------------------------------------------
seed 42
var samples (shuffle (zip (map X identity) labels))
var split (train-test-split samples 0.8)
var training (head split)
var test (last split)
print ""
print "-- KNN --"
print "split:" (length training) "train /" (length test) "test"
var model (knn-model training 3)
var predictions (knn-test model test)
print "accuracy (k=3):" (fmt-pct (accuracy predictions test) 1)
print "first 5 predictions:" (take predictions 5)
print "their labels       :" (take (map test last) 5)
print "one query" (fixed (head (head test)) 1) "->" (knn-predict-one model (head (head test)))
