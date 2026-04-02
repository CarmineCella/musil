# Musil  

# Moonil

![Musil logo](docs/musil_logo.png)

*A Scripting Language for Sound And Music Computing*

**Musil** is a tiny and expressive language designed to be easy to use, easy to expand and easy to embed in host applications.

The core of the language is made of a single [C++ header](src/musil.h) and a [standard library](src/stdlib.mu).
A more or less comprehensive overview of the language can be found [here](examples/reference.mu) and version-specific documentation can be found in [docs](docs).

**Musil** is a tool for people who cross boundaries; for musicians who code, coders who compose, and thinkers who move fluidly between algorithms and aesthetics.

For an example on how to integrate the language in your application, please check the [command line interpreter](src/musil.cpp).

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

## Lineage

*Musil* sits within a family of small, expressive languages, combining influences from both scripting and functional traditions.

It is strongly inspired by TCL, Lisp and Scheme. From these traditions, **Musil** inherits:

- first-class procedures
- dynamic evaluation mechanisms such as `eval` and `apply`  
- a minimal, compositional core  

These influences shape **Musil** as a language where programs can be constructed, transformed, and executed as data — enabling flexible and expressive workflows.

# Building

To compile, from *root* folder type:

`mkdir build`

`cd build`

`cmake ..`

`make`

# Licensing

The **Musil** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2026 by Carmine-Emanuele Cella
