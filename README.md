## Voxel ray marching

Building voxel terrain/structures as it is commonly done by for example Minecraft, using two triangles for each face of a voxel, is quickly performance limited with large render distances or small voxels.


### PLAN:
1. start with 3D textures and fixed step raymarch a basic model on GPU
2. implement FVTA algorithm
3. realtime mesh editing
4. chunking
5. LOD
6. prodedural terrain
7. parallax voxel marching


### GRID 
- grid can be represented as 3D texture (memory heavy, fast, easy to implement)
- grid can be represented as sparse octree (memory efficient, slow, hard to implement)
- it can also be completely generated from noise on gpu but if we want to be able to edit the mesh then we need to pass it from the cpu


### GRID TRAVERSAL
voxel traversal using FVTA algorithm 
http://www.cse.yorku.ca/~amana/research/grid.pdf


### MODELS
Models can be converted from mesh to voxel grid using online resources
https://github.com/davidstutz/mesh-voxelization
https://drububu.com/miscellaneous/voxelizer/?out=obj


### voxel ray marching techniques 

how Teardown (game) does it: 
https://www.youtube.com/watch?v=0VzE8ROwC58

fixed step - not perfect, but easy to implement

FVTA - go to edges of voxels to never miss

distance fields - can be made fast on gpu but still slower it seems
https://www.youtube.com/watch?v=REKcTBgkrsE

parallax voxel raymarching (fastest?)
https://www.youtube.com/watch?v=h81I8hR56vQ

### atomotage engine
- example doing some amazing stuff https://www.youtube.com/watch?v=nr5JqYYye3w
- first video they uploaded showing how LOD https://www.youtube.com/watch?v=4AYBm-9cBqs
- large scene https://www.youtube.com/watch?v=1sfWYUgxGBE

### moving voxelvolumes around and intersecting.
Have to disable early depth test since depth can change in fragment shader.
Though we can use an extension:
https://www.khronos.org/opengl/wiki/Fragment_Shader#Conservative_Depth
```glsl
layout (depth_less) out float gl_FragDepth;
```
To say that we always will move the pixel closer than the plane it was rendered on, which is the only thing we will do when rendering backfaces



cool examples:
https://www.shadertoy.com/view/cdsGz7
https://www.shadertoy.com/view/dtVSzw
https://www.shadertoy.com/view/tdlSR8
