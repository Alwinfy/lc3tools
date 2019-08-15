#include <algorithm>
#include <sstream>

#include "instruction_encoder.h"

using namespace lc3::core::asmbl;

InstructionEncoder::InstructionEncoder(lc3::utils::AssemblerLogger & logger) : InstructionHandler(), logger(logger)
{
    for(PIInstruction inst : instructions) {
        instructions_by_name[inst->name].push_back(inst);
    }
}

bool InstructionEncoder::isStringPseudo(std::string const & search) const
{
    return search.size() > 0 && search[0] == '.';
}

bool InstructionEncoder::isPseudo(StatementNew const & statement) const
{
    return statement.base && statement.base->type == StatementPiece::Type::PSEUDO;
}

bool InstructionEncoder::isInst(StatementNew const & statement) const
{
    return statement.base && statement.base->type == StatementPiece::Type::INST;
}

bool InstructionEncoder::isStringValidReg(std::string const & search) const
{
    std::string lower_search = search;
    std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), ::tolower);
    return regs.find(lower_search) != regs.end();
}

bool InstructionEncoder::isValidPseudoOrig(StatementNew const & statement, bool log_enable) const
{
    if(isPseudo(statement) && utils::toLower(statement.base->str) == ".orig") {
        return validatePseudoOperands(statement, ".orig", {StatementPiece::Type::NUM}, 1, log_enable);
    }
    return false;
}

bool InstructionEncoder::isValidPseudoFill(StatementNew const & statement, bool log_enable) const
{
    if(isPseudo(statement) && utils::toLower(statement.base->str) == ".fill") {
        return validatePseudoOperands(statement, ".fill", {StatementPiece::Type::NUM, StatementPiece::Type::STRING}, 1,
            log_enable);
    }
    return false;
}

bool InstructionEncoder::isValidPseudoFill(StatementNew const & statement, SymbolTable const & symbols,
    bool log_enable) const
{
    using namespace lc3::utils;

    if(isValidPseudoFill(statement, log_enable)) {
        if(statement.operands[0].type == StatementPiece::Type::STRING &&
            symbols.find(statement.operands[0].str) == symbols.end())
        {
            if(log_enable) {
                logger.asmPrintf(PrintType::P_ERROR, statement, statement.operands[0],
                    "could not find label");
                logger.newline();
            }
            return false;
        }
        return true;
    }
    return false;
}

bool InstructionEncoder::isValidPseudoBlock(StatementNew const & statement, bool log_enable) const
{
    if(isPseudo(statement) && utils::toLower(statement.base->str) == ".blkw") {
        bool valid = validatePseudoOperands(statement, ".blkw", {StatementPiece::Type::NUM}, 1, log_enable);
        if(valid) {
            if(log_enable && statement.operands[0].num == 0) {
                logger.asmPrintf(utils::PrintType::P_ERROR, statement, statement.operands[0],
                    "operand to .blkw must be > 0");
                logger.newline();
            }
            return statement.operands[0].num != 0;
        }
    }
    return false;
}

bool InstructionEncoder::isValidPseudoString(StatementNew const & statement, bool log_enable) const
{
    if(isPseudo(statement) && utils::toLower(statement.base->str) == ".stringz") {
        return validatePseudoOperands(statement, ".stringz", {StatementPiece::Type::STRING}, 1, log_enable);
    }
    return false;
}

bool InstructionEncoder::isValidPseudoEnd(StatementNew const & statement, bool log_enable) const
{
    if(isPseudo(statement) && utils::toLower(statement.base->str) == ".end") {
        return validatePseudoOperands(statement, ".end", {}, 0, log_enable);
    }
    return false;
}

bool InstructionEncoder::validatePseudo(StatementNew const & statement, SymbolTable const & symbols) const
{
    using namespace lc3::utils;

    if(! isPseudo(statement)) { return false; }

    std::string lower_base = utils::toLower(statement.base->str);
    if(lower_base == ".orig") {
        return isValidPseudoOrig(statement, true);
    } else if(lower_base == ".fill") {
        return isValidPseudoFill(statement, symbols, true);
    } else if(lower_base == ".blkw") {
        return isValidPseudoBlock(statement, true);
    } else if(lower_base == ".stringz") {
        return isValidPseudoString(statement, true);
    } else if(lower_base == ".end") {
        return isValidPseudoEnd(statement, true);
    } else {
#ifdef _LIBERAL_ASM
        logger.asmPrintf(PrintType::P_WARNING, statement, *statement.base, "ignoring invalid pseudo-op");
        logger.newline();
        return true;
#else
        logger.asmPrintf(PrintType::P_ERROR, statement, *statement.base, "invalid pseudo-op");
        logger.newline();
        return false;
#endif
    }
}

