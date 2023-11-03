|UbuntuBadge| |MacOSBadge| |WindowsBadge|

.. |UbuntuBadge| image:: https://github.com/LUGGPublic/CG_Labs/actions/workflows/ubuntu.yml/badge.svg
   :target: https://github.com/LUGGPublic/CG_Labs/actions/workflows/ubuntu.yml
.. |MacOSBadge| image:: https://github.com/LUGGPublic/CG_Labs/actions/workflows/macos.yml/badge.svg
   :target: https://github.com/LUGGPublic/CG_Labs/actions/workflows/macos.yml
.. |WindowsBadge| image:: https://github.com/LUGGPublic/CG_Labs/actions/workflows/windows.yml/badge.svg
   :target: https://github.com/LUGGPublic/CG_Labs/actions/workflows/windows.yml

CG_Labs contains the source code for the various Computer Graphics courses
taught at `Lund University`_ (EDAF80_ and EDAN35_). All assignments of EDAF80
are available, as well as the second assignment of EDAN35.

The public repository is found at https://github.com/LUGGPublic/CG_Labs, and
its documentation at https://luggpublic.github.io/CG_Labs.

Dependencies
============

CG_Labs uses the following dependencies:

* assimp_ (>= 5.0.0): to load OBJ models;
* CMake_ (>= 3.13): to generate project files and handle dependencies;
* `Dear ImGui`_ (1.84.2): to display information on screen, such as logs;
* Git_: to clone missing dependencies;
* GLAD_ (0.1.33): to load OpenGL extensions;
* GLFW_ (>= 3.2.0): to create a window and an OpenGL context, as well as handle inputs;
* GLM_ (0.9.9.5): for linear algebra operations;
* stb_: to load image files;
* tinyfiledialogs_ (>= 3.1.2): to display dialogue windows to the user, in case
  of errors for example.

The project contains the needed files for GLAD and Dear ImGui, and it will
download assimp, GLM, GLFW, stb and tinyfiledialogs if they are not found on
your computer, so you only need to make sure that CMake and Git are installed
on your computer.

An OpenGL 4.1 context is created by the project; if your hardware or its driver
does not support OpenGL 4.1, you should use the `OpenGL 3.3`_ branch instead
which will create a 3.3 context.

C++14 features are used by this project, so you will need a C++14-capable
compiler; if you are using Visual Studio, that means Visual Studio 2015 or
later.

The different resources used in the assignments, textures, object files, can
be found in `this ZIP file`_, which should be automatically retrieved by CMake.

Configuring and building
========================

All the informations regarding the software configuration, how to set it up, and
how to build the framework, can be found in BUILD.rst_.

Miscellaneous
=============

If you are on a laptop and would like to force the assignments to run on the
discrete GPU, set the option ``GLFW_USE_HYBRID_HPG`` to ``ON`` using CMake
— either from the CMake GUI or using CMake on the command line.

Licence
=======

* Dear ImGui is released under an MIT licence, see `Dear ImGui’s licence`_.
* This project is unlicenced, see `the UNLICENCE file`_.

.. _Lund University: http://www.lu.se/
.. _EDAF80: http://cs.lth.se/edaf80
.. _EDAN35: http://cs.lth.se/edan35
.. _CMake: https://cmake.org/
.. _Git: https://git-scm.com/
.. _GLFW: http://www.glfw.org/
.. _GLAD: https://github.com/Dav1dde/glad
.. _GLM: http://glm.g-truc.net/
.. _Dear ImGui: https://github.com/ocornut/imgui
.. _assimp: https://github.com/assimp/assimp
.. _stb: https://github.com/nothings/stb
.. _tinyfiledialogs: https://sourceforge.net/projects/tinyfiledialogs/
.. _cmake-generators(7): https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html
.. _Dear ImGui’s licence: src/external/Dear ImGui/LICENSE.txt
.. _OpenGL 3.3: https://github.com/LUGGPublic/CG_Labs/tree/OpenGL_3.3
.. _this ZIP file: http://fileadmin.cs.lth.se/cs/Education/EDA221/assignments/EDAF80_resources.zip
.. _BUILD.rst: BUILD.rst
.. _the UNLICENCE file: UNLICENCE
