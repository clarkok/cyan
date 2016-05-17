//
// Created by Clarkok on 16/5/3.
//

#ifndef CYAN_IR_HPP
#define CYAN_IR_HPP

#include <map>
#include <list>
#include <set>
#include <iostream>

#include "instruction.hpp"
#include "type.hpp"

namespace cyan {

struct BasicBlock
{
    std::list<std::unique_ptr<Instruction> > inst_list;
    std::string name;
    Instruction *condition;
    BasicBlock *then_block;
    BasicBlock *else_block;

    BasicBlock *dominator;
    std::set<BasicBlock *> preceders;
    BasicBlock *loop_header;
    int depth;

    BasicBlock(std::string name, int depth = 0)
        : name(name),
          condition(nullptr),
          then_block(nullptr),
          else_block(nullptr),
          dominator(nullptr),
          loop_header(nullptr),
          depth(depth)
    { }

    ~BasicBlock() = default;

    inline auto
    begin() -> decltype(inst_list.begin())
    { return inst_list.begin(); }

    inline auto
    end() -> decltype(inst_list.end())
    { return inst_list.end(); }

    inline auto
    cbegin() const -> decltype(inst_list.cbegin())
    { return inst_list.cbegin(); }

    inline auto
    cend() const -> decltype(inst_list.cend())
    { return inst_list.cend(); }

    inline auto
    size() const -> decltype(inst_list.size())
    { return inst_list.size(); }

    inline BasicBlock *
    getDominator() const
    { return dominator; }

    inline Instruction *
    getCondition() const
    { return condition; }

    inline BasicBlock *
    getThenBlock() const
    { return then_block; }

    inline BasicBlock *
    getElseBlock() const
    { return else_block; }

    inline std::string
    getName() const
    { return name; }

    inline int
    getDepth() const
    { return depth; }

    inline BasicBlock *
    split(std::list<std::unique_ptr<Instruction> >::iterator iterator, std::string name = "")
    {
        BasicBlock *new_bb = new BasicBlock(name.size() ? name : getName() + ".split", depth);

        new_bb->inst_list.splice(
            new_bb->inst_list.begin(),
            inst_list,
            std::next(iterator),
            inst_list.cend()
        );

        new_bb->condition = condition;
        new_bb->then_block = then_block;
        new_bb->else_block = else_block;

        condition = nullptr;
        then_block = new_bb;
        else_block = nullptr;

        return new_bb;
    }

    std::ostream &output(std::ostream &os) const;
};

struct Function
{
    std::list<std::unique_ptr<BasicBlock> > block_list;
    std::string name;
    FunctionType *prototype;
    int local_temp_counter;
    std::set<std::string> local_names;

    Function(std::string name, FunctionType *prototype)
        : name(name), prototype(prototype), local_temp_counter(0)
    { }

    ~Function() = default;

    inline auto
    begin() -> decltype(block_list.begin())
    { return block_list.begin(); }

    inline auto
    end() -> decltype(block_list.end())
    { return block_list.end(); }

    inline auto
    begin() const -> decltype(block_list.begin())
    { return block_list.begin(); }

    inline auto
    end() const -> decltype(block_list.end())
    { return block_list.end(); }

    inline auto
    cbegin() const -> decltype(block_list.cbegin())
    { return block_list.cbegin(); }

    inline auto
    cend() const -> decltype(block_list.cend())
    { return block_list.cend(); }

    inline auto
    size() const -> decltype(block_list.size())
    { return block_list.size(); }

    inline std::string
    getName() const
    { return name; }

    inline FunctionType *
    getPrototype() const
    { return prototype; }

    inline int
    countLocalTemp()
    { return local_temp_counter++; }

    inline std::string
    makeName(std::string variable_name)
    {
        if (local_names.find(variable_name) != local_names.end()) {
            variable_name += "_" + std::to_string(countLocalTemp());
        }
        local_names.insert(variable_name);
        return variable_name;
    }

    inline size_t
    inst_size() const
    {
        size_t ret = 0;
        for (auto &block : *this) {
            ret += block->size();
        }
        return ret;
    }

    std::ostream &output(std::ostream &os) const;
};

struct IR
{
    std::map<std::string, std::unique_ptr<Function> > function_table;
    std::map<std::string, Type *> global_defines;
    std::unique_ptr<TypePool> type_pool;

    IR() = default;
    ~IR() = default;

    std::ostream &output(std::ostream &os) const;
};

}

#endif //CYAN_IR_HPP
