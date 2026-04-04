# Musil  

![Musil logo](docs/musil_logo.png)

**Musil** is a tiny and expressive language designed to be easy to use, easy to expand and easy to embed in host applications.

The core of the language is made of a single [C++ header](src/core.h) and a [standard library](src/stdlib.mu).
A more or less comprehensive overview of the language can be found [here](examples/reference.mu) and version-specific documentation can be found in [docs](docs).

For an example on how to integrate the language in your application, please check the [command line interpreter](cli/musil.cpp).

**Musil** is a tool for people who cross boundaries; for musicians who code, coders who compose, and thinkers who move fluidly between algorithms and aesthetics.
It also comes with an [IDE](ide/musil_ide.cpp)!

## Libraries

Musil comes with a set of specialized libraries that extend the core language and enable more advanced workflows.

These libraries are designed around Musil’s central idea: treating data, sound, and structure in a unified and expressive way.

### Signal Processing

The signal processing library provides tools for working directly with audio and numerical signals. It includes:

- FFT and spectral transformations  
- Convolution and filtering  
- Windowing functions  
- Feature extraction (centroid, flux, etc.)  

This makes Musil suitable for low-level DSP experiments as well as higher-level sound design.

### Scientific Computing

The scientific library offers utilities for numerical computation and data analysis, including:

- Linear algebra operations  
- Statistical functions  
- Vector and array manipulation  
- Basic machine learning primitives  

It allows Musil to be used as a lightweight environment for numerical exploration and prototyping.

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


Use `ccmake ..` (notice the double `c`) if you want to build the IDE (depends on FLTK).

# Licensing

The **Musil** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2026 by Carmine-Emanuele Cella
