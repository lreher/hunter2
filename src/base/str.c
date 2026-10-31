#include "base/base.h"

i32 str_len(const char *s) {
    i32 n = 0;
    while (s[n]) n++;
    return n;
}

i32 str_append(char *dst, i32 cap, const char *src) {
    i32 n = str_len(dst);
    for (i32 i = 0; src[i] && n < cap - 1; i++) dst[n++] = src[i];
    dst[n] = 0;
    return n;
}

i32 str_append_i32(char *dst, i32 cap, i32 value) {
    char digits[12];
    i32 count = 0;
    u32 u = value < 0 ? 0u - (u32)value : (u32)value;
    do {
        digits[count++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (value < 0) digits[count++] = '-';

    char text[13];
    i32 len = 0;
    while (count) text[len++] = digits[--count];
    text[len] = 0;
    return str_append(dst, cap, text);
}
