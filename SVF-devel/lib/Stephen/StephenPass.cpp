#include "NonOptFlowSensitive.h"
#include "Stephen/StephenPass.h"
#include "Util/AnalysisUtil.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/AsmParser/Parser.h"
#include <llvm/Support/SourceMgr.h>
#include "llvm/IR/DerivedTypes.h"
//#include <stack>
#include <queue>
//#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>

using namespace llvm;

char StephenPass::ID = 0;
char StephenSimplePass::ID = 0;

static cl::opt<std::string> MethodToAnalyze("analyze-function", cl::Required);
static cl::opt<std::string> CorrectType("correct-type", cl::Optional);

// This is a simple contains method for a vector of LLVM Values
bool vector_contains(std::vector<const Value *> vec, const Value * v) {
    for(std::vector<const Value *>::iterator it = vec.begin(), end = vec.end(); it != end; ++it) {
        if(*it == v) {
            return true;
        }
    }
    return false;
}

//Hack for getting the current type from the variable which is already
//"known" to be correct. This will be replaced by the output from the Purdue
//analysis.
//This, however, only looks at global variables, which will prove difficult
//where the memory region is not reachable from one.
// The correct type is now read from the command line
Type * getCorrectType(Module& module) {
    for(Module::global_iterator g = module.global_begin(), e = module.global_end(); g != e; g++) {
        if(g->getName().str().compare("current_path") == 0) {
            return g->getType();
        }
    }
    return 0;
}

/* Maybe resurrect when I get to 4.0 and can use parseType in Parser. 
Type * getUserCorrectType(Module& module) {
    std::string globalName = "KOCySxdQ1j";
    std::string globalAssign = "@" + globalName + " = external global " + CorrectType;
    errs() << globalAssign << "\n";
    SMDiagnostic error;
    errs() << "Before parse\n";
    MemoryBufferRef F(globalAssign, "<string>");
    bool ret = parseAssemblyInto(F, module, error);
    errs() << "After parse\n";
    GlobalValue * gv = module.getNamedValue(globalName);
    Type * res = gv->getType();
    return res;
} */

//Check whether a given memory region (passed through the mrid parameter) has
//been changed since the input. This is done using the SSA version from SVF.
bool checkUnchanged(SVFG * svfg, SVFG::FormalOUTSVFGNodeSet& out, MRID mrid, VERSION ssa_vers) {
    for(SVFG::FormalOUTSVFGNodeSet::iterator fo_it = out.begin(), fo_eit = out.end(); fo_it != fo_eit; ++fo_it) {
        const FormalOUTSVFGNode * regNode = cast<FormalOUTSVFGNode>(svfg->getSVFGNode(*fo_it));
        //errs() << *regNode << "\n";
        {
            const MemSSA::RETMU* regDef = regNode->getRetMU();
            if(regDef->getMR()->getMRID() == mrid &&
                    regDef->getVer()->getSSAVersion() == ssa_vers) {
                return true;
            }
        }
    }
    return false;
}

void printCandidateValue(const Value * v) {

    if(v->hasName()) {
        errs() << "Indicator variable candidate: " << v->getName() << "\nType: " << *(v->getType()) << "\n";
        errs() << "Source loc: " << analysisUtil::getSourceLoc(v) << "\n";
    } else {
        if(const GetElementPtrInst * gep =
                dyn_cast<GetElementPtrInst>(v)) {
            const Value * w = gep->getPointerOperand();
            errs() << "Indicator variable candidate: " << w->getName() << "\nType: " << *(v->getType()) << "\n";
            errs() << "Source loc: " << analysisUtil::getSourceLoc(w) << " field #:";
            GetElementPtrInst::const_op_iterator idx =
                gep->idx_begin(), idx_e = gep->idx_end();
            idx++;
            for(; idx != idx_e; ++idx) {
                Value * idx_v = *idx;
                ConstantInt * z = dyn_cast<ConstantInt>(idx_v);

                errs() << " " << z->getValue();
            }
            errs() << "\n";
        }
    }
}

