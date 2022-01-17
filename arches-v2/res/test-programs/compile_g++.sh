riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f -entry=main -O3 $1.cpp -o $1
riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f -entry=main -O3 -S $1.cpp
riscv64-unknown-elf-objdump -d -x $1 > "$1.dump"
grep "<frame_buffer>" "$1.dump"
