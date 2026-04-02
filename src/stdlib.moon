# ─────────────────────────────────────────────────────────────────────────────
# Moonil standard library
# load with:  load("stdlib.moon")
# ─────────────────────────────────────────────────────────────────────────────

# ── Constants ─────────────────────────────────────────────────────────────────

var PI    = 3.14159265358979
var TAU   = 6.28318530717959    # 2 * PI
var E     = 2.71828182845905
var PHI   = 1.61803398874989    # golden ratio
var LN2   = 0.693147180559945
var SQRT2 = 1.41421356237310

# ── Core math ─────────────────────────────────────────────────────────────────

# Smaller / larger of two scalars
proc min (a, b) {
    if (a < b) { return a }
    return b
}

proc max (a, b) {
    if (a > b) { return a }
    return b
}

# Clamp works on both scalars and vectors (broadcasting handles vectors)
proc clamp (x, lo, hi) {
    return min(max(x, lo), hi)
}

proc sign (x) {
    if (x > 0) { return 1 }
    if (x < 0) { return -1 }
    return 0
}

proc round (x)              { return floor(x + 0.5) }
proc round_to (x, decimals) {
    var factor = pow(10, decimals)
    return floor(x * factor + 0.5) / factor
}

# Modulo — always returns positive result, consistent for negative inputs
proc mod (a, b) { return a - floor(a / b) * b }

proc even (n)   { return mod(n, 2) == 0 }
proc odd  (n)   { return mod(n, 2) != 0 }

# Returns 1 if lo <= x <= hi, else 0
proc between (x, lo, hi) { return x >= lo and x <= hi }

# Euclidean distance between two scalars
proc hypot (a, b) { return sqrt(a * a + b * b) }

proc gcd (a, b) {
    a = abs(a)
    b = abs(b)
    while (a != b) {
        while (a > b) { a = a - b }
        while (b > a) { b = b - a }
    }
    return a
}

proc lcm (a, b) { return (a / gcd(a, b)) * b }

proc ipow (base, exp) {
    var r = 1
    while (exp > 0) { r = r * base   exp = exp - 1 }
    return r
}

# Linear interpolation — works on both scalars and vectors (broadcasting)
proc lerp (a, b, t)  { return a + (b - a) * t }

# Re-map x from one range into another
proc map_range (x, in_lo, in_hi, out_lo, out_hi) {
    return out_lo + (x - in_lo) * (out_hi - out_lo) / (in_hi - in_lo)
}

proc deg2rad (d) { return d * PI / 180 }
proc rad2deg (r) { return r * 180 / PI }

# ── Number formatting ─────────────────────────────────────────────────────────

# Pad a string on the left/right to width w with character ch
proc pad_left  (s, w, ch) { while (len(s) < w) { s = ch + s }   return s }
proc pad_right (s, w, ch) { while (len(s) < w) { s = s + ch }   return s }

# Format x as decimal with exactly 'decimals' places: fmt_fixed(PI, 4) → "3.1416"
proc fmt_fixed (x, decimals) {
    var neg = x < 0
    x = abs(x)
    if (decimals == 0) {
        var result = str(floor(x + 0.5))
        if (neg) { result = "-" + result }
        return result
    }
    var factor   = pow(10, decimals)
    var shifted  = floor(x * factor + 0.5)
    var int_part = floor(shifted / factor)
    var frac_str = pad_left(str(shifted - int_part * factor), decimals, "0")
    var result   = str(int_part) + "." + frac_str
    if (neg) { result = "-" + result }
    return result
}

# Format as percentage: fmt_pct(0.752, 1) → "75.2%"
proc fmt_pct (x, decimals) { return fmt_fixed(x * 100, decimals) + "%" }

# ── String utilities ──────────────────────────────────────────────────────────

proc starts_with (s, prefix) {
    if (len(prefix) > len(s)) { return 0 }
    return sub(s, 0, len(prefix)) == prefix
}

proc ends_with (s, suffix) {
    var sl = len(s)
    var xl = len(suffix)
    if (xl > sl) { return 0 }
    return sub(s, sl - xl, sl) == suffix
}

proc ltrim (s) {
    while (len(s) > 0 and sub(s, 0, 1) == " ") { s = sub(s, 1, len(s)) }
    return s
}

proc rtrim (s) {
    while (len(s) > 0 and sub(s, len(s) - 1, len(s)) == " ") {
        s = sub(s, 0, len(s) - 1)
    }
    return s
}

proc trim (s) { return ltrim(rtrim(s)) }

proc repeat_str (s, n) {
    var out = ""
    while (n > 0) { out = out + s   n = n - 1 }
    return out
}

proc count_str (s, pattern) {
    var count = 0
    var pos   = 0
    var plen  = len(pattern)
    while (pos < len(s)) {
        var idx = find(sub(s, pos, len(s)), pattern)
        if (idx < 0) { break }
        count = count + 1
        pos   = pos + idx + plen
    }
    return count
}

proc replace (s, old_s, new_s) {
    var result = ""
    var olen   = len(old_s)
    while (len(s) > 0) {
        var idx = find(s, old_s)
        if (idx < 0) { result = result + s   s = "" }
        else         { result = result + sub(s, 0, idx) + new_s
                       s = sub(s, idx + olen, len(s)) }
    }
    return result
}

# ── Vector statistics ─────────────────────────────────────────────────────────
# These use element-wise vector arithmetic where possible.
# mean and stdev dispatch on type and work on both vectors and arrays.