bool InstructionEncoder::validatePseudoOperands(StatementNew const & statement, std::string const & pseudo,
    std::vector<StatementPiece::Type> const & valid_types, uint32_t operand_count, bool log_enable) const
{
    using namespace lc3::utils;

    if(statement.operands.size() < operand_count) {
        // If there are not enough operands, print out simple error message.
        if(log_enable) {
            logger.asmPrintf(PrintType::P_ERROR, statement, "%s requires %d more operand(s)", pseudo.c_str(),
                operand_count - statement.operands.size());
            logger.newline();
        }
        return false;
    } else if(statement.operands.size() > operand_count) {
        // If there are too many operands, print out a warning/error for each extraneous operand.
        if(log_enable) {
            for(uint32_t i = operand_count; i < statement.operands.size(); i += 1) {
                logger.asmPrintf(PrintType::P_ERROR, statement, statement.operands[i], "extraneous operand to %s",
                    pseudo.c_str());
                logger.newline();
            }
        }
        return false;
    } else {
        // If there are the correct number of operands, confirm that they are of the correct type.
        bool all_valid_types = true;
        for(uint32_t i = 0; i < operand_count; i += 1) {
            bool valid_type = false;
            for(StatementPiece::Type const & type : valid_types) {
                if(statement.operands[i].type == type) {
                    valid_type = true;
                    break;
                }
            }
            if(! valid_type) {
                all_valid_types = false;
                if(log_enable) {
                    // Some nonsense to make pretty messages depending on the expected type.
                    std::stringstream error_msg;
                    error_msg << "operand should be";
                    std::string prefix = "";
                    for(StatementPiece::Type const & type : valid_types) {
                        if(type == StatementPiece::Type::NUM) {
                            error_msg << prefix << " numeric";
                        } else {
                            error_msg << prefix << " a string";
                        }
                        prefix = " or";
                    }
                    logger.asmPrintf(PrintType::P_ERROR, statement, statement.operands[i], "%s",
                        error_msg.str().c_str());
                    logger.newline();
                }
            }
        }
        return all_valid_types;
    }

    return true;
}

