## Parallax Voxel Ray Marching
Project in High Performance Computer Graphics (3rd place) at LTH by Theodor Lundqvist, Jiuming Zeng and Jintao Yu.

https://youtu.be/21KFuvCqHIU



Some personal notes:

### GRID TRAVERSAL
voxel traversal using FVTA algorithm 
http://www.cse.yorku.ca/~amana/research/grid.pdf


### MODELS
Models can be converted from mesh to voxel grid using online resources
https://github.com/davidstutz/mesh-voxelization
https://drububu.com/miscellaneous/voxelizer/?out=obj


### voxel ray marching techniques 

how Teardown does it: 
https://www.youtube.com/watch?v=0VzE8ROwC58

distance fields - can be made fast on gpu but still slower it seems
https://www.youtube.com/watch?v=REKcTBgkrsE

parallax voxel raymarching
https://www.youtube.com/watch?v=h81I8hR56vQ

### atomotage engine
- https://www.youtube.com/watch?v=nr5JqYYye3w
- https://www.youtube.com/watch?v=4AYBm-9cBqs
- https://www.youtube.com/watch?v=1sfWYUgxGBE

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
