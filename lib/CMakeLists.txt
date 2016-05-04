set(LIBRARY_FILES cyan.hpp cyan.cpp parse.cpp parse.hpp symbols.cpp symbols.hpp location.hpp type.hpp type.cpp error_collector.cpp error_collector.hpp instrument.cpp instrument.hpp ir.cpp ir.hpp ir_builder.cpp ir_builder.hpp)
add_library(cyan ${LIBRARY_FILES})
