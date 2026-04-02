# ── Array literals and indexing ───────────────────────────────────────────────
var a = [10, 20, 30, 40, 50]
print "a        = " a
print "a[0]     = " a[0]
print "a[4]     = " a[4]
print "a[-1]    = " a[-1]          # negative index: last element
print "len(a)   = " len(a)

# ── Mutation ──────────────────────────────────────────────────────────────────
a[2] = 99
print "after a[2]=99: " a

# ── Reference semantics ───────────────────────────────────────────────────────
var b = a                          # b and a point to same array
b[0] = 777
print "a after b[0]=777: " a      # should show 777 — shared reference

var c = copy(a)                    # explicit copy breaks sharing
c[0] = 0
print "a after c[0]=0 : " a       # should be unchanged

# ── push / pop ────────────────────────────────────────────────────────────────
var s = []
push(s, "x")
push(s, "y")
push(s, "z")
print "stack after 3 pushes: " s
var top = pop(s)
print "popped: " top "  stack: " s

# ── push multiple ─────────────────────────────────────────────────────────────
var nums = [1, 2, 3]
push(nums, 4, 5, 6)
print "after push(nums,4,5,6): " nums

# ── insert / remove ───────────────────────────────────────────────────────────
var t = [1, 2, 4, 5]
insert(t, 2, 3)                    # insert 3 before index 2
print "after insert(t,2,3): " t
var removed = remove(t, 0)
print "removed t[0]=" removed "  t=" t

# ── slice / concat ────────────────────────────────────────────────────────────
var v = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
print "slice(v,2,6) = " slice(v, 2, 6)
var v2 = concat(slice(v, 0, 3), slice(v, 7, 10))
print "concat first3+last3 = " v2

# ── range ─────────────────────────────────────────────────────────────────────
print "range(0,5)      = " range(0, 5)
print "range(0,10,2)   = " range(0, 10, 2)
print "range(5,0,-1)   = " range(5, 0, -1)

# ── join / split ──────────────────────────────────────────────────────────────
var words = ["hello", "musil", "world"]
var sentence = join(words, " ")
print "join: " sentence
var parts = split(sentence, " ")
print "split back: " parts
print "parts[1] = " parts[1]

# ── 2D array (array of arrays) ────────────────────────────────────────────────
var matrix = [[1,2,3],[4,5,6],[7,8,9]]
print "matrix[1][2] = " matrix[1][2]
matrix[1][2] = 99
print "after matrix[1][2]=99: " matrix[1]

# ── for/in over array ─────────────────────────────────────────────────────────
print "--- for/in over array ---"
var total = 0
for (var x in range(1, 6)) {
    total = total + x
}
print "sum 1..5 via for/in = " total

# ── for/in over string (characters) ──────────────────────────────────────────
print "--- for/in over string ---"
var vowels = 0
for (var ch in "hello world") {
    if (ch == "a" or ch == "e" or ch == "i" or ch == "o" or ch == "u") {
        vowels = vowels + 1
    }
}
print "vowels in 'hello world' = " vowels

# ── break in for/in ───────────────────────────────────────────────────────────
var found = -1
for (var x in [3, 7, 2, 9, 1, 5]) {
    if (x == 9) { found = x   break }
}
print "found 9 at value: " found

# ── arr() constructor ─────────────────────────────────────────────────────────
var zeros = arr(5)
print "arr(5)       = " zeros
var filled = arr(4, "x")
print "arr(4,'x')   = " filled

# ── type() ────────────────────────────────────────────────────────────────────
print "type([])     = " type([])
print "type(42)     = " type(42)
print "type('hi')   = " type("hi")
