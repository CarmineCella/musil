# a.mu — loaded by test_load.mu; loads b.mu, which loads a.mu back (cycle)
var loads (append loads "a")
load "b.mu"
function from-a () "a"
