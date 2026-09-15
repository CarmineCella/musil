MicroSOL: a very small sample database in the layout of the *SOL databases (Ircam's TinySOL,
OrchideaSOL, FullSOL): the feature file `microsol.spectrum.db` next to a folder holding the
sounds by family and instrument (the paths in the feature file are relative to that folder;
the loader finds it by content, whatever it is called). Four instruments (Ob, Hn, Vn, Vc), eight sounds each around C4-G4, mf (one ff),
32 sounds in all, so that the music examples run from a fresh clone.

The full sets are much bigger and are not in the repository: `./fetch_tinysol.sh` at the root
of the tree downloads TinySOL (about 800 MB) into `datasets/`, which git ignores, and any *SOL
database placed there loads with `(db-load "../datasets/TinySOL.spectrum.db")` from the
examples (a commented line in each shows where to change the path).
