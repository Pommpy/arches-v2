#include "stdafx.hpp"

//#include "trax.hpp"
//#include "dual-streaming.hpp"
#include "describo.hpp"

//global verbosity flag
int arches_verbosity = 1;

int main(int argc, char* argv[])
{
	Arches::run_sim_describo(argc, argv);
	return 0;
}

/*
Bunny Wall BVH
Vertices: 4669568
Faces: 9338880
Nodes: 14123249
Total Size: 620045344 bytes
*/

/*
Bunny Wall TT2
Vertices: 5490368
Headers: 36480
Nodes: 3156437
Total Size: 168057760 bytes
*/

/*
Terrain BVH
Vertices: 4198401
Faces: 8388608
Nodes: 12934562
Total Size: 564950092 bytes
*/

/*
Terrain TT2
Vertices: 4291873
Headers: 512
Nodes: 2796885
Total Size: 141019180 bytes
*/

/*
Cloth BVH
Vertices: 1050625
Faces: 2097152
Nodes: 3167929
Total Size: 139147052 bytes
*/

/*
Cloth TT2
Vertices: 1143873
Headers: 2048
Nodes: 701438
Total Size: 36238028 bytes
*/