make: ray-caster ray-caster-tt2


ray-caster-tt2: ray-caster-tt2-riscv ray-caster-tt2-riscv-disasm ray-caster-tt2-x86

ray-caster-tt2-riscv: src/ray-caster-tt2.cpp
	riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -Ofast ./src/ray-caster-tt2.cpp -o ./bin/riscv/ray-caster-tt2

ray-caster-tt2-riscv-disasm: ray-caster-tt2-riscv
	riscv64-unknown-elf-objdump -d -x ./bin/riscv/ray-caster-tt2 > "./bin/riscv/ray-caster-tt2.dump"

ray-caster-tt2-x86: src/ray-caster-tt2.cpp
	g++ ./src/ray-caster-tt2.cpp -Ofast -lpthread -o ./bin/x86/ray-caster-tt2


ray-caster: ray-caster-riscv ray-caster-riscv-disasm ray-caster-x86

ray-caster-riscv: src/ray-caster.cpp
	riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -Ofast ./src/ray-caster.cpp -o ./bin/riscv/ray-caster

ray-caster-riscv-disasm: ray-caster-riscv
	riscv64-unknown-elf-objdump -d -x ./bin/riscv/ray-caster > "./bin/riscv/ray-caster.dump"

ray-caster-x86: src/ray-caster.cpp
	g++ ./src/ray-caster.cpp -Ofast -lpthread -o ./bin/x86/ray-caster
	

clean:
	rm -f bin/riscv/*
	rm -f bin/x86/*