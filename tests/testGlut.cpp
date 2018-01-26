/** License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

// DEPENDENCIES:
/*
-> glut or freeglut (the latter is recommended)
-> glew (Windows only)
*/

// HOW TO COMPILE:
/*
// LINUX:
// With -lz:
g++ -O2 ./testGlut.cpp -o testGlut -D"FBT_USE_GZ_FILE=1" -I"../" -I"./" -lglut -lGL -lz -lX11 -lm
// Without -lz:
g++ -O2 ./testGlut.cpp -o testGlut -I"../" -I"./" -lglut -lGL -lX11 -lm

// WINDOWS (here we use the static version of glew, and glut32.lib, that can be replaced by freeglut.lib):
// With libz.lib:
cl /O2 /MT ./testGlut.cpp /I"./" /I"../" /D"GLEW_STATIC"  /D"FBT_USE_GZ_FILE=1" /link /out:testGlut.exe glut32.lib glew32s.lib opengl32.lib gdi32.lib zlib.lib Shell32.lib comdlg32.lib user32.lib kernel32.lib
// Without libz.lib:
cl /O2 /MT ./testGlut.cpp /I"./" /I"../" /D"GLEW_STATIC" /link /out:testGlut.exe glut32.lib glew32s.lib opengl32.lib gdi32.lib Shell32.lib comdlg32.lib user32.lib kernel32.lib

// EMSCRIPTEN: Not supported because this demo is using the fixed function pipeline

// IN ADDITION:
By default the source file assumes that every OpenGL-related header is in "GL/".
But you can define in the command-line the correct paths you use in your system
for glut.h, glew.h, etc. with something like:
-DGLUT_PATH=\"Glut/glut.h\"
-DGLEW_PATH=\"Glew/glew.h\"
(this syntax works on Linux, don't know about Windows)
*/

/* What this demo should do:
   Using the fixed function pipeline it displays all meshes found in the .blend file using (a very simplified subset of) the "Blender Render" materials.
   Currently it does NOT read anything else (e.g. no cameras, lights, armatures, bones, shape keys, etc.).
   The only additional thing (badly) implemented is the mirror modifier (because of course if we want to support any modifier, we must reimplement it manually).

   The code is just a proof of the concept [i.e. bad style, no refactoring, just something that is here to test bftBlend.h].
   Correct extraction of all the .blend features is way beyond the scope of this project (please do not ask).
*/

//#define USE_GLEW  // By default it's only defined for Windows builds (but can be defined in Linux/Mac builds too)

#ifdef __EMSCRIPTEN__
#	undef USE_GLEW
#endif //__EMSCRIPTEN__

// These path definitions can be passed to the compiler command-line
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


#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

#define  FBTBLEND_IMPLEMENTATION
#include "../fbtBlend.h"
#include "../Blender.h"


const char* blendfilepath = "test.blend";   // This file will be loaded by default (unless specified otherwise in the commandline)
//"test_cube.blend";
//"test_texture_rep.blend";

//=========== STRUCTS WE'LL FILL =========================
// Interface we''l implement later with stb_image.h
extern void OpenGLFlipTexturesVerticallyOnLoad(bool flag_true_if_should_flip);
extern GLuint OpenGLLoadTexture(const char* filename,int req_comp=0,bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);
extern GLuint OpenGLLoadTextureFromMemory(const unsigned char* filenameInMemory, int filenameInMemorySize, int req_comp=0, bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);
// End Interface

#include <string>
#include <vector>
#include <map>
#include <algorithm>
struct Material	{
    float amb[4];
    float dif[4];
    float spe[4];
    float shi;	// In [0,1], not in [0,128]
    float emi[4];
    static void clampFloatIn01(float& f)	{f = (f<0 ? 0 : (f>1 ? 1 : f));}
    Material()  {
        amb[0]=amb[1]=amb[2]=0.25f;amb[3]=1;
        dif[0]=dif[1]=dif[2]=0.85f;dif[3]=1;
        spe[0]=spe[1]=spe[2]=0.0f;spe[3]=1;
        shi = 0.f;//40.f/128.f;
        emi[0]=emi[1]=emi[2]=0.0f;emi[3]=1;
    }
    Material(float r,float g,float b,bool useSpecularColor = true,float ambFraction=0.4f,float speAddOn=0.4f,float shininessIn01=(4.0f/128.0f)) {
        amb[0]=r*ambFraction;amb[1]=g*ambFraction;amb[2]=b*ambFraction;amb[3]=1;
        dif[0]=r;dif[1]=g;dif[2]=b;dif[3]=1;
        spe[3]=1;
        if (useSpecularColor)	{
            spe[0] = r + speAddOn;	clampFloatIn01(spe[0]);
            spe[1] = g + speAddOn;	clampFloatIn01(spe[1]);
            spe[2] = b + speAddOn;	clampFloatIn01(spe[2]);
            shi = shininessIn01;
        }
        else	{
            spe[0]=spe[1]=spe[2]=0;
            shi = 0.f;
        }
        emi[0]=emi[1]=emi[2]=0.0f;emi[3]=1;
    }
    void bind() const   {
        glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
        glMaterialfv(GL_FRONT, GL_SPECULAR, spe);
        glMaterialf(GL_FRONT, GL_SHININESS, shi * 128.0f);
        glMaterialfv(GL_FRONT, GL_EMISSION, emi);
    }
    void lighten(float q)	{
        for (int i=0;i<3;i++)	{
            amb[i]+=q;
            dif[i]+=q;
            spe[i]+=q;
        }
        clamp();
    }
    void darken(float q)	{
        for (int i=0;i<3;i++)	{
            amb[i]-=q;
            dif[i]-=q;
            spe[i]-=q;
        }
        clamp();
    }
    void clamp()	{
        for (int i=0;i<4;i++)	{
            clampFloatIn01(amb[i]);
            clampFloatIn01(dif[i]);
            clampFloatIn01(spe[i]);
            clampFloatIn01(emi[i]);
        }
        clampFloatIn01(shi);
    }
    void reset() {*this = Material();}
    void displayDebugInfo() const   {
        printf("amb{%1.2f,%1.2f,%1.2f,%1.2f};\n",amb[0],amb[1],amb[2],amb[3]);
        printf("dif{%1.2f,%1.2f,%1.2f,%1.2f};\n",dif[0],dif[1],dif[2],dif[3]);
        printf("spe{%1.2f,%1.2f,%1.2f,%1.2f};\n",spe[0],spe[1],spe[2],spe[3]);
        printf("shi: %1.4f in[0,1];\n",shi);
        printf("emi{%1.2f,%1.2f,%1.2f,%1.2f};\n",emi[0],emi[1],emi[2],emi[3]);
    }
};
struct BlenderTexture {
    // Basically, at the present time, we only use this as a temporary struct, just to modify the texture coords to take
    // into account offX,offY,repX,repY
    // Not exactly what we should do, but we save a lot of time/performance by doing so
    // Drawback: When we link a mesh (ALT+D), we can assign to the linked copy new Materials, but now
    // offX,offY,repX,repY are locked, because they become part of the Mesh Data, which is shared
    // [We could store offX,offY,repX,repY together with texId, and use the GL_TEXTURE_MATRIX to do this stuff, to fix this drawback]
    GLuint id;
    float repX;
    float repY;
    float offX;
    float offY;
    enum Mapping {      // Bit Mask (probably incomplete)
        MAP_COLOR       = 1,    // The only one we support
        MAP_NORMAL      = 2,
        MAP_SPECULAR    = 32,   // = MAP_GLOSS
        MAP_ALPHA       = 128,
        MAP_DISP        = 4096
    };
    Mapping mapping;	// (originally short, turned to Mapping): we just hard-code this to 1 (color)
    enum BlendType  {
        BT_BLEND = 0,
        BT_MUL = 1,     // The only one we support... (and we assume it's always used)
        BT_ADD = 2,
        BT_SUB = 3,
        BT_DIV = 4,
        BT_DARK = 5,
        BT_DIFF =	6,
        BT_LIGHT = 7,
        BT_SCREEN = 8,
        BT_OVERLAY = 9,
        BT_BLEND_HUE = 10,
        BT_BLEND_SAT = 11,
        BT_BLEND_VAL = 12,
        BT_BLEND_COLOR = 13,
        BT_NUM_BLENDTYPES	= 14,
        BT_SOFT_LIGHT = 15,
        BT_LIN_LIGHT = 16
    };
    BlendType blendtype;	// (originally short, turned to BlendType): we just hard-code this to 1 (multiply)
    float difffac,norfac,specfac,alphafac,dispfac;  // Not used
    BlenderTexture() : id(0),repX(1),repY(1),offX(0),offY(0),mapping(MAP_COLOR),blendtype(BT_BLEND),
    difffac(1),norfac(1),specfac(1),alphafac(1),dispfac(0.2f) {}
    void transformTexCoords(float uin,float vin,float* uvout2)	const {
        float u = offX + repX*uin;
        float v = offY + repY*vin;
        if (repX!=1) u+=0.5f;   // Of course there's no reason for this... (except some bad empirical hack)
        if (repY!=1) v+=0.5f;
        uvout2[0]=u;uvout2[1]=v;
    }
    void reset() {*this = BlenderTexture();}
};
static bool GetMaterialFromBlender(const Blender::Material* ma,Material& mOut) {
    mOut.reset();
    if (!ma) return false;

    // Colors ( not 100% sure about them )
    mOut.amb[0]=ma->ambr * ma->amb;
    mOut.amb[1]=ma->ambg * ma->amb;
    mOut.amb[2]=ma->ambb * ma->amb;
    mOut.amb[3]=ma->alpha;

    mOut.dif[0]=ma->r * ma->ref;
    mOut.dif[1]=ma->g * ma->ref;
    mOut.dif[2]=ma->b * ma->ref;
    mOut.dif[3]=ma->alpha;

    mOut.spe[0]=ma->specr * ma->spec;
    mOut.spe[1]=ma->specg * ma->spec;
    mOut.spe[2]=ma->specb * ma->spec;
    mOut.spe[3]=ma->alpha;
    //#define USE_ALTERNATIVE_SPECULAR_CONVERSION
    #ifdef USE_ALTERNATIVE_SPECULAR_CONVERSION
    if (mOut.spe[0]< mOut.dif[0]) mOut.spe[0]+= mOut.dif[0]; if (mOut.spe[0]>1) mOut.spe[0]=1;
    if (mOut.spe[1]< mOut.dif[1]) mOut.spe[1]+= mOut.dif[1]; if (mOut.spe[1]>1) mOut.spe[1]=1;
    if (mOut.spe[2]< mOut.dif[2]) mOut.spe[2]+= mOut.dif[2]; if (mOut.spe[2]>1) mOut.spe[2]=1;
    #undef USE_ALTERNATIVE_SPECULAR_CONVERSION
    #endif //USE_ALTERNATIVE_SPECULAR_CONVERSION

    mOut.emi[0]=ma->emit*ma->r;
    mOut.emi[1]=ma->emit*ma->g;
    mOut.emi[2]=ma->emit*ma->b;
    mOut.emi[3]=ma->alpha;

    mOut.shi = (GLfloat) (ma->har-1) /(510.0f);//*24.0f);   // No idea on how to rebase the ma->har value...
    //mOut.shi = (GLfloat) (512.0f-ma->har-1) /(510.0f);//*24.0f);   // No idea on how to rebase the ma->har value...
    //mOut.shi = sqrt(mOut.shi);
    //mOut.shi *= mOut.shi;   // This looks better... sometimes
    //mOut.shi *= mOut.shi;
    // Or must we use: ma->gloss_mir;   //where is it?

    static const bool correctTooDimAmbientLight = true;    //terrible hack, but usually improves things. Feel free to disable it.
    static const float dif2ambFactor = 0.7f;//0.6f;	// MUST BE <=1. If no ambient color is provided, obtain one by: amb = dif * dif2ambFactor;
    static const float correctTooDimAmbientLightDimThreshold = 0.15f*2;  //must be bigger than every component to trigger correction
    static const bool cutTooBigCorrectedAmbientLight = false;//true;

    if (correctTooDimAmbientLight && mOut.amb[0]<=correctTooDimAmbientLightDimThreshold && mOut.amb[1]<=correctTooDimAmbientLightDimThreshold && mOut.amb[2]<=correctTooDimAmbientLightDimThreshold
    //&& mOut.emi[0]==0.f && mOut.emi[1]==0.f && mOut.emi[2]==0.f
    )
    {
        //printf("Material Amb Correction: \"%s\"\n",ma->id.name);
        //printf("amb[] = {%1.4f,%1.4f,%1.4f,%1.4f};\n",mOut.amb[0],mOut.amb[1],mOut.amb[2],mOut.amb[3]);

        mOut.amb[0] = dif2ambFactor * mOut.dif[0];
        mOut.amb[1] = dif2ambFactor * mOut.dif[1];
        mOut.amb[2] = dif2ambFactor * mOut.dif[2];

        if (cutTooBigCorrectedAmbientLight && (mOut.amb[0]>=0.5f || mOut.amb[1]>=0.5f || mOut.amb[2]>=0.5f)) {
            mOut.amb[0]*=0.5f;mOut.amb[1]*=0.5f;mOut.amb[2]*=0.5f;
        }
    }

    /*
    static const bool forbidSpecularLessThenAmbient = false;
    static const bool speOnAmbMinimum = 1.2f;
    if (forbidSpecularLessThenAmbient)  {
        bool mustTweakValue = true;
        for (int i=0;i<3;i++)   {
            if (mOut.spe[i] >= mOut.dif[i]*speOnAmbMinimum)  {mustTweakValue = false;break;}
        }
        if (mustTweakValue) {
            for (int i=0;i<3;i++)   {
                mOut.spe[i] = mOut.dif[i]*speOnAmbMinimum;
                if (mOut.spe[i]>1) mOut.spe[i] = 1;
            }
        }
    }*/

    //#define DEBUG_ME
    #ifdef DEBUG_ME
    {
    printf("Material: \"%s\"\n",ma->id.name);
    printf("amb[] = {%1.4f,%1.4f,%1.4f,%1.4f};\n",mOut.amb[0],mOut.amb[1],mOut.amb[2],mOut.amb[3]);
    printf("dif[] = {%1.4f,%1.4f,%1.4f,%1.4f};\n",mOut.dif[0],mOut.dif[1],mOut.dif[2],mOut.dif[3]);
    printf("spe[] = {%1.4f,%1.4f,%1.4f,%1.4f};\n",mOut.spe[0],mOut.spe[1],mOut.spe[2],mOut.spe[3]);
    printf("emi[] = {%1.4f,%1.4f,%1.4f,%1.4f};\n",mOut.emi[0],mOut.emi[1],mOut.emi[2],mOut.emi[3]);
    printf("shi   = {%1.4f};\n",mOut.shi);
    }
    #undef DEBUG_ME
    #endif //DEBUG_ME

    return true;
}
struct OpenGLTextureFlipperScope {
    OpenGLTextureFlipperScope() {OpenGLFlipTexturesVerticallyOnLoad(true);}
    ~OpenGLTextureFlipperScope() {OpenGLFlipTexturesVerticallyOnLoad(false);}
};
static GLuint GetTextureFromBlender(const Blender::Image* im,const std::string& blendModelParentFolder="",std::map<uintptr_t,GLuint>* pPackedFileMap=NULL,std::map<std::string,GLuint>* pExternFileMap=NULL,std::string* pOptionalTextureFilePathOut=NULL) {
    if (!im) return 0;

    OpenGLTextureFlipperScope textureFlipperScope;  // Well, stb_image needs it...
    if (im->packedfile) {
        if (pOptionalTextureFilePathOut) *pOptionalTextureFilePathOut="//embedded";
        if (pPackedFileMap) {
            std::map<uintptr_t,GLuint>::const_iterator it;
            if ( (it=pPackedFileMap->find((uintptr_t) im->packedfile->data))!=pPackedFileMap->end() ) return it->second;
        }
        const GLuint texId = OpenGLLoadTextureFromMemory((const unsigned char*)im->packedfile->data,im->packedfile->size,0,true);
        if (pPackedFileMap) (*pPackedFileMap)[(uintptr_t)im->packedfile->data] = texId;
        return texId;
    }
    std::string texturePath = im->name;
    if (pExternFileMap) {
        std::map<std::string,GLuint>::const_iterator it;
        if ( (it=pExternFileMap->find(std::string(im->name)))!=pExternFileMap->end() ) return it->second;
    }
    if (texturePath.size()>2 && texturePath[0]=='/' && texturePath[1]=='/') texturePath = texturePath.substr(2);
    GLuint texId = 0;
    if (blendModelParentFolder.size()>0) {
        const std::string fullTexturePath = blendModelParentFolder + "/" + texturePath;
        texId = OpenGLLoadTexture(fullTexturePath.c_str(),0,true);
        if (pOptionalTextureFilePathOut && texId!=0) *pOptionalTextureFilePathOut = fullTexturePath;
    }
    if (texId==0) {
        texId = OpenGLLoadTexture(texturePath.c_str(),0,true);
        if (pOptionalTextureFilePathOut)    {
            if (texId!=0) *pOptionalTextureFilePathOut = texturePath;
            else  *pOptionalTextureFilePathOut = "";
        }
    }
    if (texId && pExternFileMap) (*pExternFileMap)[std::string(im->name)] = texId;

    return texId;
}
static bool GetTextureFromBlender(const Blender::Material* parentMaterial,BlenderTexture& btexOut,const std::string& blendModelParentFolder="",std::map<uintptr_t,GLuint>* pPackedFileMap=NULL,std::map<std::string,GLuint>* pExternFileMap=NULL,BlenderTexture::Mapping mapping = BlenderTexture::MAP_COLOR,std::string* pOptionalTextureFilePathOut=NULL) {
    if (pOptionalTextureFilePathOut) *pOptionalTextureFilePathOut = "";
    btexOut.reset();
    if (!parentMaterial || !parentMaterial->mtex) return false;

    Blender::MTex* const* mtexture = parentMaterial->mtex;

    const bool useLastValidTexture = true;  // Otherwise the first valid texture will be used (we don't blend multiple valid textures together
    int validTextureIndex = -1,i=-1;
    while (mtexture[++i] != 0)	{
        const Blender::MTex* mtex = mtexture[i];
        if ((parentMaterial->septex&(i+1))) continue;   // This texture slot is unchecked
        // Here we match the texture mapping arg (e.g. BlenderTexture::MAP_COLOR), and the texture type (image texture):
        if (!mtex || (mtex->mapto&((short) mapping))==0 || !mtex->tex || mtex->tex->type!=8 || !mtex->tex->ima) continue;    // tex->type!=8 -> image texture

        /*printf("mtexture[%d]=\"%s\""
               "\ttexco=%d, mapto=%d, maptoneg=%d, blendtype=%d,"
               "\tprojx=%d, projy=%d, projz=%d, mapping=%d,"
               "\tnormapspace=%d, which_output=%d, brush_map_mode=%d"
               "\tpad[0]=%d,pad[1]=%d,pad[2]=%d,pad[3]=%d,pad[4]=%d,pad[5]=%d,pad[6]=%d,"
               "\n",i,mtex->uvname ? mtex->uvname : "NULL",
               mtex->texco, mtex->mapto, mtex->maptoneg, mtex->blendtype,
               mtex->projx, mtex->projy, mtex->projz, mtex->mapping,
               mtex->normapspace, mtex->which_output,mtex->brush_map_mode,
               mtex->pad[0],mtex->pad[1],mtex->pad[2],mtex->pad[3],mtex->pad[4],mtex->pad[5],mtex->pad[6]
        );*/

        validTextureIndex = i;if (!useLastValidTexture) break;
    }

    if (validTextureIndex>=0)	{
        // We already know that the following three fields are valid:
        const Blender::MTex* mtex = mtexture[validTextureIndex];
        const Blender::Tex* tex = mtex->tex;
        const Blender::Image* im = tex->ima;

        btexOut.id = GetTextureFromBlender(im,blendModelParentFolder,pPackedFileMap,pExternFileMap,pOptionalTextureFilePathOut);
        if (btexOut.id!=0) {

        btexOut.repX = (float) tex->xrepeat*mtex->size[0];
        btexOut.repY = (float) tex->yrepeat*mtex->size[1];
        btexOut.offX = mtex->ofs[0];
        btexOut.offY = mtex->ofs[1];
        btexOut.mapping = (BlenderTexture::Mapping)mtex->mapto;
        btexOut.blendtype = (BlenderTexture::BlendType)mtex->blendtype;
        btexOut.difffac = mtex->difffac;
        btexOut.norfac = mtex->norfac;
        btexOut.specfac = mtex->specfac;
        btexOut.alphafac = mtex->alphafac;
        btexOut.dispfac = mtex->dispfac;
        //printf("Added \"%s\" texture [rep(%1.1f,%1.1f) off(%1.1f,%1.1f)] to material \"%s\"\n",im->id.name,myTex.repX,myTex.repY,myTex.offX,myTex.offY,ma->id.name);

        // Other things that can turn useful
        /*
                bool mirrorX = (tex->flag&128);	// If this is used: tex->xrepeat/=2 must be done too.
                bool mirrorY = (tex->flag&256); // If this is used: tex->yrepeat/=2 must be done too.
                bool useMipmaps = (tex->imaflag&4);
                bool clipTexture = (tex->extend==2);
                bool isNormalMapInTangentSpace = (mtex->normapspace&2048);	// image with 3 colors
                bool isNormalMapInObjectSpace = (mtex->normapspace&1024);	// grayscale image AFAIK

#define DEBUG_ME
#ifdef DEBUG_ME
                {
                    printf("off+rep[]   = {%1.4f,%1.4f,%1.4f,%1.4f};\n",btexOut.offX,btexOut.offY,btexOut.repX,btexOut.repY);
                    printf("tex->flag       = %d [mirrX:%d mirrY:%d];\n",tex->flag,(int)mirrorX,(int)mirrorY);
                    printf("useMipmaps:%d clipTexture:%d\n",(int)useMipmaps,(int)clipTexture);
                }
#undef DEBUG_ME
#endif //DEBUG_ME
                */
        }
    }

    return btexOut.id>0;
}

