//
// Created by c on 5/16/16.
//

#include "dep_analyzer.hpp"

using namespace cyan;

BasicBlock *
DepAnalyzer::findDominator(BasicBlock *p1, BasicBlock *p2)
{
    std::set<BasicBlock *> dominators;

    while (p1) {
        dominators.emplace(p1);
        p1 = p1->dominator;
    }

    while (p2) {
        if (dominators.find(p2) != dominators.end()) { return p2; }
        p2 = p2->dominator;
    }

    assert(false);
    return nullptr;
}

void
DepAnalyzer::scanDep(BasicBlock *block)
{
    if (scanned.find(block) != scanned.end()) {
        return;
    }
    scanned.emplace(block);

    if (block->condition) {
        assert(block->then_block);
        assert(block->else_block);

        setPreceder(block->then_block, block);
        scanDep(block->then_block);
        setPreceder(block->else_block, block);
        scanDep(block->else_block);
    }
    else if (block->then_block) {
        setPreceder(block->then_block, block);
        scanDep(block->then_block);
    }
}

void
DepAnalyzer::setPreceder(BasicBlock *block, BasicBlock *preceder)
{
    if (block->dominator) {
        block->dominator = findDominator(block->dominator, preceder);
    }
    else {
        block->dominator = preceder;
    }
    block->preceders.emplace(preceder);
}

void
DepAnalyzer::outputResult(std::ostream &os) const
{
    for (auto &func : ir->function_table) {
        os << func.first << ":" << std::endl;
        for (auto &block : func.second->block_list) {
            os << "\t" << block->getName() << ":" << std::endl;
            os << "\tdominator: " << (block->dominator ? block->dominator->getName() : "(null)")
               << std::endl;
            os << "\tpreceders:" << std::endl;
            for (auto &preceder : block->preceders) {
                os << "\t\t" << preceder->getName() << std::endl;
            }
            os << std::endl;
        }
        os << std::endl;
    }
}
