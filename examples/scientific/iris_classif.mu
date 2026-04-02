# ─────────────────────────────────────────────────────────────────────────────
# iris_classif.mu  —  Iris KNN classification
#
# Requires: stdlib.mu, scientific.mu loaded; scientific C++ builtins registered.
#
# Data: data/iris.data.txt — 150 rows, 5 comma-separated fields:
#         sepal_len, sepal_wid, petal_len, petal_wid, species_label
# ─────────────────────────────────────────────────────────────────────────────

load("stdlib.mu")
load("scientific.mu")

# ── 1. Read CSV ───────────────────────────────────────────────────────────────
# readcsv: parse a comma-separated file into an Array of Arrays of strings.
# Blank lines at the end of Iris files are automatically skipped.

proc readcsv (path) {
    var content = read(path)
    var lines   = split(content, "\n")
    var result  = []
    for (var line in lines) {
        var line = trim(line)
        if (len(line) > 0) {
            push(result, split(line, ","))
        }
    }
    return result
}

var IRIS_RAW = readcsv("../../data/iris.data.txt")
print "loaded " len(IRIS_RAW) " rows"

# ── 2. Build dataset of [features_vector, label] samples ─────────────────────
# Each raw row is [f1_str, f2_str, f3_str, f4_str, label_str].
# We parse the 4 numeric fields into a NumVal and keep the label as a string.

proc row2sample (row) {
    var features = vec(num(row[0]), num(row[1]), num(row[2]), num(row[3]))
    var label    = row[4]
    return [features, label]
}

var IRIS_DATA = map(IRIS_RAW, row2sample)

# Shuffle so train/test split is random
var DATA = shuffle(IRIS_DATA)

# ── 3. Dataset info ───────────────────────────────────────────────────────────

var n_samples  = len(DATA)
var n_features = len(DATA[0][0])   # length of feature vector
print n_samples " samples  -  " n_features " features"
print ""

# ── 4. Train / test split (80% / 20%) ────────────────────────────────────────

var trainsz  = floor(0.8 * n_samples)
var trainset = slice(DATA, 0, trainsz)
var testset  = slice(DATA, trainsz, n_samples)

print "split: " len(trainset) " train  /  " len(testset) " test"
print ""

# ── 5. Train KNN ──────────────────────────────────────────────────────────────

print "training..."
var K     = 3
var model = knntrain(trainset, K)
print "done"
print ""

# ── 6. Classify test set ──────────────────────────────────────────────────────

print "testing..."
var predictions = knntest(model, testset)
print "done"
print ""

# ── 7. Accuracy ───────────────────────────────────────────────────────────────

var acc = accuracy(predictions, testset)

print "accuracy       = " fmt_fixed(acc, 4)
print "accuracy (%)   = " fmt_fixed(acc * 100, 2) " %"
