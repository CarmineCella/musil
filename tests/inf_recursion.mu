# This example stops with a clean "stack overflow" error instead of crashing.
# (Registered in the test suite as a test that is expected to fail.)

function rec (a) {
    if (== (mod a 1000) 0) { print a }
    var r (rec (+ a 1))     # not a tail call: r is used after the call
    return r
}
rec 0
