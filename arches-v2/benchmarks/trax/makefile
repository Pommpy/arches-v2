make: x86 riscv x86-dissasm riscv-dissasm

riscv: src/path-tracer.cpp
	riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -Ofast ./src/path-tracer.cpp -o ./bin/riscv/path-tracer

riscv-dissasm: riscv
	riscv64-unknown-elf-objdump -d -x ./bin/riscv/path-tracer > "./bin/riscv/path-tracer.dump"

x86: src/path-tracer.cpp
	g++ ./src/path-tracer.cpp -Ofast -lpthread -o ./bin/x86/path-tracer

x86-dissasm: x86
	objdump -d ./bin/x86/path-tracer > "./bin/x86/path-tracer.dump"

clean:
	rm -f bin/riscv/*
	rm -f bin/x86/*