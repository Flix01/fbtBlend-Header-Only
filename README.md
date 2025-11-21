# fbtBlend-Header-Only


**fbtBlend** is a C++ .blend file parser from https://github.com/gamekit-developers/gamekit (available in the repository subfolder: [./Tools/FileTools/FileFormats/Blend](https://github.com/gamekit-developers/gamekit/tree/master/Tools/FileTools/FileFormats/Blend)).

Here there's a version updated to Blender 2.79 and amalgamated into two header files: **fbtBlend.h** and **Blender.h**.


## Demos

Two demos are available: **testConsole.cpp** and **testGlut.cpp** (they contain build instructions for Linux/MacOS and Windows).

A third demo is present in the subfolder **test_skeletal_animation** (build instructions are at the top of **test_skeletal_animation/main.cpp**).

## Dependencies (recommended, to load compressed .blend files)

* zlib (for Blender versions <  3.0)
* libzstd (for Blender versions >= 3.0)


**testGlut.cpp and test_skeletal_animation need:**

* glut (or freeglut)
* glew (Windows only)

## Blender versions from 2.80 to 5.0

In the *280*, *...*, *500* repository folders there are versions of **fbtBlend.h** and **Blender.h** updated for Blender 2.80, ..., 5.0.
In the *280/tests*, *.../tests*, *500/tests* subfolders there are some test programs that should compile correctly with these versions.

Please note that:
- the files: **XXX/tests/testGlut.cpp**, when **XXX>=280**, are the same as the 2.79 version, except that all materials have been stripped, because Blender>=2.80 doesn't support "Blender Internal" materials anymore.
- the files: **XXX/tests/testGlut.cpp**, when **XXX>=301**, are missing, because Blender 3.1 (or newer) has removed normals from the ```struct MVert```, and I don't know how to retrieve them.

Of course I'll **NEVER** make a ```testGlut.cpp``` that can display physics-based materials... be warned!

## Screenshots
![testGlut](./screenshots/testGlut.png)

![test_skeletal_animation_header](./screenshots/test_skeletal_animation_header.png)
-------------------------------------------------------------------------------------
![test_skeletal_animation](./screenshots/test_skeletal_animation.gif)


## Useful Links:
[Gamekit Github Page](https://github.com/gamekit-developers/gamekit)





