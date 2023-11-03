Revision history for CG_Labs


v2021.2 2021-12-02
==================

New features
------------

* Add content to the default landing page of the documentation;
* Parse common material constants when loading custom objects via
  `loadObjects()`;
* Allow per-axis movement and rotation speed for the camera;
- Allow orbiting cameras for Lab 3 and 4 of EDAF80;
* Use *Uniform Buffer Objects* in EDAN35/Lab2 for transforms and light
  attributes.

Improvements
------------

* Several debug improvements:
  - Error out if a node is lacking a VAO or program rather than silently not
    rendering it;
  - Un-ignore certain warning messages.
- Swap “sprint” and “walk” modifiers, so they match the behaviour found in most
  games;
- Improve the documentation of `TRSTransform`;
- Several improvements to EDAN35/Lab2, especially on the performance side.

Changes
-------

* Use a regular `sampler2D` for the shadow map during light accumulation;
* Automatically build the documentation using GitHub Actions and host it using
  GitHub Pages.

Fixes
-----

* Increase the camera far plane in EDAN35/Lab2 to avoid artefacts during
  lighting;
* Use a single rotation tracking system for the camera, allowing `Look*()`
  methods to be used and not instantly overwritten by other rotations;
* Set `shadowmap_texel_size`;
* Make the angular spacing between light sources in EDAN35/Lab2 dependent on
  the maximum amount of light sources.

Dependencies updates
--------------------

* Update assimp to 5.1.2, and increase the requirement to 5.1.


v2021.1 2021-09-27
==================

New features
------------

* Allow hiding the control points from the GUI in the second assignment of
  EDAF80.
* Allow pausing the wave animation from the GUI in the fourth assignment of
  EDAF80.

Improvements
------------

* Several debug improvements:
  - Ask for a debug context;
  - Re-work which debug messages are enabled and disabled, and how push and pop
    groups are printed.
  - Always show shader compilation and link logs when available, rather than
    only when it failed;
  - Label vertex arrays, buffers, and textures when importing new objects via
    `bonobo::loadObjects()`;
  - Add new debug helpers to reduce casting-related warning messages;
- Enable mipmaps even for opacity textures.

Fixes
-----

* Disable file logging when unable to open the file.
* Unlock the mutex on shutdown when file logging was disabled.
* Add missing includes.
* Tell MSVC to use UTF-8 for source and executable character sets.
* Fix more typos.
* Avoid mismatching type assignments in the basis vector shader.
* Fix framework deinitialisation in the first assignment of EDAF80.
* Address several compilation warnings.


v2021.0 2021-09-01
==================

New features
------------

* Time the different steps in `bonobo::loadObjects()` to easily determine which
  parts are taking longer.
* Add helper code to render a right-handed orthonormal basis.
* Add elapsed time queries to the second assignment of EDAN35, allowing to
  measure the time taken by the different passes.

Breaking changes
----------------

* `bonobo::createProgram()` takes shader files relative to the “shaders/” folder.

Improvements
------------

* Switch away from Travis CI and AppVeyor, and use GitHub Actions instead.
* Add a more detailed build guide.
* Avoid loading unused materials in `bonobo::loadObjects()`
* Several performance and code improvements in the second assignment of EDAN35.
* Various improvements to the `InputHandler` class, included a rework of the
  modifier handling that should fix some issues.
* Label shader programs to help with debugging.
* Remove duplicated shaders.

Fixes
-----

* Fixed typos.
* Fix the rendering of shadow maps in the second assignment of EDAN35.
* Dereference a CMake variable before using it in a generator expression.


Removal
-------

* Drop `GLStateInspection` and `GLStateInspectionView`; debuggers provide more
  details than those and at a finer granularity.

Dependencies updates
--------------------

* Update Dear ImGui to 1.84.2.
* Update the referenced commits for stb.
* Update the referenced commits for tinyfiledialogs.


v2020.1 2020-09-29
==================

New features
------------

* Culling can be controlled from the GUI for assignments 2 to 4 of EDAF80.

Improvements
------------

* Report error and early exit if shader source is empty.
* `utils::slurp_file()` now uses the framework’s logging function, ensuring
  that its error messages will be part of the log file and will be displayed in
  the console inside the framework.
* If the context creation fails, mention to the user the existence of the
  *OpenGL_3.3* branch.
* Output all text to the console as UTF-16 on Windows, to properly display
  non-ASCII characters.
* Change the locale to the user-preferred one.
* Warn when adding too many textures to an instance of the `Node` class, and
  skip adding those extra textures. Each hardware has a different limit as to
  how many textures can be bound, and going over that limit results in an
  `GL_INVALID_ENUM` error which can be hard to understand if one does not know
  how texture handling works.
