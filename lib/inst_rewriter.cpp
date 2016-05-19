//
// Created by c on 5/17/16.
//

#include "inst_rewriter.hpp"

using namespace cyan;

void
InstRewriter::rewriteFunction(Function *func)
{
    block_result.clear();
    for (auto &block_ptr : func->block_list) {
        rewriteBlock(func, block_ptr.get());
    }
}

void
InstRewriter::rewriteBlock(Function *func, BasicBlock *block)
{
    if (block_result.find(block) != block_result.end()) { return; }
    if (block->dominator) { rewriteBlock(func, block->dominator); }

    block_result.emplace(
        block,
        std::unique_ptr<InternalResultSet>(new InternalResultSet())
    );

    for (
        auto inst_iter = block->inst_list.begin();
        inst_iter != block->inst_list.end();
    ) {
        if ((*inst_iter)->is<SignedImmInst>()) {
            auto value = (*inst_iter)->to<SignedImmInst>()->getValue();
            if (imm_map.find(value) == imm_map.end()) {
                imm_map.emplace(value, inst_iter->get());
                if (block != func->block_list.front().get()) {
                    (*inst_iter)->setOwnerBlock(func->block_list.front().get());
                    func->block_list.front()->inst_list.emplace_front(inst_iter->release());
                    inst_iter = block->inst_list.erase(inst_iter);
                }
                else {
                    inst_iter++;
                }
            }
            else {
                value_map.emplace(inst_iter->get(), imm_map.at(value));
                inst_iter = block->inst_list.erase(inst_iter);
            }
            continue;
        }
        else if ((*inst_iter)->is<UnsignedImmInst>()) {
            auto value = static_cast<intptr_t>(
                (*inst_iter)->to<UnsignedImmInst>()->getValue()
            );
            if (imm_map.find(value) == imm_map.end()) {
                imm_map.emplace(value, inst_iter->get());
                if (block != func->block_list.front().get()) {
                    (*inst_iter)->setOwnerBlock(func->block_list.front().get());
                    func->block_list.front()->inst_list.emplace_front(inst_iter->release());
                    inst_iter = block->inst_list.erase(inst_iter);
                }
                else {
                    inst_iter++;
                }
            }
            else {
                value_map.emplace(inst_iter->get(), imm_map.at(value));
                inst_iter = block->inst_list.erase(inst_iter);
            }
            continue;
        }
        else if ((*inst_iter)->is<BinaryInst>()) {
            auto binary_inst = (*inst_iter)->to<BinaryInst>();
            binary_inst->resolve(value_map);
            if (
                (binary_inst->getLeft()->is<SignedImmInst>() ||
                 binary_inst->getLeft()->is<UnsignedImmInst>()) &&
                (binary_inst->getRight()->is<SignedImmInst>() ||
                 binary_inst->getRight()->is<UnsignedImmInst>())
            ) {
                Instruction *result_inst = calculateConstant(func, binary_inst);
                value_map.emplace(binary_inst, result_inst);
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            auto calculated = findCalculated(block, binary_inst);
            if (calculated) {
                value_map.emplace(binary_inst, calculated);
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            if (
                binary_inst->getLeft()->getOwnerBlock()->loop_header
                    != block->loop_header &&
                binary_inst->getRight()->getOwnerBlock()->loop_header
                    != block->loop_header
            ) {
                auto loop_header = binary_inst->getOwnerBlock()->loop_header;
                while (
                    loop_header->getDepth() > 1 &&
                    binary_inst->getLeft()->getOwnerBlock()->loop_header != 
                        loop_header->dominator->loop_header &&
                    binary_inst->getRight()->getOwnerBlock()->loop_header !=
                        loop_header->dominator->loop_header
                ) {
                    loop_header = loop_header->loop_header;
                }
                assert(loop_header);
                auto dst = loop_header->dominator;
                (*inst_iter)->setOwnerBlock(dst);
                dst->inst_list.emplace_back(inst_iter->release());
                inst_iter = block->inst_list.erase(inst_iter);
                registerResult(dst, binary_inst);
                continue;
            }

            registerResult(block, binary_inst);
            ++inst_iter;
            continue;
        }

        (*inst_iter)->resolve(value_map);
        ++inst_iter;
    }
}

Instruction *
InstRewriter::calculateConstant(Function *func, BinaryInst *inst)
{
    if (inst->getLeft()->is<UnsignedImmInst>() && inst->getRight()->is<UnsignedImmInst>()) {
        uintptr_t left = inst->getLeft()->to<UnsignedImmInst>()->getValue();
        uintptr_t right = inst->getRight()->to<UnsignedImmInst>()->getValue();

        uintptr_t result = 0;
        if (inst->is<AddInst>()) {
            result = left + right;
        }
        else if (inst->is<SubInst>()) {
            result = left - right;
        }
        else if (inst->is<MulInst>()) {
            result = left * right;
        }
        else if (inst->is<DivInst>()) {
            result = left / right;
        }
        else if (inst->is<ModInst>()) {
            result = left % right;
        }
        else if (inst->is<ShlInst>()) {
            result = left << right;
        }
        else if (inst->is<ShrInst>()) {
            result = left >> right;
        }
        else if (inst->is<OrInst>()) {
            result = left | right;
        }
        else if (inst->is<AndInst>()) {
            result = left & right;
        }
        else if (inst->is<NorInst>()) {
            result = ~(left | right);
        }
        else if (inst->is<XorInst>()) {
            result = left ^ right;
        }
        else if (inst->is<SeqInst>()) {
            result = (left == right) ? 1 : 0;
        }
        else if (inst->is<SltInst>()) {
            result = (left < right) ? 1 : 0;
        }
        else if (inst->is<SleInst>()) {
            result = (left <= right) ? 1 : 0;
        }
        else {
            assert(false);
        }
        if (imm_map.find(static_cast<intptr_t>(result)) == imm_map.end()) {
            auto imm_inst = new UnsignedImmInst(
                ir->type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
                result,
                func->block_list.front().get(),
                "_" + std::to_string(func->countLocalTemp())
            );
            func->block_list.front()->inst_list.emplace_front(imm_inst);
            imm_map.emplace(static_cast<intptr_t>(result), imm_inst);
        }
        return imm_map.at(static_cast<intptr_t>(result));
    }
    else {
        intptr_t left = inst->getLeft()->is<SignedImmInst>()
            ? inst->getLeft()->to<SignedImmInst>()->getValue()
            : static_cast<intptr_t>(inst->getLeft()->to<UnsignedImmInst>()->getValue());
        intptr_t right = inst->getRight()->is<SignedImmInst>()
            ? inst->getRight()->to<SignedImmInst>()->getValue()
            : static_cast<intptr_t>(inst->getRight()->to<UnsignedImmInst>()->getValue());

        intptr_t result = 0;
        if (inst->is<AddInst>()) {
            result = left + right;
        }
        else if (inst->is<SubInst>()) {
            result = left - right;
        }
        else if (inst->is<MulInst>()) {
            result = left * right;
        }
        else if (inst->is<DivInst>()) {
            result = left / right;
        }
        else if (inst->is<ModInst>()) {
            result = left % right;
        }
        else if (inst->is<ShlInst>()) {
            result = left << right;
        }
        else if (inst->is<ShrInst>()) {
            result = left >> right;
        }
        else if (inst->is<OrInst>()) {
            result = left | right;
        }
        else if (inst->is<AndInst>()) {
            result = left & right;
        }
        else if (inst->is<NorInst>()) {
            result = ~(left | right);
        }
        else if (inst->is<XorInst>()) {
            result = left ^ right;
        }
        else if (inst->is<SeqInst>()) {
            result = (left == right);
        }
        else if (inst->is<SltInst>()) {
            result = (left < right);
        }
        else if (inst->is<SleInst>()) {
            result = (left <= right);
        }
        else {
            assert(false);
        }
        if (imm_map.find(result) == imm_map.end()) {
            auto imm_inst = new SignedImmInst(
                ir->type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                result,
                func->block_list.front().get(),
                "_" + std::to_string(func->countLocalTemp())
            );
            func->block_list.front()->inst_list.emplace_front(imm_inst);
            imm_map.emplace(result, imm_inst);
        }
        return imm_map.at(result);
    }
}

Instruction *
InstRewriter::findCalculated(BasicBlock *block, BinaryInst *inst)
{
#define find(Op)    \
    find##Op##Result(block, inst->getLeft(), inst->getRight())

    if      (inst->is<AddInst>()) { return find(Add); }
    else if (inst->is<SubInst>()) { return find(Sub); }
    else if (inst->is<MulInst>()) { return find(Mul); }
    else if (inst->is<DivInst>()) { return find(Div); }
    else if (inst->is<ModInst>()) { return find(Mod); }
    else if (inst->is<ShlInst>()) { return find(Shl); }
    else if (inst->is<ShrInst>()) { return find(Shr); }
    else if (inst->is<OrInst >()) { return find(Or) ; }
    else if (inst->is<AndInst>()) { return find(And); }
    else if (inst->is<NorInst>()) { return find(Nor); }
    else if (inst->is<XorInst>()) { return find(Xor); }
    else if (inst->is<SeqInst>()) { return find(Seq); }
    else if (inst->is<SltInst>()) { return find(Slt); }
    else if (inst->is<SleInst>()) { return find(Sle); }
    else { assert(false); }

#undef find
}

void
InstRewriter::registerResult(BasicBlock *block, BinaryInst *inst)
{
#define result(result_set)                                  \
    block_result.at(block)->result_set.emplace(             \
        std::make_pair(inst->getLeft(), inst->getRight()),  \
        inst                                                \
    )

    if      (inst->is<AddInst>()) { result(add_results); }
    else if (inst->is<SubInst>()) { result(sub_results); }
    else if (inst->is<MulInst>()) { result(mul_results); }
    else if (inst->is<DivInst>()) { result(div_results); }
    else if (inst->is<ModInst>()) { result(mod_results); }
    else if (inst->is<ShlInst>()) { result(shl_results); }
    else if (inst->is<ShrInst>()) { result(shr_results); }
    else if (inst->is<OrInst >()) { result(or_results ); }
    else if (inst->is<AndInst>()) { result(and_results); }
    else if (inst->is<NorInst>()) { result(nor_results); }
    else if (inst->is<XorInst>()) { result(xor_results); }
    else if (inst->is<SeqInst>()) { result(seq_results); }
    else if (inst->is<SltInst>()) { result(slt_results); }
    else if (inst->is<SleInst>()) { result(sle_results); }
    else { assert(false); }

#undef result
}

#define impl_find_method(method, result)                                                        \
    Instruction *                                                                               \
    InstRewriter::method(BasicBlock *block, Instruction *left, Instruction *right) const        \
    {                                                                                           \
        auto original = block;                                                                  \
        auto value = std::make_pair(left, right);                                               \
        Instruction *ret = nullptr;                                                             \
        while (block) {                                                                         \
            auto internal_result_set = block_result.at(block).get();                            \
            if (internal_result_set->result.find(value) != internal_result_set->result.end()) { \
                ret = internal_result_set->result.at(value);                                    \
                break;                                                                          \
            }                                                                                   \
            block = block->dominator;                                                           \
        }                                                                                       \
        if (!ret) return nullptr;                                                               \
        while (original != block) {                                                             \
            block_result.at(original)->result.emplace(value, ret);                              \
            original = original->dominator;                                                     \
        }                                                                                       \
        return ret;                                                                             \
    }

impl_find_method(findAddResult, add_results)
impl_find_method(findSubResult, sub_results)
impl_find_method(findMulResult, mul_results)
impl_find_method(findDivResult, div_results)
impl_find_method(findModResult, mod_results)
impl_find_method(findShlResult, shl_results)
impl_find_method(findShrResult, shr_results)
impl_find_method(findOrResult,  or_results )
impl_find_method(findAndResult, and_results)
impl_find_method(findNorResult, nor_results)
impl_find_method(findXorResult, xor_results)
impl_find_method(findSeqResult, seq_results)
impl_find_method(findSltResult, slt_results)
impl_find_method(findSleResult, sle_results)

#undef impl_find_method
