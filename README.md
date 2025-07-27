## Parallax Voxel Ray Marcher
Project in High Performance Computer Graphics (3rd place) at LTH by Theodor Lundqvist, Jiuming Zeng and Jintao Yu.

Video:
https://youtu.be/21KFuvCqHIU (10000+ views)

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
### Grid Traversal
- **FVTA Algorithm**: Efficient voxel traversal algorithm for ray casting through a grid.  
  [Paper (Amanatides & Woo)](http://www.cse.yorku.ca/~amana/research/grid.pdf)

### Mesh to Voxel Conversion
- Convert mesh models into voxel grids:
  - [David Stutz's mesh voxelizer (GitHub)](https://github.com/davidstutz/mesh-voxelization)
  - [Drububu online voxelizer (OBJ export)](https://drububu.com/miscellaneous/voxelizer/?out=obj)

### Voxel Ray Marching
- **Teardown-style voxel rendering**:
  - [YouTube: How Teardown Does Destruction](https://www.youtube.com/watch?v=0VzE8ROwC58)
- **Parallax voxel raymarching**:
  - [YouTube](https://www.youtube.com/watch?v=h81I8hR56vQ)
- **Distance fields**:
  - Fast on GPU but can still be slower than grid-based methods  
  - [YouTube: Distance Fields Explained](https://www.youtube.com/watch?v=REKcTBgkrsE)

### Atomontage Engine
- Highly optimized voxel engine supporting dynamic updates and intersections:
  - [Overview](https://www.youtube.com/watch?v=nr5JqYYye3w)
  - [Rendering approach](https://www.youtube.com/watch?v=4AYBm-9cBqs)
  - [Advanced techniques](https://www.youtube.com/watch?v=1sfWYUgxGBE)

### Fragment Shader Techniques
- **Depth Handling in Voxel Rendering**:
  - Early depth test must be disabled when depth is modified in the fragment shader.
  - Use the `conservative_depth` extension:
    - [OpenGL: Conservative Depth](https://www.khronos.org/opengl/wiki/Fragment_Shader#Conservative_Depth)
    - Shader declaration example:
      ```glsl
      layout (depth_less) out float gl_FragDepth;
      ```
    - This declares that fragments will always move closer than their original depth (e.g. when rendering backfaces).

### Cool Shader Examples
- Impressive real-time voxel rendering:
  - [Shadertoy: Voxel Cone Tracing](https://www.shadertoy.com/view/cdsGz7)
  - [Shadertoy: Soft Voxel Global Illumination](https://www.shadertoy.com/view/dtVSzw)
  - [Shadertoy: Sparse Voxel Rendering](https://www.shadertoy.com/view/tdlSR8)
