#ifndef ASSEMBLER_H
#define ASSEMBLER_H

class Assembler
{
public:
    static Assembler& getInstance();
    static bool assembleProgram(const std::string& filename, Token *program, std::map<std::string, int>& symbolTable);

private:
    static std::vector<std::string> fileBuffer;
    static int sectionStart;

    Assembler();
    static void processOperands(Token *operands);
    static bool processInstruction(const std::string& filename, const Token *inst, const std::map<std::string, int>& symbolTable, uint32_t& encodedInstruction);
    static bool preprocessProgram(const std::string& filename, Token *program, std::map<std::string, int>& symbolTable, Token *& programStart);
    static bool processPseudo(const std::string& filename, const Token *pseudo);
    static bool getOrig(const std::string& filename, const Token *orig, bool printErrors, int& newOrig);

    // cannot duplicate the singleton
    Assembler(Assembler const&)      = delete;
    void operator=(Assembler const&) = delete;
};

#endif