// These are something I found with 'try and see' approach.. (enums are not present in "Blender.h")
enum BlenderObjectType {
    BL_OBTYPE_EMPTY         =  0,
    BL_OBTYPE_MESH          =  1,
    BL_OBTYPE_CURVE         =  2,
    BL_OBTYPE_SURF          =  3,
    BL_OBTYPE_FONT          =  4,
    BL_OBTYPE_MBALL         =  5,
    BL_OBTYPE_LAMP          = 10,
    BL_OBTYPE_CAMERA        = 11,
    BL_OBTYPE_WAVE          = 21,
    BL_OBTYPE_LATTICE       = 22,
    BL_OBTYPE_ARMATURE      = 25
};
// Also the logic of any modifier is not included in the .blend file. Here I'll only (partially) implement the mirror modifier manually, so I need to read its data
struct BlenderModifierMirror {
    enum Flag {
        CLIPPING         =               1<<0,
        MIRROR_U         =               1<<1,
        MIRROR_V         =               1<<2,
        AXIS_X           =               1<<3,
        AXIS_Y           =               1<<4,
        AXIS_Z           =               1<<5,
        VGROUP           =               1<<6,
        DONT_MERGE       =               1<<7
    };
    bool enabled,clipping,mirrorU,mirrorV,axisX,axisY,axisZ,vGroup,merge;
    float tolerance;
    BlenderModifierMirror(const Blender::MirrorModifierData& md)
        : enabled(true),clipping(md.flag&CLIPPING),mirrorU(md.flag&MIRROR_U),mirrorV(md.flag&MIRROR_V),
          axisX(md.flag&AXIS_X),axisY(md.flag&AXIS_Y),axisZ(md.flag&AXIS_Z),vGroup(md.flag&VGROUP),merge(!(md.flag&DONT_MERGE)),
          tolerance(md.tolerance)
    {}
    BlenderModifierMirror() : enabled(false) {}
    void display() const {
        if (enabled) printf("axisX:%d axisY:%d axisZ:%d tolerance:%1.5f clipping:%d mirrorU:%d mirrorV:%d vGroup:%d merge:%d\n",(int)axisX,(int)axisY,(int)axisZ,tolerance,(int)clipping,(int)mirrorU,(int)mirrorV,(int)vGroup,(int)merge);
        else printf("enabled:false\n");
    }
    int getNumVertsMultiplier() const {
        int m(1);
        if (enabled) {
            if (axisX) m+=1;
            if (axisY) m+=1;
            if (axisZ) m+=1;
            if (m==3) return 4;
            if (m==4) return 8;
        }
        return m;
    }
    inline const float* mirrorUV(const float* tc2,float* tcOut2) const {
        if (mirrorU) tcOut2[0] = -tc2[0];
        else tcOut2[0] = tc2[0];
        if (mirrorV) tcOut2[1] = -tc2[1];
        else tcOut2[1] = tc2[1];
        return tcOut2;
    }
};


// These are just our transitional structs we'll fill from the blend data,
// and we create a display list for every mesh part (split by Material)
union Vec2 {struct {float x, y;};float v[2];};
inline Vec2 GetVec2(float _x,float _y) {Vec2 v;v.x=_x;v.y=_y;return v;}
typedef std::vector<Vec2> Vector2Array;
union Vec3 {struct {float x, y, z;};float v[3];};
inline Vec3 GetVec3(float _x,float _y,float _z) {Vec3 v;v.x=_x;v.y=_y;v.z=_z;return v;}
inline void Vec3Normalize(Vec3& v) {float len=v.x*v.x+v.y*v.y+v.z*v.z;if (len>0.0001f) {len=sqrt(len);v.x/=len;v.y/=len;v.z/=len;} else {v.x=v.z=0;v.y=1;}}
inline float Vec3Normalize(float& x,float& y, float& z) {float len=x*x+y*y+z*z;if (len>0.0001f) {len=sqrt(len);x/=len;y/=len;z/=len;return len;} else {x=z=0;y=1;return 1.f;}}
inline Vec3 Vec3Cross(const Vec3& a,const Vec3& b) {
    return GetVec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}
