## Arches

Arches is a general-purpose and fast cycle-level hardware simulator.

The name is a pun on being able to simulate various architectures and on Arches National Park, Utah
(it being traditional at the University of Utah to name software projects after Utah or Salt Lake
City features).

## Setup (Simulator)

Visual Studio Components needed:
*  Windows Universal CRT SDK 
    *  Install using VS Installer
*  Windows 8.1 sdk 
    *  Download from https://go.microsoft.com/fwlink/p/?LinkId=323507
*  MSVC v142 - VS 2020 C++ x64/x86 build tools (v14.16) 
    *  Install via VS Installer; use the correct build tools for your environment ie. x64/x86

To build the simulator, simply open `arches.sln` in Visual Studio and batch build all.

The simulator itself will be built at `.build/[platform]/[config]/arches-sim.lib`, and any included
samples using it will be built as executables in the same directory.

To run a sample, such as "sim-sample-mips", simply select it in the solution explorer and run it.

## Setup (RISCV, Ubuntu Linux)
Testing programs on the framework requires the use of a RISC-V Cross-Compiler. Fortunately, many many people have devoted numerous hours to getting a decent RISC-V cross compiler implemented using GCC; this is available [here](https://github.com/riscv/riscv-gnu-toolchain). 
For our own testing, these are the instructions we followed:
```bash
$ git clone https://github.com/riscv/riscv-gnu-toolchain
$ cd riscv-gnu-toolchain
$ git submodule update --init --recursive
$ sudo apt-get install autoconf automake autotools-dev curl python3 libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev
$ ./configure --prefix=/opt/riscv --with-arch=rv64imfa --with-abi=lp64f
$ make -j$(nproc)
$ echo -e "export PATH=\"/opt/riscv/bin:$PATH\"" >> ~/.bashrc
$ . ~/.bashrc
```
After these steps are completed, users are able to use `riscv64-uknown-elf-gcc` to compile C code and `riscv64-unknown-elf-g++` to compile C/C++ code. 

## Adding custom instructions
A key reason we used RISC-V over other research languages (i.e. MIPS) was the ease with which RISC-V can be extended. This allows researchers to use our framework to test novel hardware designs that may rely on custom instructions for drastic performance improvements. It's important to note that this method allows progams using custom instructions to compile; however, it doesn't act as a true cross-compiler, i.e. users will need to use inline ASM to include their custom instructions.

To add custom instructions, we followed the guide available [here](https://nitish2112.github.io/post/adding-instruction-riscv/). Since the time of this guide's writing, there have been a few notable changes to the build tree of the riscv-gnu-toolchain, so we will only note the differences below:
- Use `riscv-gnu-toolchain/riscv-binutils/include/opcode/riscv-opc.h` instead of `riscv-gnu-toolchain/riscv-binutils-gdb/include/opcode/riscv-opc.h`.
- Similarly, use `riscv-gnu-toolchain/riscv-binutils/opcodes/riscv-opc.c` instead of `riscv-gnu-toolchain/riscv-binutils-gdb/opcodes/riscv-opc.c`. 
As the original guide notes, after the custom instruction has been added the toolchain will need to be rebuilt. This can be easily accomplished by running the following:
```
$ cd {PATH/TO/RISCV_GNU_TOOLCHAIN}
$ make clean
$ ./configure --prefix=/opt/riscv --with-arch=rv64g --with-abi=lp64d
$ make -j$(nproc)
```
After rebuilding, the user should be able to run the cross-compiled programs with their custom instructions on the Arches framework, assuming they have extended the implementation of the RISC-V ISA provided by Arches to contain their custom instruction.

More details about the RISC-V ISA are available [by reading the specification](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMFDQC-and-Priv-v1.11/riscv-privileged-20190608.pdf), but it's expected that users are at least familiar with RISC-V before they attempt to implement custom instructions. 

## Contributing

Follow the coding style in the existing code.  Additionally, aim for cleanliness above all else.
