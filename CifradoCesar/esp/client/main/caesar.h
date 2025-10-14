#pragma once
#include <stddef.h>
#include <stdint.h>
void caesar_encrypt(const char *in, char *out, size_t n, uint8_t shift);
void caesar_decrypt(const char *in, char *out, size_t n, uint8_t shift);
