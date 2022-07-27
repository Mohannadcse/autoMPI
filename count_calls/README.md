# Dependencies

The code in this repository is written is OCaml. I strongly suggest you install [ opam ](https://opam.ocaml.org/) to install the dependencies.

Once you have done that, you can install the dependencies with:

    opam install menhir containers ocamlbuild ocamlfind

# Building

To build the project, run

    ocamlbuild -use-ocamlfind find_callback_function.native

# Running

To run the project, run

    ./find_callback_function.native <file 1> <file 2> <optional number>

I have included an input directory, which contains sample input. Try running

    ./find_callback_function.native input/leafpad_1file.txt input/leafpad_2file.txt 0

and 
    ./find_callback_function.native input/most_1file.txt input/most_2file.txt 0

