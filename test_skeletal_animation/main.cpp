// https://github.com/Flix01/fbtBlend-Header-Only
//
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

/*
The 3d model ManuelLab140_Test.blend is generated using the open source tool
“ManuelbastioniLAB” created by Manuel Bastioni (www.manuelbastioni.com) and
is licensed under CC-BY 4.0.
*/

// DEPENDENCIES:
/*
-> glut or freeglut (the latter is recommended)
-> glew (Windows only)
*/

// HOW TO COMPILE:
/*
// IMPORTANT:
// LINUX / MacOS:
g++ -O2 -no-pie *.cpp -o test_skeletal_animation -D"FBT_USE_GZ_FILE=1" -I"../" -I"./" -lglut -lGL -lz -lX11 -lm
// WINDOWS (here we use the static version of glew, and glut32.lib, that can be replaced by freeglut.lib):
cl /O2 /MT *.cpp /I"./" /I"../" /D"GLEW_STATIC"  /D"FBT_USE_GZ_FILE=1" /link /out:test_skeletal_animation.exe glut32.lib glew32s.lib opengl32.lib gdi32.lib zlib.lib Shell32.lib comdlg32.lib user32.lib kernel32.lib
// EMSCRIPTEN (untested):
em++ -O2 -D"FBT_USE_GZ_FILE=1" -I"../" -I"./" *.cpp --preload-file man4.blend  --preload-file ManuelLab140_Test.blend -o test_skeletal_animation.html -s USE_ZLIB=1 -s USE_WEBGL2=1 -s LEGACY_GL_EMULATION=0 --closure 1

// IN ADDITION:
By default the source file assumes that every OpenGL-related header is in "GL/".
But you can define in the command-line the correct paths you use in your system
for glut.h, glew.h, etc. with something like:
-DGLUT_PATH=\"Glut/glut.h\"
-DGLEW_PATH=\"Glew/glew.h\"
(this syntax works on Linux, don't know about Windows)
*/

/* What this demo should do:
   It's a dirty and bad-designed demo that shows how to use fbtBlend to load and play a .blend file with a skeletal animated mesh in it.
   Currently it does NOT read anything else (e.g. no cameras, lights, shape keys, etc.).
   The only additional thing (badly) implemented is the mirror modifier (because of course if we want to support any modifier, we must reimplement it manually).

   Also it's very possible that this implementation does not work with MOST of other .blend files (I've only tested these 2 models).
   For sure it won't work with IK bones (animations using them are just stored as they are in the .blend file and remapping them to
   the deformable bones they refer to is beyond the scope of this demo).
   So please don't overestimate this demo.

   The code is just a proof of the concept [i.e. bad style, no refactoring, just something that is here to test bftBlend.h].
   Correct extraction of all the .blend features is way beyond the scope of this project (please do not ask).
*/

/* POSSIBLE IMPROVEMENTS (I'll never make):
 *
 * -> Remove any reference to STL.
 * -> Remove all shared_ptrs.
 * -> Remove openGLTextureShaderAndBufferHelpers.h/.cpp and replace them with something simpler:
 *    -> Better remove all the shader stuff and write code that uses software skinning instead,
 *       so that it's usable with the fixed-function-pipeline too, and it's more flexible,
 *       'cause users need to replace the hard-coded shaders in any case to suit their needs.
 * -> [*] If possible rewrite math_helper.hpp in plain C (this would require almost a full rewrite of all code).
 * -> Rename mesh.h/mesh.cpp to something like animatedMesh.h/animatedMesh.cpp.
 * -> Write code to load/save an animated mesh from/to file (so that we can use the code without meshBlender.cpp and fbtBlend.h too).
 * -> Add (optionally) meshAssimp.cpp so that we can use the assimp library if necessary (I must have already an old version of this file somewhere...).

 * [*] UPDATE: about the rewrite of math_helper.hpp in plain C, I'm making: https://github.com/Flix01/Header-Only-GL-Helpers/blob/master/minimath.h
 *     It's a C/C++ math library. Once downloaded, it's possible to replace math_helper.hpp with minimath.h by defining USE_MINIMATH_H
 *     However it's not recommended: for this demo math_helper.hpp is probably more reliable (and that's why minimath.h is not included).
*/

#ifdef USE_MINIMATH_H
#   define MINIMATH_IMPLEMENTATION
#endif

#include "mesh.h"
#include <assert.h>

#ifndef M_PI
#	define M_PI 3.1415
#endif

