#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
//#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
//Deprecated
#include "llvm/Analysis/CallGraph.h"

#include "llvm/IR/InstIterator.h"
#include<stack>
#include<set>
#include<algorithm>
#include <map>

using namespace llvm;

using std::vector;


cl::opt<std::string> AppName("appName", cl::desc("String containing app name"));

namespace {
std::vector<Value *> param_values(Argument * A) {
  unsigned idx = A->getArgNo();
  Function * F = A->getParent();

  std::vector<Value *> result;
  for(Value::use_iterator U = F->use_begin(), E = F->use_end();
      U != E; ++U) {
    if( CallInst * call = dyn_cast<CallInst>(U->getUser()) ) {
      Value * actual = call->getArgOperand(idx);
      result.push_back(actual);
    }
  }

  return result;
}


DenseMap<Value *, unsigned> partially_visited;
unsigned ref_num = 0;

void unflatten(Value * val, std::string indent) {
  if( partially_visited.count(val) ) {
    errs() << indent << "Recursive: " << partially_visited.lookup(val) << "\n";
  } else {
    ref_num++;

    std::pair<Value *, unsigned> pairing({val, ref_num});
    partially_visited.insert(pairing);

    errs() << indent << ref_num << ": ";

    if( Instruction * inst = dyn_cast<Instruction>(val) ) {
      errs() << inst->getOpcodeName() << "\n";
    } else if (ConstantExpr * cexpr = dyn_cast<ConstantExpr>(val)) {
      errs() << cexpr->getOpcodeName() << "\n";
    } else if( Function * f = dyn_cast<Function>(val) ) {
      errs() << f->getName() << "\n";
    } else {
      errs() << *val << "\n";
    }

    if( User * usr = dyn_cast<User>(val) ) {
      std::string new_ind = indent + "  ";
      for( User::value_op_iterator O = usr->value_op_begin(), E = usr->value_op_end();
        O != E; ++O ) {

        unflatten(*O, new_ind);
      }

    }
  }
}

int print_value(Value * val, int i, raw_string_ostream &buf, std::vector<Argument *> * used_params) {

  if( GlobalVariable * gv = dyn_cast<GlobalVariable>(val) ) {

    buf << i << ": " << "Global Variable " << gv->getName() << "\n";
    buf << "\n";
  } else if (Argument * a = dyn_cast<Argument>(val) ) {
    if( used_params != 0 ) {
      used_params->push_back(a);
    }
    buf << i << ": " << "Parameter " << a->getName() << "\n";
    buf << "\n";
  } else if ( User * usr = dyn_cast<User>(val) ) {
    if( Instruction * inst = dyn_cast<Instruction>(usr) ) {
      if( !isa<LoadInst>(inst) ) {
        buf << i << ": " << inst->getOpcodeName() << "\n";
        buf << "Operands: ";
      } else {
        --i;
      }
    } else if( ConstantExpr * cexpr = dyn_cast<ConstantExpr>(val) ) {
        buf << i << ": " << cexpr->getOpcodeName() << "\n";
        buf << "Operands: ";
    } else {
      //This should not print "Operands" since it is largely for literals
      buf << i << ": " << *val << "\n";
      buf << "\n";
    }

    std::string temporary;
    raw_string_ostream children(temporary);

    for( User::value_op_iterator O = usr->value_op_begin(), E = usr->value_op_end();
        O != E; ++O ) {

      i = i + 1;
      if( !isa<LoadInst>(usr) ) {
        buf << i << " ";
      }
      i = print_value(*O, i, children, used_params);

    }

    std::string contents = children.str();
    if( !contents.empty() )  {
      if( !isa<LoadInst>(usr) ) {
        buf << "\n\n";
      }
      buf << children.str();
    }
  }

  return i;

}

void print_actual_arguments_recursive(vector<Argument *> used_params) {

      std::sort(used_params.begin(), used_params.end());
      auto A_E = std::unique(used_params.begin(), used_params.end());
      for(vector<Argument *>::iterator A_I = used_params.begin();
          A_I != A_E; ++A_I) {

        //errs() << *A_I << "\n";
        vector<Value *> values = param_values(*A_I);
        errs() << (*A_I)->getName() << ":\n";
        //errs() << "Number of actuals: " << values.size() << "\n";

        for(vector<Value *>::iterator A_A = values.begin(), A_E = values.end();
            A_A != A_E; ++A_A) {

          //errs() << "In loop ";
          std::string temp2;
          raw_string_ostream buf2(temp2);

          vector<Argument *> sub_used_params;
          print_value(*A_A, 0, buf2, &sub_used_params);
          errs() << buf2.str() << "\n";

          //TODO: I'm pretty sure this was getting into an infinite loop
          //errs() << "Used params: " << sub_used_params.size() << "\n";
          //print_actual_arguments_recursive(sub_used_params);

        }

      }
      errs() << "\n";

}

struct FindStoresPass : public ModulePass {
  static char ID;
  FindStoresPass() : ModulePass(ID) {}