// I believe this is actually "older than old" -- in other words, this is from
// before I was really doing any actualy analysis for indicator variables.
bool StephenPass::runOnModuleOld(llvm::Module& module) {
    NonOptFlowSensitive * _pta = new NonOptFlowSensitive();
    _pta->analyze(module);
    SVFG * svfg = _pta->getSVFG();

    PAG * pag = svfg->getPAG();
    for(Module::global_iterator g = module.global_begin(), e = module.global_end(); g != e; g++) {
        errs() << *g << "\n";
        NodeID id = pag->getValueNode(&*g);
        errs() << "NodeID: " << id << "\n";
        PAGNode * pn = pag->getPAGNode(id);
        errs() << *pn << "\n";
        const SVFGNode * s_node = svfg->getDefSVFGNode(pn);
        errs() << *s_node << "\n";

        errs() << "\n";
    }

    for(Module::iterator F = module.begin(), fe = module.end(); F != fe; F++) {
        for(inst_iterator I = inst_begin(*F), E = inst_end(*F); I != E; ++I) {
            if (I->getType()->isPointerTy()) {
                NodeID id = pag->getValueNode(&*I);
                //errs() << "NodeID: " << id << "\n";
                PAGNode * pn = pag->getPAGNode(id);
                errs() << *pn << "\n";
                errs() << *I->getType() << "\n";
                errs() << *svfg->getDefSVFGNode(pn) << "\n";
                errs() << "\n";
            }
        }
    }

    for(GenericSVFGGraphTy::iterator N = svfg->begin(), E=svfg->end();
            N != E; N++) {
        SVFGNode * node = N->getSecond();//>dump();
        if(FormalINSVFGNode* fi = dyn_cast<FormalINSVFGNode>(node)) {
            //errs() << "Has value: " << node->hasValue() << "\n";
            if (const MemRegion* mr = fi->getEntryChi()->getMR()) {
                errs() << "MR: " << mr << "\n";
                //errs() << mr->dumpStr() << "\n";
            }
        }
        if(StmtSVFGNode * si = dyn_cast<StmtSVFGNode>(node)) {
            errs() << *si << "\n";
            PAGNode * src = si->getPAGSrcNode();
            if(src->hasValue()) {
                const Value * srcV = src->getValue();
                errs() << "Src: " << *srcV << "\n";
            }
            PAGNode * dst = si->getPAGDstNode();
            if(dst->hasValue()) {
                const Value * dstV = dst->getValue();
                errs() << "Dst: " << *dstV << "\n";
            }
            if(const Instruction * I = si->getInst()) {
                errs() << "Instruction: " << *I << "\n";
            }

            for(SVFGNode::iterator Ed = node->InEdgeBegin(), f = node->InEdgeEnd(); Ed != f; ++Ed) {
                NodeID e_src = (*Ed)->getSrcID();
                NodeID e_dst = (*Ed)->getDstID();
                errs() << "In-Edge: " << e_src << " to " << e_dst;
                errs() << " indirect? " << (*Ed)->isIndirectVFGEdge() << "\n";
            }

            errs() << "\n";
        }
        //errs() << *node << "\n";

    }
}

void printInOut(SVFG * svfg, SVFG::FormalINSVFGNodeSet& in, SVFG::FormalOUTSVFGNodeSet& out) {
    errs() << "In-nodes:\n";
    for(SVFG::FormalINSVFGNodeSet::iterator fo_it = in.begin(), fo_eit = in.end(); fo_it != fo_eit; ++fo_it) {

        FormalINSVFGNode * regNode = cast<FormalINSVFGNode>(svfg->getSVFGNode(*fo_it));
        errs() << *regNode << "\n";
        const MemSSA::ENTRYCHI* regDef = regNode->getEntryChi();
        errs() << "Memory region: " << regDef->getMR()->getMRID() << "V_" << regDef->getResVer()->getSSAVersion() << "\n";

    }
    errs() << "\n";

    errs() << "Out-nodes:\n";
    for(SVFG::FormalOUTSVFGNodeSet::iterator fo_it = out.begin(), fo_eit = out.end(); fo_it != fo_eit; ++fo_it) {
        const FormalOUTSVFGNode * regNode = cast<FormalOUTSVFGNode>(svfg->getSVFGNode(*fo_it));

        const MemSSA::RETMU* regDef = regNode->getRetMU();
        errs() << "Memory region: " << regDef->getMR()->getMRID() << "V_" << regDef->getVer()->getSSAVersion() << "\n";
    }
    errs() << "\n";
}

