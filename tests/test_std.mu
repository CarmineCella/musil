# test_std.mu — systematic, self-checking test of the standard library (std.h + std.mu).
#
# Every check is a (check ...) from test.mu. A passing run prints only the final line.
# Run with: musil tests/test_std.mu

load "test.mu"

function square (x) (* x x)
function inc (x) (+ x 1)

# --- vectors (std.h: range, seed, rand, sort; std.mu: zeros, ones, linspace, mean, prod, dot, norm, cumsum, clip, fixed) ---
check (equal? (range 4) (vec 0 1 2 3)) "range n"
check (equal? (range 2 5) (vec 2 3 4)) "range a b"
check (equal? (range 0 1 0.25) (vec 0 0.25 0.5 0.75)) "range a b step"
check (equal? (range 5 0 -2) (vec 5 3 1)) "range negative step"
check (== (length (range 0)) 0) "range 0 is empty"
check (contains? (error-of (function () (range 0 1 0))) "nonzero") "range: zero step is an error"
check (contains? (error-of (function () (range))) "range: expected") "range: arity"
seed 42
var r1 (rand 5)
seed 42
var r2 (rand 5)
check (equal? r1 r2) "seed makes rand reproducible"
check (== (length r1) 5) "rand n"
check (and (>= (rand) 0) (< (rand) 1)) "rand in [0,1)"
check (equal? (sort (vec 3 1 2)) (vec 1 2 3)) "sort"
check (equal? (zeros 3) (vec 0 0 0)) "zeros"
check (equal? (ones 2) (vec 1 1)) "ones"
check (== (length (zeros 0)) 0) "zeros 0"
check (equal? (linspace 0 1 5) (vec 0 0.25 0.5 0.75 1)) "linspace"
check (contains? (error-of (function () (linspace 0 1 1))) ">= 2") "linspace: n must be >= 2"
var v (vec 1 2 3)
check (== (mean v) 2) "mean"
check (contains? (error-of (function () (mean (vec)))) "empty") "mean: empty vector"
check (== (prod v) 6) "prod"
check (== (prod (vec)) 1) "prod: empty vector is 1"
check (== (dot v v) 14) "dot"
check (contains? (error-of (function () (dot v (vec 1 2)))) "size mismatch") "dot: size mismatch"
check (== (norm (vec 3 4)) 5) "norm"
check (equal? (cumsum (vec 1 2 3 4)) (vec 1 3 6 10)) "cumsum"
check (equal? (clip (vec -5 0.5 5) 0 1) (vec 0 0.5 1)) "clip"
check (== (fixed 3.14159 2) 3.14) "fixed"
check (equal? (fixed (vec 1.234 5.678) 1) (vec 1.2 5.7)) "fixed: elementwise"

# --- from Musil 1's stdlib: constants, scalar math, statistics ---
check (< (abs (- tau (* 2 pi))) 1e-12) "tau"
check (< (abs (- (log euler) 1)) 1e-12) "euler"
check (< (abs (- (* phi phi) (+ phi 1))) 1e-12) "phi: golden ratio identity"
check (< (abs (- (exp ln2) 2)) 1e-12) "ln2"
check (< (abs (- (* sqrt2 sqrt2) 2)) 1e-12) "sqrt2"
check (equal? (sign (vec -3 0 2)) (vec -1 0 1)) "sign: elementwise"
check (and (even? 4) (odd? 7) (not (even? 7))) "even? odd?"
check (and (between? 5 1 10) (not (between? 11 1 10))) "between?"
check (== (hypot 3 4) 5) "hypot"
check (== (gcd 48 18) 6) "gcd"
check (== (lcm 4 6) 12) "lcm"
check (== (lerp 0 10 0.25) 2.5) "lerp"
check (equal? (lerp (vec 0 0) (vec 10 20) 0.5) (vec 5 10)) "lerp: vectors"
check (== (map-range 5 0 10 0 100) 50) "map-range"
check (< (abs (- (deg->rad 180) pi)) 1e-12) "deg->rad"
check (== (rad->deg pi) 180) "rad->deg"
check (== (stdev (vec 2 4 4 4 5 5 7 9)) 2) "stdev: population"
check (equal? (normalize (vec 0 5 10)) (vec 0 0.5 1)) "normalize"
check (equal? (normalize (vec 3 3)) (vec 0 0)) "normalize: constant vector"
check (== (min-of (list 3 1 2)) 1) "min-of"
check (== (max-of (list 3 1 2)) 3) "max-of"

