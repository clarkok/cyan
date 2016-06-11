#include <iostream>
#include <cstdlib>

#include "rlutil/rlutil.h"
#include "libcyan.hpp"
#include "codegen_x64.hpp"
#include "vm.hpp"
#include "lib_functions.hpp"

using namespace cyan;

struct Exception : public std::exception
{
    std::string _what;

    Exception(std::string what)
        : _what(what)
    { }

    virtual const char *
    what() const noexcept
    { return _what.c_str(); }
};

void
print_help()
{
    static const int OPTIONS_COLUMN_SIZE = 16;
    static const char *OPTIONS_PAIR[][2] = {
        {"-d",                  "output debug info to stderr"},
        {"-e <GCC|IR|X64>",     "pass to GCC or emitting IR code or assembly"},
        {"-h",                  "print this help"},
        {"-o <file>",           "define output file"},
        {"-O0",                 "no optimization"},
        {"-O1",                 "basic optimization"},
        {"-O2",                 "normal optimization"},
        {"-O3",                 "full optimization"},
        {"-r",                  "run the code"},
        {"-v",                  "show version"},
    };

    rlutil::saveDefaultColor();
    rlutil::setColor(rlutil::WHITE);
    std::cout << "cyanc: cyan compiler\n"
                 "    By Clarkok Zhang (mail@clarkok.com)" << std::endl;
    rlutil::resetColor();
    std::cout << "Usage: cyanc [options] file..." << std::endl;
    std::cout << "Options:" << std::endl;
    for (auto &option_pair : OPTIONS_PAIR) {
        rlutil::setColor(rlutil::WHITE);
        std::cout << "  " << option_pair[0];
        for (auto i = std::strlen(option_pair[0]); i < OPTIONS_COLUMN_SIZE; ++i) {
            std::cout << " ";
        }
        rlutil::resetColor();
        std::cout << option_pair[1] << std::endl;
    }
}

struct Config
{
    ~Config() = default;

    ErrorCollector *error_collector;
    std::string output_file;
    int optimize_level;
    std::string emit_code;
    std::vector<std::string> input_files;
    std::unique_ptr<Parser> parser;
    bool run;
    bool debug_out;

private:
    Config()
        : error_collector(dynamic_cast<ErrorCollector*>(
                              ChainErrorCollector::Builder()
                                  .addCollector(new LimitErrorCollector(10))
                                  .addCollector(new ScreenOutputErrorCollector())
                                  .release()
                          )),
          output_file(),
          optimize_level(2),
          emit_code("GCC"),
          parser(new Parser(error_collector)),
          run(false),
          debug_out(false)
    { }

public:
    static std::unique_ptr<Config>
    factory(int argc, const char **argv)
    {
        std::unique_ptr<Config> ret(new Config());

        --argc; ++argv;
        while (argc) {
            if ((*argv)[0] == '-') {
                switch ((*argv)[1]) {
                    case 'o':
                        --argc; ++argv;
                        ret->output_file = *argv;
                        break;
                    case 'e':
                        --argc; ++argv;
                        ret->emit_code = *argv;
                        for (auto &ch : ret->emit_code) {
                            ch = std::toupper(ch);
                        }
                        break;
                    case 'O':
                    {
                        switch ((*argv)[2]) {
                            case '0':
                                ret->optimize_level = 0;
                                break;
                            case '1':
                                ret->optimize_level = 1;
                                break;
                            case '2':
                                ret->optimize_level = 2;
                                break;
                            case '3':
                                ret->optimize_level = 3;
                                break;
                            default:
                                ret->error_collector->error(
                                    Exception(std::string("unknown optimization level: ") + *argv)
                                );
                                print_help();
                                exit(-1);
                        }
                        break;
                    }
                    case 'h':
                        print_help();
                        exit(0);
                    case 'v':
                        std::cout << CYAN_VERSION << std::endl;
                        exit(0);
                    case 'r':
                        ret->run = true;
                        break;
                    case 'd':
                        ret->debug_out = true;
                        break;
                    default:
                        ret->error_collector->error(
                            Exception(std::string("unknown option: ") + *argv)
                        );
                        print_help();
                        exit(-1);
                }
            }
            else {
                ret->input_files.emplace_back(*argv);
            }
            --argc; ++argv;
        }

        if (ret->input_files.empty()) {
            ret->error_collector->error(
                Exception(std::string("no input files"))
            );
            print_help();
            exit(-1);
        }

        if (ret->output_file.length() == 0) {
            if (ret->emit_code == "X64") {
                ret->output_file = "a.s";
            }
            else if (ret->emit_code == "IR") {
                ret->output_file = "a.ir";
            }
            else if (ret->emit_code == "GCC") {
                ret->output_file = "a.out";
            }
            else {
                assert(false);
            }
        }

        return ret;
    }
};

std::string
get_env(std::string name)
{
    auto var = std::getenv(name.c_str());
    return var ? var : "";
}

int
main(int argc, const char **argv)
{
    auto config = Config::factory(argc, argv);

    for (auto &input_file : config->input_files) {
        if (!config->parser->parseFile(input_file.c_str())) {
            exit(-1);
        }
    }

    auto ir = config->parser->release();
    if (config->debug_out) {
        switch (config->optimize_level) {
            case 0: ir.reset(DebugOptimizerLevel0(ir.release()).release()); break;
            case 1: ir.reset(DebugOptimizerLevel1(ir.release()).release()); break;
            case 2: ir.reset(DebugOptimizerLevel2(ir.release()).release()); break;
            case 3: ir.reset(DebugOptimizerLevel3(ir.release()).release()); break;
            default:
                assert(false);
        }
    }
    else {
        switch (config->optimize_level) {
            case 0: ir.reset(OptimizerLevel0(ir.release()).release()); break;
            case 1: ir.reset(OptimizerLevel1(ir.release()).release()); break;
            case 2: ir.reset(OptimizerLevel2(ir.release()).release()); break;
            case 3: ir.reset(OptimizerLevel3(ir.release()).release()); break;
            default:
                assert(false);
        }
    }

    if (config->run) {
        auto gen = vm::VirtualMachine::GenerateFactory(ir.release());
        registerLibFunctions(gen.get());
        gen->generate();
        auto vm = gen->release();
        auto ret_val = vm->start();

        std::cout << "exit with code " << ret_val << std::endl;
        return ret_val;
    }

    if (config->emit_code == "IR") {
        std::ofstream output(config->output_file);
        ir->output(output);
    }
    else if (config->emit_code == "X64") {
        std::ofstream output(config->output_file);
        CodeGenX64 codegen(ir.release());
        codegen.generate(output);
    }
    else if (config->emit_code == "GCC") {
        auto temp_name = ".__temp_asm__" + config->output_file + ".s";
        auto runtime_path = get_env("CYAN_RUNTIME_DIR");
        if (!runtime_path.length()) {
            config->error_collector->error(Exception("Cannot find runtime lib in $CYAN_RUNTIME_DIR"));
            exit(-1);
        }

        std::ofstream output(temp_name);
        CodeGenX64 codegen(ir.release());
        codegen.generate(output);

        std::system(("gcc -m64 -o " + config->output_file + " " + temp_name + " " + runtime_path).c_str());
        std::system(("rm " + temp_name).c_str());
    }
    else {
        config->error_collector->error(Exception("unknown emitting: " + config->emit_code));
    }

    return 0;
}