inline void Vec3Cross(float& xOut,float& yOut, float& zOut, float ax,float ay, float az,float bx, float by, float bz) {
    xOut = ay * bz - az * by;
    yOut = az * bx - ax * bz;
    zOut = ax * by - ay * bx;
}

typedef std::vector<Vec3> Vector3Array;
typedef std::vector<int> IndexArray;

struct MeshPart {
    // It's a just part of a mesh split by material
    Material material;
    GLuint textureId;
    GLuint displayListId;
    bool visible;
    MeshPart() : textureId(0),displayListId(0),visible(true) {}
    MeshPart(const Material& m,GLuint tId=0,GLuint dId=0) : material(m),textureId(tId),displayListId(dId),visible(true) {}

    static void Draw(const std::vector < MeshPart >& parts) {
        GLuint boundTexId = 0;
        for (size_t i = 0,isz=parts.size();i<isz;i++) {
            const MeshPart& m = parts[i];
            if (m.displayListId>0 && m.visible) {
                m.material.bind();
                if (boundTexId!=m.textureId) {
                    glBindTexture(GL_TEXTURE_2D,m.textureId);
                    boundTexId=m.textureId;
                }
                glCallList(m.displayListId);
            }
        }
        glBindTexture(GL_TEXTURE_2D,0);
    }
};
struct MeshPartVector : public std::vector < MeshPart > {
    typedef std::vector < MeshPart > Base;
public:
    float mMatrix[16];
    float mScaling[3];
    int layer;          // Bit Mask: e.g. 1,2,4,...
    bool visible;
    const Blender::Mesh* pBlendMesh;  // intenal use only!
    MeshPartVector() : visible(true),pBlendMesh(NULL) {
        for (int i=0;i<16;i++) mMatrix[i]=0.f;
        mMatrix[0]=mMatrix[5]=mMatrix[10]=mMatrix[15]=1.f;
        mScaling[0]=mScaling[1]=mScaling[2]=1.f;
        visible = true;
    }
};
typedef std::vector < MeshPartVector > MeshPartVectorContainer;
/*struct MeshPartVectorContainer : public std::vector < MeshPartVector > {
    typedef std::vector < MeshPartVector > Base;
    public:
};*/
MeshPartVectorContainer meshPartsContainer; // This is the only output of our blend parsing!

struct MeshVerts {
    Vector3Array verts;
    Vector3Array norms;
    Vector2Array texcoords;
    void print() const {
        printf("numVerts=%d\tnumNorms=%d\tnumTexCoords=%d\n",(int)verts.size(),(int)norms.size(),(int)texcoords.size());
        const int numElemsToDisplay = 3;
        for (int i=0,iSz=numElemsToDisplay<(int)verts.size()?numElemsToDisplay:(int)verts.size();i<iSz;i++) {
            const Vec3& v = verts[i];
            printf("verts[%d]\t%1.3f,%1.3f,%1.3f\n",i,v.x,v.y,v.z);
        }
        for (int i=0,iSz=numElemsToDisplay<(int)norms.size()?numElemsToDisplay:(int)norms.size();i<iSz;i++) {
            const Vec3& n = norms[i];
            printf("norms[%d]\t%1.3f,%1.3f,%1.3f\n",i,n.x,n.y,n.z);
        }
        for (int i=0,iSz=numElemsToDisplay<(int)texcoords.size()?numElemsToDisplay:(int)texcoords.size();i<iSz;i++) {
            const Vec2& tc = texcoords[i];
            printf("texcoords[%d]\t%1.3f,%1.3f\n",i,tc.x,tc.y);
        }
    }
};
typedef IndexArray MeshPartInds;

typedef std::vector < MeshPartInds > MeshPartIndsVector;

struct TriFaceInfo {
    char flags;                 // Plain Blender flags (tells us if the face is flat or smooth).
    char belongsToLastPoly;     // (Optional) !=0 if this triangle is part of a previous quad or ngon.
    inline TriFaceInfo() : flags(0),belongsToLastPoly(0) {}
    inline TriFaceInfo(char _f,char _b=0) : flags(_f),belongsToLastPoly(_b) {}
};
typedef std::vector<TriFaceInfo> TriFaceInfoVector;

inline static bool IsEqual(float a,float b) {static const float eps = 0.00001;return (a>b) ? a-b<eps : b-a<eps;}
inline static bool IsEqual(const Vec2& a,const Vec2& b) {return IsEqual(a.x,b.x) && IsEqual(a.y,b.y);}
inline static int AddTexCoord(int id,const Vec2& tc,MeshVerts& mesh,std::vector<int>& numTexCoordAssignments,std::multimap<int,int>& texCoordsSingleVertsVertsMultiMap) {
    std::vector < Vec2 >& texCoords = mesh.texcoords;

    if (numTexCoordAssignments[id]==0) {
        ++(numTexCoordAssignments[id]);
        texCoords[id]=tc;
        //texCoordsSingleVertsVertsMultiMap.insert(std::make_pair(id, id));
        //printf("Set! %d = %d\n",id,id);
        return id;
    }

    if (!IsEqual(texCoords[id],tc)) {
        std::multimap<int,int>::iterator start=texCoordsSingleVertsVertsMultiMap.lower_bound(id),last = texCoordsSingleVertsVertsMultiMap.upper_bound(id);
        for (std::multimap<int,int>::iterator it=start; it!=last; ++it) {
            const int id2 = it->second;
            if (IsEqual(texCoords[id2],tc)) {
                //printf("Found! %d = %d\n",id,id2);
                return id2;
            }
        }
        // We must create a new vertex here:
        const int id2 = mesh.verts.size();
        mesh.verts.push_back(mesh.verts[id]);
        mesh.norms.push_back(mesh.norms[id]);
        texCoords.push_back(tc);

        texCoordsSingleVertsVertsMultiMap.insert(std::make_pair(id, id2));
        //printf("New! %d = %d\n",id,id2);
        return id2;
    }
    //printf("OK! %d = %d\n",id,id);
    return id;
}


//=========== END STRUCTS WE'LL FILL FROM ======================

//=========== stb_image.h binding ============================================
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void OpenGLFreeTexture(GLuint& texid)	{if (texid) {glDeleteTextures(1,&texid);texid=0;}}
void OpenGLGenerateOrUpdateTexture(GLuint& texid,int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible,bool wraps,bool wrapt)	{
    if (!pixels || channels<0 || channels>4) return;
    if (texid==0) glGenTextures(1, &texid);

    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wraps ? GL_REPEAT : GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrapt ? GL_REPEAT : GL_CLAMP);
    //const GLfloat borderColor[]={0.f,0.f,0.f,1.f};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (useMipmapsIfPossible)   {
#       ifdef NO_OPENGL_GLGENERATEMIPMAP		// Tweakable
#           ifndef GL_GENERATE_MIPMAP
#               define GL_GENERATE_MIPMAP 0x8191
#           endif //GL_GENERATE_MIPMAP
        // I guess this is compilable, even if it's not supported:
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);    // This call must be done before glTexImage2D(...) // GL_GENERATE_MIPMAP can't be used with NPOT if there are not supported by the hardware of GL_ARB_texture_non_power_of_two.
#       endif //NO_OPENGL_GLGENERATEMIPMAP
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmapsIfPossible ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const GLenum ifmt = channels==1 ? GL_ALPHA : channels==2 ? GL_LUMINANCE_ALPHA : channels==3 ? GL_RGB : GL_RGBA;  // channels == 1 could be GL_LUMINANCE, GL_ALPHA, GL_RED ...
    const GLenum fmt = ifmt;
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels);

#   ifndef NO_OPENGL_GLGENERATEMIPMAP
    if (useMipmapsIfPossible) glGenerateMipmap(GL_TEXTURE_2D);
#   endif //NO_OPENGL_GLGENERATEMIPMAP
}
GLuint OpenGLGenerateTexture(int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible,bool wraps,bool wrapt) {
    GLuint texid = 0;
    OpenGLGenerateOrUpdateTexture(texid,width,height,channels,pixels,useMipmapsIfPossible,wraps,wrapt);
    return texid;
}
void OpenGLFlipTexturesVerticallyOnLoad(bool flag_true_if_should_flip)	{stbi_set_flip_vertically_on_load(flag_true_if_should_flip);}
static bool GetFileContent(const char *filePath, std::vector<char> &contentOut, bool clearContentOutBeforeUsage, const char *modes, bool appendTrailingZeroIfModesIsNotBinary)   {
    std::vector<char>& f_data = contentOut;
    if (clearContentOutBeforeUsage) f_data.clear();
//----------------------------------------------------
    if (!filePath) return false;
    const bool appendTrailingZero = appendTrailingZeroIfModesIsNotBinary && modes && strlen(modes)>0 && modes[strlen(modes)-1]!='b';
    FILE* f;
    if ((f = fbtFile::UTF8_fopen(filePath, modes)) == NULL) return false;
    if (fseek(f, 0, SEEK_END))  {
        fclose(f);
        return false;
    }
    const long f_size_signed = ftell(f);
    if (f_size_signed == -1)    {
        fclose(f);
        return false;
    }
    size_t f_size = (size_t)f_size_signed;
    if (fseek(f, 0, SEEK_SET))  {
        fclose(f);
        return false;
    }
    f_data.resize(f_size+(appendTrailingZero?1:0));
    const size_t f_size_read = f_size>0 ? fread(&f_data[0], 1, f_size, f) : 0;
    fclose(f);
    if (f_size_read == 0 || f_size_read!=f_size)    return false;
    if (appendTrailingZero) f_data[f_size] = '\0';
//----------------------------------------------------
    return true;
}
GLuint OpenGLLoadTexture(const char* filename,int req_comp,bool useMipmapsIfPossible,bool wraps,bool wrapt)	{
    int w,h,n;
    unsigned char* pixels = NULL;
    //pixels = stbi_load(filename,&w,&h,&n,req_comp);    // works, but on Windows filename can't be UTF8 as far as I know
    // workaround
    {
        std::vector<char> buffer;
        if (GetFileContent(filename,buffer,true,"rb",true) && buffer.size()>0)
           pixels = stbi_load_from_memory((const unsigned char*)&buffer[0],buffer.size(),&w,&h,&n,req_comp);
    }
    // end workaround
    if (!pixels) {
        fprintf(stderr,"Error: can't load texture: \"%s\".\n",filename);
        return 0;
    }
    //else fprintf(stderr,"Texture \"%s\" correctly loaded.\n",filename);

    if (req_comp>0 && req_comp<=4) n = req_comp;

    GLuint texId = 0;
    OpenGLGenerateOrUpdateTexture(texId,w,h,n,pixels,useMipmapsIfPossible,wraps,wrapt);

    stbi_image_free(pixels);

    return texId;
}
GLuint OpenGLLoadTextureFromMemory(const unsigned char* filenameInMemory, int filenameInMemorySize, int req_comp, bool useMipmapsIfPossible,bool wraps,bool wrapt)	{
    int w,h,n;
    unsigned char* pixels = stbi_load_from_memory(filenameInMemory,filenameInMemorySize,&w,&h,&n,req_comp);
    if (!pixels) {
        fprintf(stderr,"Error: can't load texture from memory\n");
        return 0;
    }
    if (req_comp>0 && req_comp<=4) n = req_comp;

    GLuint texId = 0;
    OpenGLGenerateOrUpdateTexture(texId,w,h,n,pixels,useMipmapsIfPossible,wraps,wrapt);

    stbi_image_free(pixels);

    return texId;
}
//=========== end stb_image.h binding ========================================

static std::string GetDirectoryName(const std::string& filePath)    {
    if (filePath[filePath.size()-1]==':' || filePath=="/") return filePath;
    size_t beg=filePath.find_last_of('\\');
    size_t beg2=filePath.find_last_of('/');
    if (beg==std::string::npos) {
        if (beg2==std::string::npos) return "";
        return filePath.substr(0,beg2);
    }
    if (beg2==std::string::npos) return filePath.substr(0,beg);
    beg=(beg>beg2?beg:beg2);
    return filePath.substr(0,beg);
}

