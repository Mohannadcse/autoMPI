export B=$(basename $1 '.bc')
export C=/nobackup/snlee/llvm-loom-old/Release/bin
$C/opt -load /nobackup/snlee/loom/Release/lib/LLVMLoom.so -loom -loom-file $2 -o Inst$B.bc < $1
$C/clang Inst$B.bc stderr_print.bc $3 -o $B

# If argument 2 is missing
if [ -z "$2" ]; then
  echo "\nDid you forget the policy file?\n";
fi