proc mean (x) {
    if (type(x) == "vector") { return sum(x) / len(x) }
    # array path — iterate
    var s = 0
    for (var v in x) { s = s + v }
    return s / len(x)
}

proc stdev (x) {
    var m = mean(x)
    if (type(x) == "vector") {
        var d = x - m                       # element-wise deviation
        return sqrt(sum(d * d) / len(x))    # population std dev
    }
    # array path
    var ss = 0
    for (var v in x) { var d = v - m   ss = ss + d * d }
    return sqrt(ss / len(x))
}

# Scale a numeric vector to the range [0, 1]
proc normalize (v) {
    var lo = minimum(to_arr(v))
    var hi = maximum(to_arr(v))
    if (hi == lo) { return zeros(len(v)) }
    return (v - lo) / (hi - lo)
}

# Dot product of two numeric vectors
proc dot (a, b) { return sum(a * b) }

# ── Array operations ──────────────────────────────────────────────────────────
# Note on naming:
#   total(a)   — sum of elements in an array  (not 'sum' which is the vector builtin)
#   minimum(a) — smallest element in an array (not 'min' which compares two scalars)
#   maximum(a) — largest element in an array  (not 'max' which compares two scalars)

# Sum of all numeric elements in an array
proc total (a) {
    var s = 0
    for (var x in a) { s = s + x }
    return s
}

# Smallest / largest numeric element in an array
proc minimum (a) {
    var m = a[0]
    for (var x in a) { if (x < m) { m = x } }
    return m
}

proc maximum (a) {
    var m = a[0]
    for (var x in a) { if (x > m) { m = x } }
    return m
}

# Test whether val is in array a
proc contains (a, val) {
    for (var x in a) { if (x == val) { return 1 } }
    return 0
}

# First index of val in array a, or -1 if not found
proc index_of (a, val) {
    var i = 0
    while (i < len(a)) {
        if (a[i] == val) { return i }
        i = i + 1
    }
    return -1
}

# Return a new array with elements in reversed order (original unchanged)
proc reversed (a) {
    a = copy(a)
    var lo = 0
    var hi = len(a) - 1
    while (lo < hi) {
        var tmp = a[lo]
        a[lo] = a[hi]
        a[hi] = tmp
        lo = lo + 1
        hi = hi - 1
    }
    return a
}

# Return a new array sorted in ascending order (original unchanged)
# Works on numeric arrays. For string arrays, comparison throws — use a custom sort.
proc sorted (a) {
    a = copy(a)
    var n = len(a)
    var i = 0
    while (i < n - 1) {
        var j = 0
        while (j < n - i - 1) {
            if (a[j] > a[j+1]) {
                var tmp = a[j]
                a[j] = a[j+1]
                a[j+1] = tmp
            }
            j = j + 1
        }
        i = i + 1
    }
    return a
}

# Pair elements from two arrays: zip([1,2],[a,b]) → [[1,a],[2,b]]
# Stops at the shorter array.
proc zip (a, b) {
    var out = []
    var n   = min(len(a), len(b))
    var i   = 0
    while (i < n) {
        push(out, [a[i], b[i]])
        i = i + 1
    }
    return out
}

# Flatten one level of nesting: flatten([[1,2],[3,4]]) → [1,2,3,4]
proc flatten (a) {
    var out = []
    for (var x in a) {
        if (type(x) == "array") {
            for (var y in x) { push(out, y) }
        } else {
            push(out, x)
        }
    }
    return out
}

# ── Higher-order functions ────────────────────────────────────────────────────
# f must be a proc value or a name string (use apply internally).
# Example: map([1,2,3], proc(x){ return x*2 })

proc map (a, f) {
    var out = []
    for (var x in a) { push(out, f(x)) }
    return out
}

proc filter (a, pred) {
    var out = []
    for (var x in a) { if (pred(x)) { push(out, x) } }
    return out
}

proc reduce (a, f, init) {
    var acc = init
    for (var x in a) { acc = f(acc, x) }
    return acc
}

# ── Testing framework ─────────────────────────────────────────────────────────

var _tests_run    = 0
var _tests_passed = 0
var _tests_failed = 0

proc assert (cond, msg) {
    _tests_run = _tests_run + 1
    if (cond) {
        _tests_passed = _tests_passed + 1
    } else {
        _tests_failed = _tests_failed + 1
        print "FAIL: " msg
    }
}

proc assert_eq (a, b, msg) {
    _tests_run = _tests_run + 1
    if (a == b) {
        _tests_passed = _tests_passed + 1
    } else {
        _tests_failed = _tests_failed + 1
        print "FAIL: " msg " (got " a " expected " b ")"
    }
}

# Floating-point comparison within epsilon: assert_near(sin(PI), 0, 1e-10, "sin PI")
proc assert_near (a, b, eps, msg) {
    _tests_run = _tests_run + 1
    var diff = abs(a - b)
    if (diff <= eps) {
        _tests_passed = _tests_passed + 1
    } else {
        _tests_failed = _tests_failed + 1
        print "FAIL: " msg " (|" a " - " b "| = " diff " > " eps ")"
    }
}

proc test_summary () {
    print repeat_str("-", 40)
    print "tests run:    " _tests_run
    print "tests passed: " _tests_passed
    if (_tests_failed > 0) {
        print "tests FAILED: " _tests_failed
    } else {
        print "all tests passed."
    }
}