// Config file handling: basically there's an .ini file next to the
// exe that you can tweak. (it's just an extra)
const char* ConfigFileName = "testGlut.ini";
typedef struct {
    int fullscreen_width,fullscreen_height;
    int windowed_width,windowed_height;
    int fullscreen_enabled;
    int show_fps;
} Config;
void Config_Init(Config* c) {
    c->fullscreen_width=c->fullscreen_height=0;
    c->windowed_width=960;c->windowed_height=540;
    c->fullscreen_enabled=0;
    c->show_fps = 0;
}
#ifndef __EMSCRIPTEN__
int Config_Load(Config* c,const char* filePath)  {
    FILE* f = fbtFile::UTF8_fopen(filePath, "rt");  // same as fopen(...), but on Windows now filePath can be UTF8 (if this file is encoded as UTF8).
    char ch='\0';char buf[256]="";
    size_t nread=0;
    int numParsedItem=0;
    if (!f)  return -1;
    while ((ch = fgetc(f)) !=EOF)    {
        buf[nread]=ch;
        nread++;
        if (nread>255) {
            nread=0;
            continue;
        }
        if (ch=='\n') {
           buf[nread]='\0';
           if (nread<2 || buf[0]=='[' || buf[0]=='#') {nread = 0;continue;}
           if (nread>2 && buf[0]=='/' && buf[1]=='/') {nread = 0;continue;}
           // Parse
           switch (numParsedItem)    {
               case 0:
               sscanf(buf, "%d %d", &c->fullscreen_width,&c->fullscreen_height);
               break;
               case 1:
               sscanf(buf, "%d %d", &c->windowed_width,&c->windowed_height);
               break;
               case 2:
               sscanf(buf, "%d", &c->fullscreen_enabled);
               break;
               case 3:
               sscanf(buf, "%d", &c->show_fps);
               break;
           }
           nread=0;
           ++numParsedItem;
        }
    }
    fclose(f);
    if (c->windowed_width<=0) c->windowed_width=720;
    if (c->windowed_height<=0) c->windowed_height=405;
    return 0;
}
int Config_Save(Config* c,const char* filePath)  {
    FILE* f = fbtFile::UTF8_fopen(filePath, "wt");
    if (!f)  return -1;
    fprintf(f, "[Size In Fullscreen Mode (zero means desktop size)]\n%d %d\n",c->fullscreen_width,c->fullscreen_height);
    fprintf(f, "[Size In Windowed Mode]\n%d %d\n",c->windowed_width,c->windowed_height);
    fprintf(f, "[Fullscreen Enabled (0 or 1) (CTRL+RETURN)]\n%d\n", c->fullscreen_enabled);
    fprintf(f, "[Show FPS (0 or 1) (F2)]\n%d\n", c->show_fps);
    fprintf(f,"\n");
    fclose(f);
    return 0;
}
#endif //__EMSCRIPTEN__
Config config;


// glut has a special fullscreen GameMode that you can toggle with CTRL+RETURN (not in WebGL)
int windowId = 0; 			// window Id when not in fullscreen mode
int gameModeWindowId = 0;	// window Id when in fullscreen mode


// Now we can start with our program

// About float (teapot_float):
// float is just float or double: normal users should just use float and don't worry
// We use it just to test the (optional) definition: TEAPOT_MATRIX_USE_DOUBLE_PRECISION

// camera data:
float targetPos[3];             // please set it in resetCamera()
float cameraYaw;                // please set it in resetCamera()
float cameraPitch;              // please set it in resetCamera()
float cameraDistance;           // please set it in resetCamera()
float cameraPos[3];             // Derived value (do not edit)
float vMatrix[16];              // view matrix
float cameraSpeed = 0.5f;       // When moving it

// light data
float lightDirection[3] = {1,2,2};// Will be normalized

// pMatrix data:
float pMatrix[16];                  // projection matrix
const float pMatrixFovDeg = 45.f;
const float pMatrixNearPlane = 0.5f;
const float pMatrixFarPlane = 200.0f;

float instantFrameTime = 16.2f;

// custom replacement of gluPerspective(...)
static void Perspective(float res[16],float degfovy,float aspect, float zNear, float zFar) {
    const float eps = 0.0001f;
    float f = 1.f/tan(degfovy*1.5707963268f/180.0); //cotg
    float Dfn = (zFar-zNear);
    if (Dfn==0) {zFar+=eps;zNear-=eps;Dfn=zFar-zNear;}
    if (aspect==0) aspect = 1.f;

    res[0]  = f/aspect;
    res[1]  = 0;
    res[2]  = 0;
    res[3]  = 0;

    res[4]  = 0;
    res[5]  = f;
    res[6]  = 0;
    res[7] = 0;

    res[8]  = 0;
    res[9]  = 0;
    res[10] = -(zFar+zNear)/Dfn;
    res[11] = -1;

    res[12]  = 0;
    res[13]  = 0;
    res[14] = -2.f*zFar*zNear/Dfn;
    res[15] = 0;
}
// custom replacement of gluLookAt(...)
static void LookAt(float m[16],float eyeX,float eyeY,float eyeZ,float centerX,float centerY,float centerZ,float upX,float upY,float upZ)    {
    const float eps = 0.0001f;

    float F[3] = {eyeX-centerX,eyeY-centerY,eyeZ-centerZ};
    float length = F[0]*F[0]+F[1]*F[1]+F[2]*F[2];	// length2 now
    float up[3] = {upX,upY,upZ};

    float S[3] = {up[1]*F[2]-up[2]*F[1],up[2]*F[0]-up[0]*F[2],up[0]*F[1]-up[1]*F[0]};
    float U[3] = {F[1]*S[2]-F[2]*S[1],F[2]*S[0]-F[0]*S[2],F[0]*S[1]-F[1]*S[0]};

    if (length==0) length = eps;
    length = sqrt(length);
    F[0]/=length;F[1]/=length;F[2]/=length;

    length = S[0]*S[0]+S[1]*S[1]+S[2]*S[2];if (length==0) length = eps;
    length = sqrt(length);
    S[0]/=length;S[1]/=length;S[2]/=length;

    length = U[0]*U[0]+U[1]*U[1]+U[2]*U[2];if (length==0) length = eps;
    length = sqrt(length);
    U[0]/=length;U[1]/=length;U[2]/=length;

    m[0] = S[0];
    m[1] = U[0];
    m[2] = F[0];
    m[3]= 0;

    m[4] = S[1];
    m[5] = U[1];
    m[6] = F[1];
    m[7]= 0;

    m[8] = S[2];
    m[9] = U[2];
    m[10]= F[2];
    m[11]= 0;

    m[12] = -S[0]*eyeX -S[1]*eyeY -S[2]*eyeZ;
    m[13] = -U[0]*eyeX -U[1]*eyeY -U[2]*eyeZ;
    m[14]= -F[0]*eyeX -F[1]*eyeY -F[2]*eyeZ;
    m[15]= 1;
}
static __inline float Vec3Dot(const float v0[3],const float v1[3]) {
    return v0[0]*v1[0]+v0[1]*v1[1]+v0[2]*v1[2];
}
static __inline void Vec3Normalize(float v[3]) {
    float len = Vec3Dot(v,v);int i;
    if (len!=0) {
        len = sqrt(len);
        for (i=0;i<3;i++) v[i]/=len;
    }
}
static __inline void Vec3Cross(float rv[3],const float a[3],const float b[3]) {
    rv[0] =	a[1] * b[2] - a[2] * b[1];
    rv[1] =	a[2] * b[0] - a[0] * b[2];
    rv[2] =	a[0] * b[1] - a[1] * b[0];
}
static __inline float* MultMat16(float result16[16],const float ml16[16],const float mr16[16]) {
    if (result16==ml16) {
        float tmp[16];for (int i=0;i<16;i++)    tmp[i]=ml16[i];
        return MultMat16(result16,tmp,mr16);
    }
    else if (result16==mr16)    {
        float tmp[16];for (int i=0;i<16;i++)    tmp[i]=mr16[i];
        return MultMat16(result16,ml16,tmp);
    }
    // We don't support result16==mr16==ml16 (we should)
    int i,j,j4;
    for(i = 0; i < 4; i++) {
        for(j = 0; j < 4; j++) {
            j4 = 4*j;
            result16[i+j4] =
                ml16[i]    * mr16[0+j4] +
                ml16[i+4]  * mr16[1+j4] +
                ml16[i+8]  * mr16[2+j4] +
                ml16[i+12] * mr16[3+j4];
        }
    }
    return result16;
}

int current_width=0,current_height=0;  // Not sure when I've used these...
void ResizeGL(int w,int h) {
    current_width = w;
    current_height = h;
    if (h>0)	{
        // We set our pMatrix here in ResizeGL(), and we must notify teapot.h about it too.
        Perspective(pMatrix,pMatrixFovDeg,(float)w/(float)h,pMatrixNearPlane,pMatrixFarPlane);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(pMatrix);
        glMatrixMode(GL_MODELVIEW);
    }

    if (w>0 && h>0 && !config.fullscreen_enabled) {
        // On exiting we'll like to save these data back
        config.windowed_width=w;
        config.windowed_height=h;
    }

    glViewport(0,0,w,h);    // This is what people often call in ResizeGL()

}


