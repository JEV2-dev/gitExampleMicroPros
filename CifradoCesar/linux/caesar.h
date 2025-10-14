#ifndef CAESAR_H
#define CAESAR_H
#include <stddef.h>
#include <stdint.h>

// Aplica rotaciones separadas a letras (A-Z, a-z) mod 26 y dígitos (0-9) mod 10.
// No toca otros símbolos. shift debe estar en [0,25] para letras; en dígitos se toma shift % 10.
void caesar_encrypt(const char *in, char *out, size_t n, uint8_t shift);
void caesar_decrypt(const char *in, char *out, size_t n, uint8_t shift);

#endif
