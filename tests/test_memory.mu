; test_memory.mu — what a program makes is freed when nothing uses it any more.
;
; (live-objects) counts the values and environments alive in the process. Each part runs some work,
; drops it, and checks that the counts come back to where they were (a small margin for the
; bookkeeping of the check itself).
; Run with: musil tests/test_memory.mu

load "test.mu"

function envs () (getidx (live-objects) 1)
function vals () (getidx (live-objects) 0)

; --- local functions do not keep their scope alive -----------------------------------
function work (n) {
    function down (x) (if (> x 0) (down (- x 1)) 0)    ; a local recursive function
    var twice (function (x) (* 2 x))                     ; a lambda held by a local variable
    down n
    twice n
}
work 5
var e0 (envs)
for (var k 0) (< k 2000) (var k (+ k 1)) (work 10)
check (< (- (envs) e0) 20) "local functions: their environments are freed"

; a local function returned from its scope still works after the scope is gone
function make-adder (n) {
    function add (x) (+ x n)
    return add
}
var add5 (make-adder 5)
check (== (add5 1) 6) "a returned local function keeps what it needs"
var e1 (envs)
for (var k 0) (< k 1000) (var k (+ k 1)) ((make-adder k) 1)
check (< (- (envs) e1) 20) "returned local functions are freed with their scope"

; a closure that changes a variable of its scope sees and keeps the change
function counter () {
    var n 0
    function bump () (set n (+ n 1))
    bump
    bump
    return n
}
check (== (counter) 2) "a local function assigns to its scope"

; currying and lambdas passed to map leave nothing behind
var v0 (vals)
for (var k 0) (< k 500) (var k (+ k 1)) {
    var plus (function (a b) (+ a b))
    var inc (plus 1)
    map (list 1 2 3) inc
}
check (< (- (vals) v0) 200) "partial applications and lambdas are freed"

; --- deep lists free without recursion --------------------------------------------------
var v1 (vals)
var deep (list)
for (var k 0) (< k 1000000) (var k (+ k 1)) (set deep (list deep))
check (> (vals) (+ v1 1000000)) "a million nested lists exist"
set deep nil
check (< (- (vals) v1) 200) "a million nested lists are freed"

; --- a list cannot contain itself ----------------------------------------------------------
var L (list 1 2)
check (fails? (function () (push L L))) "push of a list into itself"
var M (list 1 (list 2))
check (fails? (function () (push (getidx M 1) M))) "push of a list into its own element"
check (fails? (function () (setidx M 0 M))) "setidx of a list into itself"
var r (list (list 'a 1))
check (fails? (function () (put! r 'b r))) "put! of a record into itself"
var shared (list 1)
var holder (list shared shared)
push holder (list shared)
check (== (length holder) 3) "the same list twice is allowed"

check (== (length (live-objects)) 2) "live-objects gives values and environments"

report "test_memory"
