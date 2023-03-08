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

float inline fast_sqrtf(float input)
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

float inline fast_inv_sqrtf(float input)
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