// However there are still some model-specific tweeking set in InitGL()
const char* blendFilePath1 = "./man4.blend";                // I've made this myself
const char* blendFilePath2 = "./ManuelLab140_Test.blend";   // generated using the open source tool 'ManuelbastioniLAB' created by Manuel Bastioni (www.manuelbastioni.com) and licensed under CC-BY 4.0.

// Config file handling: basically there's an .ini file next to the
// exe that you can tweak. (it's just an extra)
const char* ConfigFileName = "test_skeletal_animation.ini";
typedef struct {
    int fullscreen_width,fullscreen_height;
    int windowed_width,windowed_height;
    int fullscreen_enabled;
} Config;
void Config_Init(Config* c) {
    c->fullscreen_width=c->fullscreen_height=0;
    c->windowed_width=480;c->windowed_height=480;
    c->fullscreen_enabled=0;
}
#ifndef __EMSCRIPTEN__
int Config_Load(Config* c,const char* filePath)  {
    FILE* f = fopen(filePath, "rt");  // same as fopen(...), but on Windows now filePath can be UTF8 (if this file is encoded as UTF8).
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
    FILE* f = fopen(filePath, "wt");
    if (!f)  return -1;
    fprintf(f, "[Size In Fullscreen Mode (zero means desktop size)]\n%d %d\n",c->fullscreen_width,c->fullscreen_height);
    fprintf(f, "[Size In Windowed Mode]\n%d %d\n",c->windowed_width,c->windowed_height);
    fprintf(f, "[Fullscreen Enabled (0 or 1) (CTRL+RETURN)]\n%d\n", c->fullscreen_enabled);
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
static const char* gEulerOrderModeStrings[6] = {"EULER_XYZ","EULER_XZY","EULER_YXZ","EULER_YZX","EULER_ZXY","EULER_ZYX"};   // Names for glm::EulerRotationMode enum
OpenGLSkinningShaderData gSkinningShaderData;
struct MeshInstanceData {
	MeshInstancePtr mesh;
    glm::vec3 offset;
    glm::vec3 scaling;
    glm::EulerRotationMode eulerRotationMode;
	// TODO: animation data here
    MeshInstanceData(const struct MeshBaseData* _baseData,MeshInstancePtr _mesh) : offset(0,0,0),scaling(1,1,1),eulerRotationMode(glm::EULER_YXZ)
    {
        mesh = _mesh;
        assert(_baseData && mesh);
        //assert(_baseData->meshBase);

        // Move these to the GUI:
        //mesh->center();
        //mesh->setHeight(2.0f);

        mesh->setBindingPose();
        //mesh->resetPose(0,0);
        updateValues(*_baseData);
    }
    void debugPrintf() const {
        fprintf(stderr,"offset:     %1.2f,%1.2f,%1.2f\n",offset.x,offset.y,offset.z);
        fprintf(stderr,"scaling:    %1.2f,%1.2f,%1.2f\n",scaling.x,scaling.y,scaling.z);
    }
    inline void updateValues(const MeshBaseData& baseData);
};
struct MeshBaseData {
    MeshPtr meshBase;
    bool mustCenterMesh;
    glm::vec3 rotation;
    glm::EulerRotationMode rotationMode;
    std::vector<MeshInstanceData> instances;
    MeshBaseData(MeshPtr _mesh) : meshBase(_mesh),mustCenterMesh(true),rotation(0,0,0),rotationMode(glm::EULER_XYZ) {
        instances.push_back(MeshInstanceData(this,MeshInstance::CreateFrom(_mesh)));
        assert(_mesh);
        assert(meshBase);

    }
    void debugPrintf() const {
        fprintf(stderr,"rotation (%s):   %1.2f,%1.2f,%1.2f\n",gEulerOrderModeStrings[rotationMode],rotation.x,rotation.y,rotation.z);
    }
};
void MeshInstanceData::updateValues(const MeshBaseData &baseData) {
    MeshInstance* ins = mesh.get(); // Just for intellisense in my IDE!
    assert(mesh && ins);
    assert(baseData.meshBase);
    // Clear transform:
    ins->resetShiftScaleRotate();
    if (baseData.mustCenterMesh) ins->center();
    // Apply mesh transform (does not work):
    //ins->shift(baseData.offset);
    //ins->scale(baseData.scaling);
    switch (baseData.rotationMode)  {
    case glm::EULER_XYZ:
    ins->rotate(baseData.rotation.x,glm::vec3(1,0,0));
    ins->rotate(baseData.rotation.y,glm::vec3(0,1,0));
    ins->rotate(baseData.rotation.z,glm::vec3(0,0,1));
    break;
    case glm::EULER_XZY:
    ins->rotate(baseData.rotation.x,glm::vec3(1,0,0));
    ins->rotate(baseData.rotation.z,glm::vec3(0,0,1));
    ins->rotate(baseData.rotation.y,glm::vec3(0,1,0));
    break;
    case glm::EULER_YXZ:
    ins->rotate(baseData.rotation.y,glm::vec3(0,1,0));
    ins->rotate(baseData.rotation.x,glm::vec3(1,0,0));
    ins->rotate(baseData.rotation.z,glm::vec3(0,0,1));
    break;
    case glm::EULER_YZX:
    ins->rotate(baseData.rotation.y,glm::vec3(0,1,0));
    ins->rotate(baseData.rotation.z,glm::vec3(0,0,1));
    ins->rotate(baseData.rotation.x,glm::vec3(1,0,0));
    break;
    case glm::EULER_ZXY:
    ins->rotate(baseData.rotation.z,glm::vec3(0,0,1));
    ins->rotate(baseData.rotation.x,glm::vec3(1,0,0));
    ins->rotate(baseData.rotation.y,glm::vec3(0,1,0));
    break;
    case glm::EULER_ZYX:
    ins->rotate(baseData.rotation.z,glm::vec3(0,0,1));
    ins->rotate(baseData.rotation.y,glm::vec3(0,1,0));
    ins->rotate(baseData.rotation.x,glm::vec3(1,0,0));
    break;
    default:
    break;
    }
    // Apply instance transform:
    ins->shift(offset);
    ins->scale(scaling);
    //ins->rotate(rotation.x,glm::vec3(1,0,0));
    //ins->rotate(rotation.y,glm::vec3(0,1,0));
    //ins->rotate(rotation.z,glm::vec3(0,0,1));
    //baseData->debugPrintf();
}
std::vector<MeshBaseData> meshes;

// camera data:
glm::vec3 cameraPos,targetPos;  // Derived value (do not edit)
float cameraYaw;                // please set it in resetCamera()
float cameraPitch;              // please set it in resetCamera()
float cameraDistance;           // please set it in resetCamera()
glm::mat4 vMatrix;
float cameraSpeed = 0.5f;       // When moving it

// pMatrix data:
glm::mat4  pMatrix;                  // projection matrix
const float pMatrixFovDeg = 50.f;
const float pMatrixNearPlane = 0.1f;
const float pMatrixFarPlane = 400.0f;

float instantFrameTime = 16.2f;

void InitGL() {
    //GLfloat globalAmbientIllumination[] = { 0.3, 0.3, 0.3, 1.0};
    //glLightModelfv(GL_LIGHT_MODEL_AMBIENT,globalAmbientIllumination);	// With shaders this is probably obsolete...
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

	gSkinningShaderData.init();

    // Optional: manually create Mesh::WhiteTextureId to support non-textured animated meshes (in shaders)
    if (!Mesh::WhiteTextureId)  {
        const unsigned char wt[16]={255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255};
        glGenTextures(1,&Mesh::WhiteTextureId);
        glBindTexture(GL_TEXTURE_2D,Mesh::WhiteTextureId);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0,(int) GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, wt);
        glBindTexture(GL_TEXTURE_2D,0);
    }

    // Lights:
    glLightSourceParams lightSourceParams(
                                          glm::vec3(0,-1,-1)  // minus = towards minus x (or y or z)
                                          //glm::vec3(0,1,-1)
                                          );    //Directional light (gets normalized)
    glLightSourceParams::GetInstances().push_back(lightSourceParams);   // However the problem is that we cannot add other lights without modifying the shader code (unless we use a uber-shader technique or a very slow shader)
    //--------------------------------------------------------------------------------------------------------------------
    gSkinningShaderData.setLightSourceParams(lightSourceParams,true);


    // Load Default Mesh 1
    if (blendFilePath1 && strlen(blendFilePath1)>0)  {
        MeshPtr meshBase1 = Mesh::Create();
        const bool loadOk1 = meshBase1->loadFromFile(blendFilePath1);
        if (meshBase1) {
            if (loadOk1) {
                //meshBase1->displayDebugInfo(false,false,false,false);//true,true,false,false);
                meshBase1->generateVertexArrays();   // Mandatory
                meshes.push_back(MeshBaseData(meshBase1));   // AFAIR, this creates a single instance too (we can create other instances Manuelly)

                MeshBaseData& mbd = meshes[meshes.size()-1];
                assert(mbd.instances.size()==1);
                mbd.rotation.x=90.0f;       // This number depends on the orientation of the spacific model!
                mbd.rotation.z=180.0f;      // This number depends on the orientation of the spacific model!
                // We need to notify all instances about this change (*)

                // But before we can set some specific change in mbd.instances[0]:
                mbd.instances[0].scaling.x=
                        mbd.instances[0].scaling.y=
                        mbd.instances[0].scaling.z=1.0f;
                mbd.instances[0].offset.x+=2.0f;

                // (*) update all instances
                for (size_t j=0;j<mbd.instances.size();j++)	{
                    MeshInstanceData& mid = mbd.instances[j];
                    mid.updateValues(mbd);
                }
            }
            else meshBase1.reset();
        }
    }
    // End Load Default Mesh 1

    // Load Default Mesh 2
    if (blendFilePath2 && strlen(blendFilePath2)>0)  {
        MeshPtr meshBase2 = Mesh::Create();
        const bool loadOk2 = meshBase2->loadFromFile(blendFilePath2);
        if (meshBase2) {
            if (loadOk2) {
                //meshBase2->displayDebugInfo(false,false,false,false);//true,true,false,false);
                meshBase2->generateVertexArrays();   // Mandatory
                meshes.push_back(MeshBaseData(meshBase2));   // AFAIR, this creates a single instance too (we can create other instances Manuelly)

                MeshBaseData& mbd = meshes[meshes.size()-1];
                assert(mbd.instances.size()==1);
                mbd.rotation.x=90.0f;       // This number depends on the orientation of the spacific model!
                mbd.rotation.z=180.0f;      // This number depends on the orientation of the spacific model!
                // We need to notify all instances about this change (*)

                // But before we can set some specific change in mbd.instances[0]:
                mbd.instances[0].scaling.x=
                        mbd.instances[0].scaling.y=
                        mbd.instances[0].scaling.z=4.0f;
                mbd.instances[0].offset.x-=0.5f;

                // (*) update all instances
                for (size_t j=0;j<mbd.instances.size();j++)	{
                    MeshInstanceData& mid = mbd.instances[j];
                    mid.updateValues(mbd);
                }
            }
            else meshBase2.reset();
        }
    }
    // End Load Default Mesh 2

}

