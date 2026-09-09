# test.mu — the test harness shared by every test_*.mu file.
#
# A test file starts with (load "test.mu") and ends with (report "name").
# Each (check cond msg) is an assertion that counts; a failing check stops the
# run with its message, file and line. A passing file prints one line.
#
# This file is for the test suite only: it is not installed. What users need
# for their own scripts (assert, assert-equal, assert-near) lives in std.

load "std.mu"

var checks 0
# (check cond msg)     assert and count
function check (cond msg) {
    assert cond msg
    set checks (+ checks 1)
}
# (fails? thunk)       1 if calling thunk raises an error, else 0
function fails? (thunk) {
    try {
        thunk
        0
    } catch e 1
}
# (error-of thunk)     the error message text, or "" if none
function error-of (thunk) {
    try {
        thunk
        ""
    } catch e e
}
# (report name)        the one line a passing file prints
function report (name) (print (concat name ":") checks "checks passed")
