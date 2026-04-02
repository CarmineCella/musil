# test suite for standard library functions

load("stdlib.mu")

# ── constants ──────────────────────────────────────────────────────────────
assert_near(PI, 3.14159265358979, 1e-10, "PI")
assert_eq(round_to(TAU / 2, 10), round_to(PI, 10), "TAU/2 = PI")

# ── math ───────────────────────────────────────────────────────────────────
assert_eq(min(3, 7),          3,    "min")
assert_eq(max(3, 7),          7,    "max")
assert_eq(clamp(-5, 0, 10),   0,    "clamp lo")
assert_eq(clamp(15, 0, 10),   10,   "clamp hi")
assert_eq(clamp(5,  0, 10),   5,    "clamp mid")
assert_eq(sign(-3), -1,             "sign neg")
assert_eq(sign(0),   0,             "sign zero")
assert_eq(sign(7),   1,             "sign pos")
assert_eq(round(3.5), 4,            "round up")
assert_eq(round(3.4), 3,            "round down")
assert_eq(round_to(PI, 4), 3.1416,  "round_to")
assert_eq(mod(10, 3),  1,           "mod pos")
assert_eq(mod(-1, 5),  4,           "mod neg")
assert_eq(even(4),     1,           "even")
assert_eq(odd(7),      1,           "odd")
assert_eq(between(5, 1, 10), 1,     "between yes")
assert_eq(between(0, 1, 10), 0,     "between no")
assert_near(hypot(3, 4), 5, 1e-10,  "hypot 3-4-5")
assert_eq(gcd(48, 18),  6,          "gcd")
assert_eq(lcm(4, 6),    12,         "lcm")
assert_eq(ipow(2, 10),  1024,       "ipow")
assert_eq(lerp(0, 10, 0.5), 5,      "lerp mid")
assert_eq(map_range(5, 0, 10, 0, 100), 50, "map_range")
assert_near(deg2rad(180), PI, 1e-10, "deg2rad")
assert_eq(rad2deg(PI), 180,          "rad2deg")

# ── formatting ─────────────────────────────────────────────────────────────
assert_eq(pad_left("7",   3, "0"), "007",   "pad_left")
assert_eq(pad_right("hi", 5, "."), "hi...", "pad_right")
assert_eq(fmt_fixed(PI, 4),    "3.1416",  "fmt_fixed")
assert_eq(fmt_fixed(-2.5, 1),  "-2.5",    "fmt_fixed neg")
assert_eq(fmt_fixed(10, 0),    "10",      "fmt_fixed 0dp")
assert_eq(fmt_pct(0.752, 1),   "75.2%",   "fmt_pct")

# ── strings ────────────────────────────────────────────────────────────────
assert_eq(starts_with("musil", "mu"), 1,       "starts_with")
assert_eq(ends_with("musil", "il"),   1,       "ends_with")
assert_eq(trim("  hi  "),    "hi",            "trim")
assert_eq(repeat_str("ab", 3), "ababab",      "repeat_str")
assert_eq(count_str("abab", "ab"), 2,         "count_str")
assert_eq(replace("hello", "l", "r"), "herro","replace")

# ── vector statistics ──────────────────────────────────────────────────────
var v = vec(2, 4, 4, 4, 5, 5, 7, 9)
assert_eq(mean(v),  5,  "mean vector")
assert_eq(stdev(v), 2,  "stdev vector")

var v2 = vec(0, 5, 10)
var n2 = normalize(v2)
assert_eq(n2[0], 0,   "normalize min → 0")
assert_eq(n2[2], 1,   "normalize max → 1")
assert_eq(n2[1], 0.5, "normalize mid → 0.5")

assert_eq(dot(vec(1,2,3), vec(4,5,6)), 32, "dot product")

# ── mean / stdev work on arrays too ───────────────────────────────────────
var a = [2, 4, 4, 4, 5, 5, 7, 9]
assert_eq(mean(a),  5, "mean array")
assert_eq(stdev(a), 2, "stdev array")

# ── array operations (no arr_ prefix) ─────────────────────────────────────
var nums = [4, 7, 2, 9, 1, 5, 8, 3, 6]

assert_eq(total(nums),      45,  "total")
assert_eq(minimum(nums),     1,  "minimum")
assert_eq(maximum(nums),     9,  "maximum")
assert_eq(contains(nums, 7), 1,  "contains found")
assert_eq(contains(nums, 0), 0,  "contains not found")
assert_eq(index_of(nums, 5), 5,  "index_of found")
assert_eq(index_of(nums, 0),-1,  "index_of not found")

var rev = reversed(nums)
assert_eq(rev[0],    6,  "reversed first")
assert_eq(rev[-1],   4,  "reversed last")
assert_eq(nums[0],   4,  "reversed: original unchanged")

var srt = sorted(nums)
assert_eq(srt[0],    1,  "sorted first")
assert_eq(srt[-1],   9,  "sorted last")
assert_eq(nums[0],   4,  "sorted: original unchanged")

# ── zip / flatten ──────────────────────────────────────────────────────────
var z = zip([1, 2, 3], ["a", "b", "c"])
assert_eq(len(z),    3,   "zip length")
assert_eq(z[0][0],   1,   "zip pair first elem")
assert_eq(z[0][1], "a",   "zip pair second elem")
assert_eq(z[2][1], "c",   "zip last pair")

var z2 = zip([1, 2, 3], [4, 5])   # different lengths — stops at shorter
assert_eq(len(z2), 2, "zip unequal lengths")

var f2 = flatten([[1, 2], [3, 4], [5]])
assert_eq(len(f2), 5, "flatten length")
assert_eq(f2[0],   1, "flatten first")
assert_eq(f2[4],   5, "flatten last")

var f3 = flatten([1, [2, 3], 4])  # mixed: scalars and arrays
assert_eq(len(f3), 4, "flatten mixed")
assert_eq(f3[1],   2, "flatten mixed inner")

# ── higher-order functions ─────────────────────────────────────────────────
var doubled = map([1, 2, 3, 4], proc (x) { return x * 2 })
assert_eq(doubled[0],  2,  "map first")
assert_eq(doubled[3],  8,  "map last")

var evens = filter([1,2,3,4,5,6], proc (x) { return mod(x, 2) == 0 })
assert_eq(len(evens), 3,  "filter count")
assert_eq(evens[0],   2,  "filter first")

var total2 = reduce([1,2,3,4,5], proc (acc, x) { return acc + x }, 0)
assert_eq(total2, 15, "reduce sum")

var product = reduce([1,2,3,4,5], proc (acc, x) { return acc * x }, 1)
assert_eq(product, 120, "reduce product")

# ── assert_near ────────────────────────────────────────────────────────────
assert_near(sin(PI),      0, 1e-10, "sin PI ≈ 0")
assert_near(cos(PI),     -1, 1e-10, "cos PI ≈ -1")
assert_near(log(E),       1, 1e-10, "log E = 1")
assert_near(sqrt(2), SQRT2, 1e-10,  "sqrt(2) = SQRT2")

test_summary()
