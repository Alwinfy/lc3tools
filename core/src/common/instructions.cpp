#include "instructions.h"

using namespace core;

Operand::Operand(OperType type, uint32_t width)
{
    this->type = type;
    this->width = width;
}

Instruction::Instruction(bool setcc, std::string const & name, std::vector<Operand *> const & operands)
{
    this->setcc = setcc;
    this->name = name;
    this->operands = operands;
}

Instruction::~Instruction(void)
{
    for(uint32_t i = 0; i < operands.size(); i += 1) {
        delete operands[i];
    }
}

/*
 *bool Operand::compareTypes(int other_type) const
 *{
 *    if(   (   (other_type == OPER_TYPE_LABEL || other_type == OPER_TYPE_UNTYPED_NUM)
 *              && (type == OPER_TYPE_IMM || type == OPER_TYPE_PCOFFS || type == OPER_TYPE_PCOFFU)
 *              )
 *          || (other_type == type)
 *          )
 *        {
 *            return true;
 *        }
 *
 *    return false;
 *}
 */

InstructionGenerator::InstructionGenerator(void)
{
    instructions["add"].push_back(new ADDInstruction({
        new FixedOperand(4, 0x1),
        new RegOperand(3),
        new RegOperand(3),
        new FixedOperand(3, 0x0),
        new RegOperand(3)
    }));

    instructions["add"].push_back(new ADDInstruction({
        new FixedOperand(4, 0x1),
        new RegOperand(3),
        new RegOperand(3),
        new FixedOperand(1, 0x1),
        new NumOperand(5, true)
    }));
}

InstructionGenerator::~InstructionGenerator(void)
{
    // TODO: Go through map and delete all instructions
}

std::string Operand::udecToBin(uint32_t value, uint32_t num_bits)
{
    char * bits = new char[num_bits + 1];
    for(uint32_t i = 0; i < num_bits; i += 1) {
        bits[num_bits - i - 1] = (value & 1) + '0';
        value >>= 1;
    }
    bits[num_bits] = 0;

    return std::string(bits);
}

uint32_t FixedOperand::encode(bool log_enable, AssemblerLogger const & logger, Token const * inst, Token const * operand, uint32_t oper_count, std::map<std::string, uint32_t> const & registers)
{
    (void) operand;
    (void) registers;
    return value & ((1 << width) - 1);
}

uint32_t RegOperand::encode(bool log_enable, AssemblerLogger const & logger, Token const * inst, Token const * operand, uint32_t oper_count, std::map<std::string, uint32_t> const & registers)
{
    uint32_t token_val = registers.at(std::string(operand->str)) & ((1 << width) - 1);

    if(log_enable) {
        logger.printf(utils::PrintType::DEBUG, "%d.%d: reg %s => %s", inst->row_num, oper_count, operand->str.c_str(), udecToBin(token_val, width).c_str());
    }

    return token_val;
}

uint32_t NumOperand::encode(bool log_enable, AssemblerLogger const & logger, Token const * inst, Token const * operand, uint32_t oper_count, std::map<std::string, uint32_t> const & registers)
{
    uint32_t token_val = operand->num & ((1 << width) - 1);
    (void) registers;

    if(token_val != operand->num) {
        logger.printf(utils::PrintType::WARNING, "%d.%d: imm %d truncated to %d", inst->row_num, oper_count, operand->num, token_val);
    }

    if(log_enable) {
        logger.printf(utils::PrintType::DEBUG, "%d.%d: imm %d => %s", inst->row_num, oper_count, operand->num, udecToBin(token_val, width).c_str());
    }

    return token_val;
}
