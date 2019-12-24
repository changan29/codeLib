#pragma once

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned long uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif


struct Hash128_t
{
	uint64_t Hd;
	uint64_t Ld;
};

void MurmurHash3_x86_32(const void *key, int len, uint32_t seed, void *out);
void MurmurHash3_x64_128(const void *key, int len, uint32_t seed, void *out);

uint32_t GetMurmurHash3_32(const void *key, int len, uint32_t seed = 9);
Hash128_t GetMurmurHash3_128(const void *key, int len, uint32_t seed = 9);