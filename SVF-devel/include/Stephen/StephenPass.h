#include "MemoryModel/PointerAnalysis.h"
#include "llvm/Pass.h"
#include "llvm/IR/InstIterator.h"

class StephenPass : public llvm::ModulePass {

public:
    static char ID;

    StephenPass() : llvm::ModulePass(ID) {}
    ~StephenPass() {}

    /// LLVM analysis usage
    virtual inline void getAnalysisUsage(llvm::AnalysisUsage &au) const {
        // declare your dependencies here.
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
    }

    virtual bool runOnModule(llvm::Module& module);// { return false; }
    virtual bool runOnModuleOld(llvm::Module& module);// { return false; }

    /// PTA name
    virtual inline const char* getPassName() const {
        return "StephenPass";
    }
};

class StephenSimplePass : public llvm::ModulePass {

public:
    static char ID;

    StephenSimplePass() : llvm::ModulePass(ID) {}
    ~StephenSimplePass() {}

    /// LLVM analysis usage
    virtual inline void getAnalysisUsage(llvm::AnalysisUsage &au) const {
        // declare your dependencies here.
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
    }

    virtual bool runOnModule(llvm::Module& module);// { return false; }

    /// PTA name
    virtual inline const char* getPassName() const {
        return "StephenSimplePass";
    }
};


//int stephen_plus(int x, int y);


