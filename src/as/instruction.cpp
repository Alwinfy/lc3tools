#include <string>

#include "instruction.h"

Instruction::Instruction(std::string label, int argCount, int opcode)
{
    this->label = label;
    this->argCount = argCount;
    this->opcode = opcode;
}
