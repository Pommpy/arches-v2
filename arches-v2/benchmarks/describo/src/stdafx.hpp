#pragma once

#if defined __x86_64__ || defined _M_X64
#define ARCH_X86
#else
#define ARCH_RISCV
#endif

#ifdef ARCH_RISCV
#define INLINE __attribute__((always_inline))
//#define INLINE inline
#endif

#ifdef ARCH_X86
#define INLINE inline
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#endif

#include <algorithm>
#include <cfloat>
#include <cstdint>

typedef unsigned int uint;

//Courtesy of Cem Yuksel
template <bool ascending = true, typename T>
inline void _fast_sort4(T const in[4], T out[4])
{
	T n01 = std::min(in[1], in[0]);
	T x01 = std::max(in[0], in[1]);
	T n23 = std::min(in[2], in[3]);
	T x23 = std::max(in[3], in[2]);
	T r0 = std::min(n01, n23);
	T x02 = std::max(n23, n01);
	T n13 = std::min(x01, x23);
	T r3 = std::max(x23, x01);
	T r1 = std::min(x02, n13);
	T r2 = std::max(n13, x02);
	if(ascending) { out[0] = r0; out[1] = r1; out[2] = r2; out[3] = r3; }
	else { out[0] = r3; out[1] = r2; out[2] = r1; out[3] = r0; }
}

inline float fast_sqrtf(float input)
{
	#ifdef ARCH_X86
	return sqrtf(input);
	#endif

	#ifdef ARCH_RISCV
	float output = 0.0f;
	asm volatile ("fsqrt.s %[rd], %[rs1]\n\t" : [rd] "=f" (output) : [rs1] "f" (input));
	return output;
	#endif

	return 0;
}

inline float fast_inv_sqrtf(float input)
{
	#ifdef ARCH_X86
	return 1.0f / fast_sqrtf(input);
	#endif

	#ifdef ARCH_RISCV
	float output = 0.0f;
	asm volatile ("traxinvsqrt %[rd], %[rs1]\n\t" : [rd] "=f" (output) : [rs1] "f" (input));
	return output;
	#endif

	return 0;
}

inline uint32_t as_u32(float f)
{
	return *((uint32_t*)&f);
}

inline float as_f32(uint32_t u)
{
	return *((float*)&u);
}

//https://martinfullerblog.wordpress.com/2023/01/15/fast-near-lossless-compression-of-normal-floats/
//careful to ensure correct behaviour for normal numbers < 1.0 which roundup to 2.0 when one is added
inline uint32_t f32_to_u24(float f)
{
	//f += 1.0;
	//return (f >= 2.0) ? (1 << 23) : as_u32(f) & ((1 << 23) - 1);
	f = f * 0.5f + 1.5f;
	return (f >= 2.0) ? (1 << 23) : as_u32(f) & ((1 << 23) - 1);
}

//input needs to be low 24 bits, with 0 in the top 8 bits
inline float u24_to_f32(uint32_t u)
{
	return (u & (1 << 23)) ? 1.0 : as_f32(u | 0x3f800000) * 2.0f - 3.0f;
}

inline void __ebreak()
{
	#ifdef ARCH_RISCV
	asm volatile
	(
		"ebreak\n\t"
	);
	#endif
	return;
}