  typedef std::set<const CallGraphNode *> NodeSet;

  NodeSet dfs(const CallGraphNode * node) {
    std::stack<const CallGraphNode *> st;
    NodeSet visited;

    st.push(node);
    while( !st.empty()) {
      const CallGraphNode * top = st.top();
      //errs() << "Frodo Lives Again! ";
      st.pop();

      bool not_in = visited.insert(top).second;
      if( not_in ) {
        Function * func = top->getFunction();
        if( func && !func->empty()) {
          errs() << "Reachable function: " << func->getName() << "\n";
        }

        for( CallGraphNode::const_iterator B = top->begin(), E = top->end();
            B != E; ++B) {
          //Value &call = *B->first;
          //errs() << call << "\n";

          const CallGraphNode * neigh = B->second;
          //errs() << "Calls " << neigh->getFunction()->getName() << "\n";
          st.push(neigh);
        }
      }
    }

    return visited;
  }

  virtual bool runOnModule(Module &M) {

    std::map<std::string, std::vector<std::string> > appToFuncList;
    
    std::string funcs_alpine[5] = {"msgno_dec", "msgline_hidden", "output_debug_msg",
                      "panicking", "any_lflagged" }; // alpine // 
    std::string funcs_mutt[1] = { "menu_middle_page" };
    std::string funcs_nano[1] = {"do_insertfile"};
    std::string funcs_vim[1] = {"ex_edit"};
    std::string funcs[1] = {"ex_edit"};
    //leafpad 
    std::string funcs_leafpad[30] = { "on_file_open", "force_call_cb_modified_changed", "cb_modified_changed", "menu_sensitivity_from_modified_flag", "undo_reset_modified_step", "get_file_basename", "undo_clear_all", "undo_reset_modified_step", "file_open_real", "menu_sensitivity_from_modified_flag", "force_unblock_cb_modified_changed", "cb_mark_changed", "menu_sensitivity_from_selection_bound", "cb_mark_changed", "menu_sensitivity_from_selection_bound", "force_block_cb_modified_changed", "detect_charset", "detect_line_ending", "get_fileinfo_from_selector", "line_numbers_expose", "line_numbers_expose", "cb_focus_event", "cb_select_charset", "init_menu_item_manual_charset", "get_charset_table", "get_encoding_items", "get_encoding_code", "get_default_charset", "get_default_charset", "check_text_modification" };
    //bash 
    std::string funcs_bash_1[5] = { "set_working_directory", "sh_xmalloc","internal_malloc" };
    //bash
    std::string funcs_bash_2[22] = {"bind_variable_internal", "sh_canonpath", "sh_chkwrite", "find_variable_internal", "sh_xfree", "sh_xmalloc", "change_to_directory", "bindpwd", "make_absolute", "make_variable_value", "set_working_directory", "absolute_pathname", "hash_search", "internal_free", "sh_malloc", "bind_variable", "sh_free", "cd_builtin", "internal_getopt", "reset_internal_getopt", "internal_malloc", "get_string_value"};

    std::vector<std::string> alpineFunc(funcs_alpine, 
					funcs_alpine+ sizeof(funcs_alpine)/sizeof(std::string));
    std::vector<std::string> muttFunc(funcs_mutt, 
                                        funcs_mutt+ sizeof(funcs_mutt)/sizeof(std::string));

    std::vector<std::string> nanoFunc(funcs_nano, 
                                        funcs_nano+ sizeof(funcs_nano)/sizeof(std::string));

    std::vector<std::string> vimFunc(funcs_vim, 
                                        funcs_vim+ sizeof(funcs_vim)/sizeof(std::string));

   std::vector<std::string> leafpadFunc(funcs_leafpad, 
                                        funcs_leafpad+ sizeof(funcs_leafpad)/sizeof(std::string));

   std::vector<std::string> bash_1_Func(funcs_bash_1, 
                                        funcs_bash_1+ sizeof(funcs_bash_1)/sizeof(std::string));

   std::vector<std::string> bash_2_Func(funcs_bash_2, 
                                        funcs_bash_2+ sizeof(funcs_bash_2)/sizeof(std::string));


    appToFuncList.emplace("alpine", alpineFunc);
    appToFuncList.emplace("mutt", muttFunc);
    appToFuncList.emplace("nano", nanoFunc);
    appToFuncList.emplace("vim", vimFunc);
    appToFuncList.emplace("leafpad", leafpadFunc);
    appToFuncList.emplace("bash_1", bash_1_Func);
    appToFuncList.emplace("bash_2", bash_2_Func);
    
    auto it = appToFuncList.find(AppName.c_str());

    if (it == appToFuncList.end()){
	return false;
    } else {
	errs() << "FOUND Func: " << it->first << "\n";
	for (std::string f : it->second) {
	  Function * fp = M.getFunction(f);
	  errs() << "FunctionName:: " << f <<"\n";
	  if(fp) {
       	     runFunction(*fp);
      	  }
	}
    }

    /*
    ArrayRef<std::string> funcArray(funcs);
    //I could of course use an iterator here, but I already had the index check ...
    for( int i = 0; i < funcArray.size() ; i++ ) {
      Function * fp = M.getFunction(funcs[i]);
      if(fp) {
        runFunction(*fp);
      }
    }*/
    return false;
  }

