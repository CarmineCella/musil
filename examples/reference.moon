# ═══════════════════════════════════════════════════════════════════════════════
#  Moon v5 — Complete Language Reference
#  Run: ./moon reference.moon
#  All features demonstrated with working examples and printed output.
# ═══════════════════════════════════════════════════════════════════════════════

load("stdlib.moon")   # math, string, array utilities

proc section (title) {
    print ""
    print "── " + title + " " + repeat_str("─", 50 - len(title))
}

# ───────────────────────────────────────────────────────────────────────────────
#  1. TYPES
#  Four runtime types: number, vector, string, array, proc
#  Check with type(x) — returns one of "number" "vector" "string" "array" "proc"
# ───────────────────────────────────────────────────────────────────────────────
section("1. TYPES")

var n  = 42            # number  — internally a 1-element valarray<double>
var v  = vec(1, 2, 3)  # vector  — multi-element valarray<double>
var s  = "hello"       # string
var a  = [1, "two", 3] # array   — heterogeneous, shared_ptr semantics
var f  = proc (x) { return x * 2 }   # proc (first-class function)

print "type(42)         = " type(n)
print "type(vec(1,2,3)) = " type(v)
print "type('hello')    = " type(s)
print "type([1,'two',3])= " type(a)
print "type(proc...)    = " type(f)

# ───────────────────────────────────────────────────────────────────────────────
#  2. NUMBERS AND ARITHMETIC
#  All numbers are 1-element vectors — scalar and vectorized math are the same path.
#  Operators: + - * /   Comparison: < > <= >= == !=   Logic: and or not
# ───────────────────────────────────────────────────────────────────────────────
section("2. NUMBERS AND ARITHMETIC")

var x = 10
var y = 3
print "10 + 3  = " (x + y)
print "10 - 3  = " (x - y)
print "10 * 3  = " (x * y)
print "10 / 3  = " (x / y)           # real division — no integer truncation
print "10 ^ 3  = " pow(x, y)         # exponentiation via builtin
print "floor(10/3) = " floor(x / y)  # integer division
print "mod(10, 3)  = " mod(x, y)     # remainder (from stdlib)

# Operator precedence: * / before + -, use () to override
print "2 + 3 * 4 = " (2 + 3 * 4)    # 14, not 20
print "unary -   = " (-x)

# ───────────────────────────────────────────────────────────────────────────────
#  3. STRINGS
#  Immutable. + concatenates (coerces numbers). Sub-string, search, case, split.
# ───────────────────────────────────────────────────────────────────────────────
section("3. STRINGS")

var greeting = "Hello, " + "Moon!"
print greeting
print "length      = " len(greeting)
print "upper       = " upper(greeting)
print "lower       = " lower(greeting)
print "sub(0,5)    = " sub(greeting, 0, 5)           # "Hello"
print "find(',')   = " find(greeting, ",")           # 5
print "str(3.14)   = " str(3.14)                     # number to string
print "num('42')   = " num("42")                     # string to number
print "char(65)    = " char(65)                      # ASCII → char
print "asc('A')    = " asc("A")                      # char → ASCII code

# String + number coercion: either side can be a number
print "score: " + 99 + " / 100"

# split and join
var parts = split("a,b,c,d", ",")    # returns an array of strings
print "split: " parts
print "join:  " join(parts, " | ")

# Multi-line strings use \n
var poem = "line one\nline two\nline three"
print poem

# ───────────────────────────────────────────────────────────────────────────────
#  4. IF / ELSE IF / ELSE
#  Condition must be in parens. Any truthy value (non-zero, non-empty) works.
# ───────────────────────────────────────────────────────────────────────────────
section("4. IF / ELSE IF / ELSE")

proc classify (n) {
    if (n < 0)       { return "negative" }
    else if (n == 0) { return "zero" }
    else if (n < 10) { return "small" }
    else             { return "large" }
}
print "classify(-3) = " classify(-3)
print "classify(0)  = " classify(0)
print "classify(7)  = " classify(7)
print "classify(42) = " classify(42)

