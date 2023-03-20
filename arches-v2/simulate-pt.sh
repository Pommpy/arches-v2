mkdir ./out

mkdir ./out/bunny_rm_pt
mkdir ./out/bunny_rm_pt/profiler
mkdir ./out/bunny_rm_pt/images

for i in {0..7}
do
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer ./benchmarks/describo/res/bunny_rm 0.0 0.0 2.0f $i ./out/bunny_wall_pt/profiler/bvh_$i.prof ./out/bunny_rm_pt/images/bvh_$i.png | tee ./out/bunny_rm_pt/bvh_$i.log
    ../x64/Release/arches-v2.exe ./benchmarks/describo/bin/riscv/path-tracer-tt2 ./benchmarks/describo/res/bunny_rm 0.0 0.0 2.0 $i ./out/bunny_wall_pt/profiler/tt2_$i.prof ./out/bunny_rm_pt/images/tt2_$i.png | tee ./out/bunny_rm_pt/tt2_$i.log
done