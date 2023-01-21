g++ "./src/$1.cpp" -lpthread -std=c++17 -O3 -o "./bin/x86/$1"
#g++ -S ./src/$1.cpp -O3 -lpthread -o ./bin/x86/$1.s
objdump -d "./bin/x86/$1" > "./bin/x86/$1.dump"
