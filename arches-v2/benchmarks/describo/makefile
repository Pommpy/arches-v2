RISCV_CC = riscv64-unknown-elf-g++
RISCV_OBJDUMP = riscv64-unknown-elf-objdump
X86_CC = g++

RISCV_FLAGS = -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3
RISCV_OBJDUMP_FLAGS = -d -x -S
X86_FLAGS = -Ofast -lpthread


make: ray-caster ray-caster-tt1 ray-caster-tt2 ray-caster-tt3 path-tracer path-tracer-tt2


ray-caster: ray-caster-riscv ray-caster-riscv-disasm ray-caster-x86

ray-caster-riscv: src/ray-caster.cpp
	$(RISCV_CC) $(RISCV_FLAGS) ./src/ray-caster.cpp -o ./bin/riscv/ray-caster

ray-caster-riscv-disasm: ray-caster-riscv
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) ./bin/riscv/ray-caster > "./bin/riscv/ray-caster.dump"

ray-caster-x86: src/ray-caster.cpp
	$(X86_CC) $(X86_FLAGS) ./src/ray-caster.cpp -o ./bin/x86/ray-caster


ray-caster-tt1: ray-caster-tt1-riscv ray-caster-tt1-riscv-disasm ray-caster-tt1-x86

ray-caster-tt1-riscv: src/ray-caster-tt1.cpp
	$(RISCV_CC) $(RISCV_FLAGS) ./src/ray-caster-tt1.cpp -o ./bin/riscv/ray-caster-tt1

ray-caster-tt1-riscv-disasm: ray-caster-tt1-riscv
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) ./bin/riscv/ray-caster-tt1 > "./bin/riscv/ray-caster-tt1.dump"

ray-caster-tt1-x86: src/ray-caster-tt1.cpp
	$(X86_CC) $(X86_FLAGS) ./src/ray-caster-tt1.cpp -o ./bin/x86/ray-caster-tt1


ray-caster-tt2: ray-caster-tt2-riscv ray-caster-tt2-riscv-disasm ray-caster-tt2-x86

ray-caster-tt2-riscv: src/ray-caster-tt2.cpp
	$(RISCV_CC) $(RISCV_FLAGS) ./src/ray-caster-tt2.cpp -o ./bin/riscv/ray-caster-tt2

ray-caster-tt2-riscv-disasm: ray-caster-tt2-riscv
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) ./bin/riscv/ray-caster-tt2 > "./bin/riscv/ray-caster-tt2.dump"

ray-caster-tt2-x86: src/ray-caster-tt2.cpp
	$(X86_CC) $(X86_FLAGS) ./src/ray-caster-tt2.cpp -o ./bin/x86/ray-caster-tt2


ray-caster-tt3: ray-caster-tt3-riscv ray-caster-tt3-riscv-disasm ray-caster-tt3-x86

ray-caster-tt3-riscv: src/ray-caster-tt3.cpp
	$(RISCV_CC) $(RISCV_FLAGS) ./src/ray-caster-tt3.cpp -o ./bin/riscv/ray-caster-tt3

ray-caster-tt3-riscv-disasm: ray-caster-tt3-riscv
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) ./bin/riscv/ray-caster-tt3 > "./bin/riscv/ray-caster-tt3.dump"

ray-caster-tt3-x86: src/ray-caster-tt3.cpp
	$(X86_CC) $(X86_FLAGS) ./src/ray-caster-tt3.cpp -o ./bin/x86/ray-caster-tt3


path-tracer: path-tracer-riscv path-tracer-riscv-disasm path-tracer-x86

path-tracer-riscv: src/path-tracer.cpp
	$(RISCV_CC) $(RISCV_FLAGS) ./src/path-tracer.cpp -o ./bin/riscv/path-tracer

path-tracer-riscv-disasm: path-tracer-riscv
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) ./bin/riscv/path-tracer > "./bin/riscv/path-tracer.dump"

path-tracer-x86: src/path-tracer.cpp
	$(X86_CC) $(X86_FLAGS) ./src/path-tracer.cpp -o ./bin/x86/path-tracer

	
path-tracer-tt2: path-tracer-tt2-riscv path-tracer-tt2-riscv-disasm path-tracer-tt2-x86

path-tracer-tt2-riscv: src/path-tracer-tt2.cpp
	$(RISCV_CC) $(RISCV_FLAGS) ./src/path-tracer-tt2.cpp -o ./bin/riscv/path-tracer-tt2

path-tracer-tt2-riscv-disasm: path-tracer-tt2-riscv
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) ./bin/riscv/path-tracer-tt2 > "./bin/riscv/path-tracer-tt2.dump"

path-tracer-tt2-x86: src/path-tracer-tt2.cpp
	$(X86_CC) $(X86_FLAGS) ./src/path-tracer-tt2.cpp -o ./bin/x86/path-tracer-tt2



clean:
	rm -f bin/riscv/*
	rm -f bin/x86/*