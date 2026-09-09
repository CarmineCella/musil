# lists and vectors
# A list holds anything; a vector (vec) holds numbers and computes elementwise.

# --- vectors ---
load "std.mu"
var a (vec 10 20 30 40 50)
print "a          =" a
print "a[0]       =" (getidx a 0)
print "a[4]       =" (getidx a 4)
print "a[-1]      =" (getidx a -1)          # negative index: last element
print "length     =" (length a)
setidx a 2 99
print "after a[2]=99:" a

# --- reference semantics ---
var b a                              # b and a are the same vector
setidx b 0 777
print "a after b[0]=777:" a          # shared reference
var c (copy a)                       # explicit copy breaks sharing
setidx c 0 0
print "a after c[0]=0  :" a          # unchanged

# --- lists: push / pop ---
var s (list)
push s "x"
push s "y"
push s "z"
print "stack after 3 pushes:" s
var top (pop s)
print "popped:" top " stack:" s
var nums (list 1 2 3)
print "append:" (append nums 4 5 6)

# --- slice / concat ---
var v (range 10)
print "slice v 2 4 =" (slice v 2 4)
print "first 3 + last 3 =" (vec (take v 3) (drop v 7))

# --- range ---
print "range 5     =" (range 5)
print "range 0 10 2 =" (range 0 10 2)
print "range 5 0 -1 =" (range 5 0 -1)

# --- join / split ---
var words (list "hello" "musil" "world")
var sentence (join words " ")
print "join:" sentence
var parts (split sentence " ")
print "split back:" parts
print "parts[1] =" (getidx parts 1)

# --- nested lists ---
var matrix (list (list 1 2 3) (list 4 5 6) (list 7 8 9))
print "matrix[1][2] =" (getidx (getidx matrix 1) 2)
setidx (getidx matrix 1) 2 99
print "after matrix[1][2]=99:" (getidx matrix 1)

# --- iteration ---
print "sum 1..5 =" (sum (range 1 6))
var vowels (count (split "hello world" "") (function (ch) (contains? "aeiou" ch)))
print "vowels in 'hello world' =" vowels
print "first > 8:" (head (filter (list 3 7 2 9 1 5) (function (x) (> x 8))))

# --- constructors and types ---
print "zeros 5    =" (zeros 5)
print "ones 4     =" (ones 4)
print "type list  =" (type (list))
print "type 42    =" (type 42)
print "type vec   =" (type (vec 1 2))
print "type 'hi'  =" (type "hi")
