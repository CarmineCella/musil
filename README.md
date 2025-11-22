# Musil  

*A minimal language for sound, signals, and computational composition.*

**Musil** is a tiny, expressive language for generating and transforming audio signals.  
Built on a Scheme-like syntax and powered by efficient C++ `valarray` operations, **Musil** offers a space where sound becomes a material for thought — a meeting point of **precision and soul**.

It has been designed to be small, easy to expand and easy to embed in host applications. **Musil** is a tool for people who cross boundaries —  
for musicians who code, coders who compose, and thinkers who move fluidly between algorithms and aesthetics.

The core of the language is made of a single [C++ header](src/musil.h) (~ 700 loc in total).

**Musil** includes several features:

* homoiconicity and introspection
* tail recursion
* partial evaluation
* lambda functions with closures
* macros

For an example on how to integrate the language in your application, please check the [main file](src/musil.cpp).

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


# Licensing

The **Musil** language is released under the [BSD 2-Clause license](LICENSE.md).

(c) 2025 by Carmine-Emanuele Cella


