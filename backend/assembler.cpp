#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _ENABLE_DEBUG
    #include <chrono>
#endif

#include "aliases.h"
#include "asm_types.h"
#include "assembler.h"
#include "device_regs.h"
#include "optional.h"
#include "tokenizer.h"

using namespace lc3::core;

static constexpr uint32_t INST_NAME_CLOSENESS = 2;

std::stringstream Assembler::assemble(std::istream & buffer)
//std::vector<uint8_t> Assembler::assemble(std::istream & buffer)
{
    using namespace asmbl;
    using namespace lc3::utils;

    bool success = true;

    logger.printf(PrintType::P_EXTRA, true, "===== begin identifying tokens =====");
    std::vector<StatementNew> statements = buildStatements(buffer);
    logger.printf(PrintType::P_EXTRA, true, "===== end identifying tokens =====");
    logger.newline(PrintType::P_EXTRA);

    logger.printf(PrintType::P_EXTRA, true, "===== begin marking PCs =====");
    setStatementPCField(statements);
    logger.printf(PrintType::P_EXTRA, true, "===== end marking PCs =====");
    logger.newline(PrintType::P_EXTRA);

    logger.printf(PrintType::P_EXTRA, true, "===== begin building symbol table =====");
    optional<SymbolTable> symbols = buildSymbolTable(statements);
    success &= symbols.isValid();
    logger.printf(PrintType::P_EXTRA, true, "===== end building symbol table =====");
    logger.newline(PrintType::P_EXTRA);

    logger.printf(PrintType::P_EXTRA, true, "===== begin assembling =====");
    optional<std::vector<MemEntry>> machine_code_blob = buildMachineCode(statements, *symbols);
    success &= machine_code_blob.isValid();
    logger.printf(PrintType::P_EXTRA, true, "===== end assembling =====");
    logger.newline(PrintType::P_EXTRA);

    if(! success) {
        logger.printf(PrintType::P_ERROR, true, "assembly failed");
        throw lc3::utils::exception("assembly failed");
    }

    return std::stringstream();
}

std::vector<lc3::core::asmbl::StatementNew> Assembler::buildStatements(std::istream & buffer)
{
    using namespace asmbl;

    Tokenizer tokenizer(buffer);
    std::vector<StatementNew> statements;

    while(! tokenizer.isDone()) {
        std::vector<Token> tokens;
        Token cur_token;
        while(! (tokenizer >> cur_token) && cur_token.type != Token::Type::EOL) {
            tokens.push_back(cur_token);
        }

        if(! tokenizer.isDone()) {
            statements.push_back(buildStatement(tokens));
        }
    }

    return statements;
}

