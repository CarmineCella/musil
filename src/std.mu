# std.mu — the general-purpose standard library, Musil half.
#
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# Not loaded automatically: a program that wants it says (load "std.mu"),
# which finds it in the source tree, in MUSIL_PATH, or in ~/.musil after
# `cmake --install`. Everything here is written in the language itself; if a
# function needs C++ for speed or for access to the system, it belongs in
# std.h instead. Merged from Musil 1's stdlib.mu.

# --- constants -------------------------------------------------------------
var tau (* 2 pi)
var euler 2.71828182845904523536
var phi 1.61803398874989484820      # golden ratio
var ln2 0.69314718055994530942
var sqrt2 1.41421356237309504880

# --- scalar math (all elementwise on vectors, since the builtins are) ---------
# (sign x)             -1, 0 or 1
function sign (x) (- (> x 0) (< x 0))
# (even? n) (odd? n)
function even? (n) (== (mod n 2) 0)
function odd? (n) (!= (mod n 2) 0)
# (between? x lo hi)   lo <= x <= hi
function between? (x lo hi) (and (>= x lo) (<= x hi))
# (hypot a b)          sqrt(a^2 + b^2)
function hypot (a b) (sqrt (+ (* a a) (* b b)))
# (gcd a b) (lcm a b)  on non-negative integers
function gcd (a b) (if (== b 0) a (gcd b (mod a b)))
function lcm (a b) (* (/ a (gcd a b)) b)
# (lerp a b t)         linear interpolation, t in [0, 1]
function lerp (a b t) (+ a (* (- b a) t))
# (map-range x in-lo in-hi out-lo out-hi)   re-map x from one range into another
function map-range (x in-lo in-hi out-lo out-hi) (lerp out-lo out-hi (/ (- x in-lo) (- in-hi in-lo)))
# (deg->rad d) (rad->deg r)
function deg->rad (d) (* d (/ pi 180))
function rad->deg (r) (* r (/ 180 pi))

# --- vectors -------------------------------------------------------------
# (zeros n)            vector of n zeros
function zeros (n) (* (range n) 0)
# (ones n)             vector of n ones
function ones (n) (+ (zeros n) 1)
# (linspace a b n)     n points from a to b inclusive
function linspace (a b n) {
    if (< n 2) { error "linspace: n must be >= 2" }
    return (+ a (* (range n) (/ (- b a) (- n 1))))
}
# (mean v)             arithmetic mean
function mean (v) {
    if (== (length v) 0) { error "mean: empty vector" }
    return (/ (sum v) (length v))
}
# (prod v)             product of the elements
function prod (v) (reduce v * 1)
# (dot a b)            inner product (sizes must match)
function dot (a b) (sum (* a b))
# (norm v)             euclidean norm
function norm (v) (sqrt (dot v v))
# (cumsum v)           running sum
function cumsum (v) {
    var out (zeros (length v))
    var acc 0
    for (var k 0) (< k (length v)) (var k (+ k 1)) {
        var acc (+ acc (getidx v k))
        setidx out k acc
    }
    return out
}
# (stdev v)            population standard deviation
function stdev (v) {
    var d (- v (mean v))
    return (sqrt (/ (sum (* d d)) (length v)))
}
# (normalize v)        scale to [0, 1]; a constant vector becomes zeros
function normalize (v) {
    var lo (min v)
    var hi (max v)
    if (== hi lo) { return (zeros (length v)) }
    return (/ (- v lo) (- hi lo))
}
# (diff v)             first differences, one element shorter
function diff (v) (- (drop v 1) (take v (- (length v) 1)))
# (argmin v) (argmax v)   index of the smallest / largest element
function argmin (v) (find v (min v))
function argmax (v) (find v (max v))
# (median v)           the middle value (mean of the two middle values for even length)
function median (v) {
    var n (length v)
    if (== n 0) { error "median: empty vector" }
    var s (sort v)
    if (odd? n) { return (getidx s (floor (/ n 2))) }
    return (/ (+ (getidx s (- (/ n 2) 1)) (getidx s (/ n 2))) 2)
}
# (quantile v q)       value below which a fraction q of the data lies (linear interpolation)
function quantile (v q) {
    if (== (length v) 0) { error "quantile: empty vector" }
    if (or (< q 0) (> q 1)) { error "quantile: q must be in [0, 1]" }
    var s (sort v)
    var pos (* q (- (length v) 1))
    var lo (floor pos)
    var hi (min (+ lo 1) (- (length v) 1))
    return (lerp (getidx s lo) (getidx s hi) (- pos lo))
}
# (histogram v bins)   counts per bin over [min v, max v]; returns (list counts edges)
function histogram (v bins) {
    if (< bins 1) { error "histogram: bins must be >= 1" }
    var lo (min v)
    var hi (max v)
    var width (/ (- hi lo) bins)
    var counts (zeros bins)
    each v (function (x) {
        var k (if (== width 0) 0 (floor (/ (- x lo) width)))
        var k (min k (- bins 1))
        setidx counts k (+ (getidx counts k) 1)
    })
    return (list counts (linspace lo hi (+ bins 1)))
}
# (interp1 xs ys xq)   linear interpolation of the table (xs, ys) at the points xq;
#                      xs must be increasing; xq outside the table is clamped to the ends
function interp1 (xs ys xq) {
    var n (length xs)
    if (!= n (length ys)) { error "interp1: xs and ys must have the same length" }
    function one (x) {
        if (<= x (head xs)) { return (head ys) }
        if (>= x (last xs)) { return (last ys) }
        var k 1
        while (< (getidx xs k) x) { var k (+ k 1) }
        var x0 (getidx xs (- k 1))
        var x1 (getidx xs k)
        return (lerp (getidx ys (- k 1)) (getidx ys k) (/ (- x x0) (- x1 x0)))
    }
    return (map xq one)
}
# (clip v lo hi)       clamp every element to [lo, hi]
function clip (v lo hi) (min (max v lo) hi)
# (fixed x d)          round x to d decimal places
function fixed (x d) (/ (round (* x (pow 10 d))) (pow 10 d))

