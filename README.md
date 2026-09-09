# Musil

A small scripting language for sound and music computing.

Scheme underneath, Tcl on the surface: each line is a command, `( )` is a list,
`{ }` is a block, and `(expr ...)` gives you infix arithmetic when you want it.
The whole language is one C++17 header with no dependencies.

```
# musil
function fact (n) {
    if (< n 2) { return 1 }
    return (* n (fact (- n 1)))
}
print "10! =" (fact 10)

var v (range 8)
print "v * v + 1 =" (expr (v * v + 1))
print "squares of evens:" (map (filter v (function (x) (== (mod x 2) 0))) (function (x) (* x x)))
```

## Build

```sh
git clone https://github.com/<you>/Musil.git
cd Musil
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/musil examples/reference.mu     # tour of every core feature
./build/musil                           # REPL
ctest --test-dir build                  # run the tests
```

GNU readline is used in the REPL when found (`-DMUSIL_READLINE=OFF` to disable).

## Layout

```
src/core.h         the language: reader, evaluator, core builtins (zero dependencies)
src/musil.h        umbrella header: core + bundled libraries
cli/main.cpp       the command-line interpreter
examples/          reference.mu and other example programs
tests/             test_core.mu (systematic, self-checking), stress and smoke tests,
                   golden output of reference.mu
```

Libraries live next to the core as headers (`src/system.h`, `src/signals.h`, ...),
each exposing an `add_<name>(interp&)` that registers its functions with
`interp::def(name, fn)`. The core never includes them; `musil.h` does.

## The language in one page

```
var x 10                       # define or assign (assigns an existing binding if there is one)
print "x =" x                  # a line is a command; its words are the arguments
var y (+ x 1)                  # (...) is a sub-expression
var z (expr (x * 2 + y))       # infix inside expr; ( ) groups, calls are allowed: (expr ((sqrt x) + 1))

if (< x y) { print "less" } { print "not less" }
while (< x 20) { var x (+ x 1) }
for (var i 0) (< i 3) (var i (+ i 1)) { print i }      # break / continue inside blocks: { break }

function sq (n) (* n n)        # named function, one-expression body
function fact (n) {            # block body; return exits early
    if (< n 2) { return 1 }
    return (* n (fact (- n 1)))
}
var inc (function (n) (+ n 1)) # anonymous function
function add3 (a b c) (+ a b c)
var add1 (add3 1)              # too few args: partial application
(add1 2 3)                     # => 6
(add3 1 2 3 4)                 # too many args: the result is applied to the rest (error here: 6 is not callable)

var v (vec 1 2 3)              # numbers are vectors; scalars are vectors of size 1
(+ v 10)                       # broadcasting: (11 12 13)
(sin v)                        # elementwise
(getidx v -1)                  # indices count from the end when negative
(slice (range 10) 2 3)         # (2 3 4)

var L (list 1 "two" 'three)    # lists hold anything; 'x quotes a symbol or form
(map L str) (filter v (function (n) (> n 1))) (reduce v + 0)
(head L) (tail L) (last L) (length L) (push L 4) (pop L)

var code '(+ 1 2)              # code is data
(eval code)                    # => 3
(apply + (list 1 2 3))         # => 6

try (error "boom") catch e (print "caught:" e)
assert (== (+ 2 2) 4) "arithmetic works"
```

Errors carry the file, the line and the call stack:

```
reference.mu:424: from inner
  1> inner() at reference.mu:426
  2> outer() at reference.mu:429
```

Tail calls run in constant space, so `loop`-style recursion is the normal way to iterate.

## Embedding

```cpp
#include "core.h"
musil::interp I;
I.def("hello", [](musil::vlist& a, musil::interp& i) -> musil::vptr {
    i.argc(a, 1, "hello");
    return musil::v_str("hello, " + i.str(a[0], "hello"));
});
I.run("print (hello \"world\")");
```

`interp::argc`, `num`, `scalar`, `index`, `str`, `list`, `fn` check an argument
and raise a Musil error with file and line if it is wrong. `interp::call_fn`
calls a Musil function from C++. `interp::yield_fn` is called every 1024
evaluations, for hosts that need to service an event loop.

## Tests

- `tests/test_core.mu` — one `assert` per language feature; prints one line on success.
- `tests/test_stress.mu`, `tests/test_smoke.mu` — deep recursion, currying, I/O, broadcasting.
- `tests/golden/reference.out` — frozen output of `examples/reference.mu`. After an
  intended change to the reference, refresh it with
  `cmake -DMUSIL=build/musil -DINPUT=examples/reference.mu -DGOLDEN=tests/golden/reference.out -DUPDATE=1 -P tests/golden.cmake`.

## Roadmap

1. Harden the core, run everything through `reference.mu` and `test_core.mu`. **Done.**
2. Port `system`, `signals` (offline DSP on vectors) and `scientific` from Musil 1.
3. Choose the multimedia backend; add plotting and real-time sound.
4. Live coding.

## History

This repository continues the core of "Musil 3". The earlier attempts at the
same idea (`f8`, `musil 1` with its FLTK IDE and full library set, `pure0`,
`flux`) are archived in the `musil_legacy` repository.

## License

Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