* Warn and skip when adding a texture of ID 0 to an instance of the `Node`
  class. ID 0 is reserved for the “default” texture and no valid texture should
  have that ID.
* Log an error when setting the program for an instance of the `Node` class
  with a null pointer, rather than throwing an exception.

Fixes
-----

* Convert paths to UTF-16 on Windows when needed, to ensure that paths
  containing non-ASCII characters still properly work there.
* Do not make attributes constant in `SpinConfiguration` and
  `OrbitConfiguration`, so that the user can use their assignment operator.
* Do not append null pointers to the list of children in the `Node` class; the
  code previously warned about it but still went ahead with adding it.
* Do not make shallow clones for stb and tinyfiledialogs as we check out
  specific commits, which is not compatible with shallow clones.
* Fix the tangent and binormal visualisation shaders: they were being affected
  by translations, when they should not have, and they used the same model to
  work matrix as vertices which is usually not valid.


v2020.0 2020-08-25
==================

New features
------------

* For those with GPUs (or GPU drivers) which do not support OpenGL 4.1: check
  out the `OpenGL_3.3 branch
  <https://github.com/LUGGPublic/CG_Labs/tree/OpenGL_3.3>`_.
* In any of the assignments, you can use *F11* to toggle between windowed and
  fullscreen mode.
* In the first assignment for EDAF80, textures for all planets (plus the Sun and
  the Moon) are loaded by default, and constants for their orbit and spin
  configurations are provided.
* In the second assignment for EDAF80, a set of control points is now provided;
  there are represented as small spheres in the 3-D view (once
  `parametric_shapes::createSphere()` has been implemented).
* When using the `node::render()` function and GL_KHR_debug is available, all
  OpenGL calls will be placed within a `glPushDebugGroup()` and
  `glPopDebugGroup()` pair using the node’s name, to improve debugging.

Improvements
------------

* All assignments have received several improvements and tweaks, for example
  EDAF80’s third assignment will create and render two spheres by default (one
  for the skybox, and one on which Phong shading and normal mapping will be
  performed).
* The API of all `parametric_shapes` methods has been modified to be more
  consistent and clearer.
* `displayTexture()` no longer takes a camera as input to retrieve the near and
  far parameters used to linearise values read from depth textures, but now
  take those directly as argument instead.
* The shader for celestial bodies’ rings no longer uses an opacity texture but
  instead uses the alpha channel of the diffuse texture.
* Removed unused code and replaced other with standard types.
* The vertical inversion of textures is now performed by STB.
* Re-use functionalities from GLAD
* The `set_uniforms` parameter of several methods of `Node`, has been made
  optional.
* Switch Travis to use Ubuntu Focal and add macOS to the tested environment.
* A lot of improvements on the CMake configuration files to modernise them,
  make them clearer, remove unnecessary operations, display additional
  information when dependencies fail to download or build, etc.
* Reorganise CHANGES.rst and add release date for v2019.1

Fixes
-----

* Do not make helper functions load relative to specific directory
* Do not use `abs()` on floating point numbers
* Trigger a copy of the DLLs for all targets
* Fix typo in `TRSTransform.h`
* Fix typo in `GLStateInspection’s ToString()`

Dependencies updates
--------------------

* Bump CMake requirements to version 3.13
* CMake: Fix the version of stb and tinyfiledialogs
* Update the downloaded version of GLFW to 3.3.2
* Update GLM to 0.9.9.5 and require exact version
* Update Dear ImGui to 1.78 and ship with the code
* Re-generate the GLAD files and up to OpenGL 4.6
* Update assimp requirement to 5.0, update the downloaded version to 5.0.1 and
  apply additional fixes
* Update the resources archive


v2019.1 2019-09-06
==================

New features
------------

* Select polygon mode from GUI, and simplify it
* Toggle visualisation of light cones in wireframe mode from the GUI
* Switch between shaders from GUI
* Replace lodepng with stb, to also support JPEG file loading
* Add shader for celestial rings and load it in lab1

Improvements
------------

* Add a “CHANGES.rst” file that will list the different modifications done,
  from now on.
* Unify the TRS interfaces between the node and the `TRSTransform` classes, by
  using and exposing a `TRSTransform` instance inside the node class
* Edit node::render() to take parent transform
* AppVeyor: switch from VS 2019 Preview to VS 2019
* README: Add tinyfiledialogs to dependencies
* README: Sort the list of dependencies

Fixes
-----

* Ensure Log is destroyed before its clients
* Rename `WindowManager::CreateWindow()` to
  `WindowManager::CreateGLFWWindow()`, to avoid conflict with Windows API
  macro.
