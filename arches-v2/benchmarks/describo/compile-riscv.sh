riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -std=c++17 -Ofast "./src/$1.cpp" -o "./bin/riscv/$1"
#riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3 -S ./src/$1.cpp -o ./bin/riscv/$1.s
riscv64-unknown-elf-objdump -S -d -x "./bin/riscv/$1" > "./bin/riscv/$1.dump"