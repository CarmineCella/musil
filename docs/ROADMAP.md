# Musil — project notes for working sessions

Musil is a small scripting language for sound and music computing: Scheme underneath, Tcl on the
surface. Numbers are vectors, functions are values, code is data. C++17, one core header, libraries as
pairs of a C++ header and a Musil file, an FLTK IDE, a command-line interpreter. Version in
`src/core.h` (`MUSIL_VERSION`). These notes are what a new session (or a reviewer) needs to work on it
without re-deriving the design.

## Layout

```
src/core.h            the language: reader, evaluator (tail calls), value model, builtins, REPL
src/std.h  std.mu     vectors, statistics, sequences, strings, files, map/filter/reduce
src/system.h  .mu     processes, directories, CSV, WAV, UDP; sleep
src/scientific.h .mu  matrices (a list of row vectors), decompositions, PCA, k-means, KNN
src/signals.h  .mu    generators, FFT/STFT, phase vocoder (pvoc), descriptors, filters, reverb
src/live.h  live.mu   audio device (miniaudio), voices, synths (functions compiled to graphs),
                      scheduler (loops in beats), controls, OSC; src/live/miniaudio.h vendored
src/serve.h           the evaluation port (TCP): editors send code, the session evaluates it
src/plot.h  plot.mu   figures (FLTK windows, PNG export), subplots; src/controls.h the controls window
src/musil.h           umbrella: make_env registers every library
cli/main.cpp          musil [-i] [-e code] [--serve PORT] [--send PORT [file]] files...
ide/main.cpp          the IDE (FLTK): editor, console, variables, help, plots/controls in windows
tests/                one test_<lib>.mu per library (self-checking), goldens of the references
examples/             reference_<lib>.mu tours + programs; examples/data/ recordings and iris
tools/gendoc.py       help.txt, docs/generated/*.tex and the VS Code grammar, from doc comments
docs/musil_manual.tex the manual (pdflatex); editors/vscode/musil the VS Code extension
```

## Rules that every change follows

- **A library is a set**: `name.h` (C++ half) + `name.mu` (Musil half) + `tests/test_name.mu` +
  `examples/reference_name.mu` (+ its golden in `tests/golden/`). Register the C++ half in
  `src/musil.h`; add the library to `tools/gendoc.py` (LIBS) and to the manual (`\input{generated/name.tex}`).
- **C++ or Musil**: C++ for anything needing an element loop, state per sample, the OS, or a
  callback; Musil for anything expressible as vector operations (same speed). A per-frame pipeline
  of many vector ops (the phase vocoder) is C++ because interpretive overhead dominates.
- **Doc comments are the documentation**: `// (name args) description` above a builtin in a `.h`,
  `# (name args) description` above a function in a `.mu`; several forms on one line share the text;
  indented continuation lines extend it. `help`, the manual's references and the VS Code grammar are
  generated from them (every configure runs gendoc). A function without such a comment is a bug.
- **No optional arguments** (currying makes a missing argument a partial application): two
  functions instead (`dist`/`dist-p`, `mat-print`/`mat-print-with`). A `function` definition must be
  one line, or continued with `\`, or use `{ }`.
- **var/set**: `var` defines in the current environment, `set` assigns to the nearest existing one;
  library closures update captured state with `set`.
- **Streamable ugens**: a stateful loop in `signals.h` takes a state struct (`osc_run`, `iir_run`,
  `delay_run`, `comb_run`, `allpass_run`, `adsr_run`, `lag_run`); the builtin runs it with fresh state,
  the node in `live.h` keeps the state between blocks. `ugen_table()` in `live.h` is the list of what
  a synth function may use; `synth-render` must equal the direct call to float precision (tested).
- **Threads**: the interpreter is single-threaded. Background work (loops, OSC, the port, FLTK's
  `Fl::check`) runs from `Interp::idle()`, called between REPL lines, inside `sleep`, from the yield
  hook and by the IDE's worker while it waits. The audio callback never runs Musil: commands go
  through a lock-free queue with sample times. The IDE's interpreter is a worker thread; windows are
  created on the FLTK thread through `Fl::awake` (`plot_needs_awake()`).
- **Tests run headless**: `MUSIL_NOSHOW=1` makes `show`/`controls` no-ops, `MUSIL_NULL_AUDIO=1` uses
  the silent audio backend; ctest sets both and `MUSIL_PATH=src`. Golden files are refreshed with
  `cmake -DMUSIL=... -DINPUT=... -DGOLDEN=... -DUPDATE=1 -P tests/golden.cmake`.
- **Paths**: reading functions look for a relative file in the current directory, then next to the
  file being run, so examples find `data/` from anywhere; writes stay in the current directory.
- **Errors** carry file, line and the call stack; builtins check arity and types with `i.bad(...)`.

## Commands

```
./build.sh --test           configure (regenerates help.txt), build, ctest      ./build.sh --docs  the PDF
./build.sh --ide            open the IDE                                         ./clean.sh         fresh clone
./deploy_macos.sh --clean   dist/Musil.app, musil, musil-ide, lib, examples, manual, .vsix, zip
./deploy_linux.sh --clean   dist/musil-<v>-linux/ + tar.gz
cmake --build build --target uninstall
```

## Design decisions worth knowing

- The IDE replaced a raylib "Listener": raylib allows one window per process and has no text widget.
  FLTK gives several plot windows at once, non-modal `show` from the CLI (the idle hook keeps
  windows alive), a real editor. The evaluation port (`serve`, `--serve`, `--send`) is code over TCP
  for external editors; OSC (`osc-listen`, `osc-map`, `osc-encode/decode`) is numbers over UDP for
  controls; they are unrelated.
- The phase vocoder follows sparkle: fixed synthesis hop, analysis hop = hop/stretch, zero-phase
  windowing, peak-locked phases, spectral resampling for pitch/formants, one-pass cepstral envelope
  (order about sr/100).
- `conv` is one big FFT offline (faster than partitioned) and partitioned when streaming.
- A control is a named value with a range bound to synth parameters; controls die with the device.

## Roadmap (next: music)

1. **Score and rendering**: a score is a typed event list (onset, duration, payload, position);
   payloads are buffers (eager: `(event score t dur (pvoc-stretch x 2))` stores the buffer), note
   records `(note 'Fl 'A5 'pp 'ord)` resolved against a database at render/play time, synth events
   (a function and its parameters). `render` = resolve + `mix` (any channel layout, positions encoded
   at the edge: stereo, N speakers, ambisonics); `play-score` schedules through `live`; save/load as
   text; transformations (transpose, time-scale, select, merge, quantise). Generators are functions
   returning events (`orchestrate`, the granulator); no quoting anywhere.
2. **Dataset** (SOL and the like): scan, parse names, table of records, descriptors from `signals`
   cached as CSV, `db-query`; notes resolve to samples.
3. **Roll and export**: a `segments` layer in `plot`, `score-plot` (time × pitch, instrument colours,
   dynamics), MIDI and MusicXML export.
4. **Generators**: random score generator, orchestral granulator.
5. **C++ ports**, each as a C++ core + Musil surface returning events: sound types, Maple (matching
   pursuit), Orchidea (assisted orchestration).
6. Later: `visual`, a separate raylib process driven by OSC (never a raylib window inside FLTK);
   the `in` ugen (device input); `grains`; a mini-notation parser for patterns.
