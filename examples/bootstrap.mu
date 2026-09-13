# bootstrap: Musil evaluating Musil. Code is data, so an evaluator for the language is a
# function over lists: this file defines one (meval) with its own environments and closures,
# runs a few programs through it, and ends with a REPL in which every line you type is
# parsed by the host and evaluated by the evaluator written here.
#
# What the host provides: parse (text -> forms), the builtins (+, print, list ...), input.
# What this file provides: environments, lookup, var/set, if, do, quote, function, calls
# with closures, return, while. It is the reference kernel of the language, in the language,
# eighty lines long. Run: musil bootstrap.mu   (then type at the meta> prompt, or `exit`)
load "std.mu"

# --- environments: a list of (list name value) pairs with a parent -----------------------
function env-new (parent) (list (list) parent)
function env-lookup (env name) {
    if (equal? (type env) "nil") { error "meval: undefined: " name }
    var hit (filter (head env) (function (p) (equal? (head p) name)))
    if (> (length hit) 0) { return (last (head hit)) }
    return (env-lookup (last env) name)
}
function env-define (env name value) {
    var hit (filter (head env) (function (p) (equal? (head p) name)))
    if (> (length hit) 0) { setidx (head hit) 1 value } { push (head env) (list name value) }
    return value
}
function env-set (env name value) {
    if (equal? (type env) "nil") { error "meval: set: undefined: " name }
    var hit (filter (head env) (function (p) (equal? (head p) name)))
    if (> (length hit) 0) { setidx (head hit) 1 value
                            return value }
    return (env-set (last env) name value)
}

# --- the evaluator ---------------------------------------------------------------------
function meval (form env) {
    var t (type form)
    if (equal? t "symbol") { return (env-lookup env (str form)) }
    if (not (equal? t "list")) { return form }                     # numbers, strings, nil evaluate to themselves
    if (== (length form) 0) { return form }
    var op (head form)
    var opname (if (equal? (type op) "symbol") (str op) "")
    if (equal? opname "quote") { return (getidx form 1) }
    if (equal? opname "if") {
        if (truthy? (meval (getidx form 1) env)) { return (meval (getidx form 2) env) }
        if (> (length form) 3) { return (meval (getidx form 3) env) }
        return nil
    }
    if (equal? opname "do") { return (meval-seq (tail form) env) }
    if (equal? opname "var") { return (env-define env (str (getidx form 1)) (meval (getidx form 2) env)) }
    if (equal? opname "set") { return (env-set env (str (getidx form 1)) (meval (getidx form 2) env)) }
    if (equal? opname "function") {                                   # (function name (params) body) or (function (params) body)
        var named (equal? (type (getidx form 1)) "symbol")
        var params (map (getidx form (if named 2 1)) str)
        var body (getidx form (if named 3 2))
        var closure (list "closure" params body env)
        if named { env-define env (str (getidx form 1)) closure }
        return closure
    }
    if (equal? opname "return") { return (list "return" (if (> (length form) 1) (meval (getidx form 1) env) nil)) }   # a marker that unwinds to the function
    if (equal? opname "while") {
        var out nil
        while (and (equal? (type out) "nil") (truthy? (meval (getidx form 1) env))) {
            var r (meval (getidx form 2) env)
            if (returning? r) { set out r }
        }
        return out
    }
    # a call: evaluate the operator and the arguments, then apply
    var f (meval op env)
    var args (map (tail form) (function (a) (meval a env)))
    return (mapply f args)
}
function returning? (v) {
    if (not (equal? (type v) "list")) { return 0 }
    if (== (length v) 0) { return 0 }
    return (equal? (head v) "return")
}
function meval-seq (forms env) {                 # stops at a return marker and passes it up
    var result nil
    var k 0
    while (and (< k (length forms)) (not (returning? result))) {
        set result (meval (getidx forms k) env)
        set k (+ k 1)
    }
    return result
}
# a closure of ours handed to a host builtin (map, filter, reduce ...) is wrapped as a host function
function to-host (v) {
    if (not (and (equal? (type v) "list") (> (length v) 0))) { return v }
    if (not (equal? (head v) "closure")) { return v }
    var n (length (getidx v 1))
    if (== n 0) { return (function () (mapply v (list))) }
    if (== n 1) { return (function (a) (mapply v (list a))) }
    if (== n 2) { return (function (a b) (mapply v (list a b))) }
    return (function (a b c) (mapply v (list a b c)))
}
function mapply (f args) {
    if (equal? (type f) "function") { return (apply f (map args to-host)) }   # a host builtin: the host applies it, our closures wrapped
    if (and (equal? (type f) "list") (equal? (head f) "closure")) {    # one of ours: bind the parameters, evaluate the body
        var params (getidx f 1)
        if (!= (length params) (length args)) { error "meval: expected " (length params) " arguments, got " (length args) }
        var frame (env-new (getidx f 3))
        each (zip params args) (function (p) (env-define frame (head p) (last p)))
        var r (meval (getidx f 2) frame)
        return (if (returning? r) (last r) r)          # a return marker stops here, at the function's boundary
    }
    error "meval: not callable: " f
}
function truthy? (v) (not (or (equal? (type v) "nil") (equal? v 0) (equal? v "") (equal? v (list))))

