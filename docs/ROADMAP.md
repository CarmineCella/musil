# Musil — project notes and roadmap

Musil is a small scripting language for sound and music computing: Scheme underneath, Tcl on the
surface. Numbers are vectors, functions are values, code is data. C++17, one core header, libraries as
pairs of a C++ header and a Musil file, an FLTK IDE, a command-line interpreter. Version in
`src/core.h` (`MUSIL_VERSION`). These notes are what a new session (or a reviewer) needs to work on it
without re-deriving the design.

## Layout

```
src/core.h            the language: reader, evaluator (tail calls), value model, builtins, REPL
src/std.h  std.mu     vectors, statistics, sequences, strings, files, map/filter/reduce
src/system.h  .mu     processes, directories, CSV, WAV, UDP; sleep; the evaluation port (serve)
src/scientific.h .mu  matrices (a list of row vectors), decompositions, PCA, NMF, k-means, KNN
src/signals.h  .mu    generators, FFT/STFT, phase vocoder (pvoc), descriptors, filters, reverb
src/live.h  live.mu   audio device (miniaudio), voices, synths (functions compiled to graphs),
                      scheduler (loops in beats), controls, OSC; src/live/miniaudio.h vendored
src/music.h music.mu  the score (events: files, buffers, notes, synths, calls, scores), render/play/display,
                      SOL-like databases (db-load reads the feature file; sounds opened on demand), the
                      elements, orchestrations; src/music/midi.h reads MIDI files
src/plot.h  plot.mu   figures (FLTK windows, PNG export), subplots; src/controls.h the controls window
src/musil.h           umbrella: make_env registers every library
cli/main.cpp          musil [-i] [-e code] [--serve PORT] [--send PORT [file]] files...
ide/main.cpp          the IDE (FLTK): editor, console, variables, help, plots/controls in windows
tests/                one test_<lib>.mu per library (self-checking), goldens of the references
examples/             reference_<lib>.mu tours + programs; examples/data/ recordings and iris
tools/gendoc.py       help.txt, docs/generated/*.tex and the VS Code grammar, from doc comments
docs/musil_manual.tex the manual (pdflatex), with the programming practices and how to extend Musil;
                      editors/vscode/musil the VS Code extension
```

## Rules that every change follows (the manual's "Programming practices" has the full text)

