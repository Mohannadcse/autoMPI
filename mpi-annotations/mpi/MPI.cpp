#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/DebugInfo.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include <llvm/IR/TypeBuilder.h>
#include "llvm/IR/InstIterator.h"

#include<cstdint>

using namespace llvm;
using std::vector;


namespace {

/* TODO: every usage of this currently passes an empty vector as the second,
* output parameter.
* We could have it return, although that makes  for some long lines (since type
* information must be in the call and in the type of the variable it is
* assigned to).
* Also, the current form IS more general, since it does not allocate a new
* vector for each call (which could save memory down the line). */
template<class OrigType, class InstructionType>
void filter_uses(vector<OrigType *> &first, vector<InstructionType *> &out) {

  for( typename vector<OrigType *>::iterator O = first.begin(),
                                             O_E = first.end();
       O != O_E; ++O ) {
    for( Value::use_iterator U = (*O)->use_begin(), U_E = (*O)->use_end();
           U != U_E; ++U ) {
      if( InstructionType * N = dyn_cast<InstructionType>(U->getUser()) ) {
        out.push_back(N);
      }
    }
  }
}


//*
//This is the LLVM pass that actually does the instrumentation
struct CheckInstrumentPass : public ModulePass {
  static char ID;
  CheckInstrumentPass() : ModulePass(ID) {}

  //TODO: This should probably take F_in (below) as a parameter rather than M
  void find_actual_arguments(Module &M) {
    Function * F_in = M.getFunction("msgno_dec");

    unsigned i = 0;
    for( Function::arg_iterator A = F_in->arg_begin(), A_E = F_in->arg_end();
         A != A_E; ++A ) {

      if( A->getName().compare("msgs") == 0 ) {
        break;
      }

      i++;
    }

    unsigned numCallInsts = 0;
    unsigned numUses = 0;
    for(Value::use_iterator U = F_in->use_begin(), U_E = F_in->use_end();
        U != U_E; ++U ) {
      if( CallInst * c = dyn_cast<CallInst>(U->getUser()) ) {
        errs() << *c << "\n";
      }
      numUses++;
    }

    errs() << "Num uses: " << numUses << " CallInsts: " << numCallInsts << "\n";
  }

  //TODO: consider making this return the vector of GEPs before the stores,
  // so that the primary method can always handle casting, etc.
  vector<Value *> find_leafpad_accesses(Module &M) {
    GlobalVariable * ind = M.getGlobalVariable("pub", true);
    errs() << "Got global variable " << *ind << "\n";
    vector<Constant *> start(1, ind);

    vector<LoadInst * > first;
    filter_uses<Constant, LoadInst>(start, first);
    errs() << "After finding loads \n";
    vector<GetElementPtrInst *> geps;
    filter_uses<LoadInst, GetElementPtrInst>(first, geps);
    errs() << "After finding " << geps.size() << " geps \n";
    vector<GetElementPtrInst *> fi_geps;
    ArrayRef<uint64_t> indices({0, 0});
    for( vector<GetElementPtrInst *>::iterator I = geps.begin(), I_E = geps.end();
         I != I_E; ++I) {
      //errs() << "Filtering by indices\n";

      GetElementPtrInst * gep = *I;
      if( indices.size() == gep->getNumIndices() ) {
        bool to_add = true;
        unsigned idx = 0;
        for( User::op_iterator Idx = gep->idx_begin(),
                               Idx_E = gep->idx_end();
             Idx != Idx_E; ++Idx ) {
          ConstantInt * N = dyn_cast<ConstantInt>(Idx->get());
          if( N == 0 || !N->equalsInt(indices[idx]) ) {
            to_add = false;
            break;
          }
          idx++;
        }
        if( to_add ) {
          fi_geps.push_back(gep);
        }
      }
    }

    errs() << "Number of correct geps found: " << fi_geps.size() << "\n";
    vector<Value *> aliases;
    for( vector<GetElementPtrInst *>::iterator G = fi_geps.begin(),
                                                G_E = fi_geps.end();
         G != G_E; ++G ) {
      GetElementPtrInst * gep = *G;
      aliases.push_back(gep);
      for( Value::use_iterator U = gep->use_begin(), U_E = gep->use_end();
          U != U_E; ++U ) {
        //Note that it will never be a constant expression since its argument
        //is not constant
        if( CastInst * e = dyn_cast<CastInst>(U->getUser()) ) {
          aliases.push_back(e);
        }
      }
    }
    errs() << "Number of aliases found: " << fi_geps.size() << "\n";
    return aliases;
  }