# --- more vector statistics (std.mu: diff, argmin, argmax, median, quantile, histogram, interp1) ---
check (equal? (diff (vec 1 4 9 16)) (vec 3 5 7)) "diff"
check (== (length (diff (vec 1))) 0) "diff: single element"
check (== (argmin (vec 3 1 2)) 1) "argmin"
check (== (argmax (vec 3 1 2)) 0) "argmax"
check (== (median (vec 3 1 2)) 2) "median: odd"
check (== (median (vec 4 1 3 2)) 2.5) "median: even"
check (contains? (error-of (function () (median (vec)))) "empty") "median: empty"
check (== (quantile (range 11) 0.5) 5) "quantile: median"
check (== (quantile (range 11) 0.25) 2.5) "quantile: interpolates"
check (== (quantile (vec 7) 0.9) 7) "quantile: single element"
check (contains? (error-of (function () (quantile (vec 1) 2))) "[0, 1]") "quantile: range"
var h (histogram (vec 1 2 2 3 9) 2)
check (equal? (head h) (vec 4 1)) "histogram: counts"
check (equal? (last h) (vec 1 5 9)) "histogram: edges"
check (equal? (head (histogram (vec 5 5 5) 3)) (vec 3 0 0)) "histogram: constant data"
check (equal? (interp1 (vec 0 1 2) (vec 0 10 0) (vec 0.5 1.5)) (vec 5 5)) "interp1"
check (equal? (interp1 (vec 0 1) (vec 0 1) (vec -1 5)) (vec 0 1)) "interp1: clamps outside the table"
check (equal? (interp1 (vec 0 1) (vec 3 4) (vec 0 1)) (vec 3 4)) "interp1: hits the knots"
check (contains? (error-of (function () (interp1 (vec 0 1) (vec 1) (vec 0)))) "same length") "interp1: table sizes"