  /* Deprecated
  virtual bool runOnModuleCallGraph(Module &M) {
    CallGraphWrapperPass &cgwp = getAnalysis<CallGraphWrapperPass>();
    const CallGraph &calls = cgwp.getCallGraph();
    /
    unsigned c = 0;
    for( CallGraph::const_iterator CG = calls.begin(), E = calls.end();
         CG != E; ++CG) {
      c++;
    }
    errs() << "Number of call graph nodes: " << c << "\n"; // **

    Function * fp = M.getFunction("msgno_dec");
    if(fp) {
      const CallGraphNode * cgn = calls[fp];
      NodeSet called = dfs(cgn);

      for( NodeSet::iterator B = called.begin(), E = called.end();
           B != E; ++B)  {
        Function * called = (*B)->getFunction();
        if( called ) {
          runFunction(*called);
        }
      }

    }
    return false;
  }// */

  bool runFunction(Function &F) {
    //*
    std::string name;
    if(DISubprogram * subp = F.getSubprogram()) {
      name = F.getSubprogram()->getName();
    } else {
      name = F.getName();
    }

    //if( name.compare("foo") != 0 ) {
    /*
    if( name.compare("msgno_dec") != 0 ) {
      return false;
    } // */

    typedef std::vector<StoreInst *> StoreVector;
    StoreVector stores;
    for( inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I ) {
      if( StoreInst * s = dyn_cast<StoreInst>(&*I) ) {
        stores.push_back(s);
      }
    }

    errs() << "Size Store:: " << stores.size() << "\n";

    if( !stores.empty() ) {
      errs() << "Right function " << name << "\n"; // */
      errs() << "Candidates:\n";
    }

    //TODO: handle multiple stores with the same pointer operand -- group stores
    for( StoreVector::iterator I = stores.begin(), E = stores.end();
         I != E; ++I ) {

      Value * stored = (*I)->getValueOperand();
      if( Constant * k = dyn_cast<Constant>(stored) ) {
        if ( k->getNumOperands() == 0) {
          continue;
        }
      }

      Value * ptr = (*I)->getPointerOperand();

      if( !isa<AllocaInst>(ptr) ) {
        //errs() << "Before Candidate: print\n";
        errs() << "Candidate: " << *ptr << "\n";
        errs() << "Stored value: " << *stored << "\n";
        errs() << "Type: " << *ptr->getType() << "\n\n";

        //*
           std::string temporary;
           raw_string_ostream buf(temporary);
           std::vector<Argument *> used_params;//Maybe use SmallPtrSet eventually
           print_value(ptr, 0, buf, 0);

           errs() << buf.str() << "\n";
        //print_actual_arguments_recursive(used_params);

        //errs() << "\n"; // */
      }

    }
    //errs() << "At end\n";

    return false;
  }

  /*
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesCFG();
    AU.addRequiredTransitive<CallGraphWrapperPass>();
  } // */
};

}

char FindStoresPass::ID = 0;

static RegisterPass<FindStoresPass> Y("killed-memory", "Memory Kill analysis", false, false);
