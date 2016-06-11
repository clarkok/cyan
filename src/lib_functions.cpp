#include <iostream>
#include <cstdlib>

#include "lib_functions.hpp"

using namespace cyan;

using Slot = vm::Slot;

namespace {

struct PrintStr : public vm::LibFunction
{
    virtual Slot
    call(const Slot *arguments)
    {
        std::cout << reinterpret_cast<const char *>(arguments[0]);
        std::cout.flush();
        return 0;
    }
};

struct PrintInt : public vm::LibFunction
{
    virtual Slot
    call(const Slot *arguments)
    {
        std::cout << arguments[0];
        return 0;
    }
};

struct Rand : public vm::LibFunction
{
    virtual Slot
    call(const Slot *arguments)
    { return std::rand(); }
};

}

void
cyan::registerLibFunctions(vm::VirtualMachine::Generate *gen)
{
    gen->registerLibFunction("print_str", new PrintStr());
    gen->registerLibFunction("print_int", new PrintInt());
    gen->registerLibFunction("rand", new Rand());
}
