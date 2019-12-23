#include "instruction.h"

using namespace lc3::core;

InstructionHandler::InstructionHandler(void)
{
    regs["r0"] = 0;
    regs["r1"] = 1;
    regs["r2"] = 2;
    regs["r3"] = 3;
    regs["r4"] = 4;
    regs["r5"] = 5;
    regs["r6"] = 6;
    regs["r7"] = 7;

    /*
     *instructions.push_back(std::make_shared<ADDRegInstruction>());
     *instructions.push_back(std::make_shared<ADDImmInstruction>());
     *instructions.push_back(std::make_shared<ANDRegInstruction>());
     *instructions.push_back(std::make_shared<ANDImmInstruction>());
     *instructions.push_back(std::make_shared<BRInstruction>());
     *instructions.push_back(std::make_shared<BRnInstruction>());
     *instructions.push_back(std::make_shared<BRzInstruction>());
     *instructions.push_back(std::make_shared<BRpInstruction>());
     *instructions.push_back(std::make_shared<BRnzInstruction>());
     *instructions.push_back(std::make_shared<BRzpInstruction>());
     *instructions.push_back(std::make_shared<BRnpInstruction>());
     *instructions.push_back(std::make_shared<BRnzpInstruction>());
     *instructions.push_back(std::make_shared<NOP0Instruction>());
     *instructions.push_back(std::make_shared<NOP1Instruction>());
     *instructions.push_back(std::make_shared<JMPInstruction>());
     *instructions.push_back(std::make_shared<JSRInstruction>());
     *instructions.push_back(std::make_shared<JSRRInstruction>());
     *instructions.push_back(std::make_shared<LDInstruction>());
     *instructions.push_back(std::make_shared<LDIInstruction>());
     *instructions.push_back(std::make_shared<LDRInstruction>());
     *instructions.push_back(std::make_shared<LEAInstruction>());
     *instructions.push_back(std::make_shared<NOTInstruction>());
     *instructions.push_back(std::make_shared<RETInstruction>());
     *instructions.push_back(std::make_shared<RTIInstruction>());
     *instructions.push_back(std::make_shared<STInstruction>());
     *instructions.push_back(std::make_shared<STIInstruction>());
     *instructions.push_back(std::make_shared<STRInstruction>());
     *instructions.push_back(std::make_shared<TRAPInstruction>());
     *instructions.push_back(std::make_shared<GETCInstruction>());
     *instructions.push_back(std::make_shared<OUTInstruction>());
     *instructions.push_back(std::make_shared<PUTCInstruction>());
     *instructions.push_back(std::make_shared<PUTSInstruction>());
     *instructions.push_back(std::make_shared<INInstruction>());
     *instructions.push_back(std::make_shared<PUTSPInstruction>());
     *instructions.push_back(std::make_shared<HALTInstruction>());
     */
}

std::string IInstruction::toFormatString(void) const
{
    return "";
}

std::string IInstruction::toValueString(void) const
{
    return "";
}
