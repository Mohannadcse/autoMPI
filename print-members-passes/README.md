# print-members-passes

The code in this repository contains three LLVM passes, all in the same file. As with every LLVM pass, they take an LLVM bitcode module as input and output modified bitcode: none of the passes defined in here modify the bitcode.

One pass prints out all functions defined. Another prints out all functions declared and the third prints out all struct fields defined. Each outputs in the [ TOML ](https://github.com/toml-lang/toml) format.

Which pass in run is determined by which flag is used: '-print-functions' for defined functions, '-print-external functions' for declared functions, and '-print-structs' for struct fields.

This is written for LLVM 3.8.

# Building

Run the following commands to build the project.


    $ cd print-members-passes
    $ mkdir build
    $ cd build
    $ LLVM_DIR=<path to LLVM 3.8 build>/share/llvm/cmake cmake ..
    $ make
    $ cd ..

# Running

    $ <path to LLVM 3.8 build>/bin/opt -load print-members-passes/build/skeleton/libSkeletonPass.so -o /dev/null -print-functions < <bitcode file>