# --- sequences (std.h: reverse, slice, find; std.mu: last, take, drop, contains?, each, any?, all?, count, concat-list, flatten, zip, unique, sort-list, repeat) ---
var L (list 1 "two" 3)
check (== (last L) 3) "last: list"
check (== (last v) 3) "last: vec"
check (equal? (last "abc") "c") "last: string"
check (contains? (error-of (function () (last (list)))) "out of range") "last: empty list is an error"
check (equal? (reverse (list 1 2 3)) (list 3 2 1)) "reverse: list"
check (equal? (reverse v) (vec 3 2 1)) "reverse: vec"
check (equal? (reverse "abc") "cba") "reverse: string"
check (equal? (slice (list 1 2 3 4 5) 1 3) (list 2 3 4)) "slice: list"
check (equal? (slice (list 1 2 3 4 5) 3) (list 4 5)) "slice: to end"
check (equal? (slice (list 1 2 3 4 5) -2) (list 4 5)) "slice: negative start"
check (equal? (slice (range 10) 2 3) (vec 2 3 4)) "slice: vec"
check (equal? (slice "hello" 1 3) "ell") "slice: string"
check (equal? (slice (list 1 2) 5) (list)) "slice: start past the end is empty"
check (equal? (take (range 10) 3) (vec 0 1 2)) "take"
check (equal? (vec->list (vec 1 2)) (list 1 2)) "vec->list"
check (equal? (drop (list 1 2 3 4) 2) (list 3 4)) "drop"
check (== (find (list 1 "x" 3) "x") 1) "find: list"
check (== (find (list 1 2) 9) -1) "find: missing"
check (== (find v 3) 2) "find: vec"
check (== (find "hello" "ll") 2) "find: string"
check (contains? (list 1 2 3) 2) "contains?: list"
check (not (contains? (list 1 2 3) 9)) "contains?: missing"
check (contains? "musil" "si") "contains?: substring"
var seen (list)
each (list 1 2) (function (x) (push seen x))
check (equal? seen (list 1 2)) "each: list"
var acc 0
each (vec 1 2 3) (function (x) (set acc (+ acc x)))
check (== acc 6) "each: vec"
check (equal? (type (each (list) inc)) "nil") "each returns nil"
check (any? (list 1 3 4) (function (x) (== (mod x 2) 0))) "any?: yes"
check (not (any? (list 1 3 5) (function (x) (== (mod x 2) 0)))) "any?: no"
check (all? (vec 2 4 6) (function (x) (== (mod x 2) 0))) "all?: yes"
check (not (all? (vec 2 3 6) (function (x) (== (mod x 2) 0)))) "all?: no"
check (all? (list) inc) "all?: empty is true"
check (== (count (range 10) (function (x) (> x 6))) 3) "count"
check (equal? (concat-list (list 1) (list 2 3)) (list 1 2 3)) "concat-list"
check (equal? (concat-list (list) (list)) (list)) "concat-list: empty"
var orig (list 1)
concat-list orig (list 2)
check (equal? orig (list 1)) "concat-list does not mutate its arguments"
check (equal? (flatten (list (list 1 2) (list) (list 3))) (list 1 2 3)) "flatten"
check (equal? (zip (list "a" "b") (vec 1 2 3)) (list (list "a" 1) (list "b" 2))) "zip: truncates to the shorter"
check (equal? (unique (list 1 2 1 3 2)) (list 1 2 3)) "unique"
check (equal? (sort-list (list 3 1 2)) (list 1 2 3)) "sort-list: numbers"
check (equal? (sort-list (list "pear" "apple")) (list "apple" "pear")) "sort-list: strings"
check (equal? (sort-list (list)) (list)) "sort-list: empty"
check (equal? (repeat "ab" 3) "ababab") "repeat"
check (equal? (repeat "x" 0) "") "repeat: zero"

# --- strings (std.mu: starts-with?, ends-with?, replace, count-substr, pad-left, pad-right, fmt-fixed, fmt-pct) ---
check (starts-with? "musil" "mu") "starts-with?"
check (not (starts-with? "musil" "si")) "starts-with?: no"
check (ends-with? "musil" "sil") "ends-with?"
check (not (ends-with? "sil" "musil")) "ends-with?: suffix longer than string"
check (equal? (replace "a-b-c" "-" "+") "a+b+c") "replace"
check (equal? (replace "abc" "x" "y") "abc") "replace: no match"
check (== (count-substr "banana" "an") 2) "count-substr"
check (== (count-substr "abc" "x") 0) "count-substr: none"
check (equal? (pad-left "7" 3 "0") "007") "pad-left"
check (equal? (pad-right "ab" 4 ".") "ab..") "pad-right"
check (equal? (pad-left "long" 2 "0") "long") "pad-left: already wide enough"
check (equal? (fmt-fixed pi 4) "3.1416") "fmt-fixed"
check (equal? (fmt-fixed 2 3) "2.000") "fmt-fixed: pads zeros"
check (equal? (fmt-fixed -1.5 0) "-2") "fmt-fixed: zero decimals rounds half up"
check (equal? (fmt-fixed 0.05 1) "0.1") "fmt-fixed: rounding"
check (equal? (fmt-pct 0.752 1) "75.2%") "fmt-pct"