# --- sequences: list, vector, string --------------------------------------
# (last x)             last element
function last (x) (getidx x -1)
# (take x n)           first n elements
function take (x n) (slice x 0 n)
# (drop x n)           all but the first n elements
function drop (x n) (slice x n)
# (vec->list v)        the elements of a vector as a list
function vec->list (v) {
    var out (list)
    for (var k 0) (< k (length v)) (var k (+ k 1)) { push out (getidx v k) }
    return out
}
# (contains? x v)      is v an element of x (or a substring, for strings)?
function contains? (x v) (>= (find x v) 0)
# (each x f)           call f on every element, for side effects; returns nil
function each (x f) {
    for (var k 0) (< k (length x)) (var k (+ k 1)) { f (getidx x k) }
    return nil
}
# (any? x f)           does f hold for some element?
function any? (x f) {
    for (var k 0) (< k (length x)) (var k (+ k 1)) {
        if (f (getidx x k)) { return true }
    }
    return false
}
# (all? x f)           does f hold for every element?
function all? (x f) {
    for (var k 0) (< k (length x)) (var k (+ k 1)) {
        if (not (f (getidx x k))) { return false }
    }
    return true
}
# (count x f)          number of elements for which f holds
function count (x f) (length (filter x f))
# (concat-list a b)    a new list with the elements of a then those of b
function concat-list (a b) {
    var out (copy a)
    each b (function (e) (push out e))
    return out
}
# (flatten L)          one level of nested lists removed
function flatten (L) (reduce L concat-list (list))
# (zip a b)            list of two-element lists
function zip (a b) {
    var n (min (length a) (length b))
    var out (list)
    for (var k 0) (< k n) (var k (+ k 1)) { push out (list (getidx a k) (getidx b k)) }
    return out
}
# (unique L)           elements in first-seen order, duplicates removed
function unique (L) {
    var out (list)
    each L (function (e) (if (not (contains? out e)) { push out e }))
    return out
}
# (sort-list L)        sort a list of numbers or strings (insertion sort; fine for short lists)
function sort-list (L) {
    var out (copy L)
    for (var k 1) (< k (length out)) (var k (+ k 1)) {
        var x (getidx out k)
        var j (- k 1)
        while (and (>= j 0) (> (getidx out j) x)) {
            setidx out (+ j 1) (getidx out j)
            var j (- j 1)
        }
        setidx out (+ j 1) x
    }
    return out
}
# (min-of L) (max-of L)   smallest / largest element of a list of numbers
function min-of (L) (reduce (tail L) min (head L))
function max-of (L) (reduce (tail L) max (head L))

# --- strings ---------------------------------------------------------------
# (repeat s n)         string s repeated n times
function repeat (s n) (reduce (range n) (function (acc k) (concat acc s)) "")
# (starts-with? s prefix) (ends-with? s suffix)
function starts-with? (s prefix) (equal? (take s (length prefix)) prefix)
function ends-with? (s suffix) {
    if (> (length suffix) (length s)) { return false }
    return (equal? (drop s (- (length s) (length suffix))) suffix)
}
# (replace s old new)  every occurrence of old replaced by new
function replace (s old new) (join (split s old) new)
# (count-substr s pat) number of non-overlapping occurrences of pat
function count-substr (s pat) (- (length (split s pat)) 1)
# (pad-left s w ch) (pad-right s w ch)   pad to width w with character ch
function pad-left (s w ch) (concat (repeat ch (max 0 (- w (length s)))) s)
function pad-right (s w ch) (concat s (repeat ch (max 0 (- w (length s)))))
# (fmt-fixed x d)      x as a string with exactly d decimals: (fmt-fixed pi 4) => "3.1416"
function fmt-fixed (x d) {
    var neg (< x 0)
    var x (abs x)
    var factor (pow 10 d)
    var shifted (floor (+ (* x factor) 0.5))
    var int-part (floor (/ shifted factor))
    var out (str int-part)
    if (> d 0) { var out (concat out "." (pad-left (str (- shifted (* int-part factor))) d "0")) }
    if neg { return (concat "-" out) }
    return out
}
# (fmt-pct x d)        x as a percentage string: (fmt-pct 0.752 1) => "75.2%"
function fmt-pct (x d) (concat (fmt-fixed (* x 100) d) "%")

# --- functions -------------------------------------------------------------
# (identity x)
function identity (x) x
# (compose f g)        the function x -> (f (g x))
function compose (f g) (function (x) (f (g x)))
# (times n f)          call f n times with 0 .. n-1
function times (n f) (each (range n) f)

# --- testing ---------------------------------------------------------------
# (assert-near a b eps msg)   numeric comparison within eps
function assert-near (a b eps msg) {
    if (> (max (abs (- a b))) eps) {
        error (concat msg ": expected " (str b) " within " (str eps) ", got " (str a))
    }
    return nil
}
# (assert-equal actual expected msg)   structural comparison with a helpful message
function assert-equal (actual expected msg) {
    if (not (equal? actual expected)) {
        error (concat msg ": expected " (str expected) ", got " (str actual))
    }
    return nil
}