void oldFindVariables(SVFG * svfg, vector<SVFGNode *> in_nodes, Type * correctType, vector<const Value*> leasts) {

    for(vector<SVFGNode *>::iterator N = in_nodes.begin(), N_e = in_nodes.end(); N != N_e; ++N) {

        //errs() << "IN-node : " << **N << "\n";
        /* if(FormalINSVFGNode * toGetID = dyn_cast<FormalINSVFGNode>(*N)) {
           const MemSSA::ENTRYCHI* regDef = toGetID->getEntryChi();
           const MemRegion* region = regDef->getMR();
           errs() << " MRID: " << regDef->getMR()->getMRID() << "\n";
           } */

        queue<SVFGNode *> bfs;
        SmallSet<SVFGNode *, 10> visited;
        bfs.push(*N);
        visited.insert(*N);

        //SmallPtrSet<const Value *, 10> pointer_ops;
        //std::multiset<const Value *> pointer_ops;
        std::map<const Value *, int> pointer_ops;

        int count = 0;

        while( !bfs.empty() ) {
            SVFGNode * node = bfs.front();
            bfs.pop();

            if(StoreSVFGNode * si = dyn_cast<StoreSVFGNode>(node)) {

                const Instruction * I = si->getInst();
                if(I) {
                    const StoreInst * s = dyn_cast<StoreInst>(&*I);
                    if(s) {

                        count++;

                        /* MemSSA::CHISet * chi_set = svfg->getStoreCHI(si);
                           if(chi_set) {
                           MemSSA::CHI * first = *(chi_set->begin());
                           errs() << "Store chi: " << first->getOpVer()->getMR()->getMRID() << "\n";
                           } */

                        //errs() << "Pointer operand: " << *(s->getPointerOperand()) << "\n";
                        //errs() << "Store Instruction: " << *s << "\n";
                        //errs() << "Pointer operand: " << *(s->getPointerOperand()) << "\n\n";
                        const Value * v = s->getPointerOperand();
                        if(pointer_ops.count(v)) {
                            pointer_ops[v]++;
                        } else {
                            pointer_ops[v] = 1;
                        }
                    }
                }
            }

            const SVFG::SVFGEdgeSetTy& out_e = node->getOutEdges();
            //errs() << "Empty edges? " << out_e.empty() << "\n";
            for(SVFG::SVFGEdgeSetTy::iterator e_it = out_e.begin(), e_eit = out_e.end(); e_it != e_eit; ++e_it) {

                if((*e_it)->isIndirectVFGEdge()) {

                    NodeID id = (*e_it)->getDstID();
                    //errs() << "NodeID: " << id << "\n";
                    SVFGNode * dstNode = svfg->getSVFGNode(id);
                    if( !visited.count(dstNode) ) {
                        bfs.push(dstNode);
                    }
                }
            }

            if(count > 256) {
                break;
            }
        }

        errs() << "Count: " << count << "\n";

        //const Value * least = (pointer_ops.begin())->first;
        for(std::map<const Value *,int>::iterator p_i = pointer_ops.begin(), p_ie = pointer_ops.end(); p_i != p_ie; ++p_i) {
            const Value * v = p_i->first;

            const Type * tau = v->getType();
            /* while(tau->isPointerTy()) {
               tau = tau->getContainedType(0);
               }
               if(tau->isIntegerTy()) {
               break;
               } */

            /* if (v->getName().str() <= least->getName().str()
               && !(v->getName().empty())
               && !vector_contains(leasts, v)
               && isa<GlobalValue>(*v) )
               {
               least = v;
               } */

            if(v->getType() == correctType && !vector_contains(leasts, v)) {
                if(v->hasName()) {
                    errs() << "Indicator variable candidate: " << v->getName() << "\nType: " << *(v->getType()) << "\n";
                    errs() << "Source loc: " << analysisUtil::getSourceLoc(v) << "\n";
                } else {
                    if(const GetElementPtrInst * gep =
                            dyn_cast<GetElementPtrInst>(v)) {
                        const Value * w = gep->getPointerOperand();
                        errs() << "Indicator variable candidate: " << w->getName() << "\nType: " << *(v->getType()) << "\n";
                        errs() << "Source loc: " << analysisUtil::getSourceLoc(w) << " field #:";
                        GetElementPtrInst::const_op_iterator idx =
                            gep->idx_begin(), idx_e = gep->idx_end();
                        idx++;
                        for(; idx != idx_e; ++idx) {
                            Value * idx_v = *idx;
                            ConstantInt * z = dyn_cast<ConstantInt>(idx_v);

                            errs() << " " << z->getValue();
                        }
                        errs() << "\n";
                    }
                }

                leasts.push_back(v);
                errs() << "\n";
            }
        }


        //errs() << " Count: " << p_i->second << "\n";
        //errs() << "\n";
    }
}

