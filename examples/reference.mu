# musil — language reference
#
# A single, self-contained file demonstrating every feature of the language.
# Run with: ./musil reference.mu
#
# Surface conventions (f8-style hybrid):
#   - Top level and inside {}: each line is auto-listified as a command.
#   - (form ...) is a Lisp list — newlines inside parens are whitespace.
#   - {form ...} is a (do form ...) block, newline-separated sub-lists.
#   - "..." is a string. 'x is shorthand for (quote x).
#   - # ... is a comment to end of line. \ at end of line continues onto next.

print ""
print "================================================================"
print "  musil: language reference"
print "================================================================"

# --- 1. Variables and arithmetic ---------------------------------
print ""
print "--- variables and arithmetic ---"

var x 10
var y 3
print "x =" x "y =" y
print "x + y =" (+ x y)
print "x - y =" (- x y)
print "x * y =" (* x y)
print "x / y =" (/ x y)
print "(- 7) (unary) =" (- 7)
print "abs (- 7) =" (abs (- 7))
print "mod 17 5 =" (mod 17 5)

# --- 2. Comparisons and booleans ---------------------------------
print ""
print "--- comparisons and booleans ---"

print "x < y :" (< x y)
print "x > y :" (> x y)
print "x == 10:" (== x 10)
print "x != y :" (!= x y)
print "and 1 1:" (and 1 1)
print "or  0 1:" (or 0 1)
print "not 0  :" (not 0)
print "min:" (min (vec 5 2 9 1 7))
print "max:" (max (vec 5 2 9 1 7))

# --- 3. expr Pratt: infix math inside (expr ...) -----------------
print ""
print "--- expr Pratt: infix math ---"

print "x + y * 2     =" (expr (x + y * 2))
# Parentheses group sub-expressions, as in ordinary infix notation:
print "(x + y) * 2   =" (expr ((x + y) * 2))
# A parenthesized form that is not infix (no operator in second position) is a call:
print "(+ x y) * 2   =" (expr ((+ x y) * 2))
print "x % y         =" (expr (x % y))

# Mixed math and function calls
function inc (n) (+ n 1)
print "(inc x) + y * 2 =" (expr ((inc x) + y * 2))

# --- 4. Math functions (work on scalars and vectors) -------------
print ""
print "--- math functions ---"

print "sqrt 2     :" (sqrt 2)
print "sin pi/2   :" (sin (/ pi 2))
print "cos 0      :" (cos 0)
print "tan pi/4   :" (tan (/ pi 4))
print "exp 1      :" (exp 1)
print "log e      :" (log (exp 1))
print "floor 3.7  :" (floor 3.7)
print "ceil  3.2  :" (ceil 3.2)
print "round 3.5  :" (round 3.5)

# Random numbers
seed 42
print "rand       :" (rand)
print "rand 5     :" (rand 5)

# --- 5. Vectors and broadcast ------------------------------------
print ""
print "--- vectors and broadcast ---"

var v (vec 1 2 3 4 5)
print "v          :" v
print "v + 10     :" (+ v 10)
print "v * 2      :" (* v 2)
print "v + v      :" (+ v v)
print "sin v      :" (sin v)

# Constructors
print "range 5      :" (range 5)
print "linspace 0 1 5:" (linspace 0 1 5)
print "zeros 4      :" (zeros 4)
print "ones 3       :" (ones 3)

# Reductions
print "sum v        :" (sum v)
print "mean v       :" (mean v)
print "sort         :" (sort (vec 3 1 4 1 5 9 2 6))

# --- 6. Lists (heterogeneous ordered collections) ----------------
print ""
print "--- lists ---"

var L (list 1 "two" 3.0 (vec 4 5))
print "L            :" L
print "list 1 2 3   :" (list 1 2 3)
print "cons 0 ...   :" (cons 0 (list 1 2 3))
print "append       :" (append (list 1 2) 3 4)

# Mutation
var M (list 10 20 30)
push M 99
print "after push 99:" M
print "popped       :" (pop M)
print "after pop    :" M

# --- 7. Polymorphism: same op on lists, vectors, strings ---------
print ""
print "--- polymorphic ops (LIST, NUM, STR) ---"

var lst (list 10 20 30)
var vc  (vec  10 20 30)
print "length list:" (length lst) "  vec:" (length vc) "  str:" (length "hello")
print "head   list:" (head lst)   "  vec:" (head vc)
print "tail   list:" (tail lst)   "  vec:" (tail vc)
print "empty? ()  :" (empty? (list)) "  empty? L:" (empty? L)
print "getidx vec :" (getidx vc 2)
print "getidx str :" (getidx "hello" 1)

# Mutation via setidx
setidx vc 0 999
print "vc after setidx:" vc

# --- 8. Higher-order: map, filter, reduce ------------------------
print ""
print "--- higher-order (polymorphic on list and vec) ---"