lc3::core::asmbl::StatementNew Assembler::buildStatement(std::vector<lc3::core::asmbl::Token> const & tokens)
{
    using namespace asmbl;
    using namespace lc3::utils;

    StatementNew ret;

    // A lot of special case logic here to identify tokens as labels, instructions, pseudo-ops, etc.
    // Note: This DOES NOT check for valid statements, it just identifies tokens with what they should be
    //       based on the structure of the statement.
    // Note: There is some redundancy in the code below (not too much), but it was written this way so that it's
    //       easier to follow the flowchart.
    if(tokens.size() > 0) {
        ret.line = tokens[0].line;
        ret.row = tokens[0].row;
        uint32_t operand_start_idx = 0;

        if(tokens[0].type == Token::Type::STRING) {
            // If the first token is a string, it could be a label, instruction, or pseudo-op.
            if(encoder.isStringPseudo(tokens[0].str)) {
                // If the token is identified as a pseudo-op, mark it as such.
                ret.base = StatementPiece{tokens[0], StatementPiece::Type::PSEUDO};
                operand_start_idx = 1;
            } else {
                // If the token is not a pseudo-op, it could be either a label or an instruction.
                uint32_t dist_from_inst_name = encoder.getDistanceToNearestInstructionName(tokens[0].str);
                if(dist_from_inst_name == 0) {
                    // The token has been identified to match a valid instruction string, but don't be too hasty
                    // in marking it as an instruction yet.
                    if(tokens.size() > 1 && tokens[1].type == Token::Type::STRING) {
                        if(encoder.isStringPseudo(tokens[1].str)) {
                            // If there's a following token that's a pseudo-op, the user probably accidentally used
                            // an instruction name as a label, so mark it as such.
                            ret.label = StatementPiece{tokens[0], StatementPiece::Type::LABEL};
                            ret.base = StatementPiece{tokens[1], StatementPiece::Type::PSEUDO};
                            operand_start_idx = 2;
                        } else {
                            // In most cases, the following token doesn't make a difference and the token really is
                            // an instruction.
                            ret.base = StatementPiece{tokens[0], StatementPiece::Type::INST};
                            operand_start_idx = 1;
                        }
                    } else {
                        // If the following token is a number, this token is an instruction.
                        ret.base = StatementPiece{tokens[0], StatementPiece::Type::INST};
                        operand_start_idx = 1;
                    }
                } else {
                    // The first token wasn't identified as an instruction, so it could either be a label or a typo-ed
                    // instruction.
                    if(tokens.size() > 1) {
                        if(tokens[1].type == Token::Type::STRING) {
                            if(encoder.isStringPseudo(tokens[1].str)) {
                                // If the following token is a pseudo-op, assume the user meant to type a label.
                                ret.label = StatementPiece{tokens[0], StatementPiece::Type::LABEL};
                                ret.base = StatementPiece{tokens[1], StatementPiece::Type::PSEUDO};
                                operand_start_idx = 2;
                            } else if(encoder.isStringValidReg(tokens[1].str)) {
                                // If the following token is a register, assume the user meant to type an instruction...
                                // unless the distance from any valid instruction is too large.
                                if(dist_from_inst_name < INST_NAME_CLOSENESS) {
                                    ret.base = StatementPiece{tokens[0], StatementPiece::Type::INST};
                                    operand_start_idx = 1;
                                } else {
                                    ret.label = StatementPiece{tokens[0], StatementPiece::Type::INST};
                                    operand_start_idx = 1;
                                }
                            } else {
                                // If the following token is a string that was not identified as a pseudo-op or register,
                                // compare to see which token has the closer distance to a valid instruction. Even then,
                                // only mark as an instruction if the distance is close enough to a valid instruction.
                                uint32_t next_dist_from_inst_name = encoder.getDistanceToNearestInstructionName(
                                    tokens[1].str);
                                if(next_dist_from_inst_name < dist_from_inst_name) {
                                    if(next_dist_from_inst_name < INST_NAME_CLOSENESS) {
                                        ret.label = StatementPiece{tokens[0], StatementPiece::Type::LABEL};
                                        ret.base = StatementPiece{tokens[1], StatementPiece::Type::INST};
                                        operand_start_idx = 2;
                                    } else {
                                        ret.label = StatementPiece{tokens[0], StatementPiece::Type::LABEL};
                                        operand_start_idx = 1;
                                    }
                                } else {
                                    if(dist_from_inst_name < INST_NAME_CLOSENESS) {
                                        ret.base = StatementPiece{tokens[0], StatementPiece::Type::INST};
                                        operand_start_idx = 1;
                                    } else {
                                        ret.label = StatementPiece{tokens[0], StatementPiece::Type::LABEL};
                                        operand_start_idx = 1;
                                    }
                                }
                            }
                        } else {
                            // If the following token is a number, assume the user meant to type an instruction...
                            // unless the distance from any valid instruction is too large.
                            if(dist_from_inst_name < INST_NAME_CLOSENESS) {
                                ret.base = StatementPiece{tokens[0], StatementPiece::Type::INST};
                                operand_start_idx = 1;
                            } else {
                                ret.label = StatementPiece{tokens[0], StatementPiece::Type::INST};
                                operand_start_idx = 1;
                            }
                        }
                    } else {
                        // If there are no more tokens on the line, just assume the user typed in a label rather than a
                        // mis-typed instruction.
                        ret.label = StatementPiece{tokens[0], StatementPiece::Type::LABEL};
                        operand_start_idx = 1;
                    }
                }
            }
        } else {
            ret.label = StatementPiece{tokens[0], StatementPiece::Type::NUM};
            operand_start_idx = 1;
        }

        for(uint32_t i = operand_start_idx; i < tokens.size(); i += 1) {
            if(tokens[i].type == Token::Type::STRING) {
                if(encoder.isStringValidReg(tokens[i].str)) {
                    ret.operands.emplace_back(tokens[i], StatementPiece::Type::REG);
                } else {
                    ret.operands.emplace_back(tokens[i], StatementPiece::Type::STRING);
                }
            } else {
                ret.operands.emplace_back(tokens[i], StatementPiece::Type::NUM);
            }
        }
    }

    std::stringstream statement_str;
    statement_str << ret;
    logger.printf(PrintType::P_EXTRA, true, "%s", statement_str.str().c_str());

    return ret;
}

