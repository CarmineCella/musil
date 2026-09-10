# iris_plot: the Iris data set as pictures: k-means clusters, PCA scores, a histogram
# and the distance matrix. Run from the examples directory.
load "system.mu"
load "plot.mu"

var rows (read-csv "data/iris.data.txt")
var X (map rows (function (r) (vec (take r 4))))
var labels (map rows last)
var species (unique labels)

# 1. petal length vs width, coloured by k-means cluster (k = 3)
var petals (get-cols X 2 2)
var km (kmeans petals 3)
var f1 (scatter-clusters petals (kmeans-labels km) 3)
set-labels f1 "petal length" "petal width"
save-png f1 "/tmp/musil_iris_kmeans.png"
show f1
print "k-means cluster sizes:" (cluster-sizes (kmeans-labels km) 3)

# 2. the same points coloured by their true species
var f2 (figure "species")
each species (function (s) {
    var pts (map (filter (zip petals labels) (function (p) (equal? (last p) s))) head)
    add-scatter f2 (vec (map pts head)) (vec (map pts last)) s
})
set-labels f2 "petal length" "petal width"
save-png f2 "/tmp/musil_iris_species.png"
show f2

# 3. PCA: all four features projected on the first two components
var scores (pca-scores X 2)
var f3 (figure "PCA of the four features")
each species (function (s) {
    var pts (map (filter (zip scores labels) (function (p) (equal? (last p) s))) head)
    add-scatter f3 (vec (map pts head)) (vec (map pts last)) s
})
set-labels f3 "PC1" "PC2"
save-png f3 "/tmp/musil_iris_pca.png"
show f3
print "variance explained:" (fixed (* 100 (pca-explained (pca X))) 1) "%"

# 4. a histogram of sepal lengths
var f4 (histogram-figure (mat-col X 0) 12)
set-labels f4 "sepal length" "count"
save-png f4 "/tmp/musil_iris_hist.png"
show f4

# 5. the pairwise distance matrix as an image: three blocks for three species
var f5 (add-image (figure "distances between samples") (dist-matrix X))
save-png f5 "/tmp/musil_iris_dist.png"
show f5
print "saved /tmp/musil_iris_{kmeans,species,pca,hist,dist}.png"
