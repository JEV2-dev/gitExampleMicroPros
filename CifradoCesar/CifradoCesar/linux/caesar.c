#include "caesar.h"
#include <ctype.h>

static char rot_alpha(char c, int k) {
    if ('A' <= c && c <= 'Z') return (char)('A' + (c - 'A' + k) % 26);
    if ('a' <= c && c <= 'z') return (char)('a' + (c - 'a' + k) % 26);
    return c;
}
static char rot_digit(char c, int k) {
    if ('0' <= c && c <= '9') return (char)('0' + (c - '0' + (k % 10)) % 10);
    return c;
}

void caesar_encrypt(const char *in, char *out, size_t n, uint8_t shift) {
    for (size_t i = 0; i < n; ++i) {
        char c = in[i];
        char d = rot_alpha(c, shift);
        if (d == c) d = rot_digit(c, shift);
        out[i] = d;
    }
}

void caesar_decrypt(const char *in, char *out, size_t n, uint8_t shift) {
    // invertir rotacion (26 - shift) y (10 - shift%10)
    uint8_t s26 = (26 - (shift % 26)) % 26;
    uint8_t s10 = (10 - (shift % 10)) % 10;
    for (size_t i = 0; i < n; ++i) {
        char c = in[i];
        // revertir en el orden inverso da igual aquí (mutuamente excluyentes)
        char d = c;
        if ('A' <= c && c <= 'Z') d = (char)('A' + (c - 'A' + s26) % 26);
        else if ('a' <= c && c <= 'z') d = (char)('a' + (c - 'a' + s26) % 26);
        else if ('0' <= c && c <= '9') d = (char)('0' + (c - '0' + s10) % 10);
        out[i] = d;
    }
}
