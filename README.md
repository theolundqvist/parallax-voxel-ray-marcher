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

### Persistent voxel storage

Each volume retains one `GL_R8` 3D texture. The first render uploads the volume;
unchanged frames upload nothing. Edits track an XY rectangle per Z slice and merge
matching adjacent slices into `glTexSubImage3D` updates. This uses OpenGL 4.1 APIs,
including on macOS, without staging copies or changes to the ray-marching shader.
An update can include unchanged voxels inside its rectangle; it does not upload
untouched slices. Volume destruction releases the texture and bounding-box buffers.
Bulk generation visits X-contiguous storage order to keep CPU writes cache-local.

### Reproduce correctness and performance

Use a C++20 compiler and CMake. With Apple Clang 21, provide an installed recent
Assimp package (the native benchmark used 5.4.3); the bundled 5.1.2 fails to build
with that compiler. Set `CMAKE_PREFIX_PATH` to its installation prefix if necessary.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DLUGGCGL_DOWNLOAD_RESOURCES=OFF -DLUGGCGL_BUILD_VOXEL_BENCHMARK=ON
cmake --build build --target voxel_benchmark --parallel 1
build/src/EDAN35/voxel_benchmark --strict --output build/voxel
```

The benchmark uses the real volume implementation and FVTA shader with an offscreen
1000×1000 framebuffer. macOS uses native CGL; Linux/Windows use GLFW (Linux needs a
display, or `xvfb-run -a` for software-rendering correctness checks).
Defaults: 10 warmup frames, 100 measured frames, 100 × 128³ volumes for static,
spherical boundary edits, and scattered edits; four volumes for full regeneration.
The terrain is a deterministic sinusoidal heightfield, not the paper's original world.

CSV output separates uploaded bytes/calls, CPU upload time, CPU mutation time, and
GPU-completed frame time. Texture readbacks are checked and raw RGBA images are saved
outside timing; `--scenario smoke --strict` checks mutations, no-op updates, bounds, unpack
state, and texture lifetime. `--isolate-uploads` adds GPU waits around uploads for
diagnosis only: do not treat those serialized frames as normal frame-rate results.
For before/after comparisons, use the same harness, dependencies, scene, resolution,
and arguments on both revisions; omit `--strict` only on the old implementation,
which does not satisfy the new persistence contract. Report the actual GL renderer:
software rasterization is not hardware GPU performance.

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