void Assembler::setStatementPCField(std::vector<lc3::core::asmbl::StatementNew> & statements)
{
    using namespace asmbl;
    using namespace lc3::utils;

    uint32_t cur_idx = 0;

    bool found_orig = false;
    bool previous_region_ended = false;
    uint32_t cur_pc = 0;

    // Iterate over the statements, setting the current PC every time a new .orig is found.
    while(cur_idx < statements.size()) {
        StatementNew & statement = statements[cur_idx];

        if(statement.base && statement.base->type == StatementPiece::Type::PSEUDO) {
            if(encoder.isValidPseudoOrig(statement)) {
                if(found_orig) {
#ifndef _LIBERAL_ASM
                    if(! previous_region_ended) {
                        // If found_orig is set, meaning we've seen at least one valid .orig, and previous_region_ended
                        // is not set, meaning we haven't seen a .end yet, then the previous .orig was not ended
                        // properly.
                        logger.asmPrintf(PrintType::P_ERROR, statement,
                            "new .orig found, but previous region did not have .end");
                        logger.newline();
                        throw utils::exception("new .orig fund, but previous region did not have .end");
                    }
#endif
                    previous_region_ended = false;
                }

                found_orig = true;
                cur_pc = encoder.encodePseudoOrig(statement);
                statement.pc = 0;
                ++cur_idx;
                logger.printf(PrintType::P_EXTRA, true, "setting current PC to 0x%0.4x", cur_pc);
                if((cur_pc & 0xffff) != cur_pc) {
#ifdef _LIBERAL_ASM
                    logger.printf(PrintType::P_WARNING, true, "truncating .orig from 0x%0.4x to 0x%0.4x", cur_pc, cur_pc & 0xffff);
                    logger.newline();
#else
                    logger.printf(PrintType::P_ERROR, true, ".orig 0x%0.4x is out of range", cur_pc);
                    logger.newline();
                    throw utils::exception(".orig is out of range");
#endif
                }
                cur_pc &= 0xffff;
                continue;
            } else if(encoder.isValidPseudoEnd(statement)) {
                // If we see a .end, make sure we've seen at least one .orig already (indicated by found_orig being set).
                if(! found_orig) { statement.valid = false; }
                previous_region_ended = true;
                statement.pc = 0;
                ++cur_idx;
                continue;
            }
        }

        if(statement.label && ! statement.base) {
            // If the line is only a label, give it the same PC as the line it is pointing to.
            if(! found_orig) { statement.valid = false; }
            statement.pc = cur_pc;
            ++cur_idx;
            continue;
        }

        if(found_orig) {
            if(cur_pc >= MMIO_START) {
                // If the PC has reached the MMIO region, abort!
                logger.asmPrintf(PrintType::P_ERROR, statement, "cannot write code into memory-mapped I/O region");
                logger.newline();
                throw utils::exception("cannot write code into memory-mapped I/O region");
            }

            if(previous_region_ended) {
                // If found_orig and previous_region_ended are both set, that means we have not set
                // previous_region_ended to false yet, which happens when we find a new .orig. In other
                // words, this means there is a line between a .end and a .orig that should be ignored.
                statement.valid = false;
                ++cur_idx;
                continue;
            }

            statement.pc = cur_pc;
            ++cur_pc;
            logger.printf(PrintType::P_EXTRA, true, "0x%0.4x : \'%s\'", statement.pc, statement.line.c_str());
        } else {
            // If we make it here and haven't found a .orig yet, then there are extraneous lines at the beginning
            // of the file.
            statement.valid = false;
        }

        // Finally, some pseudo-ops need to increment PC by more than 1.
        if(statement.base && statement.base->type == StatementPiece::Type::PSEUDO) {
            if(encoder.isValidPseudoBlock(statement)) {
                cur_pc += encoder.getPseudoBlockSize(statement) - 1;
            } else if(encoder.isValidPseudoString(statement)) {
                cur_pc += encoder.getPseudoStringSize(statement) - 1;
            }
        }

        ++cur_idx;
    }

    // Trigger an error if there was no valid .orig in the file.
    if(! found_orig) {
        logger.printf(PrintType::P_ERROR, true, "could not find valid .orig");
        logger.newline();
        throw utils::exception("could not find valid .orig");
    }

#ifndef _LIBERAL_ASM
    // Trigger error if there was no .end at the end of the file.
    if(found_orig && ! previous_region_ended) {
        logger.printf(PrintType::P_ERROR, true, "no .end at end of file");
        logger.newline();
        throw utils::exception("no .end at end of file");
    }
#endif
}

lc3::optional<SymbolTable> Assembler::buildSymbolTable(std::vector<lc3::core::asmbl::StatementNew> const & statements)
{
    using namespace asmbl;
    using namespace lc3::utils;

    SymbolTable symbols;
    bool success = true;

    for(StatementNew const & statement : statements) {
        if(statement.label) {
            if(statement.label->type == StatementPiece::Type::NUM) {
                logger.asmPrintf(PrintType::P_ERROR, statement, *statement.label, "label cannot be a numeric value");
                logger.newline();
                success = false;
            } else {
                if(! statement.base && statement.operands.size() > 0) {
                    for(StatementPiece const & operand : statement.operands) {
                        logger.asmPrintf(PrintType::P_ERROR, statement, operand, "illegal operand to a label");
                        logger.newline();
                    }
                    success = false;
                    continue;
                }

                auto search = symbols.find(statement.label->str);
                if(search != symbols.end()) {
                    uint32_t old_val = search->second;
#ifdef _LIBERAL_ASM
                    logger.asmPrintf(PrintType::P_WARNING, statement, *statement.label,
                        "redefining label \'%s\' from 0x%0.4x to 0x%0.4x", statement.label->str.c_str(),
                        old_val, statement.pc);
                    logger.newline();
#else
                    logger.asmPrintf(PrintType::P_ERROR, statement, *statement.label,
                        "attempting to redefine label \'%s\' from 0x%0.4x to 0x%0.4x", statement.label->str.c_str(),
                        old_val, statement.pc);
                    logger.newline();
                    success = false;
                    continue;
#endif
                }

#ifndef _LIBERAL_ASM
                if('0' <= statement.label->str[0] && statement.label->str[0] <= '9') {
                    logger.asmPrintf(PrintType::P_ERROR, statement, *statement.label, "label cannot begin with number");
                    logger.newline();
                    success = false;
                    continue;
                }

                if(encoder.getDistanceToNearestInstructionName(statement.label->str) == 0) {
                    logger.asmPrintf(PrintType::P_ERROR, statement, *statement.label, "label cannot be an instruction");
                    logger.newline();
                    success = false;
                    continue;
                }
#endif

                symbols[statement.label->str] = statement.pc;
                logger.printf(PrintType::P_EXTRA, true, "adding label \'%s\' => 0x%0.4x", statement.label->str.c_str(),
                    statement.pc);
            }
        }
    }

    if(success) { return symbols; }
    else { return {}; }
}

