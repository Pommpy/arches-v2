mkdir ./out

mkdir ./out/cloth
mkdir ./out/cloth/profiler
mkdir ./out/cloth/images
for j in {1..1}
do
    for i in {0..7}
    do
        ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster $j ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/$j-$i.prof ./out/cloth/images/$j-$i.png | tee ./out/cloth/$j-$i.log
    done
done
./gen-tables.sh ./out/cloth

mkdir ./out/bunny-wall
mkdir ./out/bunny-wall/profiler
mkdir ./out/bunny-wall/images
for j in {1..1}
do
    for i in {0..7}
    do
        ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster $j ./benchmarks/describo/res/bunny-wall 0.0 0.0 0.482 $i ./out/bunny-wall/profiler/$j-$i.prof ./out/bunny-wall/images/$j-$i.png | tee ./out/bunny-wall/$j-$i.log
    done
done
./gen-tables.sh ./out/bunny-wall

mkdir ./out/terrain2
mkdir ./out/terrain2/profiler
mkdir ./out/terrain2/images
for j in {1..1}
do
    for i in {0..7}
    do
        ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster $j ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/$j-$i.prof ./out/terrain2/images/$j-$i.png | tee ./out/terrain2/$j-$i.log
    done
done
./gen-tables.sh ./out/terrain2