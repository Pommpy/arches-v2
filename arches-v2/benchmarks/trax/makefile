make: x86 riscv riscv-dissasm

riscv: src/path-tracer.cpp
	riscv64-unknown-elf-g++ -nostartfiles -emain -mno-relax -mabi=lp64f -Ofast ./src/path-tracer.cpp -o ./bin/riscv/path-tracer

riscv-dissasm: riscv
	riscv64-unknown-elf-objdump -d -x ./bin/riscv/path-tracer > "./bin/riscv/path-tracer.dump"

x86: src/path-tracer.cpp
	g++ ./src/path-tracer.cpp -Ofast -lpthread -o ./bin/x86/path-tracer

clean:
	rm -f bin/riscv/*
	rm -f bin/x86/*