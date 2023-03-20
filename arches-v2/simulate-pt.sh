mkdir ./out

#mkdir ./out/cloth-pt
#mkdir ./out/cloth-pt/profiler
#mkdir ./out/cloth-pt/images
#for i in {0..7}
#do
#    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth-pt/profiler/bvh-$i.prof ./out/cloth-pt/images/bvh-$i.png | tee ./out/cloth-pt/bvh-$i.log
#    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer-tt2 ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth-pt/profiler/tt2-$i.prof ./out/cloth-pt/images/tt2-$i.png | tee ./out/cloth-pt/tt2-$i.log
#done
#./gen-tables.sh ./out/cloth-pt

mkdir ./out/bunny-wall-pt
mkdir ./out/bunny-wall-pt/profiler
mkdir ./out/bunny-wall-pt/images
for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer ./benchmarks/describo/res/bunny-wall 0.0 0.0 0.482 $i ./out/bunny-wall-pt/profiler/bvh-$i.prof ./out/bunny-wall-pt/images/bvh-$i.png | tee ./out/bunny-wall-pt/bvh-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer-tt2 ./benchmarks/describo/res/bunny-wall 0.0 0.0 0.482 $i ./out/bunny-wall-pt/profiler/tt2-$i.prof ./out/bunny-wall-pt/images/tt2-$i.png | tee ./out/bunny-wall-pt/tt2-$i.log
done
./gen-tables.sh ./out/bunny-wall-pt

mkdir ./out/terrain2-pt
mkdir ./out/terrain2-pt/profiler
mkdir ./out/terrain2-pt/images
for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2-pt/profiler/bvh-$i.prof ./out/terrain2-pt/images/bvh-$i.png | tee ./out/terrain2-pt/bvh-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer-tt2 ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2-pt/profiler/tt2-$i.prof ./out/terrain2-pt/images/tt2-$i.png | tee ./out/terrain2-pt/tt2-$i.log
done
./gen-tables.sh ./out/terrain2-pt