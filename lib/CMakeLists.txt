set(LIBRARY_FILES cyan.hpp cyan.cpp parse.cpp parse.hpp symbols.cpp symbols.hpp location.hpp type.hpp type.cpp error_collector.cpp error_collector.hpp instruction.cpp instruction.hpp ir.cpp ir.hpp ir_builder.cpp ir_builder.hpp codegen.hpp codegen_x64.cpp codegen_x64.hpp codegen.cpp inliner.cpp dep_analyzer.cpp dep_analyzer.hpp mem2reg.cpp mem2reg.hpp)
add_library(cyan ${LIBRARY_FILES})
