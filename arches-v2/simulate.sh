mkdir ./out

mkdir ./out/cloth
mkdir ./out/cloth/profiler
mkdir ./out/cloth/images
for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/bvh-$i.prof ./out/cloth/images/bvh-$i.png | tee ./out/cloth/bvh-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt1 ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/tt1-$i.prof ./out/cloth/images/tt1-$i.png | tee ./out/cloth/tt1-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/tt2-$i.prof ./out/cloth/images/tt2-$i.png | tee ./out/cloth/tt2-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt3 ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/tt3-$i.prof ./out/cloth/images/tt3-$i.png | tee ./out/cloth/tt3-$i.log
done
./gen-tables.sh ./out/cloth-pt

mkdir ./out/bunny-wall
mkdir ./out/bunny-wall/profiler
mkdir ./out/bunny-wall/images
for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/bunny-wall 0.0 0.0 0.482 $i ./out/bunny-wall/profiler/bvh-$i.prof ./out/bunny-wall/images/bvh-$i.png | tee ./out/bunny-wall/bvh-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt1 ./benchmarks/describo/res/bunny-wall 0.0 0.0 0.482 $i ./out/bunny-wall/profiler/tt1-$i.prof ./out/bunny-wall/images/tt1-$i.png | tee ./out/bunny-wall/tt1-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/bunny-wall 0.0 0.0 0.482 $i ./out/bunny-wall/profiler/tt2-$i.prof ./out/bunny-wall/images/tt2-$i.png | tee ./out/bunny-wall/tt2-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt3 ./benchmarks/describo/res/bunny-wall 0.0 0.0 0.482 $i ./out/bunny-wall/profiler/tt3-$i.prof ./out/bunny-wall/images/tt3-$i.png | tee ./out/bunny-wall/tt3-$i.log
done
./gen-tables.sh ./out/bunny-wall

mkdir ./out/terrain2
mkdir ./out/terrain2/profiler
mkdir ./out/terrain2/images
for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/bvh-$i.prof ./out/terrain2/images/bvh-$i.png | tee ./out/terrain2/bvh-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt1 ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/tt1-$i.prof ./out/terrain2/images/tt1-$i.png | tee ./out/terrain2/tt1-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/tt2-$i.prof ./out/terrain2/images/tt2-$i.png | tee ./out/terrain2/tt2-$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt3 ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/tt3-$i.prof ./out/terrain2/images/tt3-$i.png | tee ./out/terrain2/tt3-$i.log
done
./gen-tables.sh ./out/terrain2