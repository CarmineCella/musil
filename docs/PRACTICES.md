# Musil — programming practices

How to write Musil that reads well and runs fast, and how to extend the language. The
language is a Scheme in its bones (everything is an expression, functions are values,
code is data) with a Tcl surface (one command per line, no parentheses around a top-level
call). The style that fits it best is **imperative at the top, functional in the small**:
programs are sequences of steps that build and transform vectors and lists; the functional
tools (`map`, `filter`, `reduce`) are used inside those steps where they are shorter than a
loop, never as an end in themselves.

## 1. The shape of a program

A program is a list of steps. Name the intermediate results; do not nest what can be
sequenced.

```
load "signals.mu"
var w (read-wav "data/drums.wav")
var sr (head w)
var x (head (getidx w 1))
var flux (onset-strength x 1024 256)
var t-on (onsets x sr 1024 256 0.25)
print (length t-on) "onsets:" (fixed t-on 3)
write-wav "/tmp/out.wav" sr (normalize-peak x)
```

Rules of thumb:

- `and` and `or` short-circuit, left to right, and return the deciding value, so
  `(and (> k 0) (getidx v k))` is safe and `(or x default)` reads as it should.
- One idea per line. A line with a nesting depth beyond three parentheses is asking to be
  split with a `var`.
- Top-level calls need no parentheses: `print x`, `write-wav path sr x`, `audio-init`. Use
  parentheses when the call is an argument: `(print (fixed x 2))` is wrong at the top level
  (it works, but is noise); `var y (fixed x 2)` is right.
- A function's body on the next line, indented: indentation continues a top-level command.
  Braces for several statements. No backslashes are needed.

  ```
  function rms-db (x)
      (db (rms x))

  function normalise-to (x target) {
      var g (/ target (rms x))
      return (* x g)
  }
  ```

- `return` ends a function early; the last expression is the value otherwise. Inside
  `{ }`, use `return` when the body has steps, so the reader sees where the value comes
  from.

## 2. Vectors first

Numbers are vectors. Anything that can be written as an operation on whole vectors is
faster (the loop runs in C++) and shorter than a loop in Musil.

```
var t (/ (range n) sr)                     # a time axis
var y (* (exp (* -3 t)) (sin (* tau 440 t)))   # a decaying sine, no loop
var peak (max (abs y))
var clipped (clip y -0.5 0.5)
```

Broadcasting: a scalar with a vector applies to every element; two vectors must have the
same length (a mismatch is an error, on purpose). `vec` concatenates; `slice`, `take`,
`drop` cut; `add-at!` adds into a buffer in place; `mix` builds a buffer from placed pieces.

Reach for a loop over samples only when each sample depends on the previous one and no
builtin does it (`osc`, `lag`, `iir`, `delay`, `comb`, `adsr` are already there). If you
find yourself writing such a loop, the operation probably belongs in C++ (section 8).

## 3. Loops and iteration

Three forms, in order of preference:

**`each`** for doing something with every element (side effects: printing, pushing,
writing files). This is the everyday loop.

```
each files (function (f) (print f (fixed (rms (head (getidx (read-wav f) 1))) 3)))
each (zip names sources) (function (p) (write-wav (concat "/tmp/" (head p) ".wav") sr (last p)))
each (range 8) (function (k) (push hits (list (* k 4000) burst)))
```

`each` over a vector gives numbers, over a list gives its elements; `(range n)` is the
counter loop. Inside the function, `set` reaches variables of the enclosing scope:

```
var total 0
each xs (function (x) (set total (+ total x)))     # fine for a running value; (sum xs) is better here
```

**`for`** when an index and a step are the point, or the loop must stop early:

```
for (var k 1) (< k (length mags)) (var k (+ k 1)) {
    push flux (spectral-flux (getidx mags k) (getidx mags (- k 1)))
}
```

**`while`** for conditions that are not a count: reading until a value, converging,
waiting.

```
while (< (nmf-error V W H) target) { ... }
while (controls-open?) { sleep 0.1 }
```

