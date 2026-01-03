<img width="256" height="256" alt="image" src="https://github.com/user-attachments/assets/acc21c5a-4181-4a75-8188-561a96d064cf" />

# EpsiLang
Epsilang - is an interpreted embeddable programming language written in C.

## Examples
```
print("Hello, World!");
```
Check the [examples](./examples) directory to see what’s currently implemented.

## Build
```
cc -o epsl src/* -lm
./epsl examples/build_lib.epsl
```
First, we build the interpreter as a CLI tool, then as a library using the interpreter itself.