lc3::optional<std::vector<MemEntry>> Assembler::buildMachineCode(
    std::vector<lc3::core::asmbl::StatementNew> const & statements, SymbolTable const & symbols)
{
    using namespace asmbl;
    using namespace lc3::utils;

    bool success = true;

    std::vector<MemEntry> ret;

    for(StatementNew const & statement : statements) {
        bool converted = true;

        if(! statement.valid) {
#ifdef _LIBERAL_ASM
            logger.asmPrintf(PrintType::P_WARNING, statement, "ignoring statement whose address cannot be determined");
#else
            logger.asmPrintf(PrintType::P_ERROR, statement, "cannot determine address for statement");
            success = false;
#endif
            logger.newline();
            continue;
        }

        if(statement.base) {
            std::stringstream msg;
            msg << statement << " := ";

            if(statement.base->type == StatementPiece::Type::PSEUDO) {
                bool valid = encoder.validatePseudo(statement, symbols);
                if(valid) {
                    if(encoder.isValidPseudoOrig(statement)) {
                        uint32_t address = encoder.encodePseudoOrig(statement);
                        ret.emplace_back(address, true, statement.line);
                        msg << utils::ssprintf("(orig) 0x%0.4x", address);
                    } else if(encoder.isValidPseudoFill(statement, symbols)) {
                        uint32_t value = encoder.getPseudoFill(statement, symbols);
                        ret.emplace_back(value, false, statement.line);
                        msg << utils::ssprintf("0x%0.4x", value);
                    } else if(encoder.isValidPseudoBlock(statement)) {
                        uint32_t size = encoder.getPseudoBlockSize(statement);
                        for(uint32_t i = 0; i < size; i += 1) {
                            ret.emplace_back(0, false, statement.line);
                        }
                        msg << utils::ssprintf("mem[0x%0.4x:0x%04x] = 0", statement.pc, statement.pc + size - 1);
                    } else if(encoder.isValidPseudoString(statement)) {
                        std::string const & value = encoder.getPseudoString(statement);
                        for(char c : value) {
                            ret.emplace_back(c, false, std::string(1, c));
                        }
                        ret.emplace_back(0, false, statement.line);
                        msg << utils::ssprintf("mem[0x%0.4x:0x%04x] = \'%s\\0\'", statement.pc,
                            statement.pc + value.size(), value.c_str());
                    } else if(encoder.isValidPseudoEnd(statement)) {
                        msg << "(end)";
                    }
                }
                converted &= valid;
            } else if(statement.base->type == StatementPiece::Type::INST) {
                converted = false;
                /*
                 *bool valid = encoder.validateInstruction(statement, symbols);
                 *if(valid) {
                 *    ret.emplace_back(encoder.encodeInstruction(statement, symbols), false, statement.line);
                 *}
                 *success &= valid;
                 */
            } else {
#ifdef _ENABLE_DEBUG
                // buildStatement should never assign the base field anything other than INST or PSEUDO.
                assert(false);
#endif
            }

            if(converted) {
                logger.printf(PrintType::P_EXTRA, true, "%s", msg.str().c_str());
            }
            success &= converted;
        }
    }

    if(success) { return ret; }
    else { return {}; }


    /*
    uint32_t lines_after_end = 0;
    bool found_end = false;
    for(Statement const & state : statements) {
        if(found_end) {
            lines_after_end += 1;
        }
        for(StatementToken const & tok : state.invalid_operands) {
#ifdef _LIBERAL_ASM
            logger.asmPrintf(PrintType::P_WARNING, tok, "ignoring unexpected token");
#else
            logger.asmPrintf(PrintType::P_ERROR, tok, "unexpected token");
            success = false;
#endif
            // TODO: I think the only time there will be an invalid operand is if there is a number at the beginning
            // of the line (or multiple times before an instruction)
            logger.printf(PrintType::P_NOTE, false, "labels cannot look like numbers");
            logger.newline();
        }

        if(state.isInst()) {
            optional<uint32_t> encoding = encodeInstruction(state, symbol_table);
            if(encoding) {
                ret.emplace_back(*encoding, false, state.line);
            } else {
                success = false;
            }
        } else if(checkIfValidPseudoStatement(state, ".orig", false)) {
            lines_after_end = 0;
            found_end = false;
            ret.emplace_back(state.operands[0].num & 0xffff, true, state.line);
        } else if(checkIfValidPseudoStatement(state, ".stringz", false)) {
            std::string const & value = state.operands[0].str;
            for(char c : value) {
                ret.emplace_back(c, false, std::string(1, c));
            }
            ret.emplace_back((uint16_t) 0, false, state.line);
        } else if(checkIfValidPseudoStatement(state, ".blkw", false)) {
            for(uint32_t i = 0; i < (uint32_t) state.operands[0].num; i += 1) {
                ret.emplace_back(0, false, state.line);
            }
        } else if(checkIfValidPseudoStatement(state, ".fill", false)) {
            if(state.operands[0].type == TokenType::NUM) {
                ret.emplace_back(state.operands[0].num, false, state.line);
            } else if(state.operands[0].type == TokenType::LABEL) {
                // TODO: this is a duplicate of the code in LabelOperand::encode
                // eventually create a class for pseudo-ops that's similar to the instructions
                StatementToken const & oper = state.operands[0];
                auto search = symbol_table.find(oper.str);
                if(search == symbol_table.end()) {
                    logger.asmPrintf(PrintType::P_ERROR, oper, "unknown label \'%s\'", oper.str.c_str());
                    logger.newline();
                    success = false;
                    continue;
                }

                ret.emplace_back(search->second, false, state.line);
            }
        } else if(checkIfValidPseudoStatement(state, ".end", false)) {
            found_end = true;
        } else {
            if(!state.hasLabel() && state.invalid_operands.size() == 0) {
                StatementToken state_tok;
                state_tok.line = state.line;
#ifdef _LIBERAL_ASM
                logger.asmPrintf(PrintType::P_WARNING, 0, state_tok.line.size(), state_tok, "ignoring invalid line");
#else
                logger.asmPrintf(PrintType::P_ERROR, 0, state_tok.line.size(), state_tok, "invalid line");
                success = false;
#endif
                logger.newline();
            }
        }
    }

    if(!found_end) {
#ifdef _LIBERAL_ASM
        logger.printf(PrintType::P_WARNING, true, "assembly did not end in .end", lines_after_end);
#else
        logger.printf(PrintType::P_ERROR, true, "assembly did not end in .end", lines_after_end);
#endif
        logger.newline();
#ifndef _LIBERAL_ASM
        throw utils::exception("assembly did not end in .end");
#endif
    }

    if(lines_after_end > 0) {
#ifdef _LIBERAL_ASM
        logger.printf(PrintType::P_WARNING, true, "ignoring %d lines after final .end", lines_after_end);
#else
        logger.printf(PrintType::P_ERROR, true, "%d invalid lines after final .end", lines_after_end);
#endif
        logger.newline();
#ifndef _LIBERAL_ASM
        throw utils::exception("assembly did not end in .end");
#endif
    }*/

}

