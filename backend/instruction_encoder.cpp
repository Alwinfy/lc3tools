#include <algorithm>

#include "instruction_encoder.h"

using namespace lc3::core::asmbl;

InstructionEncoder::InstructionEncoder(void) : InstructionHandler()
{
    for(PIInstruction inst : instructions) {
        instructions_by_name[inst->name].push_back(inst);
    }
}

bool InstructionEncoder::isPseudo(std::string const & search) const
{
    return search.size() > 0 && search[0] == '.';
}

bool InstructionEncoder::isValidReg(std::string const & search) const
{
    std::string lower_search = search;
    std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), ::tolower);
    return regs.find(lower_search) != regs.end();
}

bool InstructionEncoder::isValidPseudoOrig(StatementNew const & statement) const
{
    bool success = statement.base && utils::toLower(statement.base->str) == ".orig";
    success &= statement.operands.size() == 1 && statement.operands[0].type == StatementPiece::Type::NUM;
    return success;
}

bool InstructionEncoder::isValidPseudoFill(StatementNew const & statement) const
{
    bool success = statement.base && utils::toLower(statement.base->str) == ".fill";
    success &= statement.operands.size() == 1 && (statement.operands[0].type == StatementPiece::Type::NUM ||
                 statement.operands[0].type == StatementPiece::Type::STRING);
    return success;
}

bool InstructionEncoder::isValidPseudoBlock(StatementNew const & statement) const
{
    bool success = statement.base && utils::toLower(statement.base->str) == ".blkw";
    success &= statement.operands.size() == 1 && statement.operands[0].type == StatementPiece::Type::NUM;
    return success;
}

bool InstructionEncoder::isValidPseudoString(StatementNew const & statement) const
{
    bool success = statement.base && utils::toLower(statement.base->str) == ".stringz";
    success &= statement.operands.size() == 1 && statement.operands[0].type == StatementPiece::Type::STRING;
    return success;
}

bool InstructionEncoder::isValidPseudoEnd(StatementNew const & statement) const
{
    bool success = statement.base && utils::toLower(statement.base->str) == ".end";
    success &= statement.operands.size() == 0;
    return success;
}

uint32_t InstructionEncoder::encodePseudoOrig(StatementNew const & statement) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoOrig(statement));
#endif
    return statement.operands[0].num;
}

uint32_t InstructionEncoder::getPseudoBlockSize(StatementNew const & statement) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoBlock(statement));
#endif
    return statement.operands[0].num;
}

uint32_t InstructionEncoder::getPseudoStringSize(StatementNew const & statement) const
{
#ifdef _ENABLE_DEBUG
    assert(isValidPseudoString(statement));
#endif
    return statement.operands[0].str.size() + 1;
}

uint32_t InstructionEncoder::getDistanceToNearestInstructionName(std::string const & search) const
{
    std::string lower_search = utils::toLower(search);
    uint32_t min_distance = 0;
    bool min_set = false;
    for(auto const & inst : instructions_by_name) {
        uint32_t distance = levDistance(inst.first, lower_search);
        if(! min_set) {
            min_distance = distance;
            min_set = true;
        }
        if(distance < min_distance) {
            min_distance = distance;
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