bool checkType(const Value * v) {

    if (CorrectType.getPosition() < 1) {
        return true;
    }

    std::string dummy;
    raw_string_ostream value_type(dummy);
    v->getType()->print(value_type);

    int cmp = value_type.str().compare(CorrectType);

    return (cmp == 0);//I am aware this is !cmp, but I think this is clearer
}

//For Apache
void checkMalloc(const Function * F, PAG * pag) {
    errs() << "\n";

    const CallInst * call = 0;
    for(const_inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if(call = dyn_cast<CallInst>(&*I)) {
        if(call->getType()->isPointerTy()) {
            NodeID id = pag->getValueNode(call);
            PAGNode * pn = pag->getPAGNode(id);

            bool hasEdge = pn->hasIncomingEdges(PAGEdge::Addr);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::Copy);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::Store);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::Load);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::Call);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::Ret);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::NormalGep);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::VariantGep);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::ThreadFork);
            hasEdge = hasEdge || pn->hasIncomingEdges(PAGEdge::ThreadJoin);

            if( !hasEdge ) {
                if( checkType(call) ) {
                    //errs() << *pn << "\n";
                    errs() << "Instruction: " << *call << "\n";
                    errs() << "Type: " << *call->getType() << "\n";
                        errs() << "Source loc: " << analysisUtil::getSourceLoc(call) << "\n";
                    errs() << "\n";
                }

                PAGEdge::PAGEdgeSetTy edges = pn->getOutgoingEdges(PAGEdge::Copy);
                for( PAGEdge::PAGEdgeSetTy::iterator e_I = edges.begin(),
                        e_E = edges.end(); e_I != e_E; ++e_I) {

                    PAGEdge * e = *e_I;
                    const Value * val = e->getValue();
                    //errs() << "Edge out: " << *e->getValue() << "\n";
                    if( checkType(val) ) {
                        errs() << "Instruction: " << *val << "\n";
                        errs() << "Type: " << *val->getType() << "\n";
                        errs() << "Source loc: " << analysisUtil::getSourceLoc(val) << "\n";
                        errs() << "\n";
                    }
                }
            }
        } } //double because can branch on pointer but not && on it
    }
}

