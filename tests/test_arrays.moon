# test_arrays.moon — comprehensive test suite for native array support

load ("stdlib.moon")

# ── Minimal test framework ────────────────────────────────────────────────────

var _tests_run    = 0
var _tests_passed = 0
var _tests_failed = 0

proc assert_eq (a, b, msg) {
    _tests_run = _tests_run + 1
    if (a == b) {
        _tests_passed = _tests_passed + 1
    } else {
        _tests_failed = _tests_failed + 1
        print "FAIL: " msg " (got " a " expected " b ")"
    }
}

proc assert_true (cond, msg) {
    _tests_run = _tests_run + 1
    if (cond) {
        _tests_passed = _tests_passed + 1
    } else {
        _tests_failed = _tests_failed + 1
        print "FAIL: " msg
    }
}

proc repeat_str (s, n) {
    var out = ""
    while (n > 0) { out = out + s   n = n - 1 }
    return out
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

# ── Literals and basic access ─────────────────────────────────────────────────

var a = [10, 20, 30, 40, 50]
assert_eq(a[0],    10,   "literal: index 0")
assert_eq(a[4],    50,   "literal: index 4 (last)")
assert_eq(a[-1],   50,   "literal: negative index -1 (last)")
assert_eq(a[-2],   40,   "literal: negative index -2")
assert_eq(len(a),  5,    "literal: len of 5-element array")
assert_eq(len([]), 0,    "literal: len of empty array")

# ── type() ────────────────────────────────────────────────────────────────────

assert_eq(type([]),      "array",  "type: empty array literal")
assert_eq(type([1,2,3]), "array",  "type: non-empty array")
assert_eq(type(0),       "number", "type: number unchanged")
assert_eq(type("hi"),    "string", "type: string unchanged")

# ── Mutation ──────────────────────────────────────────────────────────────────

var m = [1, 2, 3]
m[0] = 99
assert_eq(m[0], 99, "mutation: write index 0, read back")
m[-1] = 77
assert_eq(m[2], 77, "mutation: write via negative index, read by positive")

# ── Reference semantics ───────────────────────────────────────────────────────

var x = [1, 2, 3]
var y = x               # y and x share the same array
y[0] = 42
assert_eq(x[0], 42, "ref: alias mutation visible through original")

var z = copy(x)
z[0] = 0
assert_eq(x[0], 42, "copy: original unchanged after mutating copy")
assert_eq(z[0], 0,  "copy: copy reflects its own write")

# ── arr() constructor ─────────────────────────────────────────────────────────

var zeros = arr(4)
assert_eq(len(zeros), 4, "arr(n): correct length")
assert_eq(zeros[0],   0, "arr(n): first element is 0")
assert_eq(zeros[3],   0, "arr(n): last element is 0")

var filled = arr(3, 7)
assert_eq(len(filled), 3, "arr(n,val): correct length")
assert_eq(filled[0],   7, "arr(n,val): first element")
assert_eq(filled[2],   7, "arr(n,val): last element")

var sfilled = arr(2, "hi")
assert_eq(sfilled[0], "hi", "arr(n, string): element value")
assert_eq(sfilled[1], "hi", "arr(n, string): second element")

# ── push / pop ────────────────────────────────────────────────────────────────

var s = []
push(s, 10)
push(s, 20)
push(s, 30)
assert_eq(len(s), 3,  "push: length after 3 single pushes")
assert_eq(s[0],   10, "push: first element")
assert_eq(s[2],   30, "push: last element")

var top = pop(s)
assert_eq(top,    30, "pop: returned value")
assert_eq(len(s), 2,  "pop: length decremented")
assert_eq(s[-1],  20, "pop: new last element")

push(s, 100, 200, 300)     # variadic
assert_eq(len(s),  5,   "push variadic: length after push(s,100,200,300)")
assert_eq(s[-1],   300, "push variadic: last pushed element")
assert_eq(s[-3],   100, "push variadic: first of the new batch")

# ── insert / remove ───────────────────────────────────────────────────────────

var t = [1, 2, 4, 5]
insert(t, 2, 3)
assert_eq(len(t), 5, "insert: length after insert")
assert_eq(t[2],   3, "insert: new element in place")
assert_eq(t[3],   4, "insert: element shifted right")
assert_eq(t[0],   1, "insert: left side untouched")

var r = remove(t, 0)
assert_eq(r,      1, "remove: returned value")
assert_eq(len(t), 4, "remove: length decremented")
assert_eq(t[0],   2, "remove: next element promoted to front")

remove(t, -1)
assert_eq(len(t),  3, "remove(-1): length decremented")
assert_eq(t[-1],   4, "remove(-1): new last element")

# ── slice ─────────────────────────────────────────────────────────────────────

var v = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
var sl = slice(v, 2, 6)
assert_eq(len(sl), 4, "slice: length")
assert_eq(sl[0],   2, "slice: first element")
assert_eq(sl[3],   5, "slice: last element")
assert_eq(v[2],    2, "slice: original unchanged")

assert_eq(len(slice(v, 3, 3)),  0,  "slice: empty range lo==hi")
assert_eq(len(slice(v, 0, 10)), 10, "slice: full range")

# ── concat ────────────────────────────────────────────────────────────────────

var ca = [1, 2, 3]
var cb = [4, 5, 6]
var cc = concat(ca, cb)
assert_eq(len(cc), 6, "concat: length")
assert_eq(cc[0],   1, "concat: first element from left")
assert_eq(cc[5],   6, "concat: last element from right")
assert_eq(len(ca), 3, "concat: left operand unchanged")
assert_eq(len(cb), 3, "concat: right operand unchanged")

# ── range ─────────────────────────────────────────────────────────────────────

var r0 = range(0, 5)
assert_eq(len(r0), 5, "range: default step length")
assert_eq(r0[0],   0, "range: first element")
assert_eq(r0[4],   4, "range: last element")

var r1 = range(0, 10, 2)
assert_eq(len(r1), 5, "range step 2: length")
assert_eq(r1[0],   0, "range step 2: first")
assert_eq(r1[4],   8, "range step 2: last")

var r2 = range(5, 0, -1)
assert_eq(len(r2), 5, "range descending: length")
assert_eq(r2[0],   5, "range descending: first")
assert_eq(r2[4],   1, "range descending: last")

assert_eq(len(range(3, 3)),   0, "range: empty when lo==hi")
assert_eq(len(range(10, 5)),  0, "range: empty positive step past end")
assert_eq(len(range(0, 5, 5)),1, "range: single element when step==hi-lo")

# ── join / split ──────────────────────────────────────────────────────────────

var words = ["alpha", "beta", "gamma"]
assert_eq(join(words, ", "), "alpha, beta, gamma", "join: three words with separator")
assert_eq(join(words, ""),   "alphabetagamma",     "join: empty separator")
assert_eq(join([], "-"),     "",                   "join: empty array")

var parts = split("a,b,c,d", ",")
assert_eq(len(parts),  4,   "split: length")
assert_eq(parts[0],    "a", "split: first element")
assert_eq(parts[3],    "d", "split: last element")

var nosep = split("hello", ",")
assert_eq(len(nosep),  1,       "split: no match → single element")
assert_eq(nosep[0],    "hello", "split: no match → element is full string")

var ws = split("  hi  ", " ")
assert_eq(len(ws), 5, "split on space: preserves empty strings between delimiters")

# ── for/in over array ─────────────────────────────────────────────────────────

var total = 0
for (var n in range(1, 11)) { total = total + n }
assert_eq(total, 55, "for/in: sum 1..10 via range")

var doubled = []
for (var n in [3, 1, 4, 1, 5]) { push(doubled, n * 2) }
assert_eq(len(doubled), 5,  "for/in: collected array length")
assert_eq(doubled[0],   6,  "for/in: first doubled element")
assert_eq(doubled[4],   10, "for/in: last doubled element")

var empty_iter = 0
for (var n in []) { empty_iter = empty_iter + 1 }
assert_eq(empty_iter, 0, "for/in: empty array body never runs")

# ── for/in over string (characters) ──────────────────────────────────────────

var vowels = 0
for (var ch in "hello world") {
    if (ch == "a" or ch == "e" or ch == "i" or ch == "o" or ch == "u") {
        vowels = vowels + 1
    }
}
assert_eq(vowels, 3, "for/in string: vowel count in 'hello world'")

var chars = []
for (var ch in "abc") { push(chars, ch) }
assert_eq(len(chars), 3,   "for/in string: correct length")
assert_eq(chars[0],   "a", "for/in string: first char")
assert_eq(chars[2],   "c", "for/in string: last char")

# ── break inside for/in ───────────────────────────────────────────────────────

var found = -1
for (var x in [3, 7, 2, 9, 1, 5]) {
    if (x == 9) { found = x   break }
}
assert_eq(found, 9, "break in for/in: value at break point")

var iters = 0
for (var x in range(0, 100)) {
    if (x == 10) { break }
    iters = iters + 1
}
assert_eq(iters, 10, "break in for/in: exact iteration count before break")

# ── 2D arrays ─────────────────────────────────────────────────────────────────

var mat = [[1,2,3],[4,5,6],[7,8,9]]
assert_eq(mat[0][0], 1, "2D: top-left")
assert_eq(mat[1][1], 5, "2D: center")
assert_eq(mat[2][2], 9, "2D: bottom-right")

mat[0][2] = 99
assert_eq(mat[0][2], 99, "2D: write and read back")
assert_eq(mat[0][0],  1, "2D: adjacent cell unchanged after write")

# 3×3 identity matrix built programmatically
var I = []
var i = 0
while (i < 3) {
    var row = arr(3, 0)
    row[i] = 1
    push(I, row)
    i = i + 1
}
assert_eq(I[0][0], 1, "identity 3x3: [0][0]")
assert_eq(I[0][1], 0, "identity 3x3: [0][1]")
assert_eq(I[1][1], 1, "identity 3x3: [1][1]")
assert_eq(I[2][2], 1, "identity 3x3: [2][2]")
assert_eq(I[2][0], 0, "identity 3x3: [2][0]")

# ── Mixed-type arrays ─────────────────────────────────────────────────────────

var mixed = [1, "two", 3.14, "four", 5]
assert_eq(type(mixed[0]), "number", "mixed: type of number element")
assert_eq(type(mixed[1]), "string", "mixed: type of string element")
assert_eq(mixed[1],       "two",    "mixed: string value correct")
assert_eq(mixed[2],       3.14,     "mixed: float value correct")

# array containing arrays
var nested = [[1,2],[3,4],[5,6]]
assert_eq(type(nested[0]), "array",  "mixed: element type is array")
assert_eq(nested[1][0],    3,        "mixed: nested read correct")

var data = [4, 7, 2, 9, 1, 5, 8, 3, 6]
assert_eq(total(data),   45, "algo: sum of [4..9]")
assert_eq(maximum(data),    9, "algo: max")
assert_eq(minimum(data),    1, "algo: min")
assert_eq(total(range(1,101)), 5050, "algo: sum 1..100 via range")

var rev = reversed(data)
assert_eq(rev[0],    6, "algo: reverse first element")
assert_eq(rev[8],    4, "algo: reverse last element")
assert_eq(data[0],   4, "algo: reverse did not modify original")

assert_eq(contains(data, 7), 1, "algo: contains (present)")
assert_eq(contains(data, 0), 0, "algo: contains (absent)")

var sorted = sorted(data)
assert_eq(sorted[0],   1, "algo: sort first")
assert_eq(sorted[4],   5, "algo: sort middle")
assert_eq(sorted[8],   9, "algo: sort last")
assert_eq(data[0],     4, "algo: sort did not modify original")

test_summary()