# --- strings (std.h) ---
check (equal? (concat "a" "b" 42 (list 1)) "ab42(1)") "concat"
check (equal? (split "a,b,c" ",") (list "a" "b" "c")) "split"
check (equal? (split "abc" "") (list "a" "b" "c")) "split: chars"
check (equal? (split "" ",") (list "")) "split: empty string gives one empty field"
check (equal? (join (list "x" "y") "-") "x-y") "join"
check (equal? (join (list) "-") "") "join: empty"
check (equal? (upper "musil") "MUSIL") "upper"
check (equal? (lower "MUSIL") "musil") "lower"
check (contains? (error-of (function () (upper 5))) "expected string") "upper: type"
check (equal? (trim "  hi \n") "hi") "trim"
check (equal? (format "{} + {} = {}" 1 2 3) "1 + 2 = 3") "format"
check (equal? (format "no holes") "no holes") "format: no arguments"
check (contains? (error-of (function () (format "{} {}" 1))) "not enough") "format: too few"
check (contains? (error-of (function () (format "{}" 1 2))) "too many") "format: too many"
check (equal? (chr 65) "A") "chr"
check (== (ord "A") 65) "ord"
check (contains? (error-of (function () (ord ""))) "empty") "ord: empty"

# --- files and console (std.h) ---
var path "/tmp/musil_test_std.txt"
write path "hello\n"
append-file path "world\n"
check (equal? (read path) "hello\nworld\n") "write/append-file/read"
check (== (exists? path) 1) "exists?"
check (== (exists? "/tmp/definitely/not/here") 0) "exists?: missing"
check (contains? (error-of (function () (read "/tmp/definitely/not/here"))) "cannot open") "read: missing file"
write path (vec 1 2 3)
check (equal? (read path) "(1 2 3)") "write: non-strings are printed"

# --- higher-order (std.h: map, filter, reduce) ---
check (equal? (map (list 1 2 3) square) (list 1 4 9)) "map: list"
check (equal? (map (vec 1 2 3) square) (vec 1 4 9)) "map: vec"
check (equal? (map (list) square) (list)) "map: empty list"
check (contains? (error-of (function () (map "s" square))) "expected list or number") "map: type"
check (contains? (error-of (function () (map (list 1) 5))) "expected function") "map: fn type"
check (contains? (error-of (function () (map (vec 1) (function (x) "s")))) "expected number") "map on vec requires numeric results"
check (equal? (filter (list 1 2 3 4) (function (x) (> x 2))) (list 3 4)) "filter: list"
check (equal? (filter (range 10) (function (x) (== (mod x 3) 0))) (vec 0 3 6 9)) "filter: vec"
check (equal? (reduce (list "a" "b") (function (acc x) (concat acc x)) "") "ab") "reduce: list"
check (== (reduce (range 5) + 0) 10) "reduce: vec with a builtin"
check (== (reduce (list) + 42) 42) "reduce: empty returns the initial value"
function add (a b) (+ a b)
check (equal? (map (vec 1 2 3) (add 10)) (vec 11 12 13)) "map with a partial application"

# --- functions (std.mu: identity, compose, times) ---
check (== (identity 7) 7) "identity"
check (equal? (identity "s") "s") "identity: any type"
check (== ((compose square inc) 3) 16) "compose: (f (g x))"
var ticks 0
times 4 (function (k) (set ticks (+ ticks 1)))
check (== ticks 4) "times"

# --- testing (std.h: assert; std.mu: assert-equal) ---
check (equal? (type (assert 1)) "nil") "assert: passes silently"
check (contains? (error-of (function () (assert 0 "custom" 42))) "custom 42") "assert: message"
check (contains? (error-of (function () (assert 0))) "assertion failed") "assert: default message"
check (equal? (type (assert-equal (list 1 2) (list 1 2) "same")) "nil") "assert-equal: passes"
check (contains? (error-of (function () (assert-equal 1 2 "one vs two"))) "one vs two: expected 2, got 1") "assert-equal: message"
check (equal? (type (assert-near (sin pi) 0 1e-10 "sin pi")) "nil") "assert-near: passes"
check (equal? (type (assert-near (vec 1 2) (vec 1.001 2) 0.01 "vec")) "nil") "assert-near: vectors"
check (contains? (error-of (function () (assert-near 1 2 0.1 "off"))) "off: expected 2 within 0.1") "assert-near: message"

report "test_std"