void InitGL(void) {    
    // IMPORTANT CALLS--------------------------------------------------------
    {
        fbtBlend blendfile; // we keep it scoped inside InitGL() to save memory
        if (blendfile.parse(blendfilepath)!=fbtFile::FS_OK) {
            printf("ERROR: \"%s\" does not exist, or can't be loaded.\n",blendfilepath);
            exit(1);
        }
        else printf("File \"%s\" loaded successfully.\n",blendfilepath);
        //-----------------------------------------------------------------------
        // NOW COMES THE HARD PART OF CONVERTING MESHES IN blendfile INTO THE GLOBAL VAR meshPartContainer:
        std::map<uintptr_t,GLuint> packedFileMap;       // Map used to reuse already created embedded textures
        std::map<std::string,GLuint> externFileMap;     // Map used to reuse already created extern textures
        const std::string parentFolderPath = ::GetDirectoryName(std::string(blendfilepath));

        //-----------------------------------------------------------------------
        fbtList& objects = blendfile.m_object;long cnt=-1;
        for (Blender::Object* ob = (Blender::Object*)objects.first; ob; ob = (Blender::Object*)ob->id.next)	{
            ++cnt;
            if (!ob->data || ob->type != (short) BL_OBTYPE_MESH)	continue;
            // It's a Mesh
            //printf("Mesh Object Name: \"%s\" (lay=%d)\n",ob->id.name,ob->lay);

            // Now we check if the mesh has mirror modifiers or not:
            BlenderModifierMirror mirrorModifier;
            // Fill 'mirrorModifier' and skip mesh if not good
            const Blender::ModifierData* mdf = (const Blender::ModifierData*) ob->modifiers.first;
            while (mdf)    {
                // mdf->type(5=mirror 8=armature)
                //printf("Object '%s' had modifier: %s (type=%d mode=%d stackindex=%d)\n",ob->id.name,mdf->name,mdf->type,mdf->mode,mdf->stackindex);
                if (mdf->error) {
                    //printf("Error: %s\n",mdf->error);
                }
                else {
                    if (mdf->type==5) {
                        //Mirror modifier:
                        const Blender::MirrorModifierData* md = (const Blender::MirrorModifierData*) mdf;
                        //printf("MirrorModifier: axis:%d flag:%d tolerance:%1.4f\n",md->axis,md->flag,md->tolerance);
                        //if (md->mirror_ob) printf("\nMirrorObj: %s\n",md->mirror_ob->id.name);
                        if (!mirrorModifier.enabled && !md->mirror_ob) {
                            mirrorModifier = BlenderModifierMirror(*md);
                            //mirrorModifier.display();
                        }
                    }
                    else fprintf(stderr,"Detected unknown modifier: %d for object: %s\n",mdf->type,ob->id.name);
                }
                mdf = mdf->next;    // see next modifier
            }
            //---------------------

            const Blender::Mesh* me = (const Blender::Mesh*)ob->data;
            // Warning: when we use ALT+D in Blender to duplicate/link a mesh, we still
            // find here the same "me" we have used before.
            // But also note that linked copies can have different materials (see below in the MATERIALS section).

            // VALIDATION-------------------------------------------
            //printf("\tMesh Name: %s\n", me->id.name);
            const bool hasFaces = me->totface>0 && me->mface;
            const bool hasPolys = me->totpoly>0 && me->mpoly && me->totloop>0 && me->mloop;
            const bool hasVerts = me->totvert>0;
            const bool isValid = hasVerts && (hasFaces || hasPolys);
            if (!isValid) continue;
            //-------------------------------------------------------


            // WE MUST NOW SEE IF WE HAVE ALREADY PROCESSED me:
            int oldMeshPartsIndex=-1;
            for (int i=0,iSz=(int)meshPartsContainer.size();i<iSz;i++) {
                const MeshPartVector& mpv = meshPartsContainer[i];
                if (mpv.pBlendMesh==me) {
                    oldMeshPartsIndex = i;
                    break;
                }
            }

            // MATERIALS---------------------------------------------
            // Load Materials first
            // Original materials are always present in me->mat, but they can be overridden in ob->mat
            const bool areMaterialsOverridden = ob->mat && *ob->mat;
            Blender::Material	**mat = areMaterialsOverridden ? ob->mat : me->mat;
            const short totcol = areMaterialsOverridden ? ob->totcol : me->totcol;  // However we assume ob->totcol==me->totcol (don't know what we should do otherwise)
            const int numMaterials = totcol > 0 ? totcol : 1;
            std::vector< ::Material > materialVector(numMaterials);
            std::vector< BlenderTexture > textureVector(numMaterials);
            std::vector< std::string > materialNameVector(numMaterials);
            std::vector< std::string > textureFilePathVector(numMaterials);
            if (mat && *mat)	{
                for (int i = 0; i < ob->totcol; ++i)	{
                    const Blender::Material* ma =  mat[i];
                    if (!ma) continue;

                    //printf("Mat[%d]=\"%s\"\n",i,ma->id.name);
                    /*printf("Mat[%d]=\"%s\""
                           "\ttot_slots=%d mapto_textured=%d material_type=%d flag=%d use_nodes=%d,"
                           "\tmode=%d,mode_l=%d,mode2=%d,mode2_l=%d,"
                           "\tpr_lamp=%d,pr_texture=%d,ml_flag=%d,mapflag=%d,"
                           "\ttexco=%d,mapto=%d,septex=%d\n",i,ma->id.name,
                    ma->tot_slots,ma->mapto_textured,ma->material_type,ma->flag,ma->use_nodes,
                    ma->mode,ma->mode_l,ma->mode2,ma->mode2_l,
                    ma->pr_lamp, ma->pr_texture, ma->ml_flag, ma->mapflag,
                    ma->texco, ma->mapto, ma->septex
                    );*/

                    // What I'd like to find out is how to detect if a texture slot in a material is unchecked or not.
                    // Using the Python API is easier: material.use_textures[ slotid ]
                    // FOUND AFTER A COUPLE OF HOURS...
                    // ma->septex is the ORed flag of the (1-based) texture slots TO EXCLUDE (not to use)

                    GetMaterialFromBlender(ma,materialVector[i]);
                    materialNameVector[i] = std::string(ma->id.name);                    
                    GetTextureFromBlender(ma,textureVector[i],parentFolderPath,&packedFileMap,&externFileMap,BlenderTexture::MAP_COLOR,&textureFilePathVector[i]);
                }
            }
            //-------------------------------------------------------

            // MESH -------------------------------------------------
            meshPartsContainer.push_back(MeshPartVector());
            MeshPartVector& meshParts = meshPartsContainer[meshPartsContainer.size()-1];
            if (oldMeshPartsIndex>=0)   meshParts = meshPartsContainer[oldMeshPartsIndex];  // Deep copy (reusing display lists)
            meshParts.resize(numMaterials);
            meshParts.pBlendMesh = me;
            meshParts.layer = ob->lay;

            for (int i=0;i<numMaterials;i++) {
                MeshPart& m = meshParts[i];
                //m.meshName = meshName;
                m.material = materialVector[i]; //m.materialName = materialNameVector[i];
                m.textureId = textureVector[i].id; //if (m.textureId!=0) m.textureFilePath = textureFilePathVector[i];
            }


            // OBJECT TRANSFORM
            {
                float mMatrix[16]; // The one stored in ob->obmat, in float16 format

                // Filling mMatrix
                {
                    // Here we support parenting (TO BE TESTED)
                    Blender::Object* obcur = ob;
                    // Retrieve data from ob:
                    float mMatrixParent[16];
                    while (obcur) {
                        for (int i=0;i<4;i++) {
                            for (int j=0;j<4;j++) {
                                mMatrix[j*4+i] = obcur->obmat[j][i];
                            }
                        }
                        Blender::Object* obcurparent = obcur->parent;
                        if (obcurparent) {
                            for (int i=0;i<4;i++) {
                                for (int j=0;j<4;j++) {
                                    mMatrixParent[j*4+i] = obcurparent->obmat[j][i];
                                }
                            }
                            MultMat16(mMatrix,mMatrixParent,mMatrix);
                        }
                        obcur = obcurparent;
                    }
                }

#               define EXTRACT_SCALING  // Optional (having a mMatrix without scaling can help in some application)
#               ifdef EXTRACT_SCALING
                // We remove the scaling inside mMatrix[16], putting it into meshParts.mScaling[3]
                meshParts.mScaling[0] = Vec3Normalize(mMatrix[0],mMatrix[1],mMatrix[2]);
                meshParts.mScaling[1] = Vec3Normalize(mMatrix[4],mMatrix[5],mMatrix[6]);
                meshParts.mScaling[2] = Vec3Normalize(mMatrix[8],mMatrix[9],mMatrix[10]);
                //printf("mScaling[3]={%1.2f,%1.2f,%1.2f};\tob->size[3]={%1.2f,%1.2f,%1.2f};\n",meshParts.mScaling[0],meshParts.mScaling[1],meshParts.mScaling[2],ob->size[0],ob->size[1],ob->size[2]);
#               undef EXTRACT_SCALING
#               endif //EXTRACT_SCALING

                /*// Optional: Orthogonalize mMatrix:
                Vec3Cross(mMatrix[4],mMatrix[5],mMatrix[6], mMatrix[8],mMatrix[9],mMatrix[10],  mMatrix[0],mMatrix[1],mMatrix[2]);  // Y = Z cross X;
                Vec3Cross(mMatrix[0],mMatrix[1],mMatrix[2], mMatrix[4],mMatrix[5],mMatrix[6],   mMatrix[8],mMatrix[9],mMatrix[10]); // X = Y cross Z;
                //  X.Normalise();Y.Normalise();Z = X cross Y;Y = Z cross X;

                //mMatrix[3]=mMatrix[7]=mMatrix[11]=0.f;mMatrix[15]=1.f;*/


                // Ok, now we convert mMatrix to meshParts.mMatrix:
                for (int i=0;i<16;i++) meshParts.mMatrix[i] = mMatrix[i];
                // We must adjust meshParts.mMatrix so that: newY = oldZ; newZ = -oldY
                for (int i=0;i<4;i++) {
                    meshParts.mMatrix[4*i+1] = mMatrix[4*i+2];
                    meshParts.mMatrix[4*i+2] = -mMatrix[4*i+1];
                }


                /*printf("\t\tmeshParts.mMatrix:\n");
                for (int i=0;i<4;i++) {
                    printf("\t\t\t");
                    for (int j=0;j<4;j++) {
                        printf("%1.2f\t",meshParts.mMatrix[j*4+i]);
                    }
                    printf("\n");
                }
                printf("\n");*/


            }

            if (oldMeshPartsIndex>=0) {
                //printf("Mesh: \"%s\" is reusing display list from mesh: \"%s\"\n",ob->id.name,me->id.name);
                continue; // Skip the mesh we're linking to
            }

            MeshPartIndsVector meshPartInds;
            meshPartInds.resize(numMaterials);
            std::vector< TriFaceInfoVector > meshPartTriFaceInfo;    // Needed for smooth face
            meshPartTriFaceInfo.resize(numMaterials);

            MeshVerts meshVerts;
            std::vector < Vec3 >& verts = meshVerts.verts;
            std::vector < Vec3 >& norms = meshVerts.norms;
            std::vector < Vec2 >& texCoords = meshVerts.texcoords;
            bool hasTexCoords = false;

            // VERTS AND NORMS--(NOT MIRRORED)--------------------------
            int numSingleVerts = me->totvert;
            int numVerts = mirrorModifier.enabled ? numSingleVerts * mirrorModifier.getNumVertsMultiplier() : numSingleVerts;
            verts.reserve(numVerts);norms.reserve(numVerts);texCoords.reserve(numVerts);
            verts.resize(numSingleVerts);norms.resize(numSingleVerts);texCoords.resize(numSingleVerts);
            const float MAX_SHORT = (32767);//INT16_MAX;
            for (int i=0;i<numSingleVerts;i++) {
                const Blender::MVert& v = me->mvert[i];
                Vec3& V = verts[i];
                Vec3& N = norms[i];
                // Correct:
                V.v[0]=v.co[0];V.v[1]=v.co[1];V.v[2]=v.co[2];
                // Normals are stored as short int!
                N.v[0]=(float)v.no[0]/MAX_SHORT;N.v[1]=(float)v.no[1]/MAX_SHORT;N.v[2]=(float)v.no[2]/MAX_SHORT;
                //            // Hack to convert to OpenGL coords (better doing it at the end):
                //            V.v[0]=v.co[0];V.v[1]=v.co[2];V.v[2]=-v.co[1];
                //            N.v[0]=v.no[0];N.v[1]=v.no[2];N.v[2]=-v.no[1];
                Vec3Normalize(N); // Needed ?
                texCoords[i] = GetVec2(0,0);
            }


            IndexArray* pinds = NULL;
            TriFaceInfoVector* pTriFaceFlags = NULL;
            const BlenderTexture* pBlenderTexture = NULL;

            Vec2 tc;int li0,li1,li2,li3,vi0,vi1,vi2,vi3;

            // INDS AND TEXCOORDS--(NOT MIRRORED)..........................
            std::vector<int> numTexCoordAssignments(numSingleVerts,0);
            std::multimap<int,int> texCoordsSingleVertsVertsMultiMap;
            if (hasFaces)
            {
                // NEVER TESTED: only old .blend files have this
                pBlenderTexture = NULL;pinds = NULL;pTriFaceFlags = NULL;  // reset

                short mat_nr = me->mface[0].mat_nr;bool mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;int ic,ic2;
                //int smoothingGroup = (me->mface[0].flag&1) ? 1 : -1;
                //bool flatShading = (!(me->mface[0].flag&1)) ? true : false;
                bool isTriangle =  me->mface[0].v4==0 ? true : false;	// Here I suppose that 0 can't be a vertex index in the last point of a quad face (e.g. quad(0,1,2,0) is a triangle(0,1,2)). This assunction SHOULD BE WRONG, but I don't know how to spot if a face is a triangle or a quad...
                // TODO: also MTFace::mode can be: 512 (TF_TWOSIDE), 256 (TF_BILLBOARD), 4096 (TF_BILLBOARD2), 1024 (TF_INVISIBLE), 2 (TF_ALPHASORT), 4 (TF_TEX)
                // and MTFace::transp can be: 0 (TF_SOLID), 2 (TF_ALPHA)
                if (mat_nr_ok)  {
                    pBlenderTexture =           &textureVector[mat_nr];
                    pinds =                     &meshPartInds[mat_nr];
                    pTriFaceFlags =             &meshPartTriFaceInfo[mat_nr];
                }

                for (int i=0;i<me->totface;i++) {
                    const Blender::MFace& mface = me->mface[i];
                    if (mat_nr!= mface.mat_nr)	{
                        mat_nr = mface.mat_nr;

                        pBlenderTexture = NULL;pinds = NULL;pTriFaceFlags = NULL;
                        mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;
                        // TODO: also MTFace::mode can be: 512 (TF_TWOSIDE), 256 (TF_BILLBOARD), 4096 (TF_BILLBOARD2), 1024 (TF_INVISIBLE), 2 (TF_ALPHASORT), 4 (TF_TEX)
                        // and MTFace::transp can be: 0 (TF_SOLID), 2 (TF_ALPHA)
                        if (mat_nr_ok)  {
                            pBlenderTexture =           &textureVector[mat_nr];
                            pinds =                     &meshPartInds[mat_nr];
                            pTriFaceFlags =             &meshPartTriFaceInfo[mat_nr];
                        }
                    }
                    if (!pinds) continue;
                    //smoothingGroup = (mface.flag&1) ? 1 : -1;
                    //flatShading=!(mface.flag&1);
                    isTriangle=(mface.v4==0);
                    const Blender::MTFace* mtf = (pBlenderTexture->id>0 && me->mtface) ? &me->mtface[i] : NULL;

                    // Add (first) triangle:
                    pinds->push_back(mface.v1);
                    pinds->push_back(mface.v2);
                    pinds->push_back(mface.v3);
                    pTriFaceFlags->push_back(TriFaceInfo(mface.flag));

                    if (mtf) {
                        hasTexCoords = true;
                        pBlenderTexture->transformTexCoords(mtf->uv[0][0],mtf->uv[0][1],tc.v);
                        ic = mface.v1;
                        if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                            (*pinds)[pinds->size()-3] = ic2;

                        pBlenderTexture->transformTexCoords(mtf->uv[1][0],mtf->uv[1][1],tc.v);
                        ic = mface.v2;
                        if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                            (*pinds)[pinds->size()-2] = ic2;

                        pBlenderTexture->transformTexCoords(mtf->uv[2][0],mtf->uv[2][1],tc.v);
                        ic = mface.v3;
                        if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                            (*pinds)[pinds->size()-1] = ic2;
                    }

                    if (!isTriangle)	{
                        // Add second triangle:
                        pinds->push_back(mface.v3);
                        pinds->push_back(mface.v4);
                        pinds->push_back(mface.v1);
                        pTriFaceFlags->push_back(TriFaceInfo(mface.flag,1));

                        if (mtf) {
                            hasTexCoords = true;
                            pBlenderTexture->transformTexCoords(mtf->uv[2][0],mtf->uv[2][1],tc.v);
                            ic = mface.v3;
                            if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                (*pinds)[pinds->size()-3] = ic2;

                            pBlenderTexture->transformTexCoords(mtf->uv[3][0],mtf->uv[3][1],tc.v);
                            ic = mface.v4;
                            if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                (*pinds)[pinds->size()-2] = ic2;

                            pBlenderTexture->transformTexCoords(mtf->uv[0][0],mtf->uv[0][1],tc.v);
                            ic = mface.v1;
                            if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                (*pinds)[pinds->size()-1] = ic2;
                        }
                    }

                }

            }
            else if (hasPolys) {
                pBlenderTexture = NULL;pinds = NULL;pTriFaceFlags = NULL;   // reset

                short mat_nr = me->mpoly[0].mat_nr;bool mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;int ic,ic2;
                //bool flatShading = (!(me->mpoly[0].flag&1)) ? true : false;
                // TODO: also MTFace::mode can be: 512 (TF_TWOSIDE), 256 (TF_BILLBOARD), 4096 (TF_BILLBOARD2), 1024 (TF_INVISIBLE), 2 (TF_ALPHASORT), 4 (TF_TEX)
                // and MTFace::transp can be: 0 (TF_SOLID), 2 (TF_ALPHA)
                if (mat_nr_ok)  {
                    pBlenderTexture =           &textureVector[mat_nr];
                    pinds =                     &meshPartInds[mat_nr];
                    pTriFaceFlags =             &meshPartTriFaceInfo[mat_nr];
                }

                for (int i=0;i<me->totpoly;i++) {
                    const Blender::MPoly& poly = me->mpoly[i];
                    if (mat_nr!= poly.mat_nr)	{
                        mat_nr = poly.mat_nr;

                        pBlenderTexture = NULL;pinds = NULL;pTriFaceFlags = NULL;

                        mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;
                        if (mat_nr_ok)  {
                            pBlenderTexture =           &textureVector[mat_nr];
                            pinds =                     &meshPartInds[mat_nr];
                            pTriFaceFlags =             &meshPartTriFaceInfo[mat_nr];
                        }
                    }
                    if (!pinds) continue;

                    //smoothingGroup = (poly.flag&1) ? 1 : -1;
                    //printf("%d ",smoothingGroup);
                    //flatShading=!(poly.flag&1);

                    // Flat shading should be probably inside poly.flag; but I don't know how to do it....
                    // Maybe we should store a std::vector<short> with poly.flag (one per triangle), and use it on the fly
                    // when we generate the display list (i.e. calculating the triface normal and replacing the triangle normals)
                    // But this solution won't be easily ported to vertex arrays ( display lists are WAY better ).

                    //if (strcmp(ob->id.name,"OBCube")==0)
                    //printf("Mesh:\"%s\": poly[%d]: flag=%u totloop=%d\n",me->id.name,i,poly.flag,poly.totloop);
                    const int numLoops = poly.totloop;
                    const Blender::MTexPoly* mtp = (pBlenderTexture->id>0 && me->mtpoly && me->mloopuv) ? &me->mtpoly[i] : NULL;

                    {
                        // Is triangle or quad:
                        if (numLoops==3 || numLoops==4) {

                            // draw first triangle
                            if (poly.loopstart+2>=me->totloop) {
                                printf("me->mtpoly ERROR: poly.loopstart+2(%d)>=me->totloop(%d)\n",poly.loopstart+2,me->totloop);
                                continue;
                            }

                            li0 = poly.loopstart;
                            li1 = poly.loopstart+1;
                            li2 = poly.loopstart+2;

                            vi0 = me->mloop[li0].v;
                            vi1 = me->mloop[li1].v;
                            vi2 = me->mloop[li2].v;

                            if (vi0 >= me->totvert || vi1 >= me->totvert || vi2 >= me->totvert) continue;

                            // Add (first) triangle:
                            pinds->push_back(vi0);
                            pinds->push_back(vi1);
                            pinds->push_back(vi2);
                            pTriFaceFlags->push_back(TriFaceInfo(poly.flag));


                            if (mtp) {
                                hasTexCoords = true;
                                const Blender::MLoopUV* uv = &me->mloopuv[li0];
                                pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                ic = vi0;
                                if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                    (*pinds)[pinds->size()-3] = ic2;


                                uv = &me->mloopuv[li1];
                                pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                ic = vi1;
                                if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                    (*pinds)[pinds->size()-2] = ic2;

                                uv = &me->mloopuv[li2];
                                pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                ic = vi2;
                                if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                    (*pinds)[pinds->size()-1] = ic2;
                            }

                            // isQuad
                            if (numLoops==4)	{
                                if (poly.loopstart+3>=me->totloop) {
                                    printf("me->mtpoly ERROR: poly.loopstart+3(%d)>=me->totloop(%d)\n",poly.loopstart+3,me->totloop);
                                    continue;
                                }
                                li3 = poly.loopstart+3;
                                vi3 = me->mloop[li3].v;if (vi3 >= me->totvert) continue;

                                // Add second triangle ----------------
                                pinds->push_back(vi2);
                                pinds->push_back(vi3);
                                pinds->push_back(vi0);
                                pTriFaceFlags->push_back(TriFaceInfo(poly.flag,1));


                                if (mtp) {
                                    hasTexCoords = true;
                                    const Blender::MLoopUV* uv = &me->mloopuv[li2];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                    ic = vi2;
                                    if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-3] = ic2;

                                    uv = &me->mloopuv[li3];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                    ic = vi3;
                                    if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-2] = ic2;

                                    uv = &me->mloopuv[li0];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                    ic = vi0;
                                    if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-1] = ic2;
                                }
                                //--------------------------------------
                            }


                        }
                        else if (numLoops>4)    {
                            //is n-gon
                            //--------------------------------------------------------------------
                            const int faceSize = poly.totloop;
                            int currentFirstVertPos = 0;
                            int currentLastVertPos = faceSize-1;
                            int remainingFaceSize = faceSize;
                            bool isQuad = true;
                            TriFaceInfo tfi(poly.flag);
                            do {
                                isQuad = remainingFaceSize>=4;

                                if (poly.loopstart + currentLastVertPos>=me->totloop) {
                                    printf("me->mtpoly ERROR: poly.loopstart + currentLastVertPos(%d)>=me->totloop(%d)\n",poly.loopstart + currentLastVertPos,me->totloop);
                                    break;
                                }

                                li0 = poly.loopstart + currentLastVertPos;
                                li1 = poly.loopstart + currentFirstVertPos;
                                li2 = poly.loopstart + currentFirstVertPos+1;

                                vi0 = me->mloop[li0].v;
                                vi1 = me->mloop[li1].v;
                                vi2 = me->mloop[li2].v;

                                if (vi0 >= me->totvert || vi1 >= me->totvert || vi2 >= me->totvert) break;

                                // Add triangle -----------------------------------------------------
                                pinds->push_back(vi0);
                                pinds->push_back(vi1);
                                pinds->push_back(vi2);
                                pTriFaceFlags->push_back(tfi);
                                tfi.belongsToLastPoly = 1;


                                if (mtp) {
                                    hasTexCoords = true;
                                    const Blender::MLoopUV* uv = &me->mloopuv[li0];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                    ic = vi0;
                                    if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-3] = ic2;

                                    uv = &me->mloopuv[li1];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                    ic = vi1;
                                    if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-2] = ic2;

                                    uv = &me->mloopuv[li2];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                    ic = vi2;
                                    if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-1] = ic2;
                                }
                                // -----------------------------------------------------------------------

                                if (isQuad) {
                                    if (poly.loopstart + currentLastVertPos>=me->totloop) {
                                        printf("me->mtpoly ERROR: poly.loopstart + currentLastVertPos(%d)>=me->totloop(%d)\n",poly.loopstart + currentLastVertPos,me->totloop);
                                        break;
                                    }

                                    li0 = poly.loopstart + currentFirstVertPos + 1;
                                    li1 = poly.loopstart + currentLastVertPos - 1;
                                    li2 = poly.loopstart + currentLastVertPos;

                                    vi0 = me->mloop[li0].v;
                                    vi1 = me->mloop[li1].v;
                                    vi2 = me->mloop[li2].v;

                                    if (vi0 >= me->totvert || vi1 >= me->totvert || vi2 >= me->totvert) break;

                                    // Add triangle -----------------------------------------------------
                                    pinds->push_back(vi0);
                                    pinds->push_back(vi1);
                                    pinds->push_back(vi2);
                                    pTriFaceFlags->push_back(tfi);


                                    if (mtp) {
                                        hasTexCoords = true;
                                        const Blender::MLoopUV* uv = &me->mloopuv[li0];
                                        pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                        ic = vi0;
                                        if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                            (*pinds)[pinds->size()-3] = ic2;

                                        uv = &me->mloopuv[li1];
                                        pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                        ic = vi1;
                                        if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                            (*pinds)[pinds->size()-2] = ic2;

                                        uv = &me->mloopuv[li2];
                                        pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc.v);
                                        ic = vi2;
                                        if ( (ic2=AddTexCoord(ic,tc,meshVerts,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                            (*pinds)[pinds->size()-1] = ic2;
                                    }
                                    // -----------------------------------------------------------------------
                                }

                                ++currentFirstVertPos;
                                --currentLastVertPos;
                                remainingFaceSize-=2;

                            } while (remainingFaceSize>=3);
                            //-------------------------------------------------------------------------------------------

                        }

                    }

                }



            }
            if (hasTexCoords) {
                const float vertsRatio = (float) verts.size()/(float)numSingleVerts;
                if (vertsRatio>2) {
                    printf("Warning: .blend object: '%s', without texture mapping would be %1.2f times faster.\n",ob->id.name,vertsRatio);
                }

                numSingleVerts = verts.size();
                numVerts = mirrorModifier.enabled ? numSingleVerts * mirrorModifier.getNumVertsMultiplier() : numSingleVerts;
                verts.reserve(numVerts);norms.reserve(numVerts);
                texCoords.reserve(numVerts);
            }
            else {
                std::vector < Vec2 > empty;
                texCoords.swap(empty);
            }
            //------------------------------------------------------------

