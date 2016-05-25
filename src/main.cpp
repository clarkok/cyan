#include <iostream>

#include "rlutil/rlutil.h"
#include "libcyan.hpp"
#include "codegen_x64.hpp"

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
        {"-e <IR|X64>", "emit IR code or assembly"},
        {"-h",          "print this help"},
        {"-o <file>",   "define output file"},
        {"-O0",         "no optimization"},
        {"-O1",         "basic optimization"},
        {"-O2",         "normal optimization"},
        {"-O3",         "full optimization"},
        {"-v",          "show version"},
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

private:
    Config()
        : error_collector(dynamic_cast<ErrorCollector*>(
                              ChainErrorCollector::Builder()
                                  .addCollector(new LimitErrorCollector(10))
                                  .addCollector(new ScreenOutputErrorCollector())
                                  .release()
                          )),
          output_file("a.out"),
          optimize_level(2),
          emit_code("X64"),
          parser(new Parser(error_collector))
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

        return ret;
    }
};

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
    switch (config->optimize_level) {
        case 0: ir.reset(OptimizerLevel0(ir.release()).release()); break;
        case 1: ir.reset(OptimizerLevel1(ir.release()).release()); break;
        case 2: ir.reset(OptimizerLevel2(ir.release()).release()); break;
        case 3: ir.reset(OptimizerLevel3(ir.release()).release()); break;
        default:
            assert(false);
    }

    std::ofstream output(config->output_file);
    if (config->emit_code == "IR") {
        ir->output(output);
    }
    else if (config->emit_code == "X64") {
        CodeGenX64 codegen(ir.release());
        codegen.generate(output);
    }
    else {
        config->error_collector->error(Exception("unknown emitting: " + config->emit_code));
    }

    return 0;
}
