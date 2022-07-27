#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DebugInfo.h"

using namespace llvm;


namespace {

struct PrintStructsPass : public ModulePass {
  static char ID;
  PrintStructsPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) {

    DebugInfoFinder Finder;
    Finder.processModule(M);
    for( DebugInfoFinder::type_iterator i = Finder.types().begin(), E = Finder.types().end();
        i != E; ++i) {

      DIType * type = *i;
      if (type->getTag() == dwarf::DW_TAG_structure_type &&
          !type->getName().empty()) {

        //type->print(errs()); errs() << "\n";

        errs() << "[[structures]]\nname = '" << type->getName() << "'\n\n";

        //errs() << type->getName() << "\n";
        DICompositeType * c = cast<DICompositeType>(type);

        //DINodeArray is MDTupleTypedArrayWrapper<DINode>
        DINodeArray arr = c->getElements();

        for( DINodeArray::iterator M = arr.begin(), M_e = arr.end();
             M != M_e; ++M ) {
          errs() << "[[structures.fields]]" << "\n";
          DIDerivedType * sub = dyn_cast<DIDerivedType>(*M);
          errs() << "name = '" << sub->getName() << "'\n";
          errs() << "operations = ['read', 'write']" << "\n\n";
        }

        errs() << "\n\n";
      }
    }
      /* if( DICompositeType * c = dyn_cast<DICompositeType>(type) ) {
        type->print(errs()); errs() << "\n";
      } */


    return false;
  }
};

struct PrintExternalFunctionsPass : public ModulePass {
  static char ID;
  PrintExternalFunctionsPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) {

    for( Module::iterator F = M.begin(), F_E = M.end(); F != F_E; ++F) {
      if( F->isDeclaration() && !F->isIntrinsic() ) {
        errs() << "[[functions]]\nname = '" << F->getName() << "'\n";
        errs() << "caller = ['entry', 'exit']" << "\n\n";
      }
    }

    return false;
  }
};

struct PrintFunctionsPass : public FunctionPass {
  static char ID;
  PrintFunctionsPass() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) {
    //errs() << "I saw a function called " << F.getName() << "!\n";
    errs() << "[[functions]]\nname = '" << F.getName() << "'\n";
    errs() << "callee = ['entry', 'exit']" << "\n\n";
    return false;
  }
};


/* struct CheckBitCastPass : public BasicBlockPass {
   static char ID;
   CheckBitCastPass() : BasicBlockPass(ID) {}

   virtual bool runOnBasicBlock(BasicBlock &B) {
//errs() << "I saw a function called " << F.getName() << "!\n";
for( BasicBlock::iterator I = B.begin(), E = B.end(); I != E; ++I) {
if( BitCastInst * bc = dyn_cast<BitCastInst>(I) ) {
Type * tau = bc->getType();
if( !isa<PointerType>(tau) ) {
bc->print(errs()); errs() << "\n";
}
}
}
return false;
}
}; */
}

char PrintStructsPass::ID = 0;

static RegisterPass<PrintStructsPass> X("print-structs", "Struct Type Pass", false, false);

char PrintFunctionsPass::ID = 0;

static RegisterPass<PrintFunctionsPass> Y("print-functions", "Print Functions Pass", false, false);

char PrintExternalFunctionsPass::ID = 0;

static RegisterPass<PrintExternalFunctionsPass> Z("print-external-functions", "Print External Functions Pass", false, false);


/*
   char CheckBitCastPass::ID = 0;

static RegisterPass<CheckBitCastPass> Z("check-bit-cast", "Print Functions Pass", false, false); */
