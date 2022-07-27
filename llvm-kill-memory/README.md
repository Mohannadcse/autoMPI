# llvm-kill-memory

__TODO__

This is written for LLVM 3.8.

# Building

Run the following commands to build the project.


    $ cd llvm-kill-memory
    $ mkdir build
    $ cd build
    $ LLVM_DIR=<path to LLVM 3.8 build>/share/llvm/cmake cmake ..
    $ make
    $ cd ..

# Running

    $ <path to LLVM 3.8 build>/bin/opt -load llvm-kill-memory/build/source/libKillMemoryPass.so -o /dev/null -killed-memory < <bitcode file> 2>&1
