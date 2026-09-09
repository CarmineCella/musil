# chords: pitch-class sets as vectors, and what the scientific library says about them
#
# A pitch class is a number 0..11 (C = 0). A chord is a vector of pitch classes.
# Transposition and inversion are arithmetic mod 12; the interval vector and the
# 12-bin profile turn a chord into features; distances, clustering and PCA
# then apply as to any data.

load "scientific.mu"

var names (split "C,C#,D,D#,E,F,F#,G,G#,A,A#,B" ",")
function pc-name (pc) (getidx names (mod pc 12))
function chord-name (chord) (join (map (vec->list chord) pc-name) " ")

# --- transformations ---
function transpose-pc (chord n) (sort (mod (+ chord n) 12))
function invert-pc (chord) (sort (mod (- 12 chord) 12))
# normal form: the rotation of the sorted set that is most packed to the left
function normal-form (chord) {
    var s (sort chord)
    var n (length s)
    var rotations (map (vec->list (range n)) (function (k) (mod (+ 12 (- (vec (drop s k) (take s k)) (getidx s k))) 12)))
    var spans (vec (map rotations last))
    return (getidx rotations (argmin spans))
}

var C-major (vec 0 4 7)
var A-minor (vec 9 0 4)
print "C major         :" (chord-name C-major)
print "up a fifth      :" (chord-name (transpose-pc C-major 7))
print "inverted        :" (chord-name (invert-pc C-major)) "(C minor: the inversion of a major triad)"
print "normal form Am  :" (normal-form A-minor) "  C major:" (normal-form C-major) "(the two triad shapes)"

# --- features ---
# interval-class vector: how many of each interval class 1..6 the chord contains
function interval-vector (chord) {
    var iv (zeros 6)
    var n (length chord)
    for (var a 0) (< a n) (var a (+ a 1)) {
        for (var b (+ a 1)) (< b n) (var b (+ b 1)) {
            var d (mod (abs (- (getidx chord a) (getidx chord b))) 12)
            var ic (min d (- 12 d))
            setidx iv (- ic 1) (+ (getidx iv (- ic 1)) 1)
        }
    }
    return iv
}
# 12-bin profile: which pitch classes are present
function profile (chord) (vec (map (vec->list (range 12)) (function (pc) (contains? chord pc))))

print ""
print "interval vector of C major   :" (interval-vector C-major)
print "interval vector of C dim7    :" (interval-vector (vec 0 3 6 9))
print "profile of C major           :" (profile C-major)

# --- a small chord vocabulary ---
var vocabulary (list
    (list "C"      (vec 0 4 7))
    (list "Cm"     (vec 0 3 7))
    (list "G"      (vec 7 11 2))
    (list "Am"     (vec 9 0 4))
    (list "F"      (vec 5 9 0))
    (list "Cdim"   (vec 0 3 6))
    (list "Caug"   (vec 0 4 8))
    (list "C7"     (vec 0 4 7 10))
    (list "Cmaj7"  (vec 0 4 7 11))
    (list "Dm7"    (vec 2 5 9 0))
    (list "F#"     (vec 6 10 1))
    (list "Bdim"   (vec 11 2 5)))
var labels (map vocabulary head)
var chords (map vocabulary last)
var P (map chords profile)                 # 12 chords x 12 pitch classes

# --- distances: which chords share the most notes? ---
print ""
print "--- distances between chord profiles (0 = same notes) ---"
var D (dist-matrix P)
var idx (vec->list (range (length labels)))
each idx (function (i) {
    var row (getidx D i)
    var others (filter (vec->list (range (length labels))) (function (j) (!= j i)))
    var j (getidx others (argmin (vec (map others (function (j) (getidx row j))))))
    print (pad-right (getidx labels i) 6 " ") "closest:" (getidx labels j)
})
print "farthest from C major:" (getidx labels (argmax (head D)))

# --- clustering: the profiles fall into groups by shared notes ---
print ""
print "--- k-means on the profiles, k = 3 ---"
var km (kmeans P 3)
each (range 3) (function (c) {
    var members (filter (zip labels (vec->list (kmeans-labels km))) (function (p) (== (last p) c)))
    print "cluster" c ":" (join (map members head) " ")
})

# --- PCA: a two-dimensional chord map ---
print ""
print "--- PCA of the profiles: coordinates on the first two components ---"
var scores (pca-scores P 2)
print "explained variance:" (fixed (take (pca-explained (pca P)) 3) 3)
each (zip labels scores) (function (p) (print (pad-right (head p) 6 " ") (fixed (last p) 2)))

# --- interval-vector similarity, the set-theory way ---
print ""
print "--- interval vectors ---"
var IV (map chords interval-vector)
each (zip labels IV) (function (p) (print (pad-right (head p) 6 " ") (last p)))
print "Cdim and Bdim are the same set class:" (equal? (interval-vector (vec 0 3 6)) (interval-vector (vec 11 2 5)))
print "C and Cm share an interval vector too:" (equal? (interval-vector C-major) (interval-vector (vec 0 3 7))) "(Z-relation aside, inversion preserves it)"
