# autoMPI
This repo provides the implementation of `autoMPI`, which aims to automate the annotation process of channel and indicator variables required by [MPI Tool](https://www.usenix.org/system/files/conference/usenixsecurity17/sec17-ma.pdf)

# Running the tool:
Following steps explain the details for running autoMPI. The steps are described w.r.t the program `nano` to facilitate the explnation.

## Preprocessing Steps:

0. Build the source code using `WLLVM` and then generate the bitcode using the command `extract-bc`

   a. Run `touch wllvm.log` and `export WLLVM_OUTPUT_FILE=$(realpath wllvm.log)` (you can use a different file if you wish. It only matters to you.)

   b. Search for 'linkArgs' in wllvm.log. Copy the list for that key somewhere. For nano, it is "['-lz', '-lncurses']", which will generally be used as "-lz -lncurses"

1. Run `$LLVM_COMPILER_PATH/opt -mem2reg -o src/nano.opt < src/nano.bc`. You will use `nano.opt` the rest of the process.

## Dynamic Trace Analysis Steps
### Generating policy:
0. Build the projct [print-members-passes](./print-members-passes) as described in [README](./print-members-passes/README.md)

1. Run `$LLVM_COMPILER_PATH/opt -load print_members_passes/build/skeleton/libSkeletonPass.so -print-functions < nano.opt -o /dev/null 2>&1 | python print_members_passes/generate_policy_from_toml_output.py --defaults > nano.policy`. Following is the interpretation of this command:

   a. Let's unpack that command. First, we are running the LLVM opt command on a pass we created, `build/skeleton/libSkeletonPass.so`

   b. We pass the '-print-functions' argument to opt, so that it knows to run the pass that prints out defined functions.

        i. The library libSkeletonPass.so also contains passes to print out all structs ("-print-structs') and to print out all declared functions ("-print-external-functions') (this includes both defined functions and library functions).
    
        ii. The pass also prints out the correct operations for each flag: in our case, this is "callee = ['entry', 'exit']" for each function.

   c. Next, we have it read in the optimized bitcode file we created in the `Preprocessing Steps`. It outputs that same bitcode unmodified, so send that output to /dev/null. 

   d. Redirect those side effects, which are printed to STDERR, to STDOUT, and pipe those to the next command.

   e. That next command takes the output from the LLVM pass, which is in the TOML format and converts that to YAML, which [Loom](https://github.com/cadets/loom) (LLVM instrumentation library) uses.

      i. The `--defaults` argument tells that to prepend "strategy:callout\n\nlogging: stderr_printf\n\n" to its output, which we in general want.

   f. Finally, we write all that to the file `nano.policy`

### Instrumentation:
0. Clone [Loom project](https://github.com/leestephenn/loom/tree/wisconsin) and build it. Make sure you checkout `wisconsin` branch.
```cmake -G Ninja -DCMAKE_CXX_COMPILER=/bin/clang++ -DCMAKE_C_COMPILER=/bin/clang -D CMAKE_BUILD_TYPE=Release -DLLVM_DIR=/llvm/Release/lib/cmake/llvm/ ..```

1. Build the file [stderr_print.c](./Artifacts/stderr_print.c) using `$LLVM_COMPILER_PATH/clang -emit-llvm -c stderr_print.c -o stderr_print_0.bc` and `$LLVM_COMPILER_PATH/opt -mem2reg stderr_print_0.bc -o stderr_print.bc`.

2. Build the project [count_calls](./count_calls/) based on the instructions described in [README](./count_calls/README.md).

3. Run `sh ./LoomBuildScript.sh nano.opt nano.policy '-lz -lncurses'`. Note that '-lz -lncurses' is the library string from step `0.b` in `Preprocessing Steps`. This should produce an executable named 'nano' in your current folder. Here you can find the script [LoomBuildScript.sh](./Artifacts/LoomBuildScript.sh) 

   a. You can find a list of linking args for the programs in our experiment in [LibrariesToLink.toml](./Artifacts/LibrariesToLink.toml)
   
   b. The script `LoomBuildScript.sh` also links in the file `stderr_print.bc`.

4. Run the instrumented binary generated in the previous step `./nano -F testFile1.txt > nano_1file.txt` and `./nano -F testFile1.txt > nano_2file.txt`. For the second run, open another text file with CTRL-R. The file 'testFile1.txt' can be anything.

   a. I generally actually keep my output in a subdirectory, so it is actually `... > dynamicOutput/nano_2file.txt`.

   b. One thing in general: keep your tests simple. If you can avoid using something (say, SSL), do. The simplest possible test works in almost every case.

   c. It is often helpful to pass whatever you are opening (file, website, mailbox) at the command line, instead of starting the program and then opening it.

        i.  For pine and mutt, I used rfc2047-samples.mbox, which you can find at https://raw.githubusercontent.com/qsnake/git/master/t/t5100/rfc2047-samples.mbox.

         ii. for yafc, I used ftp://anonymous@speedtest.tel2.net (blank password) and ftp://demo@test.rebex.net

5. Run `count_calls/find_callback_function.native <...>/nano_[12]file.txt 0` on the two files generated in the previous step. 

   a. This will find all the functions with 0 calls in nano_1file.txt and 1 call in nano_2file.txt. It gives the output:
           
           Candidates:
           rank     func name
           8	do_insertfile_void
           1	unlink_opennode
           1	switch_to_prevnext_buffer


## Static Liveness Analysis
0. Build the project [mpi-annotations](./SVF-devel) based on the provided instructions in [README](./SVF-devel/README.md).

1. Run `/nobackup/snlee/SVF-devel/build/bin/wpa nano.opt -analyze-function do_insertfile_void`. Notice we used the function name `do_insertfile_void`, which received the higher ranking among other candidate functions. This gives 44 indicator variable candidates. We want:
        
        Indicator variable candidate: openfile
        Type: %struct.openfilestruct**
        Source loc: Glob ln: 106 fl: global.c

## Automatic Annotation
0. Build the project [mpi-annotations](./mpi-annotations/) based on the provided instructions in [README](./mpi-annotations/README.md).

1. Build the [call_kill.c](/Artifacts/call_kill.c) using `$LLVM_COMPILER_PATH/clang -emit-llvm -c call_kill.c -o call_kill_0.bc` and `$LLVM_COMPILER_PATH/opt -mem2reg call_kill_0.bc -o call_kill.bc`. 

2. In the mpi-replacement repository, look at runModule(Module &M) in mpi/MPI.cpp. Add a line (already present, so uncomment it) with
        const char * var_name = "openfile";

3. Re-compile the mpi-replacement code by changing to the 'build' directory and running `make`.

4. Back in the folder where you have `nano.opt`, run `$LLVM_COMPILER_PATH/opt -load mpi-replacement/build/mpi/libMPIPass.so -mpi-substitute < nano.opt -o nano_mpi.bc`. 

5. Run `$LLVM_COMPILER_PATH/clang -lz -lncurses new_nano_mpi.bc call_kill.bc  -o nano_mpi`. 

    a. This uses `call_kill.bc`, which was compiled from step 1.

    b. Currently, the call_kill function in call_kill.c prints to stderr. It can be any function that takes two shorts as input. In particular, it can just call the kill system call.

6. The instrumented executable is now nano_mpi. You can run it and collect its output (currently, sent to STDERR).


# Note
We used LLVM 3.8 for developing and building this tool. Therefore, all programs and cloned projects should be built using LLVM 3.8.
