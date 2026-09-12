# musil — language reference: the core (core.h)
#
# A single, self-contained file demonstrating every feature of the language
# itself. It loads nothing: only what core.h provides is used. The libraries
# have their own references: reference_std.mu, reference_system.mu.
# Run with: musil reference.mu
#
# Surface conventions (f8-style hybrid):
#   - Top level and inside {}: each line is auto-listified as a command.
#   - (form ...) is a Lisp list — newlines inside parens are whitespace.
#   - {form ...} is a (do form ...) block, newline-separated sub-lists.
#   - "..." is a string. 'x is shorthand for (quote x).
#   - # ... is a comment to end of line. \ at end of line continues onto next.

print ""
print "================================================================"
print "  musil: language reference (core)"
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

# var defines (or redefines) a variable in the current environment;
# set assigns to an existing one, however far out. A function's var is
# always local, so a library can never clobber your variables by accident.
var counter 0
function bump () (set counter (+ counter 1))
bump
bump
function shadow () {
    var counter 99             # a different, local counter
    return counter
}
print "counter after two bumps =" counter ", shadow returns" (shadow) ", counter still" counter
print "set on an unknown name:" (try (set unknown 1) catch e e)

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
# Comparisons are elementwise on vectors, lexicographic on two strings
print "vec == vec :" (== (vec 1 2 3) (vec 1 0 3))
print "\"a\" < \"b\"  :" (< "a" "b")
# == on non-numbers and equal? compare structure
print "equal? lists:" (equal? (list 1 (list 2)) (list 1 (list 2)))
# Constants
print "true false nil pi inf:" true false nil pi inf

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
print "pow 2 10   :" (pow 2 10)
print "atan2 1 1  :" (atan2 1 1)
# Every math function is elementwise on vectors
print "sqrt vec   :" (sqrt (vec 1 4 9 16))

# --- 5. Vectors and broadcast ------------------------------------
print ""
print "--- vectors and broadcast ---"

var v (vec 1 2 3 4 5)
print "v          :" v
print "v + 10     :" (+ v 10)
print "v * 2      :" (* v 2)
print "v + v      :" (+ v v)
print "sin v      :" (sin v)

# vec builds from numbers, vectors and lists of numbers; a scalar is a vector of size 1
print "vec 1 (vec 2 3) 4 :" (vec 1 (vec 2 3) 4)
print "vec (list 7 8)    :" (vec (list 7 8))
print "type 3 / type v   :" (type 3) (type v)

# Reductions
print "sum v        :" (sum v)
print "min v, max v :" (min v) (max v)
# range, linspace, zeros, mean, sort, ...: see reference_std.mu

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
print "getidx -1  :" (getidx lst -1) "(negative indices count from the end)"

# Mutation via setidx
setidx vc 0 999
print "vc after setidx:" vc

# --- 8. Strings in the core -------------------------------------
print ""
print "--- strings (core) ---"

# The core knows how to make, print, compare, index and convert strings.
# split, join, upper, format, ...: see reference_std.mu
var greeting "hello"
print "length     :" (length greeting)
print "getidx 0   :" (getidx greeting 0)
print "== / <     :" (== greeting "hello") (< "apple" "pear")
print "str 3.5    :" (str 3.5) "  num \"42\":" (num "42") "  sym \"abc\":" (sym "abc")
print "escapes    :" "tab\there, quote \" and newline:"
print "  multi-line strings are fine\n  (the newline is in the string)"

# --- 9. Control flow --------------------------------------------
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

# --- 10. Functions: named, lambda, multi-line, closure -----------
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
        set c (+ c 1)
        return c
    })
}
var ctr (make-counter)
print "counter:" (ctr) (ctr) (ctr) (ctr)

# --- 11. Currying (implicit partial application) -----------------
print ""
print "--- currying: too few args returns a partial ---"

function add3 (a b c) (+ a b c)
var p (add3 1)
var q (p 2)
print "((add3 1) 2) 100:" (q 100)

# A partial is an ordinary function value
function mul (a b) (* a b)
var double (mul 2)
print "double 21  :" (double 21)

# Too many args: the result is applied to the remaining args
function twice (f) (function (x) (f (f x)))
function inc1 (n) (+ n 1)
print "(twice inc1 5) :" (twice inc1 5)

# --- 12. Recursion + TCO -----------------------------------------
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

# --- 13. Quote: data vs code -------------------------------------
print ""
print "--- quote: code as data ---"

# 'x is shorthand for (quote x). Quote returns its argument unevaluated.
print "(+ 1 2) evaluated :" (+ 1 2)
print "'(+ 1 2) is data  :" '(+ 1 2)
print "'foo (a symbol)   :" 'foo
print "type of '(+ 1 2)  :" (type '(+ 1 2))
print "type of 'foo      :" (type 'foo)

# --- 14. eval and apply: first-class code execution --------------
print ""
print "--- eval and apply ---"

# eval runs a quoted form
var program '(+ 1 2 3 4)
print "eval program:" (eval program)

# parse reads text into forms (what reading a file does): text -> form -> eval is a REPL in two calls.
# examples/bootstrap.mu goes all the way: an evaluator for the language written in the language.
print "parse text    :" (parse "print 1\nvar q 2")
print "eval of parsed:" (eval (parse "(* 6 7)"))

# Build a program at runtime
var op '+
var prog (list op 10 20 30)
print "built then eval:" (eval prog)

# apply spreads a list as positional arguments
function add4 (a b c d) (+ a b c d)
var args (list 1 2 3 4)
print "apply add4:" (apply add4 args)

# --- 15. Meta: type, str, num ------------------------------------
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
print "defined? square:" (defined? 'square) "  defined? nope:" (defined? 'nope)
print "vars has print :" (> (length (vars)) 50)
print "version        :" (type version)
# (exit [code]) ends the program; clock is a monotonic time in seconds

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

# --- 16. References and explicit copy ----------------------------
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

# --- 17. Error handling: try / catch / error ---------------------
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

# error inside a nested try propagates to the nearest handler
print "nested     :" (try (try (error "inner") catch e (error "outer saw " e)) catch e e)

# Errors carry file, line and the call stack (innermost first)
function inner (x) (error "from inner")
function outer (x) {
    var r (inner x)
    return r
}
print "call stack :" (try (outer 1) catch e e)

# --- 18. load: bring in another .mu file -------------------------
print ""
print "--- load ---"
# (load "file.mu") runs another file once, in the global environment.
# Search order: the directory of the loading file, the current directory,
# each entry of MUSIL_PATH, then ~/.musil (where `cmake --install` puts the
# libraries). A file that ran is not run again; a file that failed is.
print "(load \"std.mu\") would bring in the standard library from ~/.musil or MUSIL_PATH"
print "missing file:" (try (load "no-such-file.mu") catch e e)

# --- 19. Closing demo: Newton's method for sqrt ------------------
print ""
print "--- closing: Newton's method for sqrt ---"

function newton-sqrt (target guess) {
    var next (/ (+ guess (/ target guess)) 2)
    if (< (max (abs (- next guess))) 0.0000001) {
        return next
    }
    return (newton-sqrt target next)
}

print "newton sqrt 2   :" (newton-sqrt 2 1)
print "newton sqrt 612 :" (newton-sqrt 612 10)
print "math   sqrt 612 :" (sqrt 612)

# Newton's iteration works on a whole vector at once, since / and + broadcast
print "newton on vector:" (newton-sqrt (vec 1 4 9 16 25) (vec 1 1 1 1 1))

print ""
print "================================================================"
print "  end of reference"
print "================================================================"
