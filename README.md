# Musil  

*A minimal language for sound, signals, and computational composition.*

**Musil** is a tiny, expressive language for generating and transforming audio signals. 
It is a hybrid dialect of Lisp, Scheme and TCL, powered by efficient C++ `valarray` operations; for more info, please see the [genealogy](docs/musil_anchestors.png).

It has been designed to be small, easy to expand and easy to embed in host applications. **Musil** is a tool for people who cross boundaries —  
for musicians who code, coders who compose, and thinkers who move fluidly between algorithms and aesthetics.

The core of the language is made of a single [C++ header](src/core.h) (~ 1000 loc in total) and a [standard library](src/stdlib.scm) written in **Musil**.

**Musil** includes several features:

* homoiconicity and introspection
* tail recursion
* partial evaluation
* lambda functions with closures
* macros

For an example on how to integrate the language in your application, please check the [command line interpreter](cli/musil.cpp) or the [IDE](ide/musil_ide.cpp).

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

# Building

To compile, from *root* folder type:

`mkdir build`

`cd build`

`cmake ..`

`make`

To build the IDE, use `cmake .. -DBUILD_MUSIL_IDE=ON`. This requires FLTK to be installed ([www.fltk-lib.org](https://www.fltk.org)).

# Licensing

The **Musil** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2025 by Carmine-Emanuele Cella


