# mpi-replacement

This is written for LLVM 3.8.

__TODO__ : description


# Building

Run the following commands to build the project.


    $ cd mpi-replacement
    $ mkdir build
    $ cd build
    $ LLVM_DIR=<path to LLVM 3.8 build>/share/llvm/cmake cmake ..
    $ make
    $ cd ..

# Running

    $ <path to LLVM 3.8 build>/bin/opt -load mpi-replacement/build/skeleton/libSkeletonPass.so -o <new bitcode file> -mpi-substitute < <bitcode file>
