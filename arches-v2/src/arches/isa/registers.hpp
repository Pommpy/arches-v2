#pragma once

#include "../../stdafx.hpp"


namespace Arches { namespace ISA { namespace RISCV {

class Register32 final {
public:
	union {
		uint8_t  u8;  int8_t  s8;
		uint16_t u16; int16_t s16;
		uint32_t u32; int32_t s32;
		float f32;
	};
};

class Register64 final {
public:
	union {
		uint8_t  u8;  int8_t  s8;
		uint16_t u16; int16_t s16;
		uint32_t u32; int32_t s32;
		uint64_t u64; int64_t s64;
		float f32; double f64;
	};
};

class FloatingPointControlStatusRegister final {
public:
	union {
		uint32_t data;
		struct {
			uint32_t NX		: 1;
			uint32_t UF		: 1;
			uint32_t OF		: 1;
			uint32_t DZ		: 1;
			uint32_t NV		: 1;
			uint32_t frm	: 3;
			uint32_t		: 24;
		};
	};
};

class IntegerRegisterFile final 
{
	public:
		//See table of conventional uses for these registers here:
		//	http://www.cs.uwm.edu/classes/cs315/Bacon/Lecture/HTML/ch05s03.html
		union 
		{
			Register64 registers[32];
			struct
			{
				Register64 zero;
				Register64 ra;
				Register64 sp;
				Register64 gp;
				Register64 tp;
				Register64 t0;
				Register64 t1;
				Register64 t2;
				Register64 s0;
				Register64 s1;
				Register64 a0;
				Register64 a1;
				Register64 a2;
				Register64 a3;
				Register64 a4;
				Register64 a5;
				Register64 a6;
				Register64 a7;
				Register64 s2;
				Register64 s3;
				Register64 s4;
				Register64 s5;
				Register64 s6;
				Register64 s7;
				Register64 s8;
				Register64 s9;
				Register64 s10;
				Register64 s11;
				Register64 t3;
				Register64 t4;
				Register64 t5;
				Register64 t6;
			};
			struct
			{
				Register64 _[8];
				Register64 fp;
			};
			struct 
			{
				Register64  x0;
				Register64  x1; 
				Register64  x2; 
				Register64  x3; 
				Register64  x4; 
				Register64  x5; 
				Register64  x6; 
				Register64  x7; 
				Register64  x8;   
				Register64  x9; 
				Register64 x10; 
				Register64 x11; 
				Register64 x12; 
				Register64 x13; 
				Register64 x14; 
				Register64 x15; 
				Register64 x16; 
				Register64 x17; 
				Register64 x18; 
				Register64 x19; 
				Register64 x20; 
				Register64 x21; 
				Register64 x22; 
				Register64 x23; 
				Register64 x24; 
				Register64 x25; 
				Register64 x26; ;
				Register64 x27; ;
				Register64 x28; 
				Register64 x29; 
				Register64 x30; 
				Register64 x31; 
			};
		};
		bool valid[32];

	public:
		IntegerRegisterFile();
		~IntegerRegisterFile() = default;
		void print()
		{
			for (uint i = 0; i < 32u; ++i)
				printf("Register%d: %lld\n", i, registers[i].u64);
		}
};

class FloatingPointRegisterFile final {
public:
	union {
		Register32 registers[32];
		struct
		{
			Register32 ft0;
			Register32 ft1;
			Register32 ft2;
			Register32 ft3;
			Register32 ft4;
			Register32 ft5;
			Register32 ft6;
			Register32 ft7;
			Register32 fs0;
			Register32 fs1;
			Register32 fa0;
			Register32 fa1;
			Register32 fa2;
			Register32 fa3;
			Register32 fa4;
			Register32 fa5;
			Register32 fa6;
			Register32 fa7;
			Register32 fs2;
			Register32 fs3;
			Register32 fs4;
			Register32 fs5;
			Register32 fs6;
			Register32 fs7;
			Register32 fs8;
			Register32 fs9;
			Register32 fs10;
			Register32 fs11;
			Register32 ft8;
			Register32 ft9;
			Register32 ft10;
			Register32 ft11;
		};
		struct 
		{
			Register32  f0;
			Register32  f1;
			Register32  f2;
			Register32  f3;
			Register32  f4;
			Register32  f5;
			Register32  f6;
			Register32  f7;
			Register32  f8;
			Register32  f9;
			Register32 f10;
			Register32 f11;
			Register32 f12;
			Register32 f13;
			Register32 f14;
			Register32 f15;
			Register32 f16;
			Register32 f17;
			Register32 f18;
			Register32 f19;
			Register32 f20;
			Register32 f21;
			Register32 f22;
			Register32 f23;
			Register32 f24;
			Register32 f25;
			Register32 f26;
			Register32 f27;
			Register32 f28;
			Register32 f29;
			Register32 f30;
			Register32 f31;
		};
	};
	bool valid[32];

	FloatingPointControlStatusRegister fcsr;

public:
	FloatingPointRegisterFile();
	~FloatingPointRegisterFile() = default;
};

}}}
