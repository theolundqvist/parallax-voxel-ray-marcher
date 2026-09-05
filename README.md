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

### Explore floating islands

Build `EDAN35_Project` with the configuration below, then run:

```sh
cmake --build build --target EDAN35_Project --parallel 1
build/src/EDAN35/EDAN35_Project --world ./island-save --seed 20260905
```

WASD flies, Q/E moves down/up, Shift sprints, Space/left-click carves,
X/right-click builds stone, and Escape opens the menu.
The menu adjusts brush size, toggles empty-space skipping, and returns to spawn.
Press R to reload shaders. `--demo` opens the original demonstration scenes.

The seeded world contains grass-topped islands, stone arches, caves, and emissive
crystals. The world shader writes actual voxel-hit depth and skips empty 8³-voxel
cells without changing visible geometry. Sunlight, hemisphere lighting, and a
shared sky/fog model add depth without approximate cross-chunk shadows.

**Saved worlds currently require macOS or Linux.** Without `--world`, saves go
under `~/Library/Application Support/ParallaxVoxel/worlds/islands` on macOS, or
`$XDG_DATA_HOME/parallax-voxel/worlds/islands` on Linux (default `~/.local/share`).
An existing world's seed and generator version cannot silently change.

Brush edits become visible only after a durable save. A redo journal makes a
cross-chunk brush recoverable as one operation; startup finishes an interrupted
checkpoint before loading chunks. Corrupt files and storage failures stop edits
rather than regenerate saved terrain. Retry recovery from the menu, or explicitly
close while retaining the recovery files. An unacknowledged interrupted brush may
complete when reopening. Only one process may open a save directory at a time.

Residency is based on distance, not which way the camera points. The load radius
is five chunks with a six-chunk retention preference and a hard limit of 640
resident keys; frustum culling only suppresses draws. Each chunk is 32³ voxels at
0.25 world units per voxel. One background worker, eight queued requests, two
reply slots, and four normal chunk ingests per frame bound streaming work.
One committed brush may instead ingest up to eight chunks.

Inactive chunks use the smallest of uniform, run-length, or raw encoding in a
64 MiB/2,048-entry RAM cache. Edited chunks remain on disk; unedited evicted chunks
regenerate from the seed. The resident material payload is at most 20 MiB each on
CPU and GPU, plus occupancy, meshes, bounded transaction buffers, and driver
overhead. Cache limits count allocated encoded payload capacity, not just lengths.
Horizontal coordinates are bounded to ±131,072 world units; vertical bounds are
−32 to 48. This is a bounded-memory world, not an infinite-precision world.

### Persistent voxel storage

Each volume retains one `GL_R8` 3D texture. The first render uploads the volume;
unchanged frames upload nothing. Edits track an XY rectangle per Z slice and merge
matching adjacent slices into `glTexSubImage3D` updates. This uses OpenGL 4.1 APIs,
including on macOS, without staging copies. The original demo modes remain available.
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

For the generated world and its storage/worker contracts:

```sh
cmake --build build --target voxel_benchmark world_storage_smoke world_stream_smoke --parallel 1
build/src/EDAN35/world_storage_smoke
build/src/EDAN35/world_stream_smoke ./new-smoke-save
build/src/EDAN35/voxel_benchmark --scenario islands --frames 100 --warmup 10 --pairs 3 --output build/islands
```

The stream smoke requires a new scratch directory and never deletes an existing
world. It exercises stale camera requests, edits surviving teleport and shutdown,
and 1,000-chunk travel. The storage smoke checks encoding/cache bounds, negative
cross-chunk edits, restart, exclusive access, corruption, and journal recovery
using real filesystem failures.

The islands benchmark renders the actual generator and compares the same scene
with empty-space skipping disabled and enabled. It checks material, shaded color,
and depth equivalence before timing; CSV GPU timer results and GPU-completed CPU
wall time remain separate. `--scenario world` adds focused traversal cases;
`--world-lighting 0` measures the lighting-disabled path. Native offscreen results
do not include window presentation, UI, VSync, or ongoing streaming.

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
