# test_load.mu — load resolution: relative to the loading file, once per file, cycles safe.
var loads (list)
load "load/a.mu"
assert (equal? loads (list "a" "b" "c")) "each file runs exactly once, in load order:" loads
assert (equal? (from-a) "a") "definitions from a.mu are visible"
assert (equal? (from-b) "b+c") "definitions from b.mu and sub/c.mu are visible"
load "load/a.mu"
load "load/sub/../a.mu"
assert (== (length loads) 3) "reloading by another spelling of the same path is a no-op"
assert (>= (find (try (load "load/nope.mu") catch e e) "not found") 0) "missing file is a clean error"
var err (try (load "load/bad.mu") catch e e)
assert (>= (find err "bad.mu:2") 0) "errors inside a loaded file name that file and line:" err
print "test_load: ok"
