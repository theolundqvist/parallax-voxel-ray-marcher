voxel ray marching

GRID 
grid can be represented as 3D texture (memory heavy, fast, easy to implement)
grid can be represented as sparse octree (memory efficient, slow, hard to implement)


GRID TRAVERSAL
ray marching using FVTA algorithm 
http://www.cse.yorku.ca/~amana/research/grid.pdf


MODELS
Models can be converted from mesh to voxel grid using online resources
https://github.com/davidstutz/mesh-voxelization


PLAN:
1. start with 3D textures and fixed step raymarch a basic model on GPU
2. implement FVTA algorithm
3. edit mesh
4. chunking
5. prodedural terrain
6. octree?




### voxel ray marching techniques 

how Teardown (game) works does it: 
https://www.youtube.com/watch?v=0VzE8ROwC58

fixed step - not perfect, but easy to implement

FVTA - go to edges of voxels to never miss

distance fields - can be made fast on gpu but still slower it seems
https://www.youtube.com/watch?v=REKcTBgkrsE

parallax voxel raymarching (fastest?)
https://www.youtube.com/watch?v=h81I8hR56vQ


cool examples:
https://www.shadertoy.com/view/cdsGz7
https://www.shadertoy.com/view/dtVSzw
https://www.shadertoy.com/view/tdlSR8