lc3::optional<lc3::core::PIInstruction> InstructionEncoder::validateInstruction(StatementNew const & statement,
    SymbolTable const & symbols) const
{
    using namespace lc3::utils;

    if(! isInst(statement)) { return {}; }

    using Candidate = std::tuple<PIInstruction, uint32_t, uint32_t>;
    std::vector<Candidate> candidates;

    std::string statement_op_string = "";
    for(StatementPiece const & statement_op : statement.operands) {
        switch(statement_op.type) {
            case StatementPiece::Type::NUM: statement_op_string += 'n'; break;
            case StatementPiece::Type::STRING: statement_op_string += 's'; break;
            case StatementPiece::Type::REG: statement_op_string += 'r'; break;
            default: break;
        }
    }

    for(auto const & candidate_inst_name : instructions_by_name) {
        uint32_t inst_name_dist = levDistance(statement.base->str, candidate_inst_name.first);
        if(inst_name_dist < 2) {
            for(PIInstruction candidate_inst : candidate_inst_name.second) {
                // Convert the operand types of the candidate and the statement into a string so that Levenshtein
                // distance can be used on the operands too.
                std::string candidate_op_string = "";
                for(PIOperand candidate_op : candidate_inst->operands) {
                    switch(candidate_op->type) {
                        case OperType::NUM: candidate_op_string += 'n'; break;
                        case OperType::LABEL: candidate_op_string += 's'; break;
                        case OperType::REG: candidate_op_string += 'r'; break;
                        default: break;
                    }
                }

                uint32_t op_dist = levDistance(statement_op_string, candidate_op_string);
                candidates.emplace_back(std::make_tuple(candidate_inst, inst_name_dist, op_dist));
            }
        }
    }

    std::sort(std::begin(candidates), std::end(candidates), [](Candidate a, Candidate b) {
        return (std::get<1>(a) + std::get<2>(a)) < (std::get<1>(b) + std::get<2>(b));
    });

    // If there are no candidates or if the instruction itself was not a perfect match, print out unique candidates.
    if(candidates.size() == 0 || std::get<1>(candidates[0]) != 0) {
        logger.asmPrintf(PrintType::P_ERROR, statement, *statement.base, "invalid instruction");
        std::string prev_candidate_inst_name = "";
        uint32_t candidate_count = 0;
        for(Candidate const & candidate : candidates) {
            std::string const & candidate_inst_name = std::get<0>(candidate)->name;
            if(candidate_inst_name != prev_candidate_inst_name) {
                if(candidate_count < 3) {
                    logger.printf(PrintType::P_NOTE, false, "did you mean \'%s\'?", candidate_inst_name.c_str());
                    prev_candidate_inst_name = candidate_inst_name;
                }
                candidate_count += 1;
            }
        }
        if(candidate_count > 3) {
            logger.printf(PrintType::P_NOTE, false, "...or %d other candidate(s) (not shown)", candidate_count - 3);
        }
        logger.newline();
        return {};
    }

    // If the instruction was a match but the operands weren't, print out more detail about candidates for that
    // instruction.
    if(std::get<2>(candidates[0]) != 0) {
        logger.asmPrintf(PrintType::P_ERROR, statement, *statement.base, "invalid usage of \'%s\' instruction",
            statement.base->str.c_str());
        uint32_t candidate_count = 0;
        for(Candidate const & candidate : candidates) {
            if(statement.base->str == std::get<0>(candidate)->name) {
                if(candidate_count < 3) {
                    logger.printf(PrintType::P_NOTE, false, "did you mean \'%s\'?",
                        std::get<0>(candidate)->toFormatString().c_str());
                }
                candidate_count += 1;
            }
        }
        if(candidate_count > 3) {
            logger.printf(PrintType::P_NOTE, false, "...or %d other candidate(s) (not shown)", candidate_count - 3);
        }
        logger.newline();
        return {};
    }

    // Finally, confirm that any string operands are actually valid symbols
    bool valid = true;
    std::cout << "wat\n";
    for(auto x : symbols) {
        std::cout << x.first << ": " << x.second << "\n";
    }
    for(StatementPiece const & operand : statement.operands) {
        if(operand.type == StatementPiece::Type::STRING) {
            if(symbols.find(operand.str) == symbols.end()) {
                logger.asmPrintf(PrintType::P_ERROR, statement, operand, "could not find label");
                logger.newline();
                valid = false;
            }
        }
    }

    if(valid) {
        return std::get<0>(candidates[0]);
    } else {
        return {};
    }
}

uint32_t InstructionEncoder::getNum(StatementNew const & statement, StatementPiece const & piece, bool log_enable) const
{
    uint32_t value = piece.num;

    if(log_enable && ((value & 0xffff) != value)) {
        logger.asmPrintf(utils::PrintType::P_WARNING, statement, piece, "truncating operand to 16-bits");
        logger.newline();
    }

    return value & 0xffff;
}

uint32_t InstructionEncoder::getPseudoOrig(StatementNew const & statement) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoOrig(statement));
#endif
    return getNum(statement, statement.operands[0], true);
}

uint32_t InstructionEncoder::getPseudoFill(StatementNew const & statement, SymbolTable const & symbols) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoFill(statement, symbols));
#endif
    if(statement.operands[0].type == StatementPiece::Type::NUM) {
        return getNum(statement, statement.operands[0], true);
    } else {
        return symbols.at(statement.operands[0].str);
    }
}

uint32_t InstructionEncoder::getPseudoBlockSize(StatementNew const & statement) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoBlock(statement));
#endif
    return getNum(statement, statement.operands[0], true);
}

uint32_t InstructionEncoder::getPseudoStringSize(StatementNew const & statement) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoString(statement));
#endif
    return statement.operands[0].str.size() + 1;
}

std::string const & InstructionEncoder::getPseudoString(StatementNew const & statement) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoString(statement));
#endif
    return statement.operands[0].str;
}

uint32_t InstructionEncoder::encodeInstruction(StatementNew const & statement, SymbolTable const & symbols,
    PIInstruction candidate) const
{
    return 0;
}

