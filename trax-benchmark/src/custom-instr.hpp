#pragma once
#include "stdafx.hpp"

#ifdef ARCH_X86
static std::atomic_uint32_t _next_thread;
#endif

uint32_t inline fchthrd()
{
	#ifdef ARCH_RISCV
	uint32_t value;
	asm volatile("fchthrd %[rd]\n\t" : [rd] "=r" (value));
	return value;
	#endif

	#ifdef ARCH_X86
	return _next_thread++;
	#endif

	return 0;
}

inline void ebreak()
{
	#ifdef ARCH_RISCV
	asm volatile
	(
		"ebreak\n\t"
	);
	#endif
}

float inline static rcp(float in)
{
#ifdef ARCH_RISCV
	float out;
	asm volatile("frcp.s %0,%1\n\t"
		: "=f" (out)
		: "f" (in));
	return out;
#else
	return 1.0f / in;
#endif
}