  vector<StoreInst *> find_accesses_mutt(Module &M) {
    Function * F_in = M.getFunction("msgno_dec");

    Type * base_type = 0;

    for( Function::arg_iterator A = F_in->arg_begin(), A_E = F_in->arg_end();
         A != A_E; ++A ) {

      if( A->getName().compare("msgs") == 0 ) {
        base_type = A->getType();
        if( PointerType * pt = dyn_cast<PointerType>(base_type) ) {
          base_type = pt->getElementType();
        }
        //errs() << "Found argument\n";
        break;
      }
    }

    if( !base_type ) {
      //errs() << "In if of else\n";
      vector<StoreInst *> rslt;
      return rslt;
    }
    //else

    //errs() << "Type: " << base_type << " " << base_type->getTypeID() << " " << *base_type << "\n";

    vector<GetElementPtrInst * > first;
    //unsigned numNonConstant = 0;
    //*
    for( Module::iterator F = M.begin(), F_E = M.end(); F != F_E; ++F ) {
      for(inst_iterator I = inst_begin(&*F), I_E = inst_end(&*F);
          I != I_E; ++I) {
        if( GetElementPtrInst * gep = dyn_cast<GetElementPtrInst>(&*I) ) {
          Type * s_el_t = gep->getSourceElementType();

          if( s_el_t == base_type ) {
            //TODO: allow user to enter
            ArrayRef<uint64_t> indices({0, 0});
            if( indices.size() == gep->getNumIndices() ) {
              bool to_add = true;
              unsigned idx = 0;
              for( User::op_iterator Idx = gep->idx_begin(),
                                     Idx_E = gep->idx_end();
                   Idx != Idx_E; ++Idx ) {

                ConstantInt * N = dyn_cast<ConstantInt>(Idx->get());
                if( N == 0 || !N->equalsInt(indices[idx]) ) {
                  to_add = false;
                  break;
                }
                idx++;
              }

              if( to_add ) {
                first.push_back(gep);
              }
            }

            /* if( !gep->hasAllConstantIndices() ) {
              //numNonConstant++;
            }*/
          }
        }
      }
    }// */

    errs() << "Number of GEP's found: " << first.size() << "\n";
    //errs() << "Number with at least one non-constant index: " << numNonConstant << "\n";

    vector<LoadInst*> loads;
    //For our specific example, we know that the result will always be a
    //double-pointer
    filter_uses<GetElementPtrInst, LoadInst>(first, loads);
    errs() << "Number of loads found: " << loads.size() << "\n";

    vector<GetElementPtrInst * > second;
    //TODO: worry about going through bitcasts, etc (later)
    filter_uses<LoadInst, GetElementPtrInst>(loads, second);
    errs() << "Number of secondary GEP's found: " << second.size() << "\n";

        /* This was previously in loops that are now in filter_uses -- it
           involves the F_in variable here, so I can't just copy it.
           It was for debugging in any case.
        if( Instruction * inst = dyn_cast<Instruction>(*O) ) {
          if( inst->getFunction() == F_in ) {
            errs() << **O << "\t" <<  *U->getUser() << "\n";
          }
        }// */

    vector<StoreInst*> stores;
    //TODO: currently leave the loop because of what it still prints
    //filter_uses<GetElementPtrInst, StoreInst>(second, stores);
    for( vector<GetElementPtrInst *>::iterator G = second.begin(),
                                               G_E = second.end();
         G != G_E; ++G ) {
      bool unstored = true;

      for( Value::use_iterator U = (*G)->use_begin(), U_E = (*G)->use_end();
           U != U_E; ++U ) {


        if( StoreInst * s = dyn_cast<StoreInst>(U->getUser()) ) {
          stores.push_back(s);
          /* if( unstored ) {
            unstored = false;
            errs() << "Stored to: " << *(*G)->getOperand(1) << "\n";
          } */
        }
      }
    }
    errs() << "Number of stores found: " << stores.size() << "\n";

    return stores;
  }

  bool instrumentStoreInstruction(StoreInst * S/*, Value * ind*/) {

      Value * ind = S->getPointerOperand();
      BasicBlock * orig = S->getParent();
      //errs() << "block: " << *orig << "\n";
      /*
      errs() << "Terminator: " << *orig->getTerminator() << "\n";
      errs() << "First inst: " << orig->front() << "\n";
      errs() << "Last inst: " << orig->back() << "\n";// */
      BasicBlock * nova = orig->splitBasicBlock(S);
      Function * F = orig->getParent();
      Module& M = *F->getParent();
      BasicBlock * printer = BasicBlock::Create(M.getContext(), "", F, nova);

      //errs() << "block: " << *orig << "\n";
      orig->getTerminator()->eraseFromParent();

      IRBuilder<> instr(orig);
      Value * ind_val = instr.CreateLoad(ind);
      Value * cmp = instr.CreateICmp(CmpInst::ICMP_NE, ind_val,
                                     S->getValueOperand());
      Value * br = instr.CreateCondBr(cmp, printer, nova);

      instr.SetInsertPoint(printer);
      //TODO: let the user enter this -- maybe read a bitcode file with the
      //single function to call
      Function * call_kill = M.getFunction("call_kill");
      FunctionType * tau;
      if( !call_kill ) {

        tau = TypeBuilder<int(short, short), false>::get(M.getContext());
        call_kill = Function::Create(tau, Function::ExternalLinkage, "call_kill",
                                     &M);
      } else {
        tau = call_kill->getFunctionType();
      }
      //TODO: let the user enter this
      short args[2] = { -100, 1 };
      //Value * args[2] = { ind_val, S->getValueOperand() };

      unsigned idx = 0;
      Value * arg_values[2];
      for( FunctionType::param_iterator A_T = tau->param_begin(),
                                        A_E = tau->param_end();
           A_T != A_E; ++A_T ) {

        arg_values[idx] = ConstantInt::get(*A_T, args[idx]);
        //arg_values[idx] = instr.CreatePtrToInt(args[idx], *A_T);
        idx++;
      }
      ArrayRef<Value *> arg_values_ref(arg_values, 2);
      Value * call = instr.CreateCall(call_kill, arg_values_ref);
      Value * _ = instr.CreateBr(nova);


      //errs() << *F << "\n";
  }

  //Constant because adding constant exprs to it
  vector<Constant *> getGlobalVariableAndCasts(Module &M, const char * var_name) {
    //The true is because the variable is internal in one case
    GlobalVariable * ind = M.getGlobalVariable(var_name, true);
    errs() << "Got global variable " << *ind << "\n";

    //Constant because adding constant exprs to it
    vector<Constant *> start(1, ind);

    //Currently, this only finds cast constant expressions. There are, of course,
    //other ways to alias the value
    //Note this cannot use filter_uses because of the isCast check
    for( Value::use_iterator U = ind->use_begin(), U_E = ind->use_end();
         U != U_E; ++U ) {
      if( ConstantExpr * e = dyn_cast<ConstantExpr>(U->getUser()) ) {
        if( e->isCast() ) {
          start.push_back(e);
        }
      }
    }
    return start;
  }

  //For this, you need to have link or have linked with a module with a function 'call_kill' of type (short, short) -> int
  virtual bool runOnModule(Module &M) {
    //Declare the list of store instructions up front since it is used in two different places
    vector<StoreInst*> store_insts;

    //mc const char * var_name = "current_path";
    //w3m const char * var_name = "CurrentTab";
    //vim const char * var_name = "curbuf";
    const char * var_name = "openfile";//nano
    vector<Constant*> start = getGlobalVariableAndCasts(M, var_name);
    filter_uses<Constant, StoreInst>(start, store_insts);


    /*
    //Constant because adding constant exprs to it
    vector<Constant *> start(1, ind);

    //Currently, this only finds cast constant expressions. There are, of course,
    //other ways to alias the value
    //Note this cannot use filter_uses because of the isCast check
    for( Value::use_iterator U = ind->use_begin(), U_E = ind->use_end();
         U != U_E; ++U ) {
      if( ConstantExpr * e = dyn_cast<ConstantExpr>(U->getUser()) ) {
        if( e->isCast() ) {
          start.push_back(e);
        }
      }
    }// */


    //find_actual_arguments(M);
    //vector<StoreInst *> store_insts = find_accesses_mutt(M);
    //vector<Value *> start = find_leafpad_accesses(M);
    //errs() << "After finding leafpad locs\n";

    //*
    //vector<StoreInst*> store_insts;
    //filter_uses<Value, StoreInst>(start, store_insts);
    //filter_uses<Constant, StoreInst>(start, store_insts);
    /* This was equivalent to the above, with some comments, before I started
       working with constant expressions.
      for( Value::use_iterator U = ind->use_begin(), U_E = ind->use_end();
         U != U_E; ++U ) {
      if( StoreInst * s = dyn_cast<StoreInst>(U->getUser()) ) {
        //It could be stored in another pointer, but we'll worry about that with
        //aliasing
        if( ind == s->getPointerOperand() ) {
          store_insts.push_back(s);
        }
      }
    }// */

    for(vector<StoreInst *>::iterator itr = store_insts.begin(),
                                           itr_e = store_insts.end();
        itr != itr_e; ++itr) {

      instrumentStoreInstruction(*itr/*, ind*/);

    }// */
    return true;
  }
};// */
}
//*
char CheckInstrumentPass::ID = 0;

static RegisterPass<CheckInstrumentPass> Z("mpi-substitute", "Instrument stores to indicator variables", false, false);// */