# --- the global environment of the meta-level: the host's builtins, by name -------------------
var meta-global (env-new nil)
each (list "+" "-" "*" "/" "<" ">" "<=" ">=" "==" "!=" "equal?" "not" "list" "head" "tail" "length" "cons" "append" "push" "vec" "sum" "range" "map" "filter" "reduce" "print" "str" "concat" "sin" "cos" "sqrt" "abs" "mod" "type" "getidx")
    (function (name) (env-define meta-global name (eval (sym name))))
env-define meta-global "pi" pi

# --- programs run by the evaluator (each is parsed by the host, evaluated here) ---------------
function run (text) (meval (parse text) meta-global)
print "3 + 4 * 2       :" (run "(+ 3 (* 4 2))")
print "closures        :" (run "var k 10\nfunction add-k (x) (+ x k)\nadd-k 5")
print "recursion       :" (run "function fact (n) (if (< n 2) 1 (* n (fact (- n 1))))\nfact 10")
print "counter (set)   :" (run "function make-counter () { var c 0\n return (function () { set c (+ c 1) }) }\nvar ctr (make-counter)\nctr\nctr\n(ctr)")
print "higher order    :" (run "(map (list 1 2 3) (function (x) (* x x)))")
print "vectors         :" (run "(+ (range 4) 10)")
print "quote is data   :" (run "'(+ 1 2)")
print "while           :" (run "var i 0\nvar acc 0\nwhile (< i 5) { set acc (+ acc i)\n set i (+ i 1) }\n(+ acc 0)")
print "an error        :" (try (run "(nope 1)") catch e e)

# --- a REPL: the host reads and parses, the evaluator written above evaluates -----------------
# (only when someone is at the keyboard: not under the test suite, which sets MUSIL_NOSHOW, nor a pipe)
load "system.mu"
if (and (== (length args) 0) (interactive?) (equal? (type (getenv "MUSIL_NOSHOW")) "nil")) {
    print ""
    print "meta> is Musil evaluated by meval; type Musil, or exit"
    function done? (l) (if (equal? (type l) "nil") 1 (equal? (trim l) "exit"))   # (or evaluates both arguments)
    var line (input "meta> ")
    while (not (done? line)) {
        if (> (length (trim line)) 0) {
            var r (try (meval (parse line) meta-global) catch e (concat "error: " e))
            if (and (equal? (type r) "list") (> (length r) 0)) { if (equal? (head r) "closure") { set r "<closure>" } }   # a closure holds its environment, which holds the closure
            if (not (equal? (type r) "nil")) { print r }
        }
        set line (input "meta> ")
    }
}
