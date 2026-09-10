# The Musil Listener

A window for people who don't use the command line, and for everyone when there is
something to see or hear.

- **Console** (left): everything Musil prints, and a line at the bottom to type Musil
  into. Enter runs it; Up/Down recall history; Esc (or Ctrl-C) stops a running
  program.
- **Files** (top right): drop `.mu` files on the window to run them. They stay in the
  list, are watched, and re-run each time you save them: edit in any editor, save,
  see the result. Click a file to run it again.
- **Variables** (bottom right): the globals and a preview of their values; user
  functions below. Click one to print it in the console.
- **Plot** (top left): `(show fig)` from `plot.mu` opens the figure here. The wheel
  zooms around the cursor, dragging pans, `r` resets, the coordinates under the cursor
  and the nearest data point are shown, `s` or the save button writes a PNG; drag to
  orbit a surface; `x` or Esc closes it. `(save-png fig "file.png")` writes a file
  from a program.
- **Files** run with their own directory as the place to look for data: a relative path
  that does not exist in the current directory is looked for next to the file, so
  `examples/plots.mu` finds `data/cage.wav` wherever the Listener was started.
- **Sound**: `(play signal sr)` plays a vector, or `(list left right)`; `(stop-audio)`
  stops it.

The Listener is the CLI's interpreter running on a worker thread: one program at a
time, the window stays alive while it runs. Libraries are found in the app bundle
(`Musil.app/Contents/Resources/lib`), next to the binary in `lib/`, or in the source
tree when built from it; `MUSIL_PATH` works as for the CLI.

Build: `cmake --build build --target musil-listener`. macOS bundle with icon:
`./deploy_macos.sh` (add `--dmg` for a disk image).

Font: JetBrains Mono, SIL Open Font License (see assets/).
