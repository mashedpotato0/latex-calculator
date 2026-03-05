# Mathematical Symbolic Engine

A 100% C++ modular structure to parse, evaluate, derive and integrate mathematical expressions.

## Build and execution

```bash
g++ src/main.cpp -o app -O2
./app
```
## Code Structure
Uses abstract expression nodes (`ExprPtr` / `SubNode`, etc.) with chains holding runtime variable evaluation bindings successfully locally mapped!

Functions mapping user environment functions are dynamically updated inside `<userFuncMap>` using chain-rule derivations logically executing `clone` rules seamlessly! Integral rules operate off symbolic substitutions successfully built dynamically natively over parsing engines logically without 3rd party lib integrations natively implementing simple math interfaces across commandlines!
