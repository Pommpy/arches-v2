#pragma once

#ifndef __riscv
#include <cmath>
#include <intrin.h>
#endif

namespace rtm
{
    
inline float min(float a, float b) { return (b < a) ? b : a;}
inline float max(float a, float b) { return (a < b) ? b : a;}
inline float abs(float a) { return a > 0.0f ? a : -a; }

inline float sqrtf(float input)
{
    #ifdef __riscv
    float output = 0.0f;
	asm volatile ("fsqrt.s %[rd], %[rs1]\n\t" : [rd] "=f" (output) : [rs1] "f" (input));
	return output;
    #else
	return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ps1(input)));
	#endif
}

inline float inv_sqrtf(float input)
{
	#ifdef __riscv
	float output = 0.0f;
	asm volatile ("invsqrt %[rd], %[rs1]\n\t" : [rd] "=f" (output) : [rs1] "f" (input));
	return output;
	#else
	return _mm_cvtss_f32(_mm_invsqrt_ps(_mm_set_ps1(input)));
	#endif
}

}