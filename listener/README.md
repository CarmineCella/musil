# The Musil Listener

A window for people who don't use the command line, and for everyone when there is
something to look at.

Keyboard first: raylib never loses a key, while a quick trackpad click can fall between
two frames, so nothing depends on a mouse click. **Tab** (Shift-Tab backwards) moves the
focus through the four panels; the focused one has a blue frame.

- **Console** (left): everything Musil prints, and a line at the bottom to type Musil
  into. Enter runs it, or continues it on a new line while braces or parentheses are
  open (Shift-Enter runs regardless); Up/Down recall history; Right arrow completes a
  name. Esc or Ctrl-C stop a running program; Ctrl-L clears the console; Ctrl-R runs
  the last file again; Ctrl-+ / Ctrl-- / Ctrl-0 (or Ctrl-wheel) change the text size.
- **Files** (top right): drop `.mu` files anywhere on the window to run them. They stay
  in the list, are watched, and run again each time you save them: edit in any editor,
  save, look. With the focus here, Up/Down select and Enter runs the selected file again.
- **Variables** (middle right): the globals with a preview of their values, a coloured
  dot per kind (number, string, list, function), user functions below. Up/Down select,
  Enter prints the value in the console.
- **Help** (bottom right): type a name or a word to search the documentation of every
  builtin and library function (the same text `help` prints); Enter prints the first
  match. `(manual)` in the console opens the PDF manual.
- **Controls** (bottom right, once a program declares some with `control`/`toggle`):
  sliders and toggles for the hot parameters; Up/Down select, Left/Right change (Shift
  for bigger steps), Space toggles.
- **From the editor**: the Listener opens the evaluation port 7770 at start-up. Code sent
  there (VS Code with the extension in `editors/vscode/musil`: Cmd-Enter sends the block
  around the cursor; or `nc localhost 7770 < block.mu`) is evaluated in the session and
  shown in a pane above the console. Ctrl-V pastes into the input.
- **Plots**: `(show fig)` puts the figure in place of the console, with the session's
  figures listed on the right; Esc, `q` or the window's close button return to the
  console (the close button never quits the Listener while a figure is up). `,` and `.`
  page through the figures, Ctrl-G reopens the last one. In a figure, `+`/`-` zoom, `W A S D`
  pan, arrows orbit a surface, `0` or `r` reset, dragging pans or orbits, `e` exports a PNG
  (to the current directory when writable, else your home; the path is printed).
  `(save-png fig "file.png")` writes a file and returns when it is written. Several
  figures side by side: `(subplots title figs rows cols)`. Sound is not the Listener's
  business: it belongs to the `live` library.

The Listener is the CLI's interpreter running on a worker thread: one program at a
time, the window stays alive while it runs. Libraries are found in the app bundle
(`Musil.app/Contents/Resources/lib`), next to the binary in `lib/`, or in the source
tree when built from it; `MUSIL_PATH` works as for the CLI.

Build: `cmake --build build --target musil-listener`. macOS bundle with icon:
`./deploy_macos.sh` (add `--dmg` for a disk image).

Font: JetBrains Mono, SIL Open Font License (see assets/).
