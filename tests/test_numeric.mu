# test suite for numeric vectors and related built-ins

load("stdlib.mu")

# ── Numeric vectors ───────────────────────────────────────────────────────────

var v = vec(1, 2, 3, 4, 5)
assert_eq(type(v),    "vector", "type of vec")
assert_eq(type(42),   "number", "type of scalar")
assert_eq(len(v),     5,        "len of vec")
assert_eq(v[0],       1,        "vec index 0")
assert_eq(v[-1],      5,        "vec negative index")

# Arithmetic: element-wise
var w = vec(10, 20, 30, 40, 50)
var s = v + w
assert_eq(s[0], 11, "vec+vec first")
assert_eq(s[4], 55, "vec+vec last")

var scaled = v * 3
assert_eq(scaled[0], 3,  "vec*scalar first")
assert_eq(scaled[2], 9,  "vec*scalar mid")

var shifted = w - 5
assert_eq(shifted[0], 5,  "vec-scalar first")
assert_eq(shifted[4], 45, "vec-scalar last")

var divided = w / 2
assert_eq(divided[0], 5,  "vec/scalar first")
assert_eq(divided[2], 15, "vec/scalar mid")

# Unary minus
var neg = -v
assert_eq(neg[0], -1, "unary minus vec first")
assert_eq(neg[4], -5, "unary minus vec last")

# Math functions element-wise
var angles = linspace(0, PI, 5)
assert_eq(len(angles),          5,   "linspace len")
assert_eq(round_to(angles[0],5), 0,   "linspace first = 0")
assert_eq(round_to(angles[4],5), round_to(PI,5), "linspace last = PI")

var cosines = cos(angles)
assert_eq(round_to(cosines[0], 5),  1,  "cos(0) = 1")
assert_eq(round_to(cosines[4], 5), -1,  "cos(PI) = -1")

var roots = sqrt(vec(1, 4, 9, 16, 25))
assert_eq(roots[0], 1, "sqrt vec[0]")
assert_eq(roots[4], 5, "sqrt vec[4]")

# sum, all, any
assert_eq(sum(v),      15,  "sum(v)")
assert_eq(all(v),      1,   "all(v) — all nonzero")
assert_eq(all(vec(1,0,1)), 0, "all with zero")
assert_eq(any(vec(0,0,1)), 1, "any with nonzero")
assert_eq(any(zeros(3)),   0, "any(zeros)")

# zeros and ones
var z = zeros(4)
assert_eq(len(z),  4,   "zeros len")
assert_eq(z[0],    0,   "zeros[0]")
assert_eq(z[3],    0,   "zeros[3]")

var o = ones(3)
assert_eq(len(o),  3,   "ones len")
assert_eq(o[1],    1,   "ones[1]")

# Element-wise comparison → mask (NumVal of 0s and 1s)
var mask = v > 3
assert_eq(mask[0], 0, "mask[0] = 0 (1 not > 3)")
assert_eq(mask[3], 1, "mask[3] = 1 (4 > 3)")
assert_eq(mask[4], 1, "mask[4] = 1 (5 > 3)")

# all / any on masks
assert_eq(all(v > 0),   1, "all(v > 0)")
assert_eq(all(v > 3),   0, "all(v > 3) false")
assert_eq(any(v > 4),   1, "any(v > 4)")
assert_eq(any(v > 10),  0, "any(v > 10) false")

# Indexed assignment on numeric vector
var a = vec(10, 20, 30)
a[1] = 99
assert_eq(a[1], 99, "vec indexed assign")
assert_eq(a[0], 10, "vec indexed assign: left unchanged")

# for/in over numeric vector
var total = 0
for (var x in vec(1, 2, 3, 4, 5)) { total = total + x }
assert_eq(total, 15, "for/in over vec")

var doubled = []
for (var x in linspace(1, 3, 3)) { push(doubled, x * 2) }
assert_eq(doubled[0], 2, "for/in linspace: first doubled")
assert_eq(doubled[2], 6, "for/in linspace: last doubled")

# ── eval ─────────────────────────────────────────────────────────────────────
eval("var eval_result = 42 * 2")
assert_eq(eval_result, 84, "eval creates global var")

eval("proc double_it(x) { return x * 2 }")
assert_eq(double_it(7), 14, "eval defines proc")

# ── apply ─────────────────────────────────────────────────────────────────────
proc triple (x) { return x * 3 }
assert_eq(apply("triple", [5]),    15,  "apply user proc")
assert_eq(apply("sqrt",   [144]),  12,  "apply builtin")
assert_eq(apply("upper",  ["hi"]), "HI","apply string builtin")

# apply to each element (higher-order pattern)
proc apply_each (proc_name, arr) {
    var out = []
    for (var x in arr) { push(out, apply(proc_name, [x])) }
    return out
}
var triples = apply_each("triple", [1, 2, 3, 4])
assert_eq(triples[0], 3,  "apply_each first")
assert_eq(triples[3], 12, "apply_each last")

# ── shuffle ───────────────────────────────────────────────────────────────────
var original = [1, 2, 3, 4, 5, 6, 7, 8]
var shuffled = shuffle(original)
assert_eq(len(shuffled),  8, "shuffle: same length")
assert_eq(len(original),  8, "shuffle: original unchanged")
# the elements sum should be the same
var orig_sum = 0
for (var x in original) { orig_sum = orig_sum + x }
var shuf_sum = 0
for (var x in shuffled) { shuf_sum = shuf_sum + x }
assert_eq(orig_sum, shuf_sum, "shuffle: sum preserved")

# ── call stack trace ──────────────────────────────────────────────────────────
# (just verify deep calls still work — we can't easily test the trace text)
proc a_deep (n) {
    if (n == 0) { return 0 }
    return a_deep(n - 1) + 1
}
assert_eq(a_deep(10), 10, "deep recursion result")

test_summary()
