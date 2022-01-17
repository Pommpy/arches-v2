g++ ./src/main.cpp -Og -lpthread -o ./bin/x86/trax
g++ -S ./src/main.cpp -Og -lpthread -o ./bin/x86/trax.s
objdump -d ./bin/x86/trax > "./bin/x86/trax.dump"
