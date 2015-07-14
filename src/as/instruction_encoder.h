#ifndef INSTRUCTION_ENCODER_H
#define INSTRUCTION_ENCODER_H

typedef enum {
      OPER_TYPE_REG = 2
    , OPER_TYPE_IMM
    , OPER_TYPE_PCOFFS
    , OPER_TYPE_PCOFFU
    , OPER_TYPE_LABEL
    , OPER_TYPE_UNKNOWN
} OperType;

class Operand
{
public:
    int type;
    int hi, lo;

    Operand();
    Operand(int type, int hi, int lo);

    bool compareTypes(int otherType);
};

class Instruction
{
public:
    bool setcc;
    std::string label;
    std::vector<Operand *> operTypes;
    int *bitTypes;

    Instruction(int width, bool setcc, const std::string& label);
    ~Instruction();
};

class InstructionEncoder
{
public:
    static InstructionEncoder& getInstance(bool printEnable);

    static std::map<std::string, std::list<Instruction *> > insts;
    static std::vector<std::string> regs;
    static bool encodeInstruction(bool printEnable, const Instruction *pattern, const Token *inst, uint32_t& encodedInstruction);

private:
    static int regWidth;

    InstructionEncoder(bool printEnable);
    ~InstructionEncoder();
};

#endif
