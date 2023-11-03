/// \mainpage CG_Labs — Documentation
///
///
/// \tableofcontents
///
///
/// \section intro Introduction
///
/// Here you can find the documentation for the source code used in the courses
/// <a href="https://cs.lth.se/edaf80/">EDAF80</a> and
/// <a href="https://cs.lth.se/edan35/">EDAN35</a> given at Lund University,
/// Sweden.
/// The source code itself is hosted on GitHub at
/// https://github.com/LUGGPublic/CG_Labs.
///
///
/// \subsection intro_doc Documentation
///
/// Information about the dependencies and main requirements for building and
/// running the project can be found in the
/// <a href="https://github.com/LUGGPublic/CG_Labs#dependencies">README.rst</a>
/// file.<br>
/// A guide for setting up and building the project is provided in
/// <a href="https://github.com/LUGGPublic/CG_Labs/blob/master/BUILD.rst">BUILD.rst</a>.
///
/// In the top-right corner of this page, there is a search field you can use
/// to quickly look up a function or class.
/// Via the navigation bar, you can quickly get an overview of all classes and
/// namespaces found in the framework.
///
///
/// \subsection intro_framework Framework
///
/// There are a couple of folders in the main directory you will be interacting
/// with:
/// - _res/_ which contains all the resources used in the course, such as
///    images and 3D models;
/// - _src/_ for all the C++ code, which runs on the CPU;
/// - _shaders/_ for all the GLSL code that will be executed on the GPU.
///
/// _src/_ is further subdivided in:
/// - a _core/_ folder containing various helpers and code shared by the
///   different assignments;
/// - a _EDAF80/_ folder for files specific to assignments for the EDAF80
///   course; _shaders/EDAF80_ is its associated shader folder.
/// - a _EDAN35/_ folder for files specific to assignments for the EDAN35
///   course; _shaders/EDAN35_ is its associated shader folder.
///
/// Each assignment uses a main file called _%assignmentX.cpp_, where _X_
/// corresponds to the number of the assignment (e.g. _%assignment1.cpp_ for the
/// first one).
///
///
/// \section quick_links Quick Links
///
/// Here is a list of classes for which you might want to check the
/// documentation:
/// - FPSCamera
/// - Node
/// - TRSTransform
/// - WindowManager
///
/// The \ref bonobo namespace contains a couple of structures and functions you
/// will be interacting with.
///
/// An overview of the to-do list (from all assignments) can be found on this
/// page: \ref todo.
