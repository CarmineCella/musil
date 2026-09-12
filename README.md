![Musil logo](docs/musil_logo.png)

**Musil** is a tiny and expressive language designed to be easy to use, easy to expand and easy to embed in host applications.

It comes as a command-line interpreter (`musil`) and as **Musil**, the IDE: an editor with syntax colouring where Cmd-Enter runs the block under the cursor, a console, your variables, a documentation search, and plots and controls in windows of their own.

The core of the language is made of a single [C++ header](src/core.h) and a more or less comprehensive overview of the language can be found [here](examples/reference.mu).

## Lineage

*Musil* sits within a family of small, expressive languages, combining influences from both scripting and functional traditions.

It is strongly inspired by TCL, Lisp and Scheme. From these traditions, **Musil** inherits:

- first-class procedures
- dynamic evaluation mechanisms such as `eval` and `apply`  
- homoiconicity 

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

## Build and install

```sh
git clone https://github.com/CarmineCella/musil.git
cd musil
./build.sh --test                       # configure, build everything into build/, run the tests
./build/musil examples/reference.mu     # tour of the core language
./build/musil-ide                       # the IDE
sudo cmake --install build              # musil and musil-ide -> /usr/local/bin, headers -> /usr/local/include/musil,
                                        # the .mu libraries, help.txt and the font -> ~/.musil
cmake --build build --target uninstall         # removes exactly that
```

`build.sh` is a thin wrapper around CMake; the equivalent commands are

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release        # configure (regenerates src/help.txt from the source comments)
cmake --build build -j                           # build musil and musil-ide
ctest --test-dir build                           # tests: one per library, goldens, examples
cmake --build build --target musil-docs          # help.txt, docs/generated/*.tex and docs/musil_manual.pdf (pdflatex)
cmake --install build --prefix ~/.local          # install without sudo
```

Configure options:

| option | default | effect |
|---|---|---|
| `-DCMAKE_BUILD_TYPE=Release\|Debug` | Release | optimisation vs symbols |
| `-DMUSIL_FLTK=ON\|OFF` | ON | fetch FLTK 1.4 and build the plot library, the controls window and the IDE; OFF builds the language and the other libraries with no dependency at all |
| `-DMUSIL_READLINE=ON\|OFF` | ON | use GNU readline in the REPL when found |
| `-DMUSIL_TESTS=ON\|OFF` | ON | register the tests with ctest |
| `-DCMAKE_INSTALL_PREFIX=dir` | /usr/local | where `cmake --install` puts `bin/musil` and `include/musil`; the `.mu` files always go to `~/.musil` |

Scripts: `./build.sh` (`--debug`, `--clean`, `--test`, `--run FILE.mu`, `--ide`, `--docs`,
`--no-fltk`), `./clean.sh` (back to a fresh clone), `./deploy_macos.sh` (universal
`dist/Musil.app`, `dist/musil`, `dist/musil-ide`, `dist/lib`, the manual, a zip),
`./deploy_linux.sh` (a folder with the same plus `run.sh`, and a tar.gz).

A complete release, from a clean tree (this is what the release assets are made of):

```sh
./clean.sh                    # fresh-clone state: no build directories, no generated documentation
./build.sh --test             # configure (regenerates help.txt and the VS Code grammar), build, run every test
./build.sh --docs             # the manual: docs/musil_manual.pdf (needs pdflatex)
./deploy_macos.sh --clean     # macOS: dist/Musil.app, dist/musil, dist/musil-ide, dist/lib, dist/musil_manual.pdf,
                              #        dist/musil-<version>.vsix (when npx is present), dist/Musil-<version>-macos.zip
./deploy_linux.sh --clean     # Linux: dist/musil-<version>-linux/ and its tar.gz, on a Linux machine
git tag -a v<version> -m "Musil <version>" && git push --tags   # then attach the zip, the tarball and the PDF to the GitHub release
```

Nothing is loaded automatically: a program that wants the Musil halves of the
libraries says `load "std.mu"` (or `load "system.mu"`, which loads std.mu itself),
and `load` finds them in `~/.musil`, in `MUSIL_PATH`, or next to the file doing the
loading. During development, `MUSIL_PATH=src` points at the source tree; the test
suite sets it for you.

## Libraries

The language comes with libraries, each a pair of a C++ header for what needs speed
or the operating system and a `.mu` file for what is better written in Musil itself:

- **std**: vectors and statistics, sequences, strings, files, `map`/`filter`/`reduce`;
- **system**: processes, directories, CSV and WAV files, UDP and OSC;
- **scientific**: matrices, decompositions, regression, PCA, k-means, KNN;
- **signals**: generators, FFT and STFT, spectral envelopes, a phase vocoder,
  descriptors, filters and reverb;
- **live**: real-time sound: an audio device, a sample-accurate clock, voices that play
  buffers and files scheduled ahead of time, and synths: an instrument is an ordinary
  function of its parameters (oscillators, envelopes, filters, delays, convolution, pan),
  called for a buffer or compiled into the audio thread with hot parameters; loops in beats
  that re-read their pattern function every cycle (redefine it: the music changes), controls
  with sliders and OSC, and an evaluation port editors send code to (a VS Code extension is
  in `editors/vscode/musil`; `musil --send PORT file.mu` sends from a shell);
- **plot**: figures of lines, points, bars, images and 3D surfaces, grids of subplots, shown in a window or saved as PNG.

Every library has a test and a runnable reference (`examples/reference_<name>.mu`);
`examples/` also holds short programs on sound and data, from pitch-class sets to
cross synthesis.

## Documentation

`(help name)` prints the signature and description of any builtin or library function.
There is also a [user manual](docs/musil_manual.pdf). Both come from the same place: the comment
above each function in `src/` (`// (name args) description` in a `.h`, `# (name args)
description` in a `.mu`). Every configure regenerates `src/help.txt` and
`docs/generated/*.tex` from those comments, so `help` is never stale; the PDF is built
on request with `cmake --build build --target musil-docs` (or `./build.sh --docs`),
which needs `pdflatex`. Documenting a function is writing its comment.

## The language in one page

```
var x 10                       # define in the current environment (a function's var is always local)
set x 11                       # assign to an existing variable, however far out; error if none
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

load "std.mu"                  # the Musil half of the standard library, from ~/.musil or MUSIL_PATH
(take (range 10) 3) (fmt-fixed pi 4) (lerp 0 10 0.5) (stdev (vec 1 2 3 4))

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
musil::Interp I;
musil::make_env(I);      // C++ halves of std and system; omit for the bare language
// name, function, min args, max args (-1 = unbounded); eval checks the count
I.def("hello", [](musil::vlist& a, musil::Interp& i) -> musil::vptr {
    return musil::v_str("hello, " + i.str(a[0]));
}, 1, 1);
I.run("print (hello \"world\")");
```

Inside a builtin, `i.num(v)`, `i.scalar(v)`, `i.index(v)`, `i.str(v)`,
`i.list(v)` and `i.fn(v)` return the payload or raise a Musil error that names
the builtin, with file and line (`hello: expected string, got number`);
`i.bad("message")` raises one with a custom text. `Interp::call_fn` calls a
Musil function from C++. `Interp::yield_fn` is called every 1024 evaluations,
for hosts that need to service an event loop.

For an example on how to integrate the language in your application, please check the [command line interpreter](cli/main.cpp).

# License

The **Musil** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2026 by Carmine-Emanuele Cella