# Truthy: non-zero number, non-empty string, non-empty array, any proc
if ("hello")   { print "non-empty string is truthy" }
if (not "")    { print "empty string is falsy" }
if (not 0)     { print "zero is falsy" }

# Logical operators: and, or, not
var a2 = 5
var b2 = 10
if (a2 > 0 and b2 > 0) { print "both positive" }
if (a2 > 100 or b2 > 5) { print "at least one large" }

# ───────────────────────────────────────────────────────────────────────────────
#  5. WHILE LOOPS
#  Loop while condition is truthy. Use break to exit early.
# ───────────────────────────────────────────────────────────────────────────────
section("5. WHILE LOOPS")

# Basic accumulator
var i = 1
var total = 0
while (i <= 10) {
    total = total + i
    i = i + 1
}
print "sum 1..10 = " total

# Break on condition
var n2 = 1
while (1) {                          # infinite loop — break exits
    if (n2 * n2 > 50) { break }
    n2 = n2 + 1
}
print "first n where n^2 > 50: " n2

# Nested while — multiplication table row
var row2 = ""
var j = 1
while (j <= 5) {
    row2 = row2 + pad_left(str(3 * j), 4, " ")
    j = j + 1
}
print "3× table: " row2

# ───────────────────────────────────────────────────────────────────────────────
#  6. FOR / IN LOOPS
#  Iterate over arrays, vectors, or strings. Variable is local to the loop.
# ───────────────────────────────────────────────────────────────────────────────
section("6. FOR / IN LOOPS")

# Over an array
for (var fruit in ["apple", "banana", "cherry"]) {
    print "  fruit: " fruit
}

# Over a range (returns an array of numbers)
var sum2 = 0
for (var k in range(1, 6)) {    # range(lo, hi): lo inclusive, hi exclusive
    sum2 = sum2 + k
}
print "sum via range(1,6) = " sum2

# Over a numeric vector
var s2 = 0
for (var x in linspace(0, 4, 5)) {    # linspace(lo, hi, n): n points, inclusive
    s2 = s2 + x
}
print "sum via linspace    = " s2

# Over a string — iterates characters
var vowels = 0
for (var ch in "hello world") {
    if (ch == "a" or ch == "e" or ch == "i" or ch == "o" or ch == "u") {
        vowels = vowels + 1
    }
}
print "vowels in 'hello world' = " vowels

# Break in for/in
var found = -1
for (var val in [3, 7, 2, 9, 1, 5]) {
    if (val == 9) { found = val   break }
}
print "found 9: " found

# ───────────────────────────────────────────────────────────────────────────────
#  7. PROCS (NAMED)
#  proc name (params) { body }   — returns last expression or explicit return
#  Procs see globals. Parameters are local copies (vectors) or shared refs (arrays).
# ───────────────────────────────────────────────────────────────────────────────
section("7. NAMED PROCS")

proc factorial (n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)    # recursion
}
print "10! = " factorial(10)

proc fib (n) {
    if (n <= 1) { return n }
    var a3 = 0
    var b3 = 1
    var i3 = 2
    while (i3 <= n) {
        var t = a3 + b3
        a3 = b3
        b3 = t
        i3 = i3 + 1
    }
    return b3
}
print "fib(20) = " fib(20)

# Multiple return paths
proc sign2 (x) {
    if (x > 0) { return 1 }
    if (x < 0) { return -1 }
    return 0
}
print "sign(-7) = " sign2(-7)
print "sign(0)  = " sign2(0)
print "sign(3)  = " sign2(3)

# Proc modifying a global counter (procs can read/write globals)
var call_count = 0
proc counted_add (a4, b4) {
    call_count = call_count + 1
    return a4 + b4
}
counted_add(1, 2)
counted_add(3, 4)
counted_add(5, 6)
print "counted_add called " call_count " times"