bool StephenPass::runOnModule(llvm::Module& module) {
    NonOptFlowSensitive * _pta = new NonOptFlowSensitive();
    errs() << "Before flow-sensitive analysis." << "\n";
    _pta->analyze(module);
    errs() << "after analysis\n";
    SVFG * svfg = _pta->getSVFG();
    errs() << "after svfg gotten\n";

    PAG * pag = svfg->getPAG();
    errs() << "after pag gotten\n";

    /* Type * correctType = getCorrectType(module);
       if ( !correctType ) {
       return false;
       }
       errs() << "Looking for type: " << *correctType << "\n"; */
    //errs() << "Position: " << CorrectType.getPosition() << "\n";
    //Type * correctType = 0;//This is safe: this variable is never de-referenced


    for(Module::iterator F = module.begin(), fe = module.end(); F != fe; F++) {

        //Maybe also compare with F->getSubprogram()->getScope()
        // (which can be the namespace or perhaps the class) or
        // with either F->getSubprogram()->getFile() or
        // F->getSubprogram()->getScope()->getFile()
        // Again, this may change once finding the callback is automated
        //errs() << "Method analyzing: " << MethodToAnalyze << "\n";
        std::string name;
        if(DISubprogram * subp = F->getSubprogram()) {
            name = F->getSubprogram()->getName();
        } else {
            name = F->getName();
        }

        //errs() << "Candidate name: " << name << "\n";
        //if(orig_name.compare("cmd_open") != 0) {
        if(name.compare(MethodToAnalyze) != 0) {
            continue;
        }
        errs() << "Right function." << "\n";
        errs() << name << "\n";
        //errs() << F->getName() << "\n";

        const Function* f_ptr = &*F;

        //checkMalloc(f_ptr, pag);

        SVFG::FormalINSVFGNodeSet& in = svfg->getFormalINSVFGNodes(f_ptr);
        SVFG::FormalOUTSVFGNodeSet& out = svfg->getFormalOUTSVFGNodes(f_ptr);
        errs() << "Size of Formal In: " << in.count() << "\n";
        //errs() << "in is empty? " << in.empty() << "\n";

        //printInOut(svfg, in, out);

        vector<SVFGNode *> in_nodes;

        errs() << "In-nodes:\n";
        for(SVFG::FormalINSVFGNodeSet::iterator fo_it = in.begin(), fo_eit = in.end(); fo_it != fo_eit; ++fo_it) {
            FormalINSVFGNode * regNode = cast<FormalINSVFGNode>(svfg->getSVFGNode(*fo_it));
            //errs() << *regNode << "\n";
            const MemSSA::ENTRYCHI* regDef = regNode->getEntryChi();

            //const PointsTo& ppp = regNode->getPointsTo();

            /* if(!regDef->getMR()) {
               errs() << "Null mr" << "\n";
               }
               if(!regDef->getOpVer()) {
               errs() << "Null opVer" << "\n";
               } */

            if( !checkUnchanged(svfg, out, regDef->getMR()->getMRID(),
                        regDef->getResVer()->getSSAVersion()) )  {
                errs() << *regNode << "\n";
                errs() << "Memory region: " << regDef->getMR()->getMRID() << "V_" << regDef->getResVer()->getSSAVersion() << "\n";
                in_nodes.push_back(regNode);
            }

        }

        //At this point, I believe in_nodes has the memory regions that changed.

        errs() << "\n\n\n";

        std::vector<const Value*> leasts;

        //oldFindVariables(svfg, in_nodes, correctType, leasts);

        for(GenericSVFGGraphTy::iterator N = svfg->begin(), E=svfg->end();
            N != E; N++) {
            //int i = N-;
            //Note: this checks every Statement node, not just called by the function, and not just stores
            if(StmtSVFGNode * sn = dyn_cast<StmtSVFGNode>(N->second)) {
                NodeID src = sn->getPAGSrcNodeID();
                NodeID dst = sn->getPAGDstNodeID();

                for(vector<SVFGNode *>::iterator I = in_nodes.begin(),
                        I_e = in_nodes.end(); I != I_e; ++I) {

                    if(FormalINSVFGNode * node =
                            dyn_cast<FormalINSVFGNode>(*I)) {

                        const PointsTo& ppp = node->getPointsTo();
                        for(PointsTo::iterator p = ppp.begin(), p_e = ppp.end();
                                p != p_e; ++p) {
                            //errs() << *node << " points to: " << *p << "\n";
                            if(*p == src) {
                                //errs() << "Matched " << *node << " " << *p << ": " << *sn << "\n";
                                const Instruction * inst = sn->getInst();
                                //all null currently
                                if(inst) {
                                    if( checkType(inst) ) {
                                        errs() << "Instruction: " << *inst << "\n";
                                        errs() << "Type: " << *inst->getType() << "\n";
                                    }
                                }

                                if(sn->getPAGDstNode()->hasValue()) {
                                    const Value * v = sn->getPAGDstNode()->getValue();
                                    /* const Type * tau = v->getType();
                                    if(tau == correctType) { */
                                    if( checkType(v) ) {
                                        printCandidateValue(v);
                                    }

                                    //errs() << "Value: " << *v << "\n";
                                    errs() << "\n";
                                }
                            }
                        }
                    }
                }
            }
        }
        errs() << "\n";


        //errs() << "\n";

        /* errs() << "out is empty? " << out.empty() << "\n";

        errs() << "Out-nodes:\n";
        for(SVFG::FormalOUTSVFGNodeSet::iterator fo_it = out.begin(), fo_eit = out.end(); fo_it != fo_eit; ++fo_it) {
            errs() << "In loop first." << "\n";
             const FormalOUTSVFGNode * regNode = cast<FormalOUTSVFGNode>(svfg->getSVFGNode(*fo_it));
            //errs() << "In loop second." << "\n";
            errs() << *regNode << "\n";
            {
                const MemSSA::RETMU* regDef = regNode->getRetMU();
                errs() << "Memory region: " << regDef->getMR()->getMRID() << "V_" << regDef->getVer()->getSSAVersion() << "\n";
            }
        } */
    }
}