`break` and `continue` work in `for` and `while`.

**`map`, `filter`, `reduce`** when the result is a new collection: `map` turns a list into
another of the same length, `filter` keeps some elements, `reduce` folds into one value.
Use them when the function fits on the line; otherwise name the function first.

```
var levels (map sources rms)
var loud (filter events (function (e) (> (get e 'amp) 0.5)))
var total (reduce lengths + 0)
```

`map` over a *list* gives a list; over a *vector* it gives a vector when every result is
a number and a list otherwise, so `(map (range n) (function (k) (list k ...)))` builds a
list of records directly.

**Recursion** is for trees and for algorithms that are recursive by nature (a nested
pattern, an evaluator, a parser). The interpreter turns a call in tail position into a
jump, so a loop written as a tail-recursive function costs nothing, but it reads worse
than `for` to most people; prefer the loop unless the structure is genuinely recursive.
The evaluation depth is bounded (`--stack`), so a non-tail recursion over a long list will
overflow: use a loop or `reduce`.

**Building lists**: start with `(list)` and `push`, or `map`. Do not `concat-list` in a
loop over long lists (each call copies). For vectors, preallocate with `zeros` and write
with `add-at!` or `setidx`.

## 4. Records: options, events, rows

A record is a list of `(list key value)` pairs. It is the shape of every option list,
every event, every row of a table, so its helpers are used everywhere: `record`, `get`,
`opt` (with a default), `has?`, `put!` (in place) and `put` (a copy), `keys`, `values`.
Keys are symbols by convention (`'freq`); a string key is a different key
(`(equal? 'freq "freq")` is false), so pick one and keep it.

```
var note (record (list 'pitch 60 'dur 0.5 'amp 0.8))
put! note 'amp 0.6
var louder (filter notes (function (n) (> (get n 'amp) 0.5)))
var by-pitch (sort-by notes (function (n) (get n 'pitch)))
var by-bar (group-by notes (function (n) (floor (/ (get n 'onset) 4))))
```

## 5. Symbols, quoting, code as data

`'x` is the symbol `x`: a name as a value, used for keys, kinds and parameter names
(`(set-param a 'cutoff 300)`). Quote a whole form to keep it as data: `'(+ 1 2)` is a list
of three elements. `eval` runs a form, `parse` turns text into forms, `apply` calls a
function on a list of arguments. This is what makes patterns, scores and instruments
plain data you can build and transform; it is rarely needed in ordinary programs, and
never as a way to "generate code" when a function will do.

## 6. Files, paths and data

- Reading: `read-wav`, `read-csv`, `read` (text), `read-lines`. A relative path is looked
  for in the current directory, then next to the file being run, so `data/drums.wav`
  works from anywhere for an example living in `examples/`. `load` and `find-file`
  also search the load path (`MUSIL_PATH`, `~/.musil`, the libraries).
- Writing: `write-wav`, `write-csv`, `write`. Relative paths write into the current
  directory; the examples write into `/tmp/` on purpose, so that running them never
  litters the tree. Name outputs with a prefix (`musil_...`) so they are easy to find and
  delete.
- WAVs come back as `(list sr channels)`; a mono file is one channel: `(head (getidx w 1))`.
  A multichannel signal is a list of channel vectors, which `write-wav` and `play` take as
  they are.
- Big files: slice what you need early (`(take x (* 10 sr))`) and keep sample rates in a
  variable; never hard-code 44100 in a function, pass `sr`.
- Data the examples share lives in `examples/data/` and is committed; how each synthetic
  file was made is on record in `make_audio_demos.mu`. Data a *library* needs (the HRTFs)
  lives in `src/` and is installed with the `.mu` files.

## 7. Errors, tests, printing

- `error` raises with a message; `try ... catch e` catches it; `error-of` (in `std`) gives
  the message of a failing thunk, which is how tests check errors.
- Check arguments at the top of a function and fail early with a message that names the
  function: `if (<= n 0) { error "hann: n must be > 0" }`.
- `print` takes any number of arguments; `fixed` rounds for display; `str` converts. Print
  the *result* of a step, not its progress, unless the step is slow.