print "map sqr list:" (map (list 1 2 3 4) (function (x) (* x x)))
print "map sqr vec :" (map (vec  1 2 3 4) (function (x) (* x x)))

print "filter list :" (filter (list 1 2 3 4 5 6) (function (x) (== (mod x 2) 0)))
print "filter vec  :" (filter (vec  1 2 3 4 5 6) (function (x) (> x 3)))

print "reduce sum  :" (reduce (list 1 2 3 4 5) (function (a b) (+ a b)) 0)
print "reduce prod :" (reduce (vec  1 2 3 4 5) (function (a b) (* a b)) 1)

# --- 9. Strings --------------------------------------------------
print ""
print "--- strings ---"

print "concat   :" (concat "hello, " "world" "! " 42)
print "split    :" (split "a,b,c,d" ",")
print "join     :" (join (list "x" "y" "z") "-")
print "upper    :" (upper "musil")
print "lower    :" (lower "MUSIL")
print "trim     :" (concat "[" (trim "  hi  ") "]")

# --- 10. Control flow --------------------------------------------
print ""
print "--- control flow ---"

# if with a then-block only
if (> x 5) {
    print "x > 5: yes"
}

# if/else
if (> x 100) {
    print "x is huge"
} {
    print "x is normal"
}

# while loop
var i 0
var s 0
while (< i 10) {
    var s (+ s i)
    var i (+ i 1)
}
print "while: sum 0..9 =" s

# for loop (init cond step body)
var t 0
for (var k 0) (< k 10) (var k (+ k 1)) {
    var t (+ t k)
}
print "for:   sum 0..9 =" t

# break
var j 0
while (< j 100) {
    if (== j 7) {
        break
    }
    var j (+ j 1)
}
print "break at j =" j

# continue
var odds (list)
var k 0
while (< k 10) {
    var k (+ k 1)
    if (== (mod k 2) 0) {
        continue
    }
    push odds k
}
print "odds 1..10 :" odds

# --- 11. Functions: named, lambda, multi-line, closure -----------
print ""
print "--- functions ---"

# Named, single-expression body
function square (n) (* n n)
print "square 7:" (square 7)

# Multi-line body in {}
function quadratic (a b c x) {
    var t1 (* a (* x x))
    var t2 (* b x)
    return (+ t1 t2 c)
}
print "2x^2 + 3x + 1 at x=4:" (quadratic 2 3 1 4)

# Anonymous lambda assigned to a var
var dbl (function (n) (* n 2))
print "lambda dbl 21:" (dbl 21)

# Closure: returned function captures n
function make-adder (n) {
    return (function (x) (+ x n))
}
var add5 (make-adder 5)
print "closure add5 100:" (add5 100)

# Stateful closure (counter)
function make-counter () {
    var c 0
    return (function () {
        var c (+ c 1)
        return c
    })
}
var ctr (make-counter)
print "counter:" (ctr) (ctr) (ctr) (ctr)

# --- 12. Currying (implicit partial application) -----------------
print ""
print "--- currying: too few args returns a partial ---"

function add3 (a b c) (+ a b c)
var p (add3 1)
var q (p 2)
print "((add3 1) 2) 100:" (q 100)

# Useful with map
function mul (a b) (* a b)
print "doubled vec:" (map (vec 1 2 3 4 5) (mul 2))

# Too many args: the result is applied to the remaining args
function twice (f) (function (x) (f (f x)))
function inc1 (n) (+ n 1)
print "(twice inc1 5) :" (twice inc1 5)

# --- 13. Recursion + TCO -----------------------------------------
print ""
print "--- recursion ---"

# Standard recursive factorial
function fact (n) {
    if (< n 2) {
        return 1
    }
    return (* n (fact (- n 1)))
}
print "fact 10:" (fact 10)

# Non-tail recursion (Fibonacci, exponential)
function fib (n) {
    if (< n 2) {
        return n
    }
    return (+ (fib (- n 1)) (fib (- n 2)))
}
print "fib 15 :" (fib 15)

# Tail-recursive accumulator — TCO eliminates stack frames
function sum-to (n acc) {
    if (== n 0) {
        return acc
    }
    return (sum-to (- n 1) (+ acc n))
}
print "sum 1..50000 (TCO):" (sum-to 50000 0)

# Mutual recursion
function evenp (n) {
    if (== n 0) {
        return 1
    }
    return (oddp (- n 1))
}
function oddp (n) {
    if (== n 0) {
        return 0
    }
    return (evenp (- n 1))
}
print "even? 100 :" (evenp 100)
print "odd?  101 :" (oddp 101)

# --- 14. Quote: data vs code -------------------------------------
print ""
print "--- quote: code as data ---"

