# test_core.mu — systematic, self-checking test of the Musil core.
#
# Every check is an (assert ...). A passing run prints only the final line.
# Run with: musil tests/test_core.mu

var checks 0
function check (cond msg) {
    assert cond msg
    var checks (+ checks 1)
}
# (fails? thunk) => 1 if calling thunk raises an error, else 0
function fails? (thunk) {
    try {
        thunk
        0
    } catch e 1
}
# (error-of thunk) => the error message text, or "" if none
function error-of (thunk) {
    try {
        thunk
        ""
    } catch e e
}
function contains? (s sub) (>= (find s sub) 0)

# --- reader ------------------------------------------------------------
check (== 1 1) "reader: numbers"
check (equal? '(a b c) (list 'a 'b 'c)) "reader: quote"
check (equal? (quote (1 2)) (list 1 2)) "reader: quote long form"
check (== (length "a\tb\nc") 5) "reader: escapes"
check (equal? "with \"quotes\"" (concat "with " (chr 34) "quotes" (chr 34))) "reader: escaped quote"
var multi (+ 1 \
            2 \
            3)
check (== multi 6) "reader: line continuation"
var inside (list 1
                 2
                 3)
check (== (length inside) 3) "reader: newlines inside parens are whitespace"
check (== ((+ 1 2)) 3) "reader: single list on a line is not re-wrapped"
check (equal? (type nil) "nil") "reader: nil constant"
check (== -3.5 (- 3.5)) "reader: negative literal"
check (equal? (type 'x) "symbol") "reader: symbol type" # trailing comment

# --- numbers and printing ------------------------------------------------
check (equal? (str 42) "42") "print: integer-valued doubles print without decimals"
check (equal? (str 2.5) "2.5") "print: fractional"
check (equal? (str (/ 1 3)) "0.333333333333333") "print: 15 significant digits"
check (equal? (str (+ 0.1 0.2)) "0.3") "print: 0.1+0.2"
check (equal? (str (* 123456789 10)) "1234567890") "print: large integers stay exact"
check (equal? (str (vec 1 2.5)) "(1 2.5)") "print: vectors"
check (equal? (str (list 1 "a" 'b)) "(1 a b)") "print: lists"
check (equal? (str (/ 1 0)) "inf") "print: infinity"

# --- arithmetic ----------------------------------------------------------
check (== (+) 0) "+: identity"
check (== (*) 1) "*: identity"
check (== (+ 1 2 3 4) 10) "+: variadic"
check (== (- 10) -10) "-: unary"
check (== (- 10 3 2) 5) "-: variadic"
check (== (/ 4) 0.25) "/: unary reciprocal"
check (== (mod 17 5) 2) "mod"
check (== (mod -7 3) -1) "mod: sign follows fmod"
check (== (pow 2 10) 1024) "pow"
check (== (abs -3) 3) "abs"
check (== (floor 2.7) 2) "floor"
check (== (ceil 2.1) 3) "ceil"
check (== (round 2.5) 3) "round"
check (< (abs (- (sin pi) 0)) 1e-12) "sin pi"
check (< (abs (- (log (exp 1)) 1)) 1e-12) "log exp"
check (== (log2 8) 3) "log2"
check (== (log10 1000) 3) "log10"
check (== (atan2 0 1) 0) "atan2"
check (== (sqrt 16) 4) "sqrt"
check (== (min 3 1 2) 1) "min: variadic"
check (== (max (vec 3 9 2)) 9) "max: reduction"
check (equal? (min (vec 1 5) (vec 3 2)) (vec 1 2)) "min: elementwise"

# --- comparisons and logic -----------------------------------------------
check (== (< 1 2) 1) "<"
check (== (>= 2 2) 1) ">="
check (== (!= 1 2) 1) "!="
check (equal? (== (vec 1 2 3) (vec 1 0 3)) (vec 1 0 1)) "==: elementwise on vectors"
check (== (== "a" "a") 1) "==: strings compare structurally"
check (== (== "a" "b") 0) "==: different strings"
check (== (== (list 1 2) (list 1 2)) 1) "==: lists compare structurally"
check (== (== 1 "1") 0) "==: different types are unequal"
check (equal? (vec 1 2) (vec 1 2)) "equal?: vectors"
check (not (equal? (vec 1 2) (vec 1 2 3))) "equal?: size differs"
check (equal? (list 1 (list 2 "x")) (list 1 (list 2 "x"))) "equal?: nested"
check (== (not 0) 1) "not"
check (== (and 1 1 0) 0) "and"
check (== (or 0 0 3) 1) "or"
check (== (and) 1) "and: empty is true"
check (not (if 0 1)) "truthiness: 0 is false"
check (not (if "" 1)) "truthiness: empty string is false"
check (not (if (list) 1)) "truthiness: empty list is false"
check (not (if (vec) 1)) "truthiness: empty vector is false"
check (not (if (vec 1 0) 1)) "truthiness: vector needs all nonzero"
check (if (vec 1 1) 1) "truthiness: all-nonzero vector is true"
check (not (if nil 1)) "truthiness: nil is false"

# --- expr ----------------------------------------------------------------
var x 10
var y 3
check (== (expr (x + y * 2)) 16) "expr: precedence"
check (== (expr ((x + y) * 2)) 26) "expr: parens"
check (== (expr (- x + 1)) -9) "expr: unary minus"
check (== (expr (x % y)) 1) "expr: modulo"
check (== (expr (x > y && y > 0)) 1) "expr: logic"
check (== (expr (x < y || y == 3)) 1) "expr: logic or"
check (equal? (expr ((vec 1 2) * 10 + 1)) (vec 11 21)) "expr: broadcast"
check (== (expr ((sqrt 16) + 1)) 5) "expr: nested call"
check (== (expr ("a" == "a")) 1) "expr: string equality"
check (fails? (function () (expr (x +)))) "expr: dangling operator is an error"
check (fails? (function () (expr (x y)))) "expr: missing operator is an error"
check (fails? (function () (expr (1 + "a")))) "expr: arithmetic on string is an error"

# --- var and scope -------------------------------------------------------
var g 1
function set-g () (var g 2)
set-g
check (== g 2) "var: assigns an existing outer binding"
function local-only () {
    var fresh 99
    return fresh
}
check (== (local-only) 99) "var: creates a local when unbound"
check (not (defined? 'fresh)) "var: local does not leak"
check (== (var z 5) 5) "var: returns the value"

# --- control flow --------------------------------------------------------
check (== (if 1 "a" "b") "a") "if: then"
check (== (if 0 "a" "b") "b") "if: else"
check (equal? (type (if 0 "a")) "nil") "if: no else yields nil"
var i 0
var s 0
while (< i 5) {
    var s (+ s i)
    var i (+ i 1)
}
check (== s 10) "while"
var t 0
for (var k 0) (< k 10) (var k (+ k 1)) {
    if (== (mod k 2) 0) { continue }
    if (== k 7) { break }
    var t (+ t k)
}
check (== t 9) "for with break and continue"
var w 0
var n 0
while 1 {
    var n (+ n 1)
    if (> n 3) (break)
    var w (+ w n)
}
check (== w 6) "while: break"
check (fails? (function () (break))) "break outside loop is an error"
check (equal? (do 1 2 3) 3) "do: returns last"
check (equal? (type (do)) "nil") "do: empty is nil"
var blk {
    var a 1
    var b 2
    (+ a b)
}
check (== blk 3) "brace block returns last"
var lit {
    var a 1
    42
}
check (== lit 42) "a line holding a single literal is that value"

# --- functions -----------------------------------------------------------
function square (x) (* x x)
check (== (square 7) 49) "function: one-liner body"
function fact (n) {
    if (< n 2) {
        return 1
    }
    return (* n (fact (- n 1)))
}
check (== (fact 10) 3628800) "function: recursion with return"
var inc (function (x) (+ x 1))
check (== (inc 5) 6) "lambda"
check (== ((function (a b) (- a b)) 10 4) 6) "lambda: immediate call"
function early (x) {
    if (> x 0) {
        return "pos"
    }
    "non-pos"
}
check (equal? (early 1) "pos") "return: early exit"
check (equal? (early -1) "non-pos") "return: fallthrough"
function nested-return (x) {
    var i 0
    while 1 {
        if (== i x) {
            return i
        }
        var i (+ i 1)
    }
}
check (== (nested-return 4) 4) "return: from inside a loop (non-tail)"
check (contains? (try (eval '(return 1)) catch e e) "return outside function") "return outside function is an error"
function make-counter () {
    var c 0
    return (function () {
        var c (+ c 1)
        return c
    })
}
var ctr (make-counter)
ctr
ctr
check (== (ctr) 3) "closures capture and mutate"
var ctr2 (make-counter)
check (== (ctr2) 1) "closures are independent"
function make-adder (n) (function (x) (+ x n))
check (== ((make-adder 10) 5) 15) "closure over parameter"
check (equal? (type square) "function") "function type"
check (equal? (type +) "function") "builtin type"

# --- early return (rewritten to tail position at definition time) --------
var log (list)
function er1 (x) {
    push log "a"
    if (> x 0) { return "pos" }
    push log "b"
    if (< x 0) {
        push log "c"
        return "neg"
    }
    push log "d"
    return "zero"
}
check (equal? (er1 1) "pos") "early return: first if"
check (equal? log (list "a")) "early return: statements after a taken return do not run"
var log (list)
check (equal? (er1 -1) "neg") "early return: second if"
check (equal? log (list "a" "b" "c")) "early return: order of side effects"
var log (list)
check (equal? (er1 0) "zero") "early return: fallthrough"
check (equal? log (list "a" "b" "d")) "early return: fallthrough side effects"
function er2 (x) {
    if (> x 0) { return "pos" } { push log "else" }
    return (concat "after-" (str x))
}
var log (list)
check (equal? (er2 -5) "after--5") "early return: if with else, else branch continues"
check (equal? log (list "else")) "early return: else branch side effect"
check (equal? (er2 5) "pos") "early return: if with else, then branch returns"
function er3 (x) {
    if (> x 0) { push log "t" } { return "neg-or-zero" }
    return "pos"
}
check (equal? (er3 0) "neg-or-zero") "early return: return in else branch"
check (equal? (er3 1) "pos") "early return: then branch continues"
function er4 (n) {
    var i 0
    while 1 {
        if (== i n) { return i }
        var i (+ i 1)
    }
}
check (== (er4 5) 5) "early return: inside a loop (non-tail, still correct)"
function er5 (x) {
    if (> x 0) {
        if (> x 10) { return "big" }
        return "small"
    }
    return "non-pos"
}
check (equal? (er5 20) "big") "early return: nested ifs"
check (equal? (er5 5) "small") "early return: nested ifs, inner fallthrough"
check (equal? (er5 -1) "non-pos") "early return: nested ifs, outer fallthrough"
function er6 (x) {
    var r (if (> x 0) "p" "n")
    if (== r "p") { return 1 }
    0
}
check (== (er6 1) 1) "early return: last statement is a literal"
check (== (er6 -1) 0) "early return: literal fallthrough"
function fib-er (n) {
    if (< n 2) { return n }
    return (+ (fib-er (- n 1)) (fib-er (- n 2)))
}
check (== (fib-er 20) 6765) "early return: fib"

# --- tail calls ----------------------------------------------------------
function loop (n acc) {
    if (== n 0) {
        return acc
    }
    return (loop (- n 1) (+ acc 1))
}
check (== (loop 100000 0) 100000) "TCO: 100k tail calls via return"
function loop2 (n acc) (if (== n 0) acc (loop2 (- n 1) (+ acc n)))
check (== (loop2 100000 0) 5000050000) "TCO: tail call in if without return"
function even? (n) (if (== n 0) 1 (odd? (- n 1)))
function odd?  (n) (if (== n 0) 0 (even? (- n 1)))
check (== (even? 50001) 0) "TCO: mutual recursion"
function deep (n) (if (== n 0) 0 (+ 1 (deep (- n 1))))
check (== (deep 1000) 1000) "non-tail recursion to depth 1000"
check (contains? (error-of (function () (deep 1000000))) "stack overflow") "non-tail recursion too deep is a clean error"

# --- currying ------------------------------------------------------------
function add3 (a b c) (+ a b c)
check (== ((add3 1) 2 3) 6) "curry: one then two"
check (== (((add3 1) 2) 3) 6) "curry: one at a time"
check (== ((add3 1 2) 3) 6) "curry: two then one"
check (equal? (map (vec 1 2 3) (add3 10 100)) (vec 111 112 113)) "curry: partial passed to map"
function twice (f) (function (x) (f (f x)))
check (== (twice inc 5) 7) "over-application: result applied to remaining args"
check (== ((twice (twice inc)) 0) 4) "over-application: nested"
check (contains? (error-of (function () (square 1 2))) "too many arguments") "over-application on a non-function result is an error"
check (equal? (type (add3 1)) "function") "curry: partial is a function"

# --- errors --------------------------------------------------------------
var msg (error-of (function () (error "boom")))
check (contains? msg "boom") "error: message"
check (contains? msg "test_core.mu:") "error: carries file name"
function lvl3 () (error "deep")
function lvl2 () (lvl3)
function lvl1 () {
    var r (lvl2)
    return r
}
var trace (error-of (function () (lvl1)))
check (contains? trace "lvl1()") "error: call stack names the outer frame"
check (contains? trace "lvl3()") "error: call stack names the inner frame"
check (> (try (/ 1 0) catch e -1) 1e300) "try: no error returns body value"
check (equal? (try (error "x") catch e e) (try (error "x") catch e e)) "try: handler sees message"
check (== (try (head (list)) catch e 0) 0) "try: builtin error is caught"
function after-catch () {
    var r (try (error "inner") catch e "caught")
    var q (lvl1)
    return q
}
var t2 (error-of (function () (after-catch)))
check (contains? t2 "after-catch()") "error: call stack is consistent after a caught error"

# --- type and arity checks -----------------------------------------------
check (contains? (error-of (function () (+ 1 "a"))) "expected number") "+: type error"
check (contains? (error-of (function () (+ (vec 1 2) (vec 1 2 3)))) "size mismatch") "+: broadcast size error"
check (contains? (error-of (function () (sqrt))) "expected 1 argument") "sqrt: arity"
check (contains? (error-of (function () (range))) "range: expected") "range: arity"
check (contains? (error-of (function () (getidx (vec 1 2) 5))) "out of range") "getidx: range"
check (contains? (error-of (function () (getidx (vec 1 2) 1.5))) "integer") "getidx: non-integer index"
check (contains? (error-of (function () (head "s"))) "expected list or number") "head: type"
check (contains? (error-of (function () (upper 5))) "expected string") "upper: type"
check (contains? (error-of (function () (map "s" square))) "expected list or number") "map: type"
check (contains? (error-of (function () (map (list 1) 5))) "expected function") "map: fn type"
check (contains? (error-of (function () (if 1))) "if:") "if: malformed"
check (contains? (error-of (function () (undefined-thing 1))) "undefined: undefined-thing") "undefined symbol"
check (contains? (error-of (function () (5 1))) "not callable") "calling a number"
check (contains? (error-of (function () (mean (vec)))) "empty") "mean: empty vector"
check (contains? (error-of (function () (max (vec)))) "empty") "max: empty vector"
check (== (sum (vec)) 0) "sum: empty vector is 0"
check (== (prod (vec)) 1) "prod: empty vector is 1"
check (contains? (error-of (function () (zeros -1))) "negative") "zeros: negative size"
check (contains? (error-of (function () (assert 0 "custom" 42))) "custom 42") "assert: message"
check (contains? (error-of (function () (assert 0))) "assertion failed") "assert: default message"

# --- lists ---------------------------------------------------------------
var L (list 1 "two" 3)
check (== (length L) 3) "list: length"
check (== (head L) 1) "list: head"
check (equal? (tail L) (list "two" 3)) "list: tail"
check (== (last L) 3) "list: last"
check (equal? (cons 0 L) (list 0 1 "two" 3)) "cons"
check (equal? (append L 4 5) (list 1 "two" 3 4 5)) "append"
check (equal? (concat-list (list 1) (list 2 3) (list)) (list 1 2 3)) "concat-list"
check (equal? (reverse (list 1 2 3)) (list 3 2 1)) "reverse: list"
var M (list)
push M 1
push M 2
check (equal? M (list 1 2)) "push mutates"
check (== (pop M) 2) "pop returns last"
check (equal? M (list 1)) "pop mutates"
check (equal? (getidx L 1) "two") "getidx: list"
check (equal? (getidx L -1) 3) "getidx: negative index"
setidx L 0 100
check (== (head L) 100) "setidx: list"
check (equal? (slice (list 1 2 3 4 5) 1 3) (list 2 3 4)) "slice: list"
check (equal? (slice (list 1 2 3 4 5) 3) (list 4 5)) "slice: to end"
check (equal? (slice (list 1 2 3 4 5) -2) (list 4 5)) "slice: negative start"
check (== (find (list 1 "x" 3) "x") 1) "find: list"
check (== (find (list 1 2) 9) -1) "find: missing"
check (== (empty? (list)) 1) "empty?: list"
check (== (empty? L) 0) "empty?: nonempty list"
var shared L
setidx shared 0 -1
check (== (head L) -1) "lists are shared by reference"
var copied (copy L)
setidx copied 0 7
check (== (head L) -1) "copy makes an independent list"

# --- vectors -------------------------------------------------------------
var v (vec 1 2 3)
check (equal? (type v) "vec") "vec type"
check (equal? (type 1) "scalar") "scalar type"
check (equal? (+ v 10) (vec 11 12 13)) "broadcast: vec + scalar"
check (equal? (* v v) (vec 1 4 9)) "elementwise: vec * vec"
check (equal? (- v) (vec -1 -2 -3)) "unary minus on vec"
check (equal? (vec (list 4 5)) (vec 4 5)) "vec from list"
check (equal? (vec v 4 (vec 5 6)) (vec 1 2 3 4 5 6)) "vec concatenates"
check (equal? (range 4) (vec 0 1 2 3)) "range n"
check (equal? (range 2 5) (vec 2 3 4)) "range a b"
check (equal? (range 0 1 0.25) (vec 0 0.25 0.5 0.75)) "range a b step"
check (equal? (range 5 0 -2) (vec 5 3 1)) "range negative step"
check (== (length (range 0)) 0) "range 0 is empty"
check (equal? (linspace 0 1 5) (vec 0 0.25 0.5 0.75 1)) "linspace"
check (equal? (zeros 3) (vec 0 0 0)) "zeros"
check (equal? (ones 2) (vec 1 1)) "ones"
check (== (sum v) 6) "sum"
check (== (prod v) 6) "prod"
check (== (mean v) 2) "mean"
check (== (dot v v) 14) "dot"
check (equal? (sort (vec 3 1 2)) (vec 1 2 3)) "sort"
check (equal? (reverse v) (vec 3 2 1)) "reverse: vec"
check (== (head v) 1) "head: vec"
check (equal? (tail v) (vec 2 3)) "tail: vec"
check (== (last v) 3) "last: vec"
check (equal? (slice (range 10) 2 3) (vec 2 3 4)) "slice: vec"
check (== (find v 3) 2) "find: vec"
check (== (getidx v -1) 3) "getidx: vec negative"
setidx v 1 20
check (equal? v (vec 1 20 3)) "setidx: vec"
seed 42
var r1 (rand 5)
seed 42
var r2 (rand 5)
check (equal? r1 r2) "seed makes rand reproducible"
check (== (length r1) 5) "rand n"
check (and (>= (rand) 0) (< (rand) 1)) "rand in [0,1)"
check (equal? (map v (function (x) (* x 2))) (vec 2 40 6)) "map: vec"
check (equal? (filter (range 10) (function (x) (== (mod x 3) 0))) (vec 0 3 6 9)) "filter: vec"
check (== (reduce (range 5) (function (a b) (+ a b)) 0) 10) "reduce: vec"
check (contains? (error-of (function () (map (vec 1) (function (x) "s")))) "expected number") "map on vec requires numeric results"

# --- strings -------------------------------------------------------------
check (equal? (concat "a" "b" 42 (list 1)) "ab42(1)") "concat"
check (equal? (split "a,b,c" ",") (list "a" "b" "c")) "split"
check (equal? (split "abc" "") (list "a" "b" "c")) "split: chars"
check (equal? (join (list "x" "y") "-") "x-y") "join"
check (equal? (upper "musil") "MUSIL") "upper"
check (equal? (lower "MUSIL") "musil") "lower"
check (equal? (trim "  hi \n") "hi") "trim"
check (equal? (format "{} + {} = {}" 1 2 3) "1 + 2 = 3") "format"
check (contains? (error-of (function () (format "{} {}" 1))) "not enough") "format: arity"
check (== (length "hello") 5) "length: string"
check (equal? (getidx "hello" 1) "e") "getidx: string"
check (equal? (slice "hello" 1 3) "ell") "slice: string"
check (== (find "hello" "ll") 2) "find: string"
check (equal? (reverse "abc") "cba") "reverse: string"
check (equal? (chr 65) "A") "chr"
check (== (ord "A") 65) "ord"
check (== (num "3.5") 3.5) "num"
check (== (num 4) 4) "num: passthrough"
check (contains? (error-of (function () (num "x"))) "not a number") "num: bad"
check (equal? (str 'sym) "sym") "str: symbol"
check (equal? (sym "abc") 'abc) "sym"
check (== (empty? "") 1) "empty?: string"

# --- higher-order on lists ---------------------------------------------
check (equal? (map (list 1 2 3) square) (list 1 4 9)) "map: list"
check (equal? (filter (list 1 2 3 4) (function (x) (> x 2))) (list 3 4)) "filter: list"
check (equal? (reduce (list "a" "b") (function (acc x) (concat acc x)) "") "ab") "reduce: list"
var seen (list)
each (list 1 2) (function (x) (push seen x))
check (equal? seen (list 1 2)) "each"
check (equal? (map (list) square) (list)) "map: empty list"

# --- quote, eval, apply ----------------------------------------------------
var code '(+ 1 2)
check (equal? (type code) "list") "quote yields a list"
check (== (eval code) 3) "eval"
check (== (eval '(* 2 (+ 1 2))) 6) "eval: nested"
check (== (apply + (list 1 2 3)) 6) "apply: builtin"
check (== (apply add3 (list 1 2 3)) 6) "apply: user fn"
check (equal? (apply list (list 1 (list 2))) (list 1 (list 2))) "apply: arguments are not re-evaluated"
var built (list '+ 10 20)
check (== (eval built) 30) "eval: constructed code"
check (== (eval (list 'square 6)) 36) "eval: sees globals"
function uses-eval (x) (eval x)
check (== (uses-eval '(+ 1 1)) 2) "eval: in tail position of a function"

# --- I/O -----------------------------------------------------------------
var path "/tmp/musil_test_core.txt"
write path "hello\n"
append-file path "world\n"
check (equal? (read path) "hello\nworld\n") "write/append-file/read"
check (== (exists? path) 1) "exists?"
check (== (exists? "/tmp/definitely/not/here") 0) "exists?: missing"
check (contains? (error-of (function () (read "/tmp/definitely/not/here"))) "cannot open") "read: missing file"
check (equal? (trim (exec "echo ok")) "ok") "exec"
check (equal? (type args) "list") "args is a list"
check (equal? version "3.0.0") "version"

# --- meta ----------------------------------------------------------------
check (== (defined? 'square) 1) "defined?: yes"
check (== (defined? "nope") 0) "defined?: no"
check (>= (find (vars) 'square) 0) "vars lists globals"
check (> (clock) 0) "clock"
check (== (length (vars)) (length (vars))) "vars is stable"

print "test_core: all" checks "checks passed"
