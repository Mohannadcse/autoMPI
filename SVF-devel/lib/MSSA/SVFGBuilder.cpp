//===- SVFGBuilder.cpp -- SVFG builder----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SVFGBuilder.cpp
 *
 *  Created on: Apr 15, 2014
 *      Author: Yulei Sui
 */
#include "MSSA/MemSSA.h"
#include "MSSA/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"

using namespace llvm;
using namespace analysisUtil;

static cl::opt<bool> SVFGWithIndirectCall("svfgWithIndCall", cl::init(false),
        cl::desc("Update Indirect Calls for SVFG using pre-analysis"));

static cl::opt<bool> SingleVFG("singleVFG", cl::init(false),
                               cl::desc("Create a single VFG shared by multiple analysis"));

SVFGOPT* SVFGBuilder::globalSvfg = NULL;

/*!
 * Create SVFG
 */
void SVFGBuilder::createSVFG(MemSSA* mssa, SVFG* graph) {
    svfg = graph;
    svfg->buildSVFG(mssa);
    if(mssa->getPTA()->printStat())
        svfg->performStat();
    //svfg->dump("FS_SVFG");
}

/// Create DDA SVFG
SVFGOPT* SVFGBuilder::buildSVFG(BVDataPTAImpl* pta, bool withAOFI) {

    if(SingleVFG) {
        if(globalSvfg==NULL) {
            /// Note that we use callgraph from andersen analysis here
            globalSvfg = new SVFGOPT();
            if (withAOFI) globalSvfg->setTokeepActualOutFormalIn();
            //Calls the output of SVFG_before_opt via createSVFG
            build(globalSvfg,pta);
        }
        return globalSvfg;
    }
    else {
        SVFGOPT* vfg = new SVFGOPT();
        if (withAOFI) vfg->setTokeepActualOutFormalIn();
        //Calls the output of SVFG_before_opt via createSVFG
        build(vfg,pta);
        //llvm::outs() << "vfg: " << vfg << "\n";

        //int c = 0;
        // SegFault here with FormalINSVFGNode Caused by the call to destroy in ~MemSSA
        // (MemSSA.h line 283)
        /* for(GenericSVFGGraphTy::iterator N = vfg->begin(), E = vfg->end(); N != E; N++) {
            SVFGNode * node = N->getSecond();//>dump();
            if(FormalOUTSVFGNode* fi = llvm::dyn_cast<FormalOUTSVFGNode>(node)) {
                {
                    const MemSSA::RETMU* omega = fi->getRetMU();
                    llvm::outs() << "Omega: " << omega << " ";
                    if(!omega->getMR()) {
                        llvm::outs() << "Null mr" << "\n";
                    }
                    if(!omega->getVer()) {
                        llvm::outs() << "Null opVer" << "\n";
                    }
                    llvm::outs() << "Second Memory region: " << omega->getMR() << "  id: " << omega->getMR()->getMRID() << "V_" << omega->getVer()->getSSAVersion() << "\n";
                }
            }
        }

        llvm::outs() << "Woozle after dump." << "\n"; */

        return vfg;
    }
}

/*!
 * Release memory
 */
void SVFGBuilder::releaseMemory(SVFG* vfg) {
    vfg->clearMSSA();
}

/*!
 * We start the pass here
 */
// SNL: Currently, the first argument is always a new object
bool SVFGBuilder::build(SVFG* graph,BVDataPTAImpl* pta) {

    MemSSA mssa(pta);

    DBOUT(DGENERAL, outs() << pasMsg("Build Memory SSA \n"));

    DominatorTree dt;
    MemSSADF df;//dominance frontier

    for (llvm::Module::iterator iter = pta->getModule()->begin(), eiter = pta->getModule()->end();
            iter != eiter; ++iter) {

        llvm::Function& fun = *iter;
        if (analysisUtil::isExtCall(&fun))
            continue;

        dt.recalculate(fun);
        df.runOnDT(dt);

        mssa.buildMemSSA(fun, &df, &dt);
    }

    mssa.performStat();
    mssa.dumpMSSA();

    DBOUT(DGENERAL, outs() << pasMsg("Build Sparse Value-Flow Graph \n"));

    createSVFG(&mssa, graph);

    /*
    for(GenericSVFGGraphTy::iterator N = graph->begin(), E = graph->end(); N != E; N++) {
        SVFGNode * node = N->getSecond();//>dump();
        if(FormalOUTSVFGNode* fi = llvm::dyn_cast<FormalOUTSVFGNode>(node)) {
            llvm::outs() << "Found formal out\n";
            {
                const MemSSA::RETMU* omega = fi->getRetMU();
                llvm::outs() << "Omega: " << omega << " ";
                if(!omega->getMR()) {
                    llvm::outs() << "Null mr" << "\n";
                }
                if(!omega->getVer()) {
                    llvm::outs() << "Null opVer" << "\n";
                }
                llvm::outs() << "First Memory region: " << omega->getMR() << "  id: " << omega->getMR()->getMRID() << "V_" << omega->getVer()->getSSAVersion() << "\n";
            }
        }
    } */

    if(SVFGWithIndirectCall || SVFGWithIndCall)
        updateCallGraph(mssa.getPTA());

    releaseMemory(graph);

    //int c = 0;
    /*
    for(GenericSVFGGraphTy::iterator N = graph->begin(), E = graph->end(); N != E; N++) {
        SVFGNode * node = N->getSecond();//>dump();
        if(FormalOUTSVFGNode* fi = llvm::dyn_cast<FormalOUTSVFGNode>(node)) {
            {
                const MemSSA::RETMU* omega = fi->getRetMU();
                llvm::outs() << "Omega: " << omega << " ";
                if(!omega->getMR()) {
                    llvm::outs() << "Null mr" << "\n";
                }
                if(!omega->getVer()) {
                    llvm::outs() << "Null opVer" << "\n";
                }
                llvm::outs() << "Alpha Memory region: " << omega->getMR() << "  id: " << omega->getMR()->getMRID() << "V_" << omega->getVer()->getSSAVersion() << "\n";
            }
        }
    }
    llvm::outs() << "Woozle after dump." << "\n";
    llvm::outs() << "graph: " << graph << "\n";
    //graph->getStat()->printStat(); */

    return false;
}



/// Update call graph using pre-analysis results
void SVFGBuilder::updateCallGraph(PointerAnalysis* pta)
{
    CallEdgeMap::const_iterator iter = pta->getIndCallMap().begin();
    CallEdgeMap::const_iterator eiter = pta->getIndCallMap().end();
    for (; iter != eiter; iter++) {
        llvm::CallSite newcs = iter->first;
        const FunctionSet & functions = iter->second;
        for (FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++) {
            const llvm::Function * func = *func_iter;
            svfg->connectCallerAndCallee(newcs, func, vfEdgesAtIndCallSite);
        }
    }
}