- **The version** is set once, in `src/core.h` (`MUSIL_VERSION`): CMake, the deploy scripts, the VS Code
  extension and the manual read it from there (gendoc writes `docs/generated/version.tex` and the
  extension's `package.json`).

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

## Roadmap (music: round one done)

1. **Score and rendering** (done): a score is a record of events (onset, duration, kind, source,
   gain, azimuth/elevation); events hold files, buffers, notes, synths (`instrument`) and calls;
   `render` to any layout (mono, stereo, binaural, ambi N, a ring, N channels), `play` through live,
   `display` as a roll drawn like music (staves, note heads, duration lines; a cursor following
   play-score). Editing in the window was tried and dropped: the score lives in the code; a
   notation font and a real editor come after the music tools (Orchidea, the granulator).
2. **Dataset** (done for SOL-like layouts): `db-load` reads the feature file (metadata from the file
   names, features in C++); sounds are opened on demand and cached; `note` resolves to the nearest
   available sound and shifts it. `db-gen` makes a feature file from a folder (Orchidea's dbgen,
   reproduced exactly); queries: db-query with lists, db-grep, db-find, db-between, db-nearest.
   The repository bundles MicroSOL (examples/data/microsol, the *SOL layout: X.spectrum.db next to X/);
   the full TinySOL is a release asset that fetch_tinysol.sh puts in datasets/ (git-ignored).
3. **Roll and export**: the roll is done (a `roll` layer in `plot`, drawn like music, a player of the
   code's score); MIDI and MusicXML export remain.
4. **Elements** (done, round one): pitches, rhythms, chords, lines, textures, pivots, interpolations,
   fragments and scores in scores (see the manual's music section); `algorithmic_comp.mu`,
   `score_in_score.mu`. Known cost: preparing a long score for playback is the hall convolution
   (about 3 s per minute of music); notes and the hall's response are cached.
5. **Orchestrations** (round one done): a result record (segments, solutions, connections) shared by
   every orchestrator; `orchestrate-granular` with envelopes, an orchestra of players (ossia, pairs)
   and five ways of choosing pitches; `midi-read` and MIDI files as fragments (src/music/midi.h).
   `orchestrate-morphological` (done, rebuilt from first principles): `target-analyse` (descriptors in
   signals: flux peaks counted a second for the density, centroid and spread for the register, loudness
   for the dynamics), the granulator driven by those envelopes, and at every event a matching pursuit
   (`mp` in scientific) over the free players' sounds at the target's spectral peaks against the
   target's spectrum less what sounds (an orchestral residual): instrument, pitch and technique from the
   atom, the length from how long the target keeps the sound (atom-persistence), cents from the peaks
   against the sound's own frequency; no segmentation; no change to the published database formats.
   Open: the length of a note is the persistence of its atom in the target (a per-note measure); a
   "wetness" of the target (the decay after each attack) would be the per-frame alternative; short
   techniques are chosen by a table of names because the average spectra in a database do not tell a
   short sound from a long one.
   Playback (done): a score is handed to the engine as cues and a loader thread reads the sounds ahead of
   the clock (`play-cues` in live: the interpreter is not involved while a score plays); the hall is a
   partitioned convolution on the output bus, on its own thread (`bus-reverb`), with a limiter. The old
   whole-score render before playing froze the window for minutes on a long FullSOL score and filled the
   memory; and `register-score` compared a score with itself by value (every note carrying its database),
   which cost a minute on FullSOL at every Play: `equal?` now answers at once for the same object, and
   `same?` asks for identity. The roll's picture is cached; only the playhead is drawn per frame. `score-save` / `score-load` in Orchidea's connection format (notes only; Save... in the
   roll). Speed: `connection` no longer compares segments by value, `merge-continuations` is indexed,
   instrument ranges are cached, the sinc resampler is tabulated (10x).
   `render-buffer` (the score in the hall as channels, as the roll plays it; `render` writes it). The
   granulator advances a quarter second at a time under one event a second, so a silence's low density no
   longer jumps over what follows. Examples: `gran_orchestration2.mu` (one parameter at a time),
   `atmospheres2.mu` (the granulator driven by the measured curves of a recording, kept as
   `data/atmospheres_curves.csv`), Modulations' opening reworked (compact chords together, then apart).
   Playability (done): the granulator draws a player's pitch only among those its instrument was recorded at
   with the technique of the moment (`db-pitches-of`, `player-pitches`); every method snaps to or chooses
   among them; a register beyond the recordings gives the nearest recorded pitches; no note is transposed. The
   orchestra is the polyphony: one note per player at a time.
   Speed: `get`, `opt`, `has?` and `put!` are builtins (they were interpreted scans over every pair, at every
   event and note); the granulator's playability test is a vector range test with a per-event memo per ossia
   spec. A 60-s tutti of 45 ossia players with three styles: 1.5 s (52 s with the first strict version).
   The players as people (done): per-player state (`'seat`: an ossia player keeps its instrument; `'inertia`:
   its technique; `'leap`: steps from its last pitch), the dynamics as a constraint (`'dynamics-strict`,
   substitutions counted; `'balance` in dB per instrument or family), `'method 'cluster` (every pitch of the
   band once before any repeats) with `'weight` and `'tilt`, `'spread` (a group's attack scattered),
   `'duration-law`, `'group-density`, one orchestra shared by several calls (`'continue`, `'start`: the
   player records keep their bookings, so overlapping sections in any order never book a player twice),
   a report per run (events due, played, skipped busy = the saturation, unplayable, substituted, each
   player's share) and `score-validate` (a matching of sounding notes to players, ossia included; shifted
   notes reported). `merge-continuations` no longer lengthens a note by the few ms two players' notes
   overlapped. `sort-by` on numeric keys uses the `argsort` builtin. Example: `gran_orchestration3.mu`;
   `atmospheres.mu` is now a seated orchestra (Ligeti's halved) sharing its players across the sections.
   Open: the physical limits beyond the samples (a recovery time per instrument and technique, a maximum
   sustained duration for winds and brass followed by a breath, what happens when the duration asked for is
   longer than the sample: cut or loop, to be chosen), a per-instrument loudness for the levels.
   Open: dynamic features per sound (following the whole file rather than its average spectrum) would let
   the morphological pursuit choose playing styles by their evolution in time, and are what Maple needs;
   they would be a second feature file next to the published ones, not a change to those.
   Next: `orchestrate-mimetic` (Orchidea: the search over combinations for a target's spectrum, sharing
   target-analyse and mp), then sound types and Maple (a temporal pursuit reusing mp's loop); MIDI and
   MusicXML export after them. The decibel functions are `amp->db` / `db->amp` (the name `db` is free
   for databases). A `player` bundle (instrument + what it can play)
   replacing the repeated db/instr/dyn/tech arguments is planned with Orchidea.
4. **Generators**: random score generator, orchestral granulator.
5. **C++ ports**, each as a C++ core + Musil surface returning events: sound types, Maple (matching
   pursuit), Orchidea (assisted orchestration).
6. Spatial is done in `signals` (ambi-encode/rotate/decode, pan-n, binaural on a spherical head)
   and streams in `live`; the score's `render` layouts use it.
7. Later: `visual`, a separate raylib process driven by OSC (never a raylib window inside FLTK);
   the `in` ugen (device input); `grains`; a mini-notation parser for patterns.