# ───────────────────────────────────────────────────────────────────────────────
#  8. FIRST-CLASS PROCS (ANONYMOUS)
#  proc (params) { body } — proc literal, stored in a variable or passed directly.
#  NOT a closure: sees globals, not the surrounding scope's locals.
# ───────────────────────────────────────────────────────────────────────────────
section("8. FIRST-CLASS PROCS")

# Store in a variable and call
var double3 = proc (x) { return x * 2 }
print "double3(7) = " double3(7)

# Pass to a higher-order proc
proc map_array (a5, f) {
    var out = []
    for (var x in a5) { push(out, f(x)) }
    return out
}
proc filter_array (a5, pred) {
    var out = []
    for (var x in a5) { if (pred(x)) { push(out, x) } }
    return out
}
proc reduce_array (a5, f, init) {
    var acc = init
    for (var x in a5) { acc = f(acc, x) }
    return acc
}

var nums = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

var squared   = map_array(nums, proc (x) { return x * x })
var evens     = filter_array(nums, proc (x) { return mod(x, 2) == 0 })
var total2    = reduce_array(nums, proc (acc, x) { return acc + x }, 0)

print "squares:  " squared
print "evens:    " evens
print "sum:      " total2

# Procs in arrays — a dispatch table
var ops = [
    proc (a6, b6) { return a6 + b6 },
    proc (a6, b6) { return a6 - b6 },
    proc (a6, b6) { return a6 * b6 }
]
print "ops[0](10,3) = " ops[0](10, 3)   # 13
print "ops[1](10,3) = " ops[1](10, 3)   # 7
print "ops[2](10,3) = " ops[2](10, 3)   # 30

# apply: call by name (string) or by proc value
print "apply('sqrt', [144]) = " apply("sqrt", [144])
print "apply(double3, [21]) = " apply(double3, [21])

# ───────────────────────────────────────────────────────────────────────────────
#  9. VECTORS (NUMERIC)
#  Homogeneous doubles. VALUE semantics — assignment copies.
#  All arithmetic is element-wise. Scalars broadcast across vectors.
# ───────────────────────────────────────────────────────────────────────────────
section("9. VECTORS")

# Construction
var v1 = vec(1, 2, 3, 4, 5)       # from scalars
var v2 = linspace(0, 1, 5)        # 0.0 0.25 0.5 0.75 1.0
var v3 = zeros(4)                  # 0 0 0 0
var v4 = ones(3)                   # 1 1 1
print "vec(1..5):       " v1
print "linspace(0,1,5): " v2
print "zeros(4):        " v3
print "ones(3):         " v4

# Arithmetic — all element-wise
print "v1 + 10:   " (v1 + 10)       # broadcast scalar
print "v1 * v1:   " (v1 * v1)       # element-wise multiply
print "v1 + v1:   " (v1 + v1)
print "-v1:       " (-v1)

# Math functions — all element-wise
print "sqrt(v1):  " sqrt(v1)
print "sin(v2*PI):" sin(v2 * PI)   # sin of [0, π/4, π/2, 3π/4, π]

# VALUE SEMANTICS: assignment copies
var va = vec(10, 20, 30)
var vb = va              # full copy
vb[0] = 99
print "va (original, unaffected): " va
print "vb (modified copy):        " vb

# Subscript and assign
va[1] = 777
print "va after va[1]=777: " va

# Reductions
print "sum(v1)    = " sum(v1)
print "all(v1>0)  = " all(v1 > 0)   # 1 — all elements > 0
print "any(v1>4)  = " any(v1 > 4)   # 1 — some elements > 4

# Comparison returns a 0/1 vector mask
var mask = v1 > 3
print "v1 > 3 mask: " mask
print "all: " all(mask) "  any: " any(mask)

