mkdir ./out

mkdir ./out/cloth
mkdir ./out/cloth/profiler
mkdir ./out/cloth/images

mkdir ./out/bunny_wall
mkdir ./out/bunny_wall/profiler
mkdir ./out/bunny_wall/images

mkdir ./out/terrain2
mkdir ./out/terrain2/profiler
mkdir ./out/terrain2/images

for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/bvh_$i.prof ./out/cloth/images/bvh_$i.png | tee ./out/cloth/bvh_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt1 ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/tt1_$i.prof ./out/cloth/images/tt1_$i.png | tee ./out/cloth/tt1_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/tt2_$i.prof ./out/cloth/images/tt2_$i.png | tee ./out/cloth/tt2_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt3 ./benchmarks/describo/res/cloth 0.0 0.556 0.556 $i ./out/cloth/profiler/tt3_$i.prof ./out/cloth/images/tt3_$i.png | tee ./out/cloth/tt3_$i.log

    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/bunny_wall 0.0 0.0 0.482 $i ./out/bunny_wall/profiler/bvh_$i.prof ./out/bunny_wall/images/bvh_$i.png | tee ./out/bunny_wall/bvh_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt1 ./benchmarks/describo/res/bunny_wall 0.0 0.0 0.482 $i ./out/bunny_wall/profiler/tt1_$i.prof ./out/bunny_wall/images/tt1_$i.png | tee ./out/bunny_wall/tt1_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/bunny_wall 0.0 0.0 0.482 $i ./out/bunny_wall/profiler/tt2_$i.prof ./out/bunny_wall/images/tt2_$i.png | tee ./out/bunny_wall/tt2_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt3 ./benchmarks/describo/res/bunny_wall 0.0 0.0 0.482 $i ./out/bunny_wall/profiler/tt3_$i.prof ./out/bunny_wall/images/tt3_$i.png | tee ./out/bunny_wall/tt3_$i.log

    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/bvh_$i.prof ./out/terrain2/images/bvh_$i.png | tee ./out/terrain2/bvh_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt1 ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/tt1_$i.prof ./out/terrain2/images/tt1_$i.png | tee ./out/terrain2/tt1_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/tt2_$i.prof ./out/terrain2/images/tt2_$i.png | tee ./out/terrain2/tt2_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt3 ./benchmarks/describo/res/terrain2 0.0 0.125 0.25 $i ./out/terrain2/profiler/tt3_$i.prof ./out/terrain2/images/tt3_$i.png | tee ./out/terrain2/tt3_$i.log
done