- A program should be silent when it succeeds and precise when it fails.

## 8. Adding a library

A library is four files plus registration. Take `signals` as the model.

1. **`src/name.h`**: the C++ half, for anything needing an element loop, per-sample state,
   the OS, or a callback. One `inline vptr sig_thing(vlist& a, Interp& i)` per builtin,
   arguments checked with `i.num`, `i.scalar`, `i.str`, `i.list`, `i.index`, errors with
   `i.bad("thing: what was wrong")`; then `inline void add_name(Interp& i)` registering
   them with `i.def("thing", sig_thing, min_args, max_args)`. Everything is `inline` in
   `namespace musil` (header-only).
2. **`src/name.mu`**: the Musil half, for anything expressible as vector operations. It
   starts with `load "std.mu"` (and whatever it builds on). Loading is explicit: a program
   does `load "name.mu"`.
3. **`tests/test_name.mu`**: self-checking. `load "test.mu"`, then `check` lines with a
   message each, then `report "test_name"`. Cover the normal case, an edge (empty, one
   element), and the errors. Register it in `CMakeLists.txt` (`add_test(NAME name ...)`).
4. **`examples/reference_name.mu`**: a tour that prints every function once, in the order
   a reader would want to meet them; its output is a golden file in `tests/golden/`,
   refreshed with the `golden.cmake` script (`-DUPDATE=1`) whenever the printed text
   changes on purpose.

Then: `#include "name.h"` and `add_name(i)` in `src/musil.h`; add the library to `LIBS` in
`tools/gendoc.py` and `\input{generated/name.tex}` plus a paragraph in the manual.

Rules that keep the libraries coherent:

- **Doc comments are the documentation.** `// (thing args) what it does` above a builtin in
  the `.h`, `# (thing args) what it does` above a function in the `.mu`; several forms on
  one line share the text; indented continuation lines extend it. `help`, the manual and
  the VS Code grammar are generated from these; a function without one is a bug.
- **No optional arguments.** Currying makes a missing argument a partial application, so
  a function has one arity; variants get names (`onsets` / `onsets-adaptive`).
- **Names** are lowercase with hyphens; predicates end in `?`; in-place mutation ends in
  `!` (`add-at!`, `put!`); a function returning a new value has no mark.
- **Element loops in C++, vector algebra in Musil.** A Musil function that loops over
  samples is a sign the loop belongs in the `.h`.
- **Offline and streaming are one definition.** A signal function that should also run in
  a synth takes a state struct in C++ (`osc_run`, `iir_run` ...), is listed in the ugen
  table in `live.h`, and must satisfy `synth-render` equals the direct call (a test in
  `test_live.mu`). Constants inside instruments are signals (`(sig gate 190)`), so they
  work in both executions.
- **Environment variables the tests rely on**: `MUSIL_NOSHOW` (no windows),
  `MUSIL_NULL_AUDIO` (the silent device). A library that opens windows or devices must
  honour them.

## 9. Adding a builtin to an existing library

Same rules, smaller: the function in the `.h` or `.mu`, its doc comment, a `check` in the
test, a line in the reference, a regenerated golden. Run `./build.sh --test` before
sending; `gendoc` runs at every configure, so `help` and the grammar follow.

## 10. Style at a glance

```
# a comment says why, the code says what
load "live.mu"
audio-init
var sr (audio-sr)

function pluck-note (gate freq)                      # instrument: a function of its parameters
    (* (adsr sr gate 0.005 0.1 0.3 0.2) (osc sr (sig gate freq) saw-table))

var p (synth pluck-note)                             # streamed
var buffer (pluck-note (vec (ones 400) (zeros 8000)) 220)   # or rendered: the same function

function bassline (cycle)                            # a pattern: events from data
    (melody p "a1 ~ a1 c2" 4 (list))
live-loop 'bassline 4
```

Prefer this to cleverness: named steps, vectors over loops, `each` over recursion,
records for structured data, one arity per function, a doc line on every definition.