# Iteration
var vsum = 0
for (var x in v1) { vsum = vsum + x }
print "for/in sum = " vsum

# Conversion to/from array
var arr_v = to_arr(v1)              # vector → array
print "to_arr:   " arr_v "  type: " type(arr_v)
var back  = to_vec(arr_v)           # array → vector
print "to_vec:   " back "  type: " type(back)

# ───────────────────────────────────────────────────────────────────────────────
#  10. ARRAYS
#  Heterogeneous — elements can be any type. REFERENCE semantics.
#  [ ] literal, or arr(n), arr(n, val). Mutable: push, pop, insert, remove.
# ───────────────────────────────────────────────────────────────────────────────
section("10. ARRAYS")

# Construction
var fruits = ["apple", "banana", "cherry"]
var mixed2 = [1, "two", [3, 4], proc (x) { return x + 1 }]
var grid   = arr(3, 0)        # [0, 0, 0]
var flags  = arr(4, 0)

print "fruits:  " fruits
print "arr(3,0):" grid

# Push, pop, insert, remove
push(fruits, "date")
push(fruits, "elderberry", "fig")    # variadic push
print "after push: " fruits

var popped = pop(fruits)
print "popped: " popped "  remaining: " fruits

insert(fruits, 1, "avocado")    # insert before index 1
print "after insert at 1: " fruits

var removed2 = remove(fruits, 0)   # remove index 0
print "removed: " removed2 "  remaining: " fruits

# REFERENCE SEMANTICS: assignment shares
var arr1 = [10, 20, 30]
var arr2 = arr1              # alias — same underlying list
arr2[0] = 99
print "arr1[0] after modifying arr2: " arr1[0]   # 99 — shared!

# copy() breaks sharing
var arr3 = copy(arr1)
arr3[0] = 777
print "arr1[0] after modifying copy: " arr1[0]  # 99 — unaffected

# Arrays passed to procs: reference shared — proc CAN mutate caller's array
proc push_ten (a7) { push(a7, 10) }
var mylist = [1, 2, 3]
push_ten(mylist)
print "after push_ten: " mylist     # [1, 2, 3, 10]

# Slice, concat
var nums2 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
print "slice(2,6):    " slice(nums2, 2, 6)
print "concat:        " concat(slice(nums2, 0, 3), slice(nums2, 7, 10))

# Shuffle (returns new shuffled copy)
var deck = range(1, 6)   # [1, 2, 3, 4, 5]
var shuffled2 = shuffle(deck)
print "original: " deck
print "shuffled: " shuffled2

# 2D arrays — arrays of arrays
var matrix = [[1,2,3],[4,5,6],[7,8,9]]
print "matrix[1][2] = " matrix[1][2]    # 6
matrix[1][2] = 99
print "after assign: " matrix[1]

# Negative indices
print "last element: " fruits[-1]
print "second to last: " fruits[-2]

# ───────────────────────────────────────────────────────────────────────────────
#  11. STDLIB — MATH UTILITIES
# ───────────────────────────────────────────────────────────────────────────────
section("11. STDLIB — MATH UTILITIES")

print "min(3,7)         = " min(3, 7)
print "max(3,7)         = " max(3, 7)
print "clamp(15,0,10)   = " clamp(15, 0, 10)
print "sign(-5)         = " sign(-5)
print "round(3.7)       = " round(3.7)
print "round_to(PI,4)   = " round_to(PI, 4)
print "mod(-1,5)        = " mod(-1, 5)      # 4, not -1 (always positive)
print "gcd(48,18)       = " gcd(48, 18)
print "lcm(4,6)         = " lcm(4, 6)
print "lerp(0,100,0.3)  = " lerp(0, 100, 0.3)
print "deg2rad(180)     = " round_to(deg2rad(180), 10)
print "E                = " E
print "PHI              = " PHI
print "SQRT2            = " SQRT2

