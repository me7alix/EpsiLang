<img width="200" height="200" alt="index" src="https://github.com/user-attachments/assets/9c7bdcfe-ad94-4713-9ea4-58caafa35b0f" />

# EpsiLang
Epsilang - is an interpreted embeddable programming language written in C.

## Examples
```
println("Hello, World!");
```
Check the [examples](./examples) directory to see what’s currently implemented.

## Build
```
cc -o epsl src/*
./epsl examples/build_lib.epsl
```
First, we build the interpreter as a CLI tool, then as a library using the interpreter itself.