uint32_t InstructionEncoder::getDistanceToNearestInstructionName(std::string const & search) const
{
    std::string lower_search = utils::toLower(search);
    uint32_t min_distance = 0;
    bool min_set = false;
    for(auto const & inst : instructions_by_name) {
        uint32_t distance = levDistance(inst.first, lower_search);
        if(! min_set || distance < min_distance) {
            min_distance = distance;
            min_set = true;
        }
    }

    return min_distance;
}

std::vector<std::pair<lc3::core::PIInstruction, uint32_t>> InstructionEncoder::getInstructionCandidates(
    Statement const & state) const
{
    std::vector<std::pair<PIInstruction, uint32_t>> ret;
    StatementToken const & search = state.inst_or_pseudo;

/*
 *    if(search.type == Token::Type::INST) {
 *        for(auto const & inst : instructions_by_name) {
 *            uint32_t inst_dist = levDistance(inst.first, search.str);
 *            if(inst_dist <= search.lev_dist) {
 *                for(PIInstruction inst_pattern : inst.second) {
 *                    std::string op_string, search_string;
 *                    for(PIOperand op : inst_pattern->operands) {
 *                        if(op->type != OperType::FIXED) {
 *                            op_string += '0' + static_cast<char>(op->type);
 *                        }
 *                    }
 *                    for(StatementToken const & op : state.operands) {
 *                        search_string += '0' + static_cast<char>(tokenTypeToOperType(op.type));
 *                    }
 *
 *                    uint32_t op_dist = levDistance(op_string, search_string);
 *                    if(op_dist < 3) {
 *                        if(inst_dist + op_dist == 0) {
 *                            ret.clear();
 *                            ret.push_back(std::make_pair(inst_pattern, inst_dist + op_dist));
 *                            break;
 *                        }
 *
 *                        ret.push_back(std::make_pair(inst_pattern, inst_dist + op_dist));
 *                    }
 *                }
 *            }
 *        }
 *    }
 */

    return ret;
}

lc3::core::OperType InstructionEncoder::tokenTypeToOperType(Token::Type type) const
{
    if(type == Token::Type::NUM) {
        return OperType::NUM;
    /*
     *} else if(type == Token::Type::REG) {
     *    return OperType::REG;
     *} else if(type == Token::Type::LABEL) {
     *    return OperType::LABEL;
     */
    } else {
        return OperType::INVALID;
    }
}

uint32_t InstructionEncoder::levDistance(std::string const & a, std::string const & b) const
{
    return levDistanceHelper(a, a.size(), b, b.size());
}

uint32_t InstructionEncoder::levDistanceHelper(std::string const & a, uint32_t a_len, std::string const & b,
    uint32_t b_len) const
{
    // lazy, redundant recursive version of Levenshtein distance...may use dynamic programming eventually
    if(a_len == 0) { return b_len; }
    if(b_len == 0) { return a_len; }

    uint32_t cost = (a[a_len - 1] == b[b_len - 1]) ? 0 : 1;

    std::array<uint32_t, 3> costs;
    costs[0] = levDistanceHelper(a, a_len - 1, b, b_len    ) + 1;
    costs[1] = levDistanceHelper(a, a_len    , b, b_len - 1) + 1;
    costs[2] = levDistanceHelper(a, a_len - 1, b, b_len - 1) + cost;

    return *std::min_element(std::begin(costs), std::end(costs));
}

lc3::optional<uint32_t> InstructionEncoder::encodeInstruction(Statement const & state, lc3::core::PIInstruction pattern,
    lc3::core::SymbolTable const & symbols, lc3::utils::AssemblerLogger & logger) const
{
    uint32_t encoding = 0;

    uint32_t oper_count = 0;
    bool first = true;

    for(PIOperand op : pattern->operands) {
        StatementToken tok;
        if(op->type == OperType::FIXED) {
            if(first) {
                first = false;
                tok = state.inst_or_pseudo;
            }
        } else {
            tok = state.operands[oper_count];
        }

        encoding <<= op->width;
        try {
            optional<uint32_t> op_encoding = op->encode(tok, oper_count, regs, symbols, logger);
            if(op_encoding) {
                encoding |= *op_encoding;
            } else {
                return {};
            }
        } catch(lc3::utils::exception const & e) {
            return {};
        }

        if(op->type != OperType::FIXED) {
            oper_count += 1;
        }
    }

    return encoding;
}