# ───────────────────────────────────────────────────────────────────────────────
#  12. STDLIB — STRING UTILITIES
# ───────────────────────────────────────────────────────────────────────────────
section("12. STDLIB — STRING UTILITIES")

print "pad_left('7',3,'0')   = " pad_left("7", 3, "0")
print "pad_right('hi',5,'.') = " pad_right("hi", 5, ".")
print "trim('  hello  ')     = " trim("  hello  ")
print "starts_with           = " starts_with("moon", "mo")
print "ends_with             = " ends_with("moon", "on")
print "repeat_str('ha',3)    = " repeat_str("ha", 3)
print "count_str             = " count_str("abcabc", "bc")
print "replace               = " replace("hello", "l", "r")
print "fmt_fixed(PI,4)       = " fmt_fixed(PI, 4)

# ───────────────────────────────────────────────────────────────────────────────
#  13. STDLIB — ARRAY ALGORITHMS
# ───────────────────────────────────────────────────────────────────────────────
section("13. STDLIB — ARRAY ALGORITHMS")

var data = [4, 7, 2, 9, 1, 5, 8, 3, 6]

print "arr_sum:      " arr_sum(data)
# print "arr_mean:     " arr_mean(data)
print "arr_min:      " arr_min(data)
print "arr_max:      " arr_max(data)
# print "arr_std:      " fmt_fixed(arr_std(data), 4)
print "contains 7:   " arr_contains(data, 7)
# print "index_of 5:   " arr_index_of(data, 5)
print "reversed:     " arr_reverse(data)
print "sorted:       " arr_sort(data)

# ───────────────────────────────────────────────────────────────────────────────
#  14. FILE I/O
#  write(path, str): create/overwrite    append(path, str): add to end
#  read(path): returns whole file as string
# ───────────────────────────────────────────────────────────────────────────────
section("14. FILE I/O")

var tmpfile = "/tmp/moon_reference_demo.txt"

# Write
write(tmpfile, "line 1\nline 2\nline 3\n")
print "wrote 3 lines to " tmpfile

# Append
append(tmpfile, "line 4\n")

# Read back
var content = read(tmpfile)
print "file length: " len(content) " chars"

# Parse first line
var nl = find(content, "\n")
print "first line:  " sub(content, 0, nl)

# Use split to get all lines (split on \n)
var lines = split(content, "\n")
print "line count:  " len(lines) " (last is empty due to trailing newline)"
print "line 2:      " lines[1]

# Build a CSV
var csvfile = "/tmp/moon_demo.csv"
write(csvfile, "name,value\n")
for (var item in [["alpha", 1], ["beta", 2], ["gamma", 3]]) {
    append(csvfile, item[0] + "," + str(item[1]) + "\n")
}
print "csv: " read(csvfile)

# ───────────────────────────────────────────────────────────────────────────────
#  15. LOAD — CODE MODULARISATION
#  load(path): execute a .moon file in the current environment.
#  Procs and vars defined in loaded file become available globally.
#  Searches ~/.moon/ if path not found directly.
# ───────────────────────────────────────────────────────────────────────────────
section("15. LOAD")

# Write a small library file, then load it
write("/tmp/moon_mylib.moon",
    "proc celsius_to_f (c) { return c * 9 / 5 + 32 }\n" +
    "var BOILING = 100\n"
)
load("/tmp/moon_mylib.moon")
print "0°C in F:   " celsius_to_f(0)
print "100°C in F: " celsius_to_f(BOILING)

# ───────────────────────────────────────────────────────────────────────────────
#  16. EVAL — RUNTIME CODE GENERATION
#  eval(string): parse and execute a string as Moon code.
#  Useful for generating code dynamically. Shares the current global environment.
# ───────────────────────────────────────────────────────────────────────────────
section("16. EVAL")

# Generate a proc definition as a string and evaluate it
var op_name = "cube"
var op_body = "proc " + op_name + " (x) { return x * x * x }"
eval(op_body)
print "cube(5) = " cube(5)

