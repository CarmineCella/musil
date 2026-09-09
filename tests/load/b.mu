# b.mu — loaded by a.mu; loads a.mu again (no-op: already loaded) and a file in a subdirectory
var loads (append loads "b")
load "a.mu"
load "sub/c.mu"
function from-b () (concat "b+" (from-c))
