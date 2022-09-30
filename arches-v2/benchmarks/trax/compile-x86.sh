g++ ./src/path-tracer.cpp -O3 -lpthread -o ./bin/x86/path-tracer
g++ -S ./src/path-tracer.cpp -O3 -lpthread -o ./bin/x86/path-tracer.s
objdump -d ./bin/x86/path-tracer > "./bin/x86/path-tracer.dump"