# Build a set of similar procs programmatically
for (var note in [["c4",60],["d4",62],["e4",64]]) {
    eval("proc midi_" + note[0] + " () { return " + str(note[1]) + " }")
}
print "midi_c4() = " midi_c4()
print "midi_e4() = " midi_e4()

# ───────────────────────────────────────────────────────────────────────────────
#  17. MISC BUILTINS
# ───────────────────────────────────────────────────────────────────────────────
section("17. MISC BUILTINS")

# Type conversion
print "num('3.14') = " num("3.14")
print "str(PI)     = " str(PI)
print "char(9786)  = " char(9786)    # ☺ (unicode codepoint as char via cast)

# Random (seeded with 42 at startup — deterministic)
print "rand()      = " fmt_fixed(rand(), 4)
print "rand()      = " fmt_fixed(rand(), 4)

# Clock — CPU seconds (useful for timing)
var t0 = clock()
var dummy = arr_sort(arr_reverse(range(1, 1000)))
var elapsed = clock() - t0
print "sort 999 items took: " fmt_fixed(elapsed * 1000, 2) " ms"

# exec — shell command (return value is exit code)
var rc = exec("echo 'exec: hello from shell'")
print "exit code: " rc

# keys — global variable names (debugging aid)
var gkeys = keys()
print "global var count: " len(gkeys)

# ───────────────────────────────────────────────────────────────────────────────
#  18. PRACTICAL EXAMPLE — Sieve of Eratosthenes
#  Combines: arrays, vectors, for/in, while, procs, stdlib
# ───────────────────────────────────────────────────────────────────────────────
section("18. PRACTICAL: SIEVE OF ERATOSTHENES")

proc primes_up_to (limit) {
    var sieve = arr(limit + 1, 1)   # all true initially
    sieve[0] = 0
    sieve[1] = 0
    var i4 = 2
    while (i4 * i4 <= limit) {
        if (sieve[i4] == 1) {
            var j4 = i4 * i4
            while (j4 <= limit) {
                sieve[j4] = 0
                j4 = j4 + i4
            }
        }
        i4 = i4 + 1
    }
    var result = []
    var k4 = 2
    while (k4 <= limit) {
        if (sieve[k4] == 1) { push(result, k4) }
        k4 = k4 + 1
    }
    return result
}

var primes = primes_up_to(50)
print "primes ≤ 50: " primes
print "count: " len(primes)

# ───────────────────────────────────────────────────────────────────────────────
#  19. PRACTICAL EXAMPLE — Music: MIDI / Hz with vector math
#  Shows real-world use of vector broadcasting and element-wise math
# ───────────────────────────────────────────────────────────────────────────────
section("19. PRACTICAL: MUSIC THEORY")

proc midi_to_hz (midi) {
    return 440 * pow(2, (midi - 69) / 12)
}

# Chromatic scale from C4 (60) using a vector of offsets
var c_major_offsets = vec(0, 2, 4, 5, 7, 9, 11, 12)   # C major intervals
var c_major_midi    = c_major_offsets + 60              # transpose to C4
var c_major_hz      = midi_to_hz(c_major_midi)          # one call for all 8

print "C major (MIDI): " c_major_midi
print "C major (Hz):   "
var note_i = 0
while (note_i < len(c_major_hz)) {
    print "  " fmt_fixed(c_major_hz[note_i], 2) " Hz"
    note_i = note_i + 1
}

# Chord: equal-tempered interval ratios
proc et_ratio (semitones) { return pow(2, semitones / 12) }
var major_triad = vec(0, 4, 7)   # unison, major third, perfect fifth
var ratios = et_ratio(major_triad)
print "major triad ratios: " ratios

# ───────────────────────────────────────────────────────────────────────────────
#  DONE
# ───────────────────────────────────────────────────────────────────────────────
section("DONE")
print "Reference demo complete."
