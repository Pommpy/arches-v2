#pragma once
#include "stdafx.hpp"

#include "ray-tracing-include.hpp"

#ifdef ARCH_X86
static std::atomic_uint32_t _next_thread;
#endif

inline uint32_t fchthrd()
{
	#ifdef ARCH_RISCV
	uint32_t value;
	asm volatile("traxamoin %[rd]\n\t" : [rd] "=r" (value));
	return value;
	#endif

	#ifdef ARCH_X86
	return _next_thread++;
	#endif

	return 0;
}

#ifdef ARCH_X86
void reset_atomicinc()
{
 	_next_thread = 0;
}
#endif

#ifdef ARCH_X86
void dump_framebuffer(uint32_t* framebuffer_address, const char* path, uint32_t width, uint32_t height)
{
	FILE* file = fopen(path, "wb");
	fprintf(file,"P6\n%u %u\n255\n", width, height);
	for (size_t j = 0; j < height; ++j)
	{
		for (size_t i = 0; i < width; ++i)
		{
			size_t index = j * width + i;
			fwrite(framebuffer_address + index, sizeof(uint8_t), 3, file);
		}
	}
	fclose(file);

}
#endif
