/*
 * 32/64 bit Fowler/Noll/Vo FNV-1a hash
 * http://www.isthe.com/chongo/tech/comp/fnv/index.html
 */

#ifndef FNV1A_H
#define FNV1A_H

#include <stddef.h>
#include <stdint.h>

#if SIZE_MAX == UINT64_MAX
    #define FNV1A_INIT    14695981039346656037U
    #define FNV1A_MUL(n)  n + (n << 1) + (n << 4) + (n << 5) + (n << 7) + (n << 8) + (n << 40)
#elif SIZE_MAX == UINT32_MAX
    #define FNV1A_INIT    2166136261U
    #define FNV1A_MUL(n)  n + (n << 1) + (n << 4) + (n << 7) + (n << 8) + (n << 24)
#else
    #error "Unsupported size_t bit width"
#endif

static inline size_t fnv1a_buf(void *buf, size_t size) {
    unsigned char *b = buf;
    size_t h = FNV1A_INIT;

    for (size_t i = 0; i < size; ++i) {
        h ^= *b++;
        h = FNV1A_MUL(h);
    }

    return h;
}

static inline size_t fnv1a_str(char *str) {
    unsigned char *s = (unsigned char *)str;
    size_t h = FNV1A_INIT;

    while (*s) {
        h ^= *s++;
        h = FNV1A_MUL(h);
    }

    return h;
}

#endif /* FNV1A_H */
