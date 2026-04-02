# Moonil  

*A tiny interpreter for music research*

**Moonil** (pron. moo-neel) is a tiny and expressive language designed to be easy to use, easy to expand and easy to embed in host applications.

The core of the language is made of a single [C++ header](src/moonil.h) and a [standard library](src/stdlib.moon).
A more or less comprehensive overview of the language can be found [here](examples/reference.moon) and version-specific documentation can be found in [docs](docs).

**Moonil** is a tool for people who cross boundaries; for musicians who code, coders who compose, and thinkers who move fluidly between algorithms and aesthetics.

For an example on how to integrate the language in your application, please check the [command line interpreter](src/moonil.cpp).

# Why the name *Moonil*?

The name **Moonil** reflects both poetic inspiration and technical lineage. **Moonil** stands for:

> **Music-Oriented Numeric Interpreted Language**

This reflects the core design of the language:
- **Music-oriented**: conceived as a tool for sound, structure, and compositional thinking  
- **Numeric**: built around vectors, arrays, and numerical transformations  
- **Interpreted**: lightweight, immediate, and interactive  

It draws inspiration from a line in *Finnegans Wake* by James Joyce:

> *When the moon of mourning is set and gone.*
> *Over Glinaduna.*
> *Lonu nula.*
> *Ourselves, oursouls alone.*
> *At the site of salvocean.*
> *And watch would the letter you’re wanting be coming may be.*
> *And cast ashore.*

This image resonates with transformation, cycles, and emergence — ideas that align closely with the language’s focus on evolving numeric and musical processes.

## Lineage

*Moonil* sits within a family of small, expressive languages, combining influences from both scripting and functional traditions.

It retains the **“moon”** reference, echoing lightweight languages such as Lua and MoonScript, emphasizing simplicity, embeddability, and flexibility. At the same time, **Moonil** is strongly inspired by: Lisp and Scheme. From these traditions, **Moonil** inherits:

- first-class procedures
- dynamic evaluation mechanisms such as `eval` and `apply`  
- a minimal, compositional core  

These influences shape **Moonil** as a language where programs can be constructed, transformed, and executed as data — enabling flexible and expressive workflows.


# Building

To compile, from *root* folder type:

`mkdir build`

`cd build`

`cmake ..`

`make`

# Licensing

The **Moonil** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2026 by Carmine-Emanuele Cella
