for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/cloth.tt2 0.0 16.0 16.0 $i ./out/cloth/profiler/tt2_$i.prof ./out/cloth/images/tt2_$i.png | tee ./out/cloth/tt2_$i.log
    #../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/cloth.obj 0.0 16.0 16.0 $i ./out/cloth/profiler/bvh_$i.prof ./out/cloth/images/bvh_$i.png | tee ./out/cloth/bvh_$i.log

    #../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/bunny_wall.tt2 0.0 0.0 6.0 $i ./out/bunny_wall/profiler/tt2_$i.prof ./out/bunny_wall/images/tt2_$i.png | tee ./out/bunny_wall/tt2_$i.log
    #../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/bunny_wall.obj 0.0 0.0 6.0 $i ./out/bunny_wall/profiler/bvh_$i.prof ./out/bunny_wall/images/bvh_$i.png | tee ./out/bunny_wall/bvh_$i.log

    #../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster-tt2 ./benchmarks/describo/res/terrain2.tt2 0.0 0.125 0.25 $i ./out/terrain2/profiler/tt2_$i.prof ./out/terrain2/images/tt2_$i.png | tee ./out/terrain2/tt2_$i.log
    #../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/ray-caster ./benchmarks/describo/res/terrain2.obj 0.0 0.125 0.25 $i ./out/terrain2/profiler/bvh_$i.prof ./out/terrain2/images/bvh_$i.png | tee ./out/terrain2/bvh_$i.log
done