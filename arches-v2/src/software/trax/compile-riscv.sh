#riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3 ./src/primary_ray_gen.cpp -o ./bin/riscv/primary_ray_gen
#riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3 -S ./src/primary_ray_gen.cpp -o ./bin/riscv/primary_ray_gen.s
#riscv64-unknown-elf-objdump -d -x ./bin/riscv/primary_ray_gen > "./bin/riscv/primary_ray_gen.dump"
#
#riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3 ./src/trace_rays.cpp -o ./bin/riscv/trace_rays
#riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3 -S ./src/trace_rays.cpp -o ./bin/riscv/trace_rays.s
#riscv64-unknown-elf-objdump -d -x ./bin/riscv/trace_rays > "./bin/riscv/trace_rays.dump"

riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3 ./src/main.cpp -o ./bin/riscv/trax
riscv64-unknown-elf-g++ -nostartfiles -mno-relax -mabi=lp64f --entry=main -O3 -S ./src/main.cpp -o ./bin/riscv/trax.s
riscv64-unknown-elf-objdump -d -x ./bin/riscv/trax > "./bin/riscv/trax.dump"