//#           define DEBUG_VERTS_AND_INDS
#           ifdef DEBUG_VERTS_AND_INDS
            // DEBUG VERTS
            meshVerts.print();
            // DEBUG INDS
            for (int midx=0,midxSz=(int)meshPartInds.size();midx<midxSz;midx++) {
                const MeshPartInds& inds = meshPartInds[midx];
                printf("numInds[%d]=%d: {",midx,(int)inds.size());
                const int numIndsToDisplay = 6;
                for (int i=0,iSz=numIndsToDisplay<(int)inds.size()?numIndsToDisplay:(int)inds.size();i<iSz;i++) {
                    if (i>0) printf(",");
                    printf("%d",inds[i]);
                }
                printf("};\n");
                // Additional Check:
                for (int i=0,iSz=(int)inds.size();i<iSz;i++) {
                    if (inds[i]>=numSingleVerts) printf("Error: inds[%d]>=%d\n",i,numSingleVerts);
                }
            }
#           endif //DEBUG_VERTS_AND_INDS

            // APPLY MIRROR MODIFIER ----------------------------------
            if (mirrorModifier.enabled) {
                // VERTS ----------------------------------------------
                verts.resize(numVerts);
                norms.resize(numVerts);
                int startNumVerts = numSingleVerts;
                if (mirrorModifier.axisX) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vec3& v = verts[i];
                        verts[startNumVerts+i] = GetVec3(-v.v[0],v.v[1],v.v[2]);
                        const Vec3& n = norms[i];
                        norms[startNumVerts+i] = GetVec3(-n.v[0],n.v[1],n.v[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisY) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vec3& v = verts[i];
                        verts[startNumVerts+i]= GetVec3(v.v[0],-v.v[1],v.v[2]);
                        const Vec3& n = norms[i];
                        norms[startNumVerts+i] = GetVec3(n.v[0],-n.v[1],n.v[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vec3& v = verts[i];
                        verts[startNumVerts+i]=  GetVec3(v.v[0],v.v[1],-v.v[2]);
                        const Vec3& n = norms[i];
                        norms[startNumVerts+i] = GetVec3(n.v[0],n.v[1],-n.v[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisX && mirrorModifier.axisY) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vec3& v = verts[i];
                        verts[startNumVerts+i]=  GetVec3(-v.v[0],-v.v[1],v.v[2]);
                        const Vec3& n = norms[i];
                        norms[startNumVerts+i] = GetVec3(-n.v[0],-n.v[1],n.v[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisX && mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vec3& v = verts[i];
                        verts[startNumVerts+i]=  GetVec3(-v.v[0],v.v[1],-v.v[2]);
                        const Vec3& n = norms[i];
                        norms[startNumVerts+i] = GetVec3(-n.v[0],n.v[1],-n.v[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisY && mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vec3& v = verts[i];
                        verts[startNumVerts+i]=  GetVec3(v.v[0],-v.v[1],-v.v[2]);
                        const Vec3& n = norms[i];
                        norms[startNumVerts+i] = GetVec3(n.v[0],-n.v[1],-n.v[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisX && mirrorModifier.axisY && mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vec3& v = verts[i];
                        verts[startNumVerts+i]=  GetVec3(-v.v[0],-v.v[1],-v.v[2]);
                        const Vec3& n = norms[i];
                        norms[startNumVerts+i] = GetVec3(-n.v[0],n.v[1],n.v[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                //if (startNumVerts!=numVerts) fprintf(stderr,"MirrorModifier ob:%s startNumVerts(%d)!=numVerts(%d) numSingleVerts(%d)\n",ob->id.name,startNumVerts,numVerts,numSingleVerts);
                // TEXCOORDS -------------------------------------------
                if (hasTexCoords)   {
                    //const bool mustMirrorUV = mirrorModifier.mirrorU || mirrorModifier.mirrorV;
                    texCoords.resize(numVerts);
                    startNumVerts = numSingleVerts;
                    for (int axis = 0; axis < 7; axis++) {
                        if (axis==0 && !mirrorModifier.axisX) continue;
                        if (axis==1 && !mirrorModifier.axisY) continue;
                        if (axis==2 && !mirrorModifier.axisZ) continue;
                        if (axis==3 && !(mirrorModifier.axisX && mirrorModifier.axisY)) continue;
                        if (axis==4 && !(mirrorModifier.axisX && mirrorModifier.axisZ)) continue;
                        if (axis==5 && !(mirrorModifier.axisY && mirrorModifier.axisZ)) continue;
                        if (axis==6 && !(mirrorModifier.axisX && mirrorModifier.axisY && mirrorModifier.axisZ)) continue;
                        const bool invertFace = axis<3 || axis==6;
                        for (int i=0;i<numSingleVerts;i++) {
                            const Vec2& tc = texCoords[i];
                            if (invertFace) mirrorModifier.mirrorUV(tc.v,texCoords[startNumVerts+i].v);
                            else  texCoords[startNumVerts+i] =  tc;
                        }
                        startNumVerts += numSingleVerts;
                    }
                }

                // INDS -----------------------------------------------
                for (int i=0,sz = (int) meshPartInds.size();i<sz;i++)   {
                    IndexArray& inds = meshPartInds[i];
                    TriFaceInfoVector& triFaceFlags = meshPartTriFaceInfo[i];
                    //std::vector<int>& sginds = m.smoothingGroupsOfInds;
                    const int numSingleInds = inds.size();
                    //int startNumInds = 0;
                    int startNumVerts = numSingleVerts;
                    //fprintf(stderr,"inds.size()=%d sginds.size()=%d tinds.size()=%d texCoords.size()=%d verts.size()=%d numSingleVerts=%d numVerts=%d\n",
                    //inds.size(),sginds.size(),tinds.size(),texCoords.size(),verts.size(),numSingleVerts,numVerts);
                    // inds.size()=54 sginds.size()=54 tinds.size()=0 texCoords.size()=0 verts.size()=24 numSingleVerts=12 numVerts=24
                    //const int mirrorMult = mirrorModifier.getNumVertsMultiplier();
                    for (int axis = 0; axis < 7; axis++) {
                        if (axis==0 && !mirrorModifier.axisX) continue;
                        if (axis==1 && !mirrorModifier.axisY) continue;
                        if (axis==2 && !mirrorModifier.axisZ) continue;
                        if (axis==3 && !(mirrorModifier.axisX && mirrorModifier.axisY)) continue;
                        if (axis==4 && !(mirrorModifier.axisX && mirrorModifier.axisZ)) continue;
                        if (axis==5 && !(mirrorModifier.axisY && mirrorModifier.axisZ)) continue;
                        if (axis==6 && !(mirrorModifier.axisX && mirrorModifier.axisY && mirrorModifier.axisZ)) continue;

                        //printf("axis:%d\n",axis);

                        const bool invertFace = axis<3 || axis==6;
                        int i0,i1,i2,triCnt=0;
                        for (int i=0;i<numSingleInds;i+=3)
                        {
                            if (i+2>=numSingleInds) break;
                            if (invertFace) {i0=i;i1=i+2;i2=i+1;}
                            else            {i0=i;i1=i+1;i2=i+2;}

                            inds.push_back(startNumVerts + inds[i0]);
                            inds.push_back(startNumVerts + inds[i1]);
                            inds.push_back(startNumVerts + inds[i2]);
                            triFaceFlags.push_back(triFaceFlags[triCnt++]);
                        }
                        //startNumInds += numSingleInds;
                        startNumVerts +=numSingleVerts;
                    }
                }
                //-----------------------------------------------------
            }
            //---------------------------------------------------------



            // Convert To OPENGL coords:
            // Actually Blender uses a RIGHT HAND system too, but:
            /*    OpenGL coords                 Blender coords
             *                                      ^  _
             *         ^ Y                         Z|  /| Y
             *         |                            | /
             *         |______\ X                   |/_____\ X
             *        /       /                    /       /
             *       /                            /
             *     |_ Z                          / But Blender Front View Is -Y!
             *
             * So best thing to do is: newY = oldZ; newZ = -oldY [we reverse Blender Y so that front view is +Z]
            */

            // The following code is mandatory if we don't use the Blender Object Transform (when we apply object loc, rot and scale in Blender)
            // But we now leave the "Edit mode mesh" in the same Blender coords, and just adjust a twaked Object Transform
            /*
            float tmp;const bool hasNorms = verts.size()==norms.size();
            for (int i=0,iSz=(int)verts.size();i<iSz;i++) {
                // newY = oldZ; newZ = oldY
                Vec3& V = verts[i];
                tmp = V.v[1]; V.v[1]=V.v[2]; V.v[2]=-tmp;
                if (hasNorms) {
                    Vec3& N = norms[i];
                    tmp = N.v[1]; N.v[1]=N.v[2]; N.v[2]=-tmp;
                }
                if (hasTexCoords) {
                    Vec2& TC = texCoords[i];
                    // ???
                }
            }
            */

            /*int tmpi=0;   // This changes the triangle winding order
            for (int i=0;i<numMaterials;i++) {
                MeshPartInds& inds = meshPartInds[i];
                for (int id=1,idSz=(int)inds.size();id<idSz;id+=3) {
                    tmpi=inds[id];inds[id]=inds[id+1];inds[id+1]=tmpi;
                }
            }*/

            // delete unused mesh parts --------------------------------
            /*for (int i=startObjectIndex;i<(int)meshParts.size();i++)   {
            Part& m = meshParts[i];
            if (m.verts.size()==0 && m.inds.size()==0) {
                for (int j = i;j<(int)meshParts.size()-1;j++)   {
                    meshParts[j] = meshParts[j+1];  // terribly slow and not-reliable...
                }
                i--;
                meshParts.resize(meshParts.size()-1);
            }
        }*/

            // Create Display Lists:
            for (int i=0;i<numMaterials;i++) {
                if (!hasVerts) continue;
                MeshPart& mp = meshParts[i];
                const MeshPartInds& inds = meshPartInds[i];
                const TriFaceInfoVector& triFaceFlags = meshPartTriFaceInfo[i];
                Vec3 flatNormal = GetVec3(0,1,0);
                mp.displayListId = glGenLists(1);           // We should delete them somewhere...

                const bool hasNormals = meshVerts.norms.size()==meshVerts.verts.size();
                glNewList(mp.displayListId,GL_COMPILE);
                glBegin(GL_TRIANGLES);
                for (int triIdx=0,numTris=(int)inds.size()/3;triIdx<numTris;triIdx++)   {
                    const int triIdx3 = 3*triIdx;
                    const char triFaceFlag = triFaceFlags[triIdx].flags;
                    const bool hasFlatNormals = hasNormals && !(triFaceFlag&1); // Not sure this is right! At all...
                    if (hasFlatNormals)    {                    
                        // Flat Normal
#                       define USE_POLY_FLAT_NORMALS  // instead of single triangle flat normal (slower, small visible changes in my test files)
#                       ifndef USE_POLY_FLAT_NORMALS
                        const Vec3& v0 = meshVerts.verts[inds[triIdx3]];
                        const Vec3& v1 = meshVerts.verts[inds[triIdx3+1]];
                        const Vec3& v2 = meshVerts.verts[inds[triIdx3+2]];

                        const Vec3 v21 = GetVec3(v2.v[0]-v1.v[0],v2.v[1]-v1.v[1],v2.v[2]-v1.v[2]);
                        const Vec3 v01 = GetVec3(v0.v[0]-v1.v[0],v0.v[1]-v1.v[1],v0.v[2]-v1.v[2]);
                        flatNormal = Vec3Cross(v21,v01);
                        Vec3Normalize(flatNormal);
#                       else // USE_POLY_FLAT_NORMALS
                        const char belongsToLastPoly = triFaceFlags[triIdx].belongsToLastPoly;
                        if (!belongsToLastPoly) {
                            //int cnt = 0;
                            flatNormal.v[0]=flatNormal.v[1]=flatNormal.v[2]=0.f;
                            for (int trid=triIdx;trid<numTris && (trid==triIdx || triFaceFlags[trid].belongsToLastPoly);trid++)   {
                                const int trid3 = 3*trid;
                                const Vec3& v0 = meshVerts.verts[inds[trid3]];
                                const Vec3& v1 = meshVerts.verts[inds[trid3+1]];
                                const Vec3& v2 = meshVerts.verts[inds[trid3+2]];

                                const Vec3 v21 = GetVec3(v2.v[0]-v1.v[0],v2.v[1]-v1.v[1],v2.v[2]-v1.v[2]);
                                const Vec3 v01 = GetVec3(v0.v[0]-v1.v[0],v0.v[1]-v1.v[1],v0.v[2]-v1.v[2]);
                                Vec3 triNormal = Vec3Cross(v21,v01);
                                //Vec3Normalize(triNormal); // The modulo is the area AFAIR, so if we don't normalize,
                                // we got a weighted sum, which is better
                                flatNormal.v[0]+=triNormal.v[0];
                                flatNormal.v[1]+=triNormal.v[1];
                                flatNormal.v[2]+=triNormal.v[2];
                                //++cnt;
                            }
                            Vec3Normalize(flatNormal);
                        }
                        // else reuse flatNormal as it is
#                       endif //USE_POLY_FLAT_NORMALS
                    }
                    for (int j=triIdx3,jsz=triIdx3+3;j<jsz;j++)    {
                        const int idx = inds[j];
                        if (hasTexCoords) {
                            const Vec2& tc = meshVerts.texcoords[idx];
                            glTexCoord2f(tc.x,tc.y);
                        }
                        if (hasNormals) {
                            if (!hasFlatNormals)   {
                                // Smooth normals (I think not 100% correct, because we should
                                // smooth normals only inside their smoothing group
                                // So that we would need to do a lot of partial calculations of smooth normals
                                // We don't.)
                                const Vec3& n = meshVerts.norms[idx];
                                glNormal3f(n.x,n.y,n.x);
                            }
                            else glNormal3f(flatNormal.x,flatNormal.y,flatNormal.x);
                        }
                        const Vec3& v = meshVerts.verts[idx];
                        glVertex3f(v.x,v.y,v.z);
                    }
                }
                glEnd();
                glEndList();

            }

        }
    }
    //-----------------------------------------------------------------------

    // NOW NORMAL INITGL STUFF HERE------------------------------------------------------

    // These are important, but often overlooked, OpenGL calls
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Otherwise transparent objects are not displayed correctly
    glClearColor(0.3f, 0.6f, 1.0f, 1.0f);

    // Lighting
    const float globalAmbient[4] = {0.45f,0.45f,0.45f,1.f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,globalAmbient);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    Vec3Normalize(lightDirection);
    const float lightPos[4] = {lightDirection[0],lightDirection[1],lightDirection[2],0};
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
    const float lightAmb[4] = {0.45f,0.45f,0.45f,1};
    glLightfv(GL_LIGHT0,GL_AMBIENT,lightAmb);
    const float lightDif[4] = {0.8f,0.8f,0.8f,1};
    glLightfv(GL_LIGHT0,GL_DIFFUSE,lightDif);
    const float lightSpe[4] = {1.f,1.f,1.f,1};
    glLightfv(GL_LIGHT0,GL_SPECULAR,lightSpe);

    glEnable(GL_NORMALIZE);

    glEnable(GL_TEXTURE_2D);

}

void DestroyGL() {
    // We simply clear meshPartsContainer: but it's not that easy:
    // we must collect displayLists and textureIds
    std::vector<GLuint> textures,displayLists;
    std::vector<GLuint>::iterator it;
    for (int i=0,iSz=(int)meshPartsContainer.size();i<iSz;i++)    {
        MeshPartVector& meshParts = meshPartsContainer[i];
        for (int j=0;j<(int)meshParts.size();j++) {
            MeshPart& meshPart = meshParts[j];
            if (meshPart.textureId && (it=std::find(textures.begin(),textures.end(),meshPart.textureId))==textures.end())
                textures.push_back(meshPart.textureId);
            if (meshPart.displayListId && (it=std::find(displayLists.begin(),displayLists.end(),meshPart.displayListId))==displayLists.end())
                displayLists.push_back(meshPart.displayListId);
        }
        meshParts.clear();
    }
    meshPartsContainer.clear();
    for (int i=0;i<(int)textures.size();i++) {
        //printf("Deleting textureId=%d\n",textures[i]);
        glDeleteTextures(1,&textures[i]);
    }
    textures.clear();
    for (int i=0;i<(int)displayLists.size();i++) {
        //printf("Deleting displayListId=%d\n",displayLists[i]);
        glDeleteLists(displayLists[i],1);
    }
    displayLists.clear();
}




void DrawGL(void) 
{	
    // We need to calculate the "instantFrameTime" for smooth camera movement and
    // we also need stuff to calculate FPS.
    const unsigned timeNow = glutGet(GLUT_ELAPSED_TIME);
    static unsigned begin = 0;
    static unsigned cur_time = 0;
    unsigned elapsed_time,delta_time;
    static unsigned long frames = 0;
    static unsigned fps_time_start = 0;
    static float FPS = 60;
    // We need this because we'll keep modifying it to pass all the model matrices
    //static float mMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    // Here are some FPS calculations
    if (begin==0) begin = timeNow;
    elapsed_time = timeNow - begin;
    delta_time = elapsed_time - cur_time;    
    instantFrameTime = (float)delta_time*0.001f;
    cur_time = elapsed_time;
    if (fps_time_start==0) fps_time_start = timeNow;
    if ((++frames)%20==0) {
        // We update our FPS every 20 frames (= it's not instant FPS)
        FPS = (20*1000.f)/(float)(timeNow - fps_time_start);
        fps_time_start = timeNow;
    }

    // view Matrix
    LookAt(vMatrix,cameraPos[0],cameraPos[1],cameraPos[2],targetPos[0],targetPos[1],targetPos[2],0,1,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(vMatrix);

    glPushMatrix();
    // Draw something here-------------

    /*
    glPushMatrix(); {
        glTranslatef(0,-1,0);
        glColor3f(.1f,.4f,.2f);
        glScalef(1.f,0.1f,1.f);
        glutSolidCube(10);
    }
    glPopMatrix();

    glFrontFace(GL_CW); // These seem to be CW rendered
    glPushMatrix(); {
        glTranslatef(0,1,0);
        glColor3f(1.0f,1.0f,0.f);
        glutSolidTeapot(2.0);
    }
    glPopMatrix();

    glFrontFace(GL_CCW);
    */

    // Draw all objects with meshes inside the .blend file
    const int blenderLayersToDisplay = -1;   // It's a bit mask (eg. 1,2,4,...). -1 means all layers
    for (int i=0,iSz=(int)meshPartsContainer.size();i<iSz;i++)    {
        const MeshPartVector& mpv = meshPartsContainer[i];  // This should represent a single mesh AFAIR
        if (mpv.visible && (mpv.layer&blenderLayersToDisplay)!=0) {
            glPushMatrix();
            glMultMatrixf(mpv.mMatrix);
            glScalef(mpv.mScaling[0],mpv.mScaling[1],mpv.mScaling[2]);
            MeshPart::Draw(mpv);
            glPopMatrix();
        }
    }

    //----------------------------------
    glPopMatrix();

if (config.show_fps) {
    if ((elapsed_time/1000)%2==0)   {
        char tmp[64]="";
        sprintf(tmp,"FPS: %1.0f (%dx%d)\n",FPS,current_width,current_height);
        printf("%s",tmp);fflush(stdout);
        config.show_fps=0;
    }
}

}

static void GlutDestroyWindow(void);
static void GlutCreateWindow();

void GlutCloseWindow(void)  {
#ifndef __EMSCRIPTEN__
Config_Save(&config,ConfigFileName);
#endif //__EMSCRIPTEN__
}

void GlutNormalKeys(unsigned char key, int x, int y) {
    const int mod = glutGetModifiers();
    switch (key) {
#	ifndef __EMSCRIPTEN__	
    case 27: 	// esc key
        Config_Save(&config,ConfigFileName);
        GlutDestroyWindow();
#		ifdef __FREEGLUT_STD_H__
        glutLeaveMainLoop();
#		else
        exit(0);
#		endif
        break;
    case 13:	// return key
    {
        if (mod&GLUT_ACTIVE_CTRL) {
            config.fullscreen_enabled = gameModeWindowId ? 0 : 1;
            GlutDestroyWindow();
            GlutCreateWindow();
        }
    }
        break;
#endif //__EMSCRIPTEN__	
    }

}

static void updateCameraPos() {
    const float distanceY = sin(cameraPitch)*cameraDistance;
    const float distanceXZ = cos(cameraPitch)*cameraDistance;
    cameraPos[0] = targetPos[0] + sin(cameraYaw)*distanceXZ;
    cameraPos[1] = targetPos[1] + distanceY;
    cameraPos[2] = targetPos[2] + cos(cameraYaw)*distanceXZ;
}

static void resetCamera() {
    // You can set the initial camera position here through:
    targetPos[0]=0; targetPos[1]=0; targetPos[2]=0; // The camera target point
    cameraYaw = 2*M_PI;                             // The camera rotation around the Y axis
    cameraPitch = M_PI*0.1f;                      // The camera rotation around the XZ plane
    cameraDistance = 20;                             // The distance between the camera position and the camera target point

    updateCameraPos();
}

void GlutSpecialKeys(int key,int x,int y)
{
    const int mod = glutGetModifiers();
    if (!(mod&GLUT_ACTIVE_CTRL))	{
        switch (key) {
        case GLUT_KEY_LEFT:
        case GLUT_KEY_RIGHT:
            cameraYaw+= instantFrameTime*cameraSpeed*(key==GLUT_KEY_LEFT ? -4.0f : 4.0f);
            if (cameraYaw>M_PI) cameraYaw-=2*M_PI;
            else if (cameraYaw<=-M_PI) cameraYaw+=2*M_PI;
            updateCameraPos();		break;
        case GLUT_KEY_UP:
        case GLUT_KEY_DOWN:
            cameraPitch+= instantFrameTime*cameraSpeed*(key==GLUT_KEY_UP ? 2.f : -2.f);
            if (cameraPitch>M_PI-0.001f) cameraPitch=M_PI-0.001f;
            else if (cameraPitch<-M_PI*0.05f) cameraPitch=-M_PI*0.05f;
            updateCameraPos();
            break;
        case GLUT_KEY_PAGE_UP:
        case GLUT_KEY_PAGE_DOWN:
            cameraDistance+= instantFrameTime*cameraSpeed*(key==GLUT_KEY_PAGE_DOWN ? 25.0f : -25.0f);
            if (cameraDistance<1.f) cameraDistance=1.f;
            updateCameraPos();
            break;
        case GLUT_KEY_F2:
            config.show_fps = !config.show_fps;
            //printf("showFPS: %s.\n",config.show_fps?"ON":"OFF");
            break;
        case GLUT_KEY_HOME:
            // Reset the camera
            resetCamera();
            break;
        }
    }
    else if (mod&GLUT_ACTIVE_CTRL) {
        switch (key) {
        case GLUT_KEY_LEFT:
        case GLUT_KEY_RIGHT:
        case GLUT_KEY_UP:
        case GLUT_KEY_DOWN:
        {
            // Here we move targetPos and cameraPos at the same time

            // We must find a pivot relative to the camera here (ignoring Y)
            float forward[3] = {targetPos[0]-cameraPos[0],0,targetPos[2]-cameraPos[2]};
            float up[3] = {0,1,0};
            float left[3];

            Vec3Normalize(forward);
            Vec3Cross(left,up,forward);
            {
                float delta[3] = {0,0,0};int i;
                if (key==GLUT_KEY_LEFT || key==GLUT_KEY_RIGHT) {
                    float amount = instantFrameTime*cameraSpeed*(key==GLUT_KEY_RIGHT ? -25.0f : 25.0f);
                    for (i=0;i<3;i++) delta[i]+=amount*left[i];
                }
                else {
                    float amount = instantFrameTime*cameraSpeed*(key==GLUT_KEY_DOWN ? -25.0f : 25.0f);
                    for ( i=0;i<3;i++) delta[i]+=amount*forward[i];
                }
                for ( i=0;i<3;i++) {
                    targetPos[i]+=delta[i];
                    cameraPos[i]+=delta[i];
                }
            }
        }
        break;
        case GLUT_KEY_PAGE_UP:
        case GLUT_KEY_PAGE_DOWN:
            // We use world space coords here.
            targetPos[1]+= instantFrameTime*cameraSpeed*(key==GLUT_KEY_PAGE_DOWN ? -25.0f : 25.0f);
            if (targetPos[1]<-50.f) targetPos[1]=-50.f;
            else if (targetPos[1]>500.f) targetPos[1]=500.f;
            updateCameraPos();
        break;
        }
    }
}

void GlutMouse(int a,int b,int c,int d) {

}

// Note that we have used GlutFakeDrawGL() so that at startup
// the calling order is: InitGL(),ResizeGL(...),DrawGL()
// Also note that glutSwapBuffers() must NOT be called inside DrawGL()
static void GlutDrawGL(void)		{DrawGL();glutSwapBuffers();}
static void GlutIdle(void)			{glutPostRedisplay();}
static void GlutFakeDrawGL(void) 	{glutDisplayFunc(GlutDrawGL);}
void GlutDestroyWindow(void) {
    if (gameModeWindowId || windowId)	{
        DestroyGL();

        if (gameModeWindowId) {
#           ifndef __EMSCRIPTEN__
            glutLeaveGameMode();
#           endif
            gameModeWindowId = 0;
        }
        if (windowId) {
            glutDestroyWindow(windowId);
            windowId=0;
        }
    }
}
void GlutCreateWindow() {
    GlutDestroyWindow();
#   ifndef __EMSCRIPTEN__
    if (config.fullscreen_enabled)	{
        char gms[16]="";
        if (config.fullscreen_width>0 && config.fullscreen_height>0)	{
            sprintf(gms,"%dx%d:32",config.fullscreen_width,config.fullscreen_height);
            glutGameModeString(gms);
            if (glutGameModeGet (GLUT_GAME_MODE_POSSIBLE)) gameModeWindowId = glutEnterGameMode();
            else config.fullscreen_width=config.fullscreen_height=0;
        }
        if (gameModeWindowId==0)	{
            const int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
            const int screenHeight = glutGet(GLUT_SCREEN_HEIGHT);
            sprintf(gms,"%dx%d:32",screenWidth,screenHeight);
            glutGameModeString(gms);
            if (glutGameModeGet (GLUT_GAME_MODE_POSSIBLE)) gameModeWindowId = glutEnterGameMode();
        }
    }
#   endif
    if (!gameModeWindowId) {
        config.fullscreen_enabled = 0;
        glutInitWindowPosition(100,100);
        glutInitWindowSize(config.windowed_width,config.windowed_height);
        windowId = glutCreateWindow("testGlut.cpp");
    }

    glutKeyboardFunc(GlutNormalKeys);
    glutSpecialFunc(GlutSpecialKeys);
    glutMouseFunc(GlutMouse);
    glutIdleFunc(GlutIdle);
    glutReshapeFunc(ResizeGL);
    glutDisplayFunc(GlutFakeDrawGL);
#   ifdef __FREEGLUT_STD_H__
#   ifndef __EMSCRIPTEN__
    glutWMCloseFunc(GlutCloseWindow);
#   endif //__EMSCRIPTEN__
#   endif //__FREEGLUT_STD_H__

#ifdef USE_GLEW
    {
        GLenum err = glewInit();
        if( GLEW_OK != err ) {
            fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err) );
            return;
        }
    }
#endif //USE_GLEW


    InitGL();

}


int main(int argc, char** argv)
{

    int dummyArgc = 1;int* pArgc = &argc; // glutInit needs these (don't know why)
    if (argc>1) {blendfilepath = argv[1]; pArgc = &dummyArgc;}

    glutInit(pArgc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#ifndef __EMSCRIPTEN__
    //glutInitContextFlags(GLUT_FORWARD_COMPATIBLE);
#ifdef __FREEGLUT_STD_H__
    glutSetOption ( GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION ) ;
#endif //__FREEGLUT_STD_H__
#endif //__EMSCRIPTEN__

    Config_Init(&config);
#ifndef __EMSCRIPTEN__
    Config_Load(&config,ConfigFileName);
#endif //__EMSCRIPTEN__

    GlutCreateWindow();

    //OpenGL info
    printf("\nGL Vendor: %s\n", glGetString( GL_VENDOR ));
    printf("GL Renderer : %s\n", glGetString( GL_RENDERER ));
    printf("GL Version (string) : %s\n",  glGetString( GL_VERSION ));
    printf("GLSL Version : %s\n", glGetString( GL_SHADING_LANGUAGE_VERSION ));
    //printf("GL Extensions:\n%s\n",(char *) glGetString(GL_EXTENSIONS));

    printf("\nKEYS:\n");
    printf("AROW KEYS + PAGE_UP/PAGE_DOWN:\tmove camera (optionally with CTRL down)\n");
    printf("HOME KEY:\t\t\treset camera\n");
#	ifndef __EMSCRIPTEN__
    printf("CTRL+RETURN:\t\ttoggle fullscreen on/off\n");
#	endif //__EMSCRIPTEN__
    printf("F2:\t\t\tdisplay FPS\n");
    printf("\n");


    resetCamera();  // Mandatory

    glutMainLoop();

    DestroyGL();

    return 0;
}



