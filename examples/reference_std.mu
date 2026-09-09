# musil — library reference: std (std.h + std.mu)
#
# The general-purpose standard library. std.h is the C++ half (registered by
# the interpreter), std.mu the Musil half (loaded here). Everything the core
# already has is in reference.mu; this file shows only what std adds.
# Run with: musil reference_std.mu     (std.mu must be in MUSIL_PATH or ~/.musil)

load "std.mu"
print ""
print "================================================================"
print "  musil: library reference (std)"
print "================================================================"

# --- 1. Vector constructors and statistics ------------------------
print ""
print "--- vectors ---"

print "range 5         :" (range 5)
print "range 2 5       :" (range 2 5)
print "range 0 1 0.25  :" (range 0 1 0.25)
print "range 5 0 -1    :" (range 5 0 -1)
print "linspace 0 1 5  :" (linspace 0 1 5)
print "zeros 4, ones 3 :" (zeros 4) (ones 3)
seed 42
print "rand, rand 3    :" (rand) (rand 3) "(seeded, so reproducible)"

var v (vec 3 1 4 1 5 9 2 6)
print "sort            :" (sort v)
print "mean, stdev     :" (mean v) (stdev v)
print "prod            :" (prod (vec 1 2 3 4))
print "dot, norm       :" (dot (vec 1 2 3) (vec 4 5 6)) (norm (vec 3 4))
print "cumsum          :" (cumsum (vec 1 2 3 4))
print "normalize       :" (normalize (vec 0 5 10))
print "clip -5..5 to 0..1:" (clip (vec -5 0.5 5) 0 1)
print "fixed pi 3      :" (fixed pi 3) "(a rounded number; fmt-fixed below gives a string)"
print "diff            :" (diff (vec 1 4 9 16))
print "argmin, argmax  :" (argmin v) (argmax v)
print "median          :" (median v) (median (vec 4 1 3 2))
print "quantile 0.25   :" (quantile (range 11) 0.25)
print "histogram 3 bins:" (histogram v 3) "(counts, then the bin edges)"
print "interp1         :" (interp1 (vec 0 1 2) (vec 0 10 0) (vec -1 0.5 1.5 3)) "(clamped outside the table)"

# --- 2. Scalar math (elementwise on vectors too) ------------------
print ""
print "--- scalar math ---"

print "constants tau euler phi ln2 sqrt2:" tau euler phi ln2 sqrt2
print "sign (vec -3 0 2):" (sign (vec -3 0 2))
print "even? 4, odd? 4  :" (even? 4) (odd? 4)
print "between? 5 1 10  :" (between? 5 1 10)
print "hypot 3 4        :" (hypot 3 4)
print "gcd 48 18, lcm 4 6:" (gcd 48 18) (lcm 4 6)
print "lerp 0 10 0.25   :" (lerp 0 10 0.25)
print "map-range 5 0 10 0 100:" (map-range 5 0 10 0 100)
print "deg->rad 180     :" (deg->rad 180)
print "rad->deg pi      :" (rad->deg pi)

# --- 3. Sequences: lists, vectors and strings alike ---------------
print ""
print "--- sequences ---"

var L (list "a" "b" "c" "d" "e")
print "last            :" (last L) (last v) (last "xyz")
print "reverse         :" (reverse L) (reverse (vec 1 2 3)) (reverse "abc")
print "slice L 1 3     :" (slice L 1 3)
print "slice L 3       :" (slice L 3) "(to the end)"
print "slice L -2      :" (slice L -2) "(negative start counts from the end)"
print "take 2, drop 3  :" (take L 2) (drop L 3)
print "find            :" (find L "c") (find v 9) (find "hello" "ll") "(-1 when absent)"
print "contains?       :" (contains? L "b") (contains? "musil" "si")
print "concat-list     :" (concat-list (list 1 2) (list 3))
print "flatten         :" (flatten (list (list 1 2) (list) (list 3)))
print "zip             :" (zip (list "x" "y") (vec 1 2))
print "unique          :" (unique (list 1 2 1 3 2))
print "sort-list       :" (sort-list (list "pear" "apple" "fig")) (sort-list (list 3 1 2))
print "min-of, max-of  :" (min-of (list 3 1 2)) (max-of (list 3 1 2))

# --- 4. Strings -----------------------------------------------------
print ""
print "--- strings ---"

print "concat          :" (concat "hello, " "world" "! " 42)
print "split           :" (split "a,b,c,d" ",") (split "abc" "")
print "join            :" (join (list "x" "y" "z") "-")
print "upper, lower    :" (upper "musil") (lower "MUSIL")
print "trim            :" (concat "[" (trim "  hi  ") "]")
print "format          :" (format "{} is {} years old" "Robert" 42)
print "chr 65, ord \"A\"  :" (chr 65) (ord "A")
print "starts/ends-with?:" (starts-with? "musil" "mu") (ends-with? "musil" "sil")
print "replace         :" (replace "a-b-c" "-" "+")
print "count-substr    :" (count-substr "banana" "an")
print "repeat          :" (repeat "ab" 3)
print "pad-left/right  :" (pad-left "7" 3 "0") (pad-right "ab" 4 ".")
print "fmt-fixed pi 4  :" (fmt-fixed pi 4) "(a string with exactly 4 decimals)"
print "fmt-fixed 2 3   :" (fmt-fixed 2 3)
print "fmt-pct 0.752 1 :" (fmt-pct 0.752 1)

# --- 5. Higher-order: map, filter, reduce, each -------------------
print ""
print "--- higher-order (polymorphic on list and vec) ---"

function sq (x) (* x x)
print "map list, vec   :" (map (list 1 2 3) sq) (map (vec 1 2 3) sq)
print "filter          :" (filter (range 10) (function (x) (== (mod x 3) 0)))
print "reduce          :" (reduce (range 5) + 0) (reduce (list "a" "b") (function (acc x) (concat acc x)) "")
print "any?, all?      :" (any? (list 1 3 4) even?) (all? (vec 2 4 6) even?)
print "count           :" (count (range 10) (function (x) (> x 6)))
var seen (list)
each (list 1 2 3) (function (x) (push seen (* x 10)))
print "each (side effects):" seen
times 3 (function (k) (print "  times:" k))
print "identity, compose:" (identity 7) ((compose sq (function (x) (+ x 1))) 3)
function add (a b) (+ a b)
print "map with a partial:" (map (vec 1 2 3) (add 100))

# --- 6. Files and console ------------------------------------------
print ""
print "--- files ---"

var path "/tmp/musil_std_demo.txt"
write path "first line\nsecond line\n"
append-file path "third line\n"
print "exists?         :" (exists? path)
print "read            :"
print (read path)
print "write non-string:" (do (write path (vec 1 2 3)) (read path))
# (input "prompt") reads a line from the console; not shown here

# --- 7. Testing ------------------------------------------------------
print ""
print "--- testing ---"

assert (== (+ 2 2) 4) "arithmetic works"
print "assert          :" (try (assert (== 1 2) "one is not two") catch e e)
print "assert-equal    :" (try (assert-equal (list 1) (list 2) "lists") catch e e)
print "assert-near     :" (type (assert-near (sin pi) 0 1e-10 "sin pi")) "(nil: passed)"
print "assert-near fail:" (try (assert-near 1 2 0.1 "off") catch e e)

print ""
print "================================================================"
print "  end of std reference"
print "================================================================"