# 'x is shorthand for (quote x). Quote returns its argument unevaluated.
print "(+ 1 2) evaluated :" (+ 1 2)
print "'(+ 1 2) is data  :" '(+ 1 2)
print "'foo (a symbol)   :" 'foo
print "type of '(+ 1 2)  :" (type '(+ 1 2))
print "type of 'foo      :" (type 'foo)

# --- 15. eval and apply: first-class code execution --------------
print ""
print "--- eval and apply ---"

# eval runs a quoted form
var program '(+ 1 2 3 4)
print "eval program:" (eval program)

# Build a program at runtime
var op '+
var prog (list op 10 20 30)
print "built then eval:" (eval prog)

# apply spreads a list as positional arguments
function add4 (a b c d) (+ a b c d)
var args (list 1 2 3 4)
print "apply add4:" (apply add4 args)

# --- 16. Meta: type, str, num ------------------------------------
print ""
print "--- meta ---"

print "type 42        :" (type 42)
print "type \"hi\"      :" (type "hi")
print "type 'sym      :" (type 'sym)
print "type (list)    :" (type (list))
print "type (vec 1 2) :" (type (vec 1 2))
print "type square    :" (type square)

print "str 42         :" (str 42)
print "str (list 1 2) :" (str (list 1 2))
print "num \"3.14\"     :" (num "3.14")

# Time measurement
var t0 (clock)
var x 0
var k 0
while (< k 100000) {
    var x (+ x k)
    var k (+ k 1)
}
var dt (- (clock) t0)
print "100k iterations took:" dt "seconds"

# --- 17. References and explicit copy ----------------------------
print ""
print "--- shared references vs explicit copy ---"

# By default, var aliases share the underlying value
var orig (vec 1 2 3)
var alias orig
setidx alias 0 99
print "orig (shared with alias):" orig

# copy makes an independent value
var src (vec 1 2 3)
var dup (copy src)
setidx dup 0 99
print "src (untouched by dup)  :" src
print "dup                      :" dup

# --- 18. Error handling: try / catch / error ---------------------
print ""
print "--- errors ---"

# Catch a real runtime error from num parsing
var caught (try (num "not a number") catch e (concat "caught: " e))
print "num parse ->" caught

# Catch from a polymorphic op
var safe (try {
    head (list)
} catch e (concat "caught: " e))
print "head empty:" safe

# Throw with the error builtin
function check-positive (n) {
    if (< n 0) {
        error "negative not allowed: " n
    }
    return n
}
print "check 5  :" (try (check-positive  5) catch e e)
print "check -3 :" (try (check-positive -3) catch e e)

# Builtins check their argument types and counts
print "type error :" (try (+ 1 "a") catch e e)
print "arity error:" (try (sqrt) catch e e)
print "size error :" (try (+ (vec 1 2) (vec 1 2 3)) catch e e)

# assert stops with a message when its condition is false
assert (== (+ 2 2) 4) "arithmetic works"
print "assert     :" (try (assert (== 1 2) "one is not two") catch e e)

# Errors carry file, line and the call stack (innermost first)
function inner (x) (error "from inner")
function outer (x) {
    var r (inner x)
    return r
}
print "call stack :" (try (outer 1) catch e e)

# --- 19. File I/O and exec ---------------------------------------
print ""
print "--- file I/O ---"

write "/tmp/musil_demo.txt" "first line\nsecond line\n"
append-file "/tmp/musil_demo.txt" "third line\n"
print "contents of /tmp/musil_demo.txt:"
print (read "/tmp/musil_demo.txt")

print "exec stat:"
print (trim (exec "wc -l /tmp/musil_demo.txt"))

# --- 20. load: bring in another .mu file -------------------------
# Uncomment if you have a sibling file to load:
#   load "lib.mu"
# load tracks files by canonical path, so a second load is a no-op
# (cycle protection).
print ""
print "--- load ---"
print "(see comment in source — load \"file.mu\" pulls in another .mu;"
print " loaded files are cached so cycles and double-loads are safe)"

# --- 21. Closing demo: Newton's method for sqrt ------------------
print ""
print "--- closing: Newton's method for sqrt ---"

function abs-diff (a b) (abs (- a b))

function newton-sqrt (target guess) {
    var next (/ (+ guess (/ target guess)) 2)
    if (< (abs-diff next guess) 0.0000001) {
        return next
    }
    return (newton-sqrt target next)
}

print "newton sqrt 2   :" (newton-sqrt 2 1)
print "newton sqrt 612 :" (newton-sqrt 612 10)
print "math   sqrt 612 :" (sqrt 612)

# Compose with map: sqrt of every element via Newton's method
function nsqrt (x) (newton-sqrt x 1)
print "newton on vector:" (map (vec 1 4 9 16 25) nsqrt)

print ""
print "================================================================"
print "  end of reference"
print "================================================================"
