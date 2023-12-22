## Parallax Voxel Ray Marcher
Project in High Performance Computer Graphics (3rd place) at LTH by Theodor Lundqvist, Jiuming Zeng and Jintao Yu.

Video:
https://youtu.be/21KFuvCqHIU

Paper:
[Parallax Voxel Ray Marcher.pdf](https://github.com/theolundqvist/parallax-voxel-ray-marcher/files/13754011/Parallax.Voxel.Ray.Marcher.pdf)

<p float="between">
<img src="https://github.com/theolundqvist/parallax-voxel-ray-marcher/assets/31588188/749a94f2-af21-409f-adc0-e0db2ac7e805" width=40% height=50%>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<img src="https://github.com/theolundqvist/parallax-voxel-ray-marcher/assets/31588188/1e64dbf6-ed7a-42e7-a3a9-4012002669f1" width=40% height=50%>
  <img src="https://github.com/theolundqvist/parallax-voxel-ray-marcher/assets/31588188/90c3f8b3-3802-4cdf-89db-8212d5adde82" width=40% height=50%>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<img src="https://github.com/theolundqvist/parallax-voxel-ray-marcher/assets/31588188/b4da6f9a-168f-4347-a2c9-c26bf00fe66e" width=40% height=50%>
</p>

\
\
\
\
\
\
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