void ResizeGL(int w, int h) {
    if (w==0 || h==0) return;
    glViewport(0,0,w,h);

    pMatrix = glm::perspective(glm::radians(pMatrixFovDeg),(GLfloat)w/(GLfloat)h,pMatrixNearPlane,pMatrixFarPlane);
    if (gSkinningShaderData.program) {
        glUseProgram(gSkinningShaderData.program);
        glUniformMatrix4fv(gSkinningShaderData.pMatrixUnifLoc,1,GL_FALSE,glm::value_ptr(pMatrix));
        glUseProgram(0);
    }
}

void DrawGL() {
    // We need to calculate the "instantFrameTime" for smooth camera movement and
    // we also need stuff to calculate FPS.
    const unsigned timeNow = glutGet(GLUT_ELAPSED_TIME);
    static unsigned begin = 0;
    static unsigned cur_time = 0;
    unsigned elapsed_time,delta_time;
    static unsigned long frames = 0;
    static unsigned fps_time_start = 0;
    static float FPS = 60;

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


	glClearColor(0.6f,0.6f,0.8f,1.0f);
  	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
	
    vMatrix = glm::lookAt(cameraPos,targetPos,glm::vec3(0,1,0));

    //-------------we need to update lights based on vMatrix-------------------------
    // ...(If we want to change the light position/direction, it's better to do it before the updateLights(...) call)
    glm::mat3 nMatrix;glLightSourceParams::CalculateNormalMatrix(vMatrix,nMatrix);
    //glLightSourceParams::GetInstances()[0] = -glm::vec3(nMatrix[0][2],nMatrix[1][2],nMatrix[2][2]); // Optional line to keep the directional light direction at the camera direction (commentable out)
    glLightSourceParams::GetInstances()[0] = -glm::vec3(nMatrix[0][2],nMatrix[1][2],nMatrix[2][2]); // Optional line to keep the directional light direction at the camera direction (commentable out)
    glLightSourceParams::updateLights(vMatrix,nMatrix); //(*)
    //-------------------------------------------------------------------------------


    // rendering:
    const GLfloat elapsedTimeMs = (GLfloat) glutGet(GLUT_ELAPSED_TIME);
    const GLfloat elapsedTimeSeconds = elapsedTimeMs*0.001f;

    if (gSkinningShaderData.program)    {
        glUseProgram(gSkinningShaderData.program);
        for (size_t i=0;i<meshes.size();i++)	{
            MeshBaseData& mbd = meshes[i];
            Mesh* meshBase = mbd.meshBase.get();

            for (size_t j=0;j<mbd.instances.size();j++)	{
                MeshInstanceData& mid = mbd.instances[j];
                MeshInstance* mesh = mid.mesh.get();   // For intellisense

                // Set the mesh instance position/orientation in world space (this can be done in any moment)
                //mesh->getModelMatrix() = glm::translate(glm::mat4(1),glm::vec3(0,0,0));

                //const size_t numBones = mesh->getNumBones();
                //printf("numBones=%d %d\n",numBones,mesh->getCurrentPoseTransforms().size());

                // Well, the truth is that in (*) we should have passed a vector of shader programs as well to be updated... we do it here:
                glUniform4fv(gSkinningShaderData.directional_lights0_vectorUnifLoc,1,glm::value_ptr(glLightSourceParams::GetInstances()[0].getPositionForShader()));

                const bool playAnimations = true;//false;//true;
                if (mbd.meshBase->getNumAnimations()>0 && playAnimations)	{
                    //-------------------------------------------------------------
                    static unsigned numCurrentAnimation = 0;
                    static unsigned currentAnimationStartTimeMs = (unsigned)elapsedTimeMs;
                    static unsigned currentAnimationlengthMs = (unsigned) (meshBase->getAnimationLength(numCurrentAnimation)*1000);
                    bool blendingTimeToTheFirstAnimationPoseInSeconds = 1.0f;    // This actually DOES NOT WORK (only works if it's 0 or it's 1 AFAICS...)
                    //--------------------------------------------------------------
#                 define KEEP_SWITCHING_ANIMATIONS
#                   ifdef KEEP_SWITCHING_ANIMATIONS
                    blendingTimeToTheFirstAnimationPoseInSeconds = 0;    // Mandatory (otherwise weird things occur)
                    meshBase->setPlayAnimationAgainWhenOvertime(true); // Mandatory (otherwise weird things occur)
                    if ((unsigned)elapsedTimeMs>=currentAnimationStartTimeMs+currentAnimationlengthMs*5)  // We let it loop 5 times
                    {
                        if (++numCurrentAnimation>=meshBase->getNumAnimations()) numCurrentAnimation=0;
                        mesh->resetPose(numCurrentAnimation,0);
                        currentAnimationlengthMs = (unsigned) (meshBase->getAnimationLength(numCurrentAnimation)*1000);
                        currentAnimationStartTimeMs=(unsigned)elapsedTimeMs;
                        //printf("numCurrentAnimation:%d\n",numCurrentAnimation);
                    }
#                   undef KEEP_SWITCHING_ANIMATIONS
#                   endif //KEEP_SWITCHING_ANIMATIONS
                    //-------------------------------------------------------------
                    mesh->setPose(numCurrentAnimation,elapsedTimeSeconds,blendingTimeToTheFirstAnimationPoseInSeconds);
                    //mesh->setBindingPose();
                    //meshBase->getBoneTransforms(0,timeElapsed,boneTransforms);
                    //meshBase->getBindingPoseBoneTransforms(boneTransforms);
                }
                else {
                    mesh->setBindingPose();
                    //meshBase->getBindingPoseBoneTransforms(boneTransforms);	//This basically resets to glm::mat(1) all the transforms (maybe not: double check it)
                    // Here we have the binding pose
                }

                mesh->render(gSkinningShaderData,vMatrix);
            }
        }
        glUseProgram(0);
    }

	glDisable(GL_CULL_FACE);
	
}

void DestroyGL() {
    if (Mesh::WhiteTextureId) {glDeleteTextures(1,&Mesh::WhiteTextureId);Mesh::WhiteTextureId=0;}
    gSkinningShaderData.destroy();
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
    cameraYaw = M_PI;                             // The camera rotation around the Y axis
    cameraPitch = M_PI*0.1f;                      // The camera rotation around the XZ plane
    cameraDistance = 10;                            // The distance between the camera position and the camera target point

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
            glm::vec3 forward(targetPos[0]-cameraPos[0],0,targetPos[2]-cameraPos[2]);
            glm::vec3 up(0,1,0);
            glm::vec3 left;

            forward = glm::normalize(forward);
            left = glm::cross(up,forward);
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
        char gms[32]="";
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
        windowId = glutCreateWindow("test_skeletal_animation");
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
    if (argc>1) {blendFilePath1 = argv[1]; pArgc = &dummyArgc;}
    if (argc>2) {blendFilePath2 = argv[2]; pArgc = &dummyArgc;}

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
    printf("\n");


    resetCamera();  // Mandatory

    glutMainLoop();

    DestroyGL();

    return 0;
}

