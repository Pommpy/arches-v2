mkdir ./out

mkdir ./out/boxd
mkdir ./out/boxd/profiler
mkdir ./out/boxd/images
for j in {0..1}
do
    for i in {0..3}
    do
        ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer $j ./benchmarks/describo/res/boxd 0.0 0.0 1.5 $i ./out/boxd/profiler/$j-$i.prof ./out/boxd/images/$j-$i.png | tee ./out/boxd/$j-$i.log
    done
done
./gen-tables.sh ./out/boxd