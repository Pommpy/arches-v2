#pragma once
#include "stdafx.hpp"

#include "ray-tracing-include.hpp"

#ifdef ARCH_X86
static std::atomic_uint32_t atomicinc_i;
#endif

uint32_t inline atomicinc()
{
	#ifdef ARCH_RISCV
	uint32_t value;
	asm volatile("fchthrd %[rd]\n\t" : [rd] "=r" (value));
	return value;
	#endif

	#ifdef ARCH_X86
	return atomicinc_i++;
	#endif

	return 0;
}

#ifdef ARCH_X86
void reset_atomicinc()
{
 	atomicinc_i = 0;
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
