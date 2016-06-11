#ifndef CYAN_CYAN_HPP
#define CYAN_CYAN_HPP

#include <cstddef>

#if __x86_64__ || __ppc64__
#define __CYAN_64__     1
#else
#define __CYAN_64__     0
#endif

static const char CYAN_VERSION[] = "v0.0.1";
#if __CYAN_64__
static const size_t CYAN_PRODUCT_BITS = 64;
#else
static const size_t CYAN_PRODUCT_BITS = 32;
#endif
static const size_t CYAN_CHAR_BITS = 8;
static const size_t CYAN_PRODUCT_BYTES = CYAN_PRODUCT_BITS / CYAN_CHAR_BITS;
static const size_t CYAN_PRODUCT_ALIGN = CYAN_PRODUCT_BYTES;

#endif //CYAN_CYAN_HPP
