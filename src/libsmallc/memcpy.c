#include "memcpy.h"

void* memcpy(void* dst, const void* src, unsigned long count) {
    char* _d = (char *)dst;
    const char* _s = (const char *)src;
    while(_d && _s && count) {
        *(_d++) = *(_s++);
        --count;
    }
    return dst;
}