print "=== stress tests ==="

# --- TCO depth ---
function loop (n acc) {
    if (== n 0) {
        return acc
    }
    return (loop (- n 1) (+ acc 1))
}
print "TCO 50000:" (loop 50000 0)

# --- currying ---
function add3 (a b c) (+ a b c)
var add5 (add3 5)
var add5_10 (add5 10)
print "curry add5_10 100:" (add5_10 100)

# --- map+curry: doubling ---
function mul (a b) (* a b)
var doubled (map (vec 1 2 3 4 5) (mul 2))
print "double via curry:" doubled

# --- polymorphism: map on list and on vec ---
print "map on list:" (map (list 1 2 3) (function (x) (* x x)))
print "map on vec :" (map (vec  1 2 3) (function (x) (* x x)))
print "filter list:" (filter (list 1 2 3 4 5) (function (x) (> x 2)))
print "filter vec :" (filter (vec  1 2 3 4 5) (function (x) (> x 2)))
print "reduce list:" (reduce (list 1 2 3 4) (function (a b) (+ a b)) 0)
print "reduce vec :" (reduce (vec  1 2 3 4) (function (a b) (+ a b)) 0)

# --- length/head/tail on list, vec, string ---
print "len list :" (length (list 1 2 3))
print "len vec  :" (length (vec  1 2 3 4 5))
print "len str  :" (length "hello")
print "head vec :" (head  (vec 10 20 30))
print "tail vec :" (tail  (vec 10 20 30))

# --- broadcast ---
var u (vec 1 2 3 4)
var w (vec 10 20 30 40)
print "u+w  :" (+ u w)
print "u*2  :" (* u 2)
print "expr :" (expr (u + w * 2))

# --- closures + lexical scope ---
function make-counter () {
    var c 0
    return (function () {
        var c (+ c 1)
        return c
    })
}
var ctr (make-counter)
print "counter:" (ctr) (ctr) (ctr)

# --- try/catch ---
var caught (try (/ 1 0) catch e e)
print "div by zero caught? size of message > 0:" (> (length caught) 0)

var caught2 (try (head (list)) catch e (concat "caught: " e))
print caught2

# --- string ops ---
print "concat   :" (concat "a" "b" 42)
print "split    :" (split "1,2,3" ",")
print "join     :" (join (list "x" "y" "z") "-")
print "trim     :" (concat "[" (trim "  hi  ") "]")

# --- copy vs reference ---
var a (vec 1 2 3)
var b a
setidx b 0 99
print "shared (a should be (99 2 3)):" a
var c (vec 1 2 3)
var d (copy c)
setidx d 0 99
print "copied (c should be (1 2 3))  :" c

# --- for loop ---
var t 0
for (var i 0) (< i 10) (var i (+ i 1)) {
    var t (+ t i)
}
print "for sum 0..9:" t

# --- break/continue ---
var found -1
var i 0
while (< i 100) {
    if (== i 7) {
        var found i
        break
    }
    var i (+ i 1)
}
print "break at:" found

# --- I/O ---
write "/tmp/musil_test.txt" "hello\nworld\n"
print "read back:" (read "/tmp/musil_test.txt")
append-file "/tmp/musil_test.txt" "more\n"
print "after append:" (read "/tmp/musil_test.txt")

print "=== stress done ==="