/*
std::stringstream Assembler::assemble(std::istream & buffer)
{
    using namespace asmbl;
    using namespace lc3::utils;

    // build statements from tokens
    Tokenizer tokenizer(buffer);
    std::vector<Statement> statements;
    while(! tokenizer.isDone()) {
        std::vector<Token> tokens;
        Token token;
        while(! (tokenizer >> token) && token.type != TokenType::EOS) {
            if(token.type != TokenType::EOS) {
                tokens.push_back(token);
            }
        }

        if(! tokenizer.isDone()) {
            statements.push_back(buildStatement(tokens));
        }
    }

    markPC(statements);
    bool success = true;
    optional<SymbolTable> symbol_table = firstPass(statements);
    success &= static_cast<bool>(symbol_table);
    optional<std::vector<MemEntry>> obj_blob = secondPass(statements, *symbol_table);
    success &= static_cast<bool>(obj_blob);

    if(! success) {
        logger.printf(PrintType::P_ERROR, true, "assembly failed");
        throw lc3::utils::exception("assembly failed");
    }

    std::stringstream ret;

    for(MemEntry entry : *obj_blob) {
        ret << entry;
    }

    return ret;
}

lc3::optional<SymbolTable> Assembler::firstPass(std::vector<asmbl::Statement> const & statements)
{
    using namespace asmbl;
    using namespace lc3::utils;

    SymbolTable symbol_table;
    bool success = true;

    for(Statement const & state : statements) {
        if(! state.hasLabel()) { continue; }

        auto search = symbol_table.find(state.label.str);
        if(search != symbol_table.end()) {
            uint32_t old_val = search->second;
#ifdef _LIBERAL_ASM
            PrintType p_type = PrintType::P_WARNING;
#else
            PrintType p_type = PrintType::P_ERROR;
            success = false;
#endif
            logger.asmPrintf(p_type, state.label, "redefining label from 0x%0.4x to 0x%0.4x", old_val,
                state.pc);
            logger.newline();
        }

        symbol_table[state.label.str] = state.pc;
        logger.printf(PrintType::P_EXTRA, true, "adding label \'%s\' => 0x%0.4x", state.label.str.c_str(), state.pc);
    }

    if(success) { return symbol_table; }
    else { return {}; }
}

lc3::optional<std::vector<MemEntry>> lc3::core::Assembler::secondPass(std::vector<asmbl::Statement> const & statements,
    SymbolTable const & symbol_table)
{
    using namespace asmbl;
    using namespace lc3::utils;

    bool success = true;

    std::vector<MemEntry> ret;
    uint32_t lines_after_end = 0;
    bool found_end = false;
    for(Statement const & state : statements) {
        if(found_end) {
            lines_after_end += 1;
        }
        for(StatementToken const & tok : state.invalid_operands) {
#ifdef _LIBERAL_ASM
            logger.asmPrintf(PrintType::P_WARNING, tok, "ignoring unexpected token");
#else
            logger.asmPrintf(PrintType::P_ERROR, tok, "unexpected token");
            success = false;
#endif
            // TODO: I think the only time there will be an invalid operand is if there is a number at the beginning
            // of the line (or multiple times before an instruction)
            logger.printf(PrintType::P_NOTE, false, "labels cannot look like numbers");
            logger.newline();
        }

        if(state.isInst()) {
            optional<uint32_t> encoding = encodeInstruction(state, symbol_table);
            if(encoding) {
                ret.emplace_back(*encoding, false, state.line);
            } else {
                success = false;
            }
        } else if(checkIfValidPseudoStatement(state, ".orig", false)) {
            lines_after_end = 0;
            found_end = false;
            ret.emplace_back(state.operands[0].num & 0xffff, true, state.line);
        } else if(checkIfValidPseudoStatement(state, ".stringz", false)) {
            std::string const & value = state.operands[0].str;
            for(char c : value) {
                ret.emplace_back(c, false, std::string(1, c));
            }
            ret.emplace_back((uint16_t) 0, false, state.line);
        } else if(checkIfValidPseudoStatement(state, ".blkw", false)) {
            for(uint32_t i = 0; i < (uint32_t) state.operands[0].num; i += 1) {
                ret.emplace_back(0, false, state.line);
            }
        } else if(checkIfValidPseudoStatement(state, ".fill", false)) {
            if(state.operands[0].type == TokenType::NUM) {
                ret.emplace_back(state.operands[0].num, false, state.line);
            } else if(state.operands[0].type == TokenType::LABEL) {
                // TODO: this is a duplicate of the code in LabelOperand::encode
                // eventually create a class for pseudo-ops that's similar to the instructions
                StatementToken const & oper = state.operands[0];
                auto search = symbol_table.find(oper.str);
                if(search == symbol_table.end()) {
                    logger.asmPrintf(PrintType::P_ERROR, oper, "unknown label \'%s\'", oper.str.c_str());
                    logger.newline();
                    success = false;
                    continue;
                }

                ret.emplace_back(search->second, false, state.line);
            }
        } else if(checkIfValidPseudoStatement(state, ".end", false)) {
            found_end = true;
        } else {
            if(!state.hasLabel() && state.invalid_operands.size() == 0) {
                StatementToken state_tok;
                state_tok.line = state.line;
#ifdef _LIBERAL_ASM
                logger.asmPrintf(PrintType::P_WARNING, 0, state_tok.line.size(), state_tok, "ignoring invalid line");
#else
                logger.asmPrintf(PrintType::P_ERROR, 0, state_tok.line.size(), state_tok, "invalid line");
                success = false;
#endif
                logger.newline();
            }
        }
    }

    if(!found_end) {
#ifdef _LIBERAL_ASM
        logger.printf(PrintType::P_WARNING, true, "assembly did not end in .end", lines_after_end);
#else
        logger.printf(PrintType::P_ERROR, true, "assembly did not end in .end", lines_after_end);
#endif
        logger.newline();
#ifndef _LIBERAL_ASM
        throw utils::exception("assembly did not end in .end");
#endif
    }

    if(lines_after_end > 0) {
#ifdef _LIBERAL_ASM
        logger.printf(PrintType::P_WARNING, true, "ignoring %d lines after final .end", lines_after_end);
#else
        logger.printf(PrintType::P_ERROR, true, "%d invalid lines after final .end", lines_after_end);
#endif
        logger.newline();
#ifndef _LIBERAL_ASM
        throw utils::exception("assembly did not end in .end");
#endif
    }

    if(success) { return ret; }
    else { return {}; }
}

asmbl::Statement Assembler::buildStatement(std::vector<asmbl::Token> const & tokens)
{
    using namespace asmbl;

    std::vector<StatementToken> ret_tokens;
    Statement ret;

    for(Token const & token : tokens) {
        ret_tokens.push_back(StatementToken(token));
    }

    // shouldn't happen, but just in case...
    if(tokens.size() == 0) { return ret; }

    markRegAndPseudoTokens(ret_tokens);
    markInstTokens(ret_tokens);
    markLabelTokens(ret_tokens);
    ret = makeStatementFromTokens(ret_tokens);

    return ret;
}

void Assembler::markRegAndPseudoTokens(std::vector<asmbl::StatementToken> & tokens)
{
    using namespace asmbl;

    for(StatementToken & token : tokens) {
        if(token.type == TokenType::STRING) {
            if(token.str.size() > 0 && token.str[0] == '.') {
                token.type = TokenType::PSEUDO;
            } else if(encoder.isStringValidReg(token.str)) {
                token.type = TokenType::REG;
            }
        }
    }
}

void Assembler::markInstTokens(std::vector<asmbl::StatementToken> & tokens)
{
    using namespace asmbl;

    if(tokens.size() == 1) {
        if(tokens[0].type == TokenType::STRING) {
            uint32_t token_0_dist = encoder.getDistanceToNearestInstructionName(tokens[0].str);
            if(token_0_dist == 0) {
                tokens[0].type = TokenType::INST;
                tokens[0].lev_dist = token_0_dist;
            } else {
                // there's only one string on this line and it's not exactly an instruction, so assume it's a label
            }
        } else {
            // the sole token on this line is not a string, so it cannot be an instruction
        }
    } else {
        if(tokens[0].type == TokenType::STRING || tokens[0].type == TokenType::NUM) {
            // if the first token is a number, it was probably an attempt at making a label that looks like a number
            // in this case, make the distance too large to be close to an instruction
            uint32_t token_0_dist = 1 << 31;
            if(tokens[0].type != TokenType::NUM) {
                token_0_dist = encoder.getDistanceToNearestInstructionName(tokens[0].str);
            }
            if(tokens[1].type == TokenType::STRING) {
                // first two tokens are both strings, maybe they're instructions?
                uint32_t token_1_dist = encoder.getDistanceToNearestInstructionName(tokens[1].str);
                // see which is closer to an instruction
                // if they're the same, lean toward thinking first token is the instruction
                // imagine a case like 'jsr jsr'; the appropriate error message should be 'jsr is not a label'
                // and for that to be the case, the first token should be marked as the instruction
                if(token_1_dist < token_0_dist) {
                    uint32_t lev_thresh = 1;
                    if(tokens.size() >= 3) {
                        if(tokens[2].type == TokenType::PSEUDO) {
                            // if the next token is a pseudo-op, be a little less lenient in assuming this is an
                            // instruction
                            lev_thresh -= 1;
                        } else {
                            // if the next token is a reg, num, or string be a little more lenient in assuming this
                            // is an instruction
                            lev_thresh += 1;
                        }
                    }
                    if(token_1_dist <= lev_thresh) {
                        tokens[1].type = TokenType::INST;
                        tokens[1].lev_dist = token_1_dist;
                    } else {
                        // too far from an instruction
                    }
                } else {
                    if(token_0_dist < 3) {
                        tokens[0].type = TokenType::INST;
                        tokens[0].lev_dist = token_0_dist;
                    } else {
                        // too far from an instruction
                    }
                }
            } else {
                uint32_t lev_thresh = 1;
                if(tokens[1].type == TokenType::PSEUDO) {
                    if(tokens[0].type == TokenType::STRING) {
                        // if the second token is a pseudo op, then the first token should be considered a label, even if
                        // it matches an instruction
                        tokens[0].type = TokenType::LABEL;
                        return;
                    }
                } else {
                    lev_thresh += 1;
                }
                if(token_0_dist <= lev_thresh) {
                    tokens[0].type = TokenType::INST;
                    tokens[0].lev_dist = token_0_dist;
                }
            }
        } else {
            // the line starts with something other than a string...so the second token cannot be an instruction
        }
    }
}

void Assembler::markLabelTokens(std::vector<asmbl::StatementToken> & tokens)
{
    using namespace asmbl;

    if(tokens.size() > 0 && tokens[0].type == TokenType::STRING) {
        tokens[0].type = TokenType::LABEL;
    }

    // mark any strings after an inst as labels
    bool found_inst = false;
    for(StatementToken & token : tokens) {
        if(found_inst && token.type == TokenType::STRING) {
            token.type = TokenType::LABEL;
        }

        if(token.type == TokenType::INST || checkIfValidPseudoToken(token, ".fill")) {
            found_inst = true;
        }
    }
}

asmbl::Statement Assembler::makeStatementFromTokens(std::vector<asmbl::StatementToken> & tokens)
{
    using namespace asmbl;

    Statement ret;
    uint32_t pos = 0;
    if(tokens.size() > 0) {
        ret.line = tokens[0].line;
        if(tokens[0].type == TokenType::LABEL) {
            StatementToken temp = tokens[0];
            std::transform(temp.str.begin(), temp.str.end(), temp.str.begin(), ::tolower);
            ret.label = temp;
            pos += 1;
        }
    }

    while(pos < tokens.size()) {
        if(tokens[pos].type == TokenType::INST || tokens[pos].type == TokenType::PSEUDO) {
            StatementToken temp = tokens[pos];
            std::transform(temp.str.begin(), temp.str.end(), temp.str.begin(), ::tolower);
            ret.inst_or_pseudo = temp;
            pos += 1;
            break;
        } else {
            // all tokens before an instruction (excluding a possible label) are invalid
            ret.invalid_operands.push_back(tokens[pos]);
        }
        pos += 1;
    }

    while(pos < tokens.size()) {
        StatementToken temp = tokens[pos];
        // at this point, the only STRING is the operand to a stringz
        if(temp.type != TokenType::STRING) {
            // everything is case insensitive
            std::transform(temp.str.begin(), temp.str.end(), temp.str.begin(), ::tolower);
        } else {
            // process stringz operand for escape sequences
            std::stringstream new_str;
            std::string const & str = temp.str;
            for(uint32_t i = 0; i < str.size(); i += 1) {
                char c = str[i];
                if(c == '\\') {
                    if(i + 1 < str.size()) {
                        // if the next character is a recognized escape sequence
                        switch(str[i + 1]) {
                            case 'n' : c = '\n'; i += 1; break;
                            case 'r' : c = '\n'; i += 1; break;
                            case 't' : c = '\t'; i += 1; break;
                            case '\\': c = '\\'; i += 1; break;
                            default: break;
                        }
                    }
                }
                new_str << c;
            }
            temp.str = new_str.str();
        }

        ret.operands.push_back(temp);
        pos += 1;
    }

    return ret;
}

void Assembler::markPC(std::vector<asmbl::Statement> & statements)
{
    using namespace asmbl;
    using namespace lc3::utils;

    uint32_t cur_pc = 0;
    uint32_t cur_pos = 0;
    bool found_orig = false;

    // find the first valid orig
    while(! found_orig && cur_pos < statements.size()) {
        while(cur_pos < statements.size())  {
            Statement const & state = statements[cur_pos];

            if(checkIfValidPseudoStatement(state, ".orig", true)) {
                found_orig = true;
                break;
            }

            logger.printf(PrintType::P_EXTRA, true, "ignoring line \'%s\' before .orig", state.line.c_str());
            cur_pos += 1;
        }

        if(cur_pos == statements.size()) {
            break;
        }

        Statement const & state = statements[cur_pos];

        uint32_t val = state.operands[0].num;
        uint32_t trunc_val = val & 0xffff;
        if(val != trunc_val) {
#ifdef _LIBERAL_ASM
            PrintType p_type = PrintType::P_WARNING;
#else
            PrintType p_type = PrintType::P_ERROR;
#endif
            logger.asmPrintf(p_type, state.operands[0], "truncating address to 0x%0.4x", trunc_val);
            logger.newline();
#ifndef _LIBERAL_ASM
            throw utils::exception("could not find valid .orig");
#endif
        }

        cur_pc = trunc_val;
        found_orig = true;
    }

    if(! found_orig) {
        logger.printf(PrintType::P_ERROR, true, "could not find valid .orig", cur_pos);
        throw utils::exception("could not find valid .orig");
    }

    if(cur_pos != 0) {
#ifdef _LIBERAL_ASM
        logger.printf(PrintType::P_WARNING, true, "ignoring %d lines before .orig", cur_pos);
#else
        logger.printf(PrintType::P_ERROR, true, "%d invalid lines before .orig", cur_pos);
#endif
        logger.newline();
#ifndef _LIBERAL_ASM
        throw utils::exception("orig is not first line in program");
#endif
    }

    // start at the statement right after the first orig
    cur_pos += 1;

    // once the first valid orig is found, mark the remaining statements
    for(uint32_t i = cur_pos; i < statements.size(); i += 1) {
        Statement & state = statements[i];

        if(cur_pc >= MMIO_START) {
            logger.asmPrintf(PrintType::P_ERROR, 0, state.line.size(), state.label, "no more room in writeable memory");
            logger.newline();
            throw utils::exception("no more room in writeable memory");
        }

        state.pc = cur_pc;
        for(StatementToken & operand : state.operands) {
            operand.pc = cur_pc;
        }

        if(checkIfValidPseudoStatement(state, ".blkw", true)) {
            cur_pc += state.operands[0].num;
        } else if(checkIfValidPseudoStatement(state, ".stringz", true)) {
            cur_pc += state.operands[0].str.size() + 1;
        } else if(checkIfValidPseudoStatement(state, ".orig", true)) {
            uint32_t val = state.operands[0].num;
            uint32_t trunc_val = val & 0xffff;
            if(val != trunc_val) {
#ifdef _LIBERAL_ASM
                PrintType p_type = PrintType::P_WARNING;
#else
                PrintType p_type = PrintType::P_ERROR;
#endif
                logger.asmPrintf(p_type, state.operands[0], "truncating address to 0x%0.4x", trunc_val);
                logger.newline();
#ifndef _LIBERAL_ASM
                throw utils::exception("could not find valid .orig");
#endif
            }

            cur_pc = trunc_val;
        } else {
            if(! state.isLabel()) {
                cur_pc += 1;
            }
        }
    }
}

lc3::optional<uint32_t> Assembler::encodeInstruction(asmbl::Statement const & state, SymbolTable const & symbol_table)
{
    using namespace asmbl;
    using namespace lc3::utils;

    std::string stripped_line = state.line;
    stripped_line.erase(stripped_line.begin(), std::find_if(stripped_line.begin(), stripped_line.end(), [](int c) {
        return ! std::isspace(c);
    }));
    StatementToken const & inst = state.inst_or_pseudo;

    // get candidates
    // first element in pair is the candidate
    // second element is the distance from the candidate
    using Candidate = std::pair<PIInstruction, uint32_t>;

    std::vector<Candidate> candidates = encoder.getInstructionCandidates(state);
    std::sort(std::begin(candidates), std::end(candidates), [](Candidate a, Candidate b) {
        return std::get<1>(a) < std::get<1>(b);
    });

    // there is an exact match iff only one candidate was found and its distance is 0
    // otherwise, list top 3 possibilities
    if(! (candidates.size() == 1 && std::get<1>(candidates[0]) == 0)) {
        if(inst.lev_dist == 0) {
            // instruction matched perfectly, but operands did not
            logger.asmPrintf(PrintType::P_ERROR, inst, "invalid usage of \'%s\' instruction", inst.str.c_str());
        } else {
            // instruction didn't match perfectly
            logger.asmPrintf(PrintType::P_ERROR, inst, "invalid instruction");
        }
        // list out possibilities
        uint32_t count = 0;
        for(auto candidate : candidates) {
            logger.printf(PrintType::P_NOTE, false, "did you mean \'%s\'?",
                std::get<0>(candidate)->toFormatString().c_str());
            count += 1;
            if(count >= 3) {
                break;
            }
        }
        if(candidates.size() > 3) {
            logger.printf(PrintType::P_NOTE, false, "...other possible options hidden");
        }
        logger.newline();
        return {};
    }

    // if we've made it here, we've found a valid instruction
    logger.printf(PrintType::P_EXTRA, true, "%s", stripped_line.c_str());
    optional<uint32_t> encoding = encoder.encodeInstruction(state, std::get<0>(candidates[0]), symbol_table, logger);
    if(encoding) {
        logger.printf(PrintType::P_EXTRA, true, " => 0x%0.4x", *encoding);
        return encoding;
    }

    return {};
}

bool Assembler::checkIfValidPseudoToken(asmbl::StatementToken const & tok, std::string const & check)
{
    using namespace asmbl;

    if(tok.type == TokenType::PSEUDO) {
        std::string temp = tok.str;
        std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
        return temp == check;
    }

    return false;
}

bool Assembler::checkIfValidPseudoStatement(asmbl::Statement const & state, std::string const & check, bool log_enable)
{
    using namespace asmbl;
    using namespace lc3::utils;

    if(! state.isPseudo()) { return false; }
    if(state.inst_or_pseudo.str != check) { return false; }

    std::vector<std::vector<TokenType>> valid_operands = {{TokenType::NUM}};
    if(check == ".stringz") {
        valid_operands = {{TokenType::STRING}};
    } else if(check == ".end") {
        valid_operands = {};
    } else if(check == ".fill") {
        valid_operands[0].emplace_back(TokenType::LABEL);
    }

    if(state.operands.size() != valid_operands.size()) {
        if(log_enable) {
            logger.asmPrintf(PrintType::P_ERROR, state.inst_or_pseudo, "incorrect number of operands");
            logger.newline();
        }
        return false;
    }

    for(uint32_t i = 0; i < valid_operands.size(); i += 1) {
        bool valid = false;
        for(TokenType it : valid_operands[i]) {
            if(state.operands[i].type == it) {
                valid = true;
                break;
            }
        }

        if(! valid) {
            if(log_enable) {
                logger.asmPrintf(PrintType::P_ERROR, state.operands[i], "invalid operand");
                logger.newline();
            }
            return false;
        }
    }

    return true;
}
*/
