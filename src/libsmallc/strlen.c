#include "strlen.h"

unsigned long strlen(const char* s) {
    unsigned long count = 0;
    while(*s) {
        count++;
        s++;
    }
    return count;
}