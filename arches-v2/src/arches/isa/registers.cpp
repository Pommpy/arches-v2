#include "registers.hpp"


namespace Arches { namespace ISA { namespace RISCV {

IntegerRegisterFile::IntegerRegisterFile() 
{
	//Zero register
	registers[0].u64 = 0ull;

	//Stack and frame pointers
	sp.u64 = 0ull;
	fp.u64 = 0ull;

	for (int i = 0; i < sizeof(valid); ++i) valid[i] = true;
}

FloatingPointRegisterFile::FloatingPointRegisterFile() 
{
	fcsr.data = 0u;
	//Make rounding mode match simulator rounding mode
	switch ((_mm_getcsr() >> 13) & 0b11) {
	case 0b00: //nearest (even)
		fcsr.frm = 0b000;
		break;
	case 0b01: //toward minus infinity
		fcsr.frm = 0b010;
		break;
	case 0b10: //toward infinity
		fcsr.frm = 0b011;
		break;
	case 0b11: //toward zero
		fcsr.frm = 0b001;
		break;
	}

	for (int i = 0; i < sizeof(valid); ++i) valid[i] = true;
}

}}}
