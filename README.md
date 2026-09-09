# Musil

![Musil logo](docs/musil_logo.png)

**Musil** is a tiny and expressive language designed to be easy to use, easy to expand and easy to embed in host applications.

The core of the language is made of a single [C++ header](src/core.h) and a more or less comprehensive overview of the language can be found [here](examples/reference.mu).

## Lineage

*Musil* sits within a family of small, expressive languages, combining influences from both scripting and functional traditions.

It is strongly inspired by TCL, Lisp and Scheme. From these traditions, **Musil** inherits:

- first-class procedures
- dynamic evaluation mechanisms such as `eval` and `apply`  
- a minimal, compositional core  

These influences shape **Musil** as a language where programs can be constructed, transformed, and executed as data — enabling flexible and expressive workflows.

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
# Why the name *Musil*?

The language is named after **Robert Musil**, the Austrian engineer-philosopher-novelist who believed that authentic creativity emerges from the interplay of **rational precision** and **subjective intuition**.

In *The Man Without Qualities*, Musil describes the human condition as a fusion of:

> *“Präzision und Seele”*  
> *precision and soul*

This duality mirrors the purpose of the Musil language:

- a core of **exact signal operations**,  
- expressed in a syntax designed for **open exploration**,  
- allowing composition as a form of thought.

To call this language **Musil** is to acknowledge an intellectual lineage where  
mathematics, sound, and imagination are not separate disciplines but different faces of the same creative activity.

## Build

```sh
git clone https://github.com/CarmineCella/musil.git
cd musil
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/musil examples/reference.mu     # tour of every core feature
./build/musil                           # REPL
ctest --test-dir build                  # run the tests
```

GNU readline is used in the REPL when found (`-DMUSIL_READLINE=OFF` to disable).

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
var M (copy L)                 # copy is shallow: a new list, same elements (nested lists/vectors stay shared)

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
#include "musil.h"     
musil::interp I;
// name, function, min args, max args (-1 = unbounded); eval checks the count
I.def("hello", [](musil::vlist& a, musil::interp& i) -> musil::vptr {
    return musil::v_str("hello, " + i.str(a[0]));
}, 1, 1);
I.run("print (hello \"world\")");
```

Inside a builtin, `i.num(v)`, `i.scalar(v)`, `i.index(v)`, `i.str(v)`,
`i.list(v)` and `i.fn(v)` return the payload or raise a Musil error that names
the builtin, with file and line (`hello: expected string, got number`);
`i.bad("message")` raises one with a custom text. `interp::call_fn` calls a
Musil function from C++. `interp::yield_fn` is called every 1024 evaluations,
for hosts that need to service an event loop.

For an example on how to integrate the language in your application, please check the [command line interpreter](cli/main.cpp).

# License

The **Musil** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2026 by Carmine-Emanuele Cella