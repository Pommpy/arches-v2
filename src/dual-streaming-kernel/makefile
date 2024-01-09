make: riscv-dissasm

riscv: *.cpp *.hpp
	riscv64-unknown-elf-g++ -ffixed-f28 -ffixed-f29 -ffixed-f30 -ffixed-f31 -mno-relax -nostartfiles -emain -Wstack-usage=512 -mabi=lp64f -Ofast -I ../../include ./main.cpp -o ./riscv/kernel

riscv-dissasm: riscv
	riscv64-unknown-elf-objdump -d -x ./riscv/kernel > "./riscv/kernel.dump"

clean:
	rm -f riscv/*