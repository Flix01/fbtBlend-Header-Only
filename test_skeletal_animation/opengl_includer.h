#pragma once

#ifdef __EMSCRIPTEN__
#	undef USE_GLEW
#endif //__EMSCRIPTEN__

// These path definitions can be passed to the compiler command-line (so that we don't need to modify code)
#ifndef GLUT_PATH
#   define GLUT_PATH "GL/glut.h"    // Mandatory
#endif //GLEW_PATH
#ifndef FREEGLUT_EXT_PATH
#   define FREEGLUT_EXT_PATH "GL/freeglut_ext.h"    // Optional (used only if glut.h comes from the freeglut library)
#endif //GLEW_PATH
#ifndef GLEW_PATH
#   define GLEW_PATH "GL/glew.h"    // Mandatory for Windows only
#endif //GLEW_PATH

#ifdef _WIN32
#	include "windows.h"
#	define USE_GLEW
#endif //_WIN32

#ifdef USE_GLEW
#	include GLEW_PATH
#else //USE_GLEW
#	define GL_GLEXT_PROTOTYPES
#endif //USE_GLEW

#include GLUT_PATH
#ifdef __FREEGLUT_STD_H__
#   ifndef __EMSCRIPTEN__
#       include FREEGLUT_EXT_PATH
#   endif //__EMSCRIPTEN__
#endif //__FREEGLUT_STD_H__


