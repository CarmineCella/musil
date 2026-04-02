# Moon  

*A tiny interpreter for music research*

**Moon** is a tiny and expressive language designed to be easy to use, easy to expand and easy to embed in host applications.

The core of the language is made of a single [C++ header](src/moon.h) and a [standard library](src/stdlib.moon).
A more or less comprehensive overview of the language can be found [here](examples/reference.moon) and version-specific documentation can be found in [docs](docs).

**Moon** is a tool for people who cross boundaries; for musicians who code, coders who compose, and thinkers who move fluidly between algorithms and aesthetics.

For an example on how to integrate the language in your application, please check the [command line interpreter](src/moon.cpp).

# Why the name *Moon*?

The language is named after a passage in **Finnegans wake**, by James Joyce:

> *When the moon of mourning is set and gone.*
> *Over Glinaduna.*
> *Lonu nula.*
> *Ourselves, oursouls alone.*
> *At the site of salvocean.*
> *And watch would the letter you’re wanting be coming may be.*
> *And cast ashore.*

# Building

To compile, from *root* folder type:

`mkdir build`

`cd build`

`cmake ..`

`make`

# Licensing

The **Moon** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2026 by Carmine-Emanuele Cella