bool StephenSimplePassRunOnModuleParams(llvm::Module& module) {
    NonOptFlowSensitive * _pta = new NonOptFlowSensitive();
    _pta->analyze(module);
    SVFG * svfg = _pta->getSVFG();
    PAG * pag = svfg->getPAG();

    for(GenericSVFGGraphTy::iterator N = svfg->begin(), E=svfg->end();
            N != E; N++) {
        SVFGNode * node = N->getSecond();//>dump();
        errs() << "Formal param nodes:\n";
        if(FormalParmSVFGNode * parm = dyn_cast<FormalParmSVFGNode>(node)) {
            errs() << *node << "\n";
        }
    }

    return false;
}

bool StephenSimplePass::runOnModule(llvm::Module& module) {
    NonOptFlowSensitive * _pta = new NonOptFlowSensitive();
    _pta->analyze(module);
    errs() << "After analysis" << "\n";
    SVFG * svfg = _pta->getSVFG();
    PAG * pag = svfg->getPAG();

    for(Module::iterator F = module.begin(), fe = module.end(); F != fe; F++) {
        std::string name;
        if(DISubprogram * subp = F->getSubprogram()) {
            name = F->getSubprogram()->getName();
        } else {
            name = F->getName();
        }

        //errs() << "Candidate name: " << name << "\n";
        //if(orig_name.compare("cmd_open") != 0)
        if(name.compare(MethodToAnalyze) != 0) {
            continue;
        }

        errs() << "Right function." << "\n";

        checkMalloc(&*F, pag);

        /* ReturnInst * ret = 0;
        for(inst_iterator I = inst_begin(&*F), E = inst_end(&*F); I != E; ++I) {
            if (ret = dyn_cast<ReturnInst>(&*I)) {
                errs() << *ret << "\n";
                NodeID id = pag->getValueNode(ret);
                errs() << "PAG node id: " << id << "\n";
                PAGNode * pn = pag->getPAGNode(id);
                errs() << *pn << "\n";
                //errs() << svfg->hasDef(pn) << "\n";
                //const SVFGNode * s_node = svfg->getActualRetSVFGNode(pn);
                //errs() << *s_node << "\n";
            }
        }

        for( GenericSVFGGraphTy::iterator N = svfg->begin(), E = svfg->end();
                N != E; N++) {
            if(StmtSVFGNode * sn = dyn_cast<StmtSVFGNode>(N->second)) {
                if(const Instruction * I = sn->getInst()) {
                    if (I == ret) {
                        errs() << *sn << "\n";
                    }
                }
            }
        } */


    }
}

bool runOnModuleEvince(llvm::Module& module) {
    NonOptFlowSensitive * _pta = new NonOptFlowSensitive();
    _pta->analyze(module);
    SVFG * svfg = _pta->getSVFG();
    PAG * pag = svfg->getPAG();

    for(Module::iterator F = module.begin(), fe = module.end(); F != fe; F++) {
        std::string name;
        if(DISubprogram * subp = F->getSubprogram()) {
            name = F->getSubprogram()->getName();
        } else {
            name = F->getName();
        }

        //errs() << "Candidate name: " << name << "\n";
        //if(orig_name.compare("cmd_open") != 0) {
        if(name.compare(MethodToAnalyze) != 0) {
            continue;
        }

        errs() << "Right function." << "\n";

        for(Function::arg_iterator A = F->arg_begin(), A_E = F->arg_end();
                A != A_E; ++A) {
            Argument * A_Addr = &*A;
            if( ! A->getType()->isMetadataTy() ) {
                errs() << "Argument: " << *A << "\n";
                //errs() << "Type: " << *(A->getType()) << "\n";
                NodeID id = pag->getValueNode(A_Addr);
                errs() << "NodeID: " << id << "\n";
                PAGNode * pn = pag->getPAGNode(id);
                //TODO: look at getFormalParmSVFGNode in SVFG using pn (it needs to be const)
                SVFGNode * sn = svfg->getFormalParmSVFGNode(pn);
                errs() << *sn << "\n";

                //errs() << *pn << "\n";
                errs() << "\n";
            }
        }
    }

    for(GenericSVFGGraphTy::iterator N = svfg->begin(), E=svfg->end();
            N != E; N++) {
        SVFGNode * node = N->getSecond();//>dump();
        if(StmtSVFGNode * sn = dyn_cast<StmtSVFGNode>(node)) {
            const Instruction * inst = sn->getInst();
            if(inst) {
                errs() << "Node: " << *sn << "\n";
                errs() << "Instruction: " << *inst << "\n";
                errs() << "Type: " << *inst->getType() << "\n";
                errs() << "\n";
            }
        }
    }
}
