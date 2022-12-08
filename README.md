# fbtBlend-Header-Only


**fbtBlend** is a C++ .blend file parser from https://github.com/gamekit-developers/gamekit (available in the repository subfolder: /tree/master/Tools/FileTools).

Here there's a version updated to Blender 2.79 and amalgamated into two header files: **fbtBlend.h** and **Blender.h**.


## Demos

Two demos are available: **testConsole.cpp** and **testGlut.cpp** (they contain build instructions for Linux and Windows).

A third demo is present in the subfolder **test_skeletal_animation** (build instructions are at the top of **test_skeletal_animation/main.cpp**).

## Dependencies (recommended, to load compressed .blend files)

* zlib (for Blender versions <  3.0)
* libzstd (for Blender versions >= 3.0)


**testGlut.cpp and test_skeletal_animation need:**

* glut (or freeglut)
* glew (Windows only)

## Blender 2.80, 2.81, 2.82, 2.83_LTS, 2.90, 2.91, 2.92, 2.93_LTS, 3.0, 3.1, 3.2, 3.3_LTS and 3.4 versions

In the *280*, *281*, *282*, *283_LTS*, *290*, *291*, *292*, *293_LTS*, *300*, *301*, *302*, *303_LTS* and *304* repository folders there are versions of **fbtBlend.h** and **Blender.h** updated for Blender 2.80, 2.81, 2.82, 2.83_LTS 2.90, 2.91, 2.92, 2.93_LTS, 3.0, 3.1, 3.2, 3.3_LTS and Blender 3.4.
In the *280/tests*, *281/tests*, *282/tests*, *283_LTS/tests*, *290/tests*, *291/tests*, *292/tests*, *293_LTS/tests*, *300/tests*, *301/tests*, *302/tests*, *303_LTS/tests* and *304/tests* subfolders there are some test programs that should compile correctly with these versions.

Please note that:
- the files: **XXX/tests/testGlut.cpp**, when **XXX>=280**, are the same as the 2.79 version, except that all materials have been stripped, because Blender>=2.80 doesn't support "Blender Internal" materials anymore.
- the files: **3xx/tests/testGlut.cpp**, when **3xx>=301**, are missing, because Blender 3.1 (or newer) has removed normals from the ```struct MVert```, and I don't know how to retrieve them (by commenting out normal extraction we could make the program compile and run with wrong normals).

Of course I'll **NEVER** make a ```testGlut.cpp``` that can display physics-based materials... be warned!

## Screenshots
![testGlut](./screenshots/testGlut.png)

![test_skeletal_animation_header](./screenshots/test_skeletal_animation_header.png)
-------------------------------------------------------------------------------------
![test_skeletal_animation](./screenshots/test_skeletal_animation.gif)


## Useful Links:
[Gamekit Github Page](https://github.com/gamekit-developers/gamekit)





