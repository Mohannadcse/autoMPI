#include "WPA/FlowSensitive.h"
#include "WPA/Andersen.h"

class NonOptFlowSensitive : public FlowSensitive {

public:
    NonOptFlowSensitive() : FlowSensitive() {}

    void initialize(llvm::Module& module) {
        std::cout << "Before initializing pointer analysis." << std::endl;
        PointerAnalysis::initialize(module);
        std::cout << "After initializing pointer analysis." << std::endl;

        std::cout << "Before Andersen's analysis." << std::endl;
        AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(module);

        std::cout << "Inst: Finished Andersen's analysis" << std::endl;
        ander->getStat()->printStat();
        //std::cout << "Stephen output done." << std::endl;

        svfg = memSSA.buildSVFG(ander, true);
        std::cout << "SVFG built" << std::endl;
        setGraph(svfg);
        //AndersenWaveDiff::releaseAndersenWaveDiff();

        stat = new FlowSensitiveStat(this);
        std::cout << "At end of initialize()" << std::endl;
    }
};

