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

### Explore the mountains

Build `EDAN35_Project` with the configuration below, then run:

```sh
cmake --build build --target EDAN35_Project --parallel 1
build/src/EDAN35/EDAN35_Project --world ./mountain-save --seed 20260905
```

The HUD lists every key. Mouse looks; WASD flies; Q/E moves down/up.
Flight starts at 48 m/s, Left Shift multiplies it by four, and Left Ctrl by 0.25.
The wheel scales flight speed from 1 to 500 m/s; the Escape menu also has a slider.
Space/left-click carves, X/right-click builds stone, and the menu adjusts brush
size, toggles empty-space skipping, returns to spawn, and closes the world.
R reloads shaders; F2 toggles the HUD, F3 logs, F11 fullscreen, B axes, M wireframe.
`--demo` opens the original demonstration scenes.

The HUD reports average/p99 milliseconds over 120 samples: full frame, CPU plus
driver time excluding presentation, presentation wait, and asynchronous GPU world
time. GPU results are read only when ready and pending queries are never
overwritten. CPU includes driver waits and overlaps GPU execution; do not add
those numbers. Update/submission breakdowns and framebuffer pixel dimensions help
separate streaming, rendering, and high-resolution display costs.

The seeded landscape is continuous procedural terrain: connected ridges and
valleys with relief at every scale, snow above the tree line, rock on steep
slopes, sand at the shore, caves in the rock, and transparent water voxels below
height 0, including submerged caves. Water has editable top and side faces, not
an analytical sea plane. The renderer accumulates water thickness through every
water/air interval up to the first opaque hit, tinting the visible seabed.
Spawn is a deterministic valley-floor viewpoint facing a ridge. Terrain heights are hashed on integer
lattices, so the same seed produces bit-identical chunks on macOS and Linux.

Nearby terrain is stored as 32³ chunks of 0.25 m voxels; distant terrain uses ten
levels of coarser chunks (each level doubles the voxel size) generated from the
same height rule, so mountains stay visible to the horizon without loading
full-resolution chunks. Each level covers a 9³ toroidal page around the camera;
finer levels replace the coarser ones near the camera and a coarser chunk is
drawn only where its finer children are not all resident. One full-screen pass
merges intersected chunks from all levels in ray-distance order, including during
partial streaming. It skips regions covered by finer levels and homogeneous air
or water occupancy cells; mixed cells traverse individual voxels. A composite pass
adds sky, distance fog, and water absorption/reflection from actual voxel
boundaries. The opaque depth is the first solid voxel hit.

**Saved worlds currently require macOS or Linux.** Without `--world`, saves go
under `~/Library/Application Support/ParallaxVoxel/worlds/mountains` on macOS, or
`$XDG_DATA_HOME/parallax-voxel/worlds/mountains` on Linux (default
`~/.local/share`). A saved seed never silently changes. Generator-4 mountain saves
upgrade automatically to generator 5 under the exclusive world lock: recover the
old journal, atomically upgrade snapshots one at a time, then replace the manifest
last. An interruption resumes safely. Solid edits and carved terrain remain;
old air that was naturally below sea level gains water. Generator-5 carved water
stays air on reopening. Earlier island formats remain unsupported.

Brush edits become visible only after a durable save. A redo journal makes a
cross-chunk brush recoverable as one operation; startup finishes an interrupted
checkpoint before loading chunks. Corrupt files and storage failures stop edits
rather than regenerate saved terrain. Retry recovery from the menu, or explicitly
close while retaining the recovery files. An unacknowledged interrupted brush may
complete when reopening. Only one process may open a save directory at a time.
Only full-resolution chunks are saved; the three next-coarser levels overlay
saved chunks when they are generated, so a carved hillside stays carved when the
camera moves away.

Residency is based on distance per level, not which way the camera points, with
one-chunk hysteresis before eviction; frustum culling only suppresses draws.
Chunk keys are 64-bit integers with a camera-relative render origin, so there is
no map edge during normal exploration. Vertical travel is bounded to −128 to
2048 m. One background worker, bounded request/reply queues, a 1 MiB per-frame
upload budget, and a 5,120-brick GPU pool (every targeted chunk can hold a brick)
bound streaming work. Brushes stay atomic across at most eight chunks.

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
cmake --build build --target voxel_benchmark world_storage_smoke world_stream_smoke \
  world_generate_test world_frontier_test --parallel 1
build/src/EDAN35/world_generate_test
build/src/EDAN35/world_frontier_test
build/src/EDAN35/world_storage_smoke
build/src/EDAN35/world_stream_smoke ./new-smoke-save
build/src/EDAN35/voxel_benchmark --scenario mountains --pose ridge --frames 100 --warmup 10 --pairs 3 --output build/mountains
```

The generator test pins the cross-platform terrain fingerprint, level consistency,
overlay superset, spawn determinism, and the cave cutoff per level. The frontier
test checks that 1,000 random anchors with random residency draw every point
exactly once, finest level first. The stream smoke requires a new scratch
directory and never deletes an existing world. It exercises stale camera requests,
edits surviving teleport and shutdown, level overlays, and 1,000-chunk travel. The
storage smoke checks encoding/cache bounds, 64-bit key boundaries, restart,
exclusive access, corruption, journal recovery, and interrupted generator-4 water
migration using real filesystem failures.

The mountains benchmark streams the actual generator around a fixed pose
(`spawn`, `ridge`, `valley`, `underwater`, or `summit`) and compares the same view
with empty-space skipping disabled and enabled. It checks shaded color, opaque
depth, accumulated water intervals, and final composite equivalence before timing.
Focused GPU cases cover water pockets, underwater exits, solid occlusion, and
interleaved LOD chunks. World-march/composite GPU times and GPU-completed CPU wall
time remain separate. `--water 0` skips the entire composite, including sky/fog;
it is an opaque-only benchmark, not a second production water implementation.
Native offscreen results do not include window presentation, UI, VSync, or ongoing
streaming.

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
