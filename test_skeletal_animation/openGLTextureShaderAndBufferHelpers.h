typedef unsigned GLuint;
typedef int GLint;
typedef char GLchar;
typedef float GLfloat;

extern void OpenGLFreeTexture(GLuint& texid);
extern void OpenGLGenerateOrUpdateTexture(GLuint& texid,int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);
extern GLuint OpenGLGenerateTexture(int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);


extern void OpenGLFlipTexturesVerticallyOnLoad(bool flag_true_if_should_flip);
extern GLuint OpenGLLoadTexture(const char* filename,int req_comp=0,bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);
extern GLuint OpenGLLoadTextureFromMemory(const unsigned char* filenameInMemory, int filenameInMemorySize, int req_comp=0, bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);



class OpenGLCompileShaderStruct {
public:
    GLuint vertexShaderOverride,fragmentShaderOverride; // when !=0, the shader codes will be ignored, and these will be used.
    GLuint programOverride;             // when !=0, the shaders will be ignored completely.
    bool dontLinkProgram;               // when "true", shaders are attached to the program, but not linked, the program is returned to "programOverride" and the shaders are returned to "vertexShaderOverride" and "fragmentShaderOverride" if "dontDeleteAttachedShaders" is true.
                                        // however, when "false" and linking is performed, then the shaders are detached and deleted IN ANY CASE.
    bool dontDeleteAttachedShaders;     // After the program is linked, by default all the attached shaders are deleted IN ANY CASE.
public:
    void reset() {vertexShaderOverride = fragmentShaderOverride = programOverride = 0;dontLinkProgram = dontDeleteAttachedShaders = false;}
    OpenGLCompileShaderStruct() {reset();}
};
// returns the shader program ID. "pOptionalOptions" can be used to tune dynamic definitions inside the shader code and some shader compilation and linking processing steps.
extern GLuint OpenGLCompileShadersFromMemory(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource,OpenGLCompileShaderStruct* pOptionalOptions=0);
// returns the shader program ID. "pOptionalOptions" can be used to tune dynamic definitions inside the shader code and some shader compilation and linking processing steps.
extern GLuint OpenGLCompileShadersFromFile(const char* vertexShaderFilePath, const char* fragmentShaderFilePath,OpenGLCompileShaderStruct* pOptionalOptions=0);


// C++ Light And Material Structs (to pass to the shader) -----------------------------------------------------------------------------

//#include <glm/glm.hpp>
//#include <glm/ext.hpp>                  // glm::perspective(...), glm::value_ptr(...),etc.
#include <math_helper.hpp>

#include <vector>
using std::vector;
#include <stdio.h>                      // printf
#include <string>
using std::string;

namespace glm
{
const glm::vec4 BLACK(0.000,0.000,0.000,1.0);
const glm::vec4 BLUE(0.000,0.000,1.000,1.0);
const glm::vec4 BROWN(0.647,0.164,0.164,1.0);
const glm::vec4 DARKBLUE(0.000,0.000,0.545,1.0);
const glm::vec4 DARKGRAY(0.662,0.662,0.662,1.0);
const glm::vec4 DARKGREEN(0.000,0.545,0.000,1.0);
const glm::vec4 DARKRED(0.545,0.000,0.000,1.0);
const glm::vec4 GREEN(0.000,1.000,0.000,1.0);
const glm::vec4 LIGHTBLUE(0.678,0.847,0.901,1.0);
const glm::vec4 LIGHTGRAY(0.827,0.827,0.827,1.0);
const glm::vec4 LIGHTGREEN(0.564,0.933,0.564,1.0);
const glm::vec4 ORANGE(1.000,0.647,0.000,1.0);
const glm::vec4 PURPLE(0.501,0.000,0.501,1.0);
const glm::vec4 RED(1.000,0.000,0.000,1.0);
const glm::vec4 WHITE(1.000,1.000,1.000,1.0);
const glm::vec4 YELLOW(1.000,1.000,0.000,1.0);
}   //glm

struct glMaterialParams
{
   glm::vec4 emission;    // Ecm
   glm::vec4 ambient;     // Acm
   glm::vec4 diffuse;     // Dcm
   glm::vec4 specular;    // Scm
   GLfloat shininess;  // Srm
   glMaterialParams(const glm::vec4& Diffuse=glm::vec4(0.8f,0.8f,0.8f,1.0f))
       : emission(glm::BLACK),ambient(Diffuse.x*0.4f,Diffuse.y*0.4f,Diffuse.z*0.4f,1.0f),diffuse(Diffuse),specular(glm::BLACK),shininess(0.2f)
   {}
};

struct glLightSourceParams
{
   glm::vec4 ambient;              // Aclarri
   glm::vec4 diffuse;              // Dcli
   glm::vec4 specular;             // Scli
   glm::vec4 position;             // Ppli  position.w = quadratic attenuation of the point light
   bool isDirectional;              // in that case position.xyz must be a normalized vector
                                    // and no attenuation is used
   bool isSpotLight;
   bool isSpotShadow;
   glm::vec3 spotDirection;
   glm::vec2 spotInnerAndOuterCosConeAngle; // (0.8,0.5)    //x > y MANDATORY! The bigger the number is (but <1!), the smaller the angle is

   glLightSourceParams(const glm::vec4& PositionAndQuadraticAttenuation=glm::vec4(20,50,75,0.0025f),const glm::vec4& Diffuse=glm::WHITE,
                       const glm::vec3* pSpotDirectionNormalized=NULL,const glm::vec2* pSpotInnerAndOuterCosConeAngle=NULL,bool _isSpotShadow=false)
       : ambient(Diffuse.x*0.4f,Diffuse.y*0.4f,Diffuse.z*0.4f,1.0f),diffuse(Diffuse),specular(glm::WHITE),
         position(PositionAndQuadraticAttenuation),isDirectional(false),
         isSpotLight(pSpotDirectionNormalized ? true : false),isSpotShadow(_isSpotShadow),spotDirection(pSpotDirectionNormalized ? *pSpotDirectionNormalized : glm::vec3(0,-1,0)),
         spotInnerAndOuterCosConeAngle(pSpotInnerAndOuterCosConeAngle ? *pSpotInnerAndOuterCosConeAngle : glm::vec2(0.8,0.5))
   {
       if (isSpotShadow) isSpotLight = false;
   }
   glLightSourceParams(const glm::vec3& normalizedDirection,const glm::vec4& Diffuse=glm::WHITE)
       : ambient(Diffuse.x*0.4f,Diffuse.y*0.4f,Diffuse.z*0.4f,1.0f),diffuse(Diffuse),specular(glm::WHITE),
         position(normalizedDirection.x,normalizedDirection.y,normalizedDirection.z,1.0f),isDirectional(true),
         isSpotLight(false),isSpotShadow(false),spotDirection(0,-1,0),spotInnerAndOuterCosConeAngle(0.8,0.5)
   {}

// This must be used to add/remove lights
inline static vector<glLightSourceParams>& GetInstances() {
    return _Get();
}


static void CalculateNormalMatrix(const glm::mat4& vMatrix,glm::mat3& nMatrixOut)
{
    nMatrixOut = glm::transpose(glm::mat3_cast(glm::inverse(vMatrix)));  //TODO: check (there must be a good paper about it somewhere...)
    // Isn't this equal to: nMatrixOut = glm::mat3(vMatrix); for orthonormal matrices (which vMatrix is) ?
}


inline static void updateLights(const glm::mat4& vMatrix,const glm::mat3 nMatrix)
{
    updateLights(_Get(),vMatrix,nMatrix);

}

// updateLights() must be called once per frame before retrieveing these values:
const glm::vec4& getPositionForShader() const {return _position;}
const glm::vec3& getSpotDirectionForShader() const {return _spotDirection;}



protected:
   glm::vec4 _position;             // private: calculated by update(...)/updateLights(...)
   glm::vec3 _spotDirection;        // idem

void update(const glm::mat4& vMatrix,const glm::mat3 nMatrix) {
        if (!isDirectional) {
            _position = vMatrix* glm::vec4(glm::vec3(position),1.0f);//1.0 is MANDATORY!
            _position.w = position.w;   // attenuation
            if (isSpotLight || isSpotShadow) _spotDirection = //glm::normalize(glm::vec3(getModelviewMatrix()* glm::vec4(glm::vec3(spotDirection),0.0f)));

                                                //glm::normalize(
                                                    nMatrix *  spotDirection
                                                  //)
                                                    ;

        }
        else _position = //glm::vec4(glm::normalize(glm::vec3(getModelviewMatrix()* glm::vec4(glm::vec3(position),0.0f))),1.0);

                            glm::vec4(
                                     //glm::normalize(
                                         nMatrix * glm::vec3(position)
                                         //)
                                     ,0);
}
inline static void updateLights(std::vector<glLightSourceParams>& lights,const glm::mat4& vMatrix,const glm::mat3 nMatrix)
{
    for (size_t i=0,sz=lights.size();i<sz;i++)
    {
        glLightSourceParams& L = lights[i];
        L.update(vMatrix,nMatrix);
    }
}

   inline static vector<glLightSourceParams>& _Get() {
       static vector<glLightSourceParams> lsp;
       return lsp;
   }
};


struct glLightModelParams
{
private:
    glm::vec4 ambient;
    glm::vec4 fogColorAndDensity;
public:
    static glLightModelParams instance;
    void setAmbientColor(const glm::vec4& amb)  {ambient=amb;}
    void setFogColorAndDensity(const glm::vec4& fog)  {fogColorAndDensity=fog;}
    void setFogColor(const glm::vec4& fog)  {fogColorAndDensity.x=fog.x;fogColorAndDensity.y=fog.y;fogColorAndDensity.z=fog.z;}
    void setFogDensity(GLfloat value)  {fogColorAndDensity.w=value;}
    const glm::vec4& getAmbientColor() const {return ambient;}
    const glm::vec4& getFogColorAndDensity() const {return fogColorAndDensity;}
    const glm::vec3 getFogColor() const {return glm::vec3(fogColorAndDensity.x,fogColorAndDensity.y,fogColorAndDensity.z);}
    GLfloat getFogDensity() const {return fogColorAndDensity.w;}

    inline static glLightModelParams& GetInstance() {return _Get();}

protected:
    glLightModelParams(const glm::vec4& amb=glm::vec4(0.25,0.25,0.25,1.0),const glm::vec4& fog=glm::vec4(0.6,0.8,0.8,0.01))
        : ambient(amb),fogColorAndDensity(fog)
    {}
    inline static glLightModelParams& _Get() {
        static glLightModelParams lmp;
        return lmp;
    }
    friend struct OpenGLSkinningShaderData;
};
//---------------------------------------------------------------------------------------------------------------------------------------

struct OpenGLSkinningShaderData {
GLuint program;

// attribute locations
GLint vertexAttrLoc,			// attribute vec4 a_vertex;       // vertex in object space
normalAttrLoc,					// attribute vec3 a_normal;       // normal in object space
texcoordsAttrLoc,				// attribute vec2 a_texcoords;     // #ifdef TEXTURE inside shader code
boneIndsAttrLoc,				// attribute ivec4/vec4 a_boneInds;	// type depends on #ifdef USE_INTEGER_BONE_INDICES inside shader code
boneWeightsAttrLoc,				// attribute vec4 a_boneWeights;

// uniform locations
textureUnit0UnifLoc,			// uniform sampler2D	u_textureUnit0;	// #ifdef TEXTURE inside shader code
/*struct MaterialParams	{
   vec4 emission;
   vec4 ambient;
   vec4 diffuse;
   #ifdef SPECULAR
   vec4 specular;
   float shininess;
   #endif
};*/
material_emissionUnifLoc,		// uniform MaterialParams u_material;  // The material of the object to render (u_material.specular.a is used to tune the power of the specular highlight)
material_ambientUnifLoc,	
material_diffuseUnifLoc,
material_specularUnifLoc,		// #ifdef SPECULAR inside shader code
material_shininessUnifLoc,		// #ifdef SPECULAR inside shader code
/*struct LightModelParams	{
	vec4 ambient;
	#ifdef USE_FOG
	vec4 fogColorAndDensity;
	#endif //USE_FOG
};*/
lightModel_ambientUnifLoc,		// uniform LightModelParams u_lightModel;	// A single global uniform
//lightModel_fogColorAndDensityUnifLoc,	// #ifdef USE_FOG inside shader code
pMatrixUnifLoc,					// uniform mat4 u_pMatrix;       	// A constant representing the camera projection matrix.
mvMatrixUnifLoc,				// uniform mat4 u_mvMatrix;        // A constant representing the combined ORTHONORMAL model/view matrix (i.e. without any scaling applied).
mMatrixScalingUnifLoc,			// uniform vec3 u_mMatrixScaling;	// default (1,1,1). Note that the scale usually refers to the model matrix only, since the view matrix is usually orthonormal (gluLookAt(...) and similiar are OK)
/*struct LightParams	{
    vec4 ambient;
    vec4 diffuse;
    #ifdef SPECULAR
    vec4 specular;
    #endif
    vec4 vector; 	// normalized light direction in eye space for point and spot lights, otherwise normalized light direction in eye space
			// For point and spot lights the last component is the attenuation factor (0 = no attenuation)

	#if NUM_SPOT_LIGHTS>0
		vec3 spotDirection;	// normalized spot direction in eye space
		vec2 spotInnerAndOuterCosConeAngle;	//.x = spotCosCutoff; (0.6,0.8)	// 0.8 = 36 degrees
	#endif //NUM_SPOT_LIGHTS
};
#if NUM_DIRECTIONAL_LIGHTS>0
uniform LightParams u_directional_lights[NUM_DIRECTIONAL_LIGHTS];
#endif //NUM_DIRECTIONAL_LIGHTS
#if NUM_POINT_LIGHTS>0
uniform LightParams u_point_lights[NUM_POINT_LIGHTS];
#endif //NUM_POINT_LIGHTS
#if NUM_SPOT_LIGHTS>0
uniform LightParams u_spot_lights[NUM_SPOT_LIGHTS];
#endif //NUM_SPOT_LIGHTS*/
directional_lights0_ambientUnifLoc,			// NUM_DIRECTIONAL_LIGHTS=1, NUM_POINT_LIGHTS=0, NUM_SPOT_LIGHTS=0
directional_lights0_diffuseUnifLoc,
directional_lights0_specularUnifLoc,		// #ifdef SPECULAR inside sader code
directional_lights0_vectorUnifLoc,
boneMatricesUnifLoc;						// uniform mat4 u_boneMatrices[MAX_NUM_BONE_MATRICES];


OpenGLSkinningShaderData() {program=0;}
~OpenGLSkinningShaderData() {destroy();}

void init();
void destroy();

bool setLightSourceParams(const glLightSourceParams &lightSourceParams, bool mustBindAndUnbindProgram);


};



// From Now on we need some STL (and shared_ptr...): TODO: remove if possible
#include <vector>
/*#include <memory>       // shared_ptr needs C++11 (is there some way to avoid it?)
using std::shared_ptr;*/  // Probably without C++11 we can't use "using" with template classes (but this issue can be solved using multiple statements)
#include "shared_ptr.hpp"   // stand-alone version (no boost or C++11 required)

// GL types and calls are included by this header
#include "opengl_includer.h"

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(i) ((char*)NULL + (i))
#endif //BUFFER_OFFSET

typedef shared_ptr<class GLMiniTriangleBatch> GLMiniTriangleBatchPtr;
class GLMiniTriangleBatch
{
public:
#ifdef GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
	typedef glm::uvec4 BoneIndsType;
    typedef GLuint BoneIndsComponentType;
#else //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
    typedef glm::vec4 BoneIndsType;
    typedef GLfloat BoneIndsComponentType;
#endif //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES

    static GLMiniTriangleBatchPtr Create() {return GLMiniTriangleBatchPtr(new GLMiniTriangleBatch());}

    GLMiniTriangleBatch();
    virtual ~GLMiniTriangleBatch();
    virtual void destroyGL();

    // Must be called BEFORE calling upload() the first time (better just use the same attribute name bindings in all shaders and call this just once)
    bool glSetVertexAttributeArrayIndices(GLuint program,
                                        const char* vertexAttributeNameInShader="a_vertex",
                                        const char* colorAttributeNameInShader="a_color",
                                        const char* normalAttributeNameInShader="a_normal",
                                        const char* tangentAttributeNameInShader="a_tangent",
                                        const char* texcoordsAttributeNameInShader="a_texcoords",
                                        const char* boneIndsAttributeNameInShader="a_boneInds",
                                        const char* boneWeightsAttributeNameInShader="a_boneWeights",
                                        bool verbose = false
                                        );
    // Usable for both creating and updating all or parts of the gpu buffer
    template < typename GLMiniTriangleBatchIndexType > inline void upload(const std::vector < GLMiniTriangleBatchIndexType >* pinds=NULL,
                const std::vector < glm::vec3 >* pverts=NULL,
                const std::vector < glm::vec4 >* pcolors=NULL,
                const std::vector < glm::vec3 >* pnorms=NULL,
                const std::vector < glm::vec4 >* ptangs=NULL,
                const std::vector < glm::vec2 >* ptexCoords=NULL,
                const std::vector < BoneIndsType >* pboneInds=NULL,
                const std::vector < glm::vec4 >* pboneWeights=NULL,
                GLenum drawType = GL_STATIC_DRAW,
                GLenum drawTypeIA = GL_STATIC_DRAW,
                bool useVertexArrayObject = true
                )
    {
        const size_t vec4SizeInGLfloatUnits = sizeof(glm::vec4)/sizeof(GLfloat);
        const size_t vec3SizeInGLfloatUnits = sizeof(glm::vec3)/sizeof(GLfloat);
        const size_t vec2SizeInGLfloatUnits = sizeof(glm::vec2)/sizeof(GLfloat);
        const size_t uvec4SizeInGLuintUnits = sizeof(BoneIndsType)/sizeof(BoneIndsComponentType);

        upload<GLMiniTriangleBatchIndexType>(
                    (pinds && pinds->size()>0) ? &((*pinds)[0]) : static_cast<const GLMiniTriangleBatchIndexType*>(NULL),
                    pinds ? pinds->size() : 0,
                    (pverts && pverts->size()>0) ? &((*pverts)[0].x) : NULL,
                    pverts ? pverts->size() : 0,
                    vec3SizeInGLfloatUnits,
                    (pcolors && pcolors->size()>0) ? &((*pcolors)[0].x) : NULL,
                    pcolors ? pcolors->size() : 0,
                    vec4SizeInGLfloatUnits,
                    (pnorms && pnorms->size()>0) ? &((*pnorms)[0].x) : NULL,
                    pnorms ? pnorms->size() : 0,
                    vec3SizeInGLfloatUnits,
                    (ptangs && ptangs->size()>0) ? &((*ptangs)[0].x) : NULL,
                    ptangs ? ptangs->size() : 0,
                    vec4SizeInGLfloatUnits,
                    (ptexCoords && ptexCoords->size()>0) ? &((*ptexCoords)[0].x) : NULL,
                    ptexCoords ? ptexCoords->size() : 0,                    
                    vec2SizeInGLfloatUnits,
                    (pboneInds && pboneInds->size()>0) ? &((*pboneInds)[0].x) : NULL,
                    pboneInds ? pboneInds->size() : 0,
                    uvec4SizeInGLuintUnits,
                    (pboneWeights && pboneWeights->size()>0) ? &((*pboneWeights)[0].x) : NULL,
                    pboneWeights ? pboneWeights->size() : 0,
                    vec4SizeInGLfloatUnits,


                    drawType,
                    drawTypeIA,
                    useVertexArrayObject
                    );
    }

    // Important: ptangs and pcolors, if present, must have 4 components, not 3:
    template < typename GLMiniTriangleBatchIndexType > void upload(
                const GLMiniTriangleBatchIndexType* pinds=NULL,
                size_t numInds = 0,
                const GLfloat* pverts=NULL,
                size_t numVerts = 0,
                size_t vertsStrideInGLfloatUnits = 3,
                const GLfloat* pnorms=NULL,
                size_t numNorms = 0,
                size_t normsStrideInGLfloatUnits = 3,
                const GLfloat* pcolors=NULL,
                size_t numColors = 0,
                size_t colorsStrideInGLfloatUnits = 4,
                const GLfloat* ptangs=NULL,
                size_t numTangs = 0,
                size_t tangsStrideInGLfloatUnits = 4,
                const GLfloat* ptexCoords=NULL,
                size_t numTexCoords = 0,
                size_t texCoordsStrideInGLfloatUnits = 2,
                const BoneIndsComponentType* pboneInds=NULL,
                size_t numBoneInds = 0,
                size_t boneIndsStrideInGLuintUnits = 4,
                const GLfloat* pboneWeights=NULL,
                size_t numBoneWeights = 0,
                size_t boneWeightsStrideInGLfloatUnits = 4,
                GLenum drawType = GL_STATIC_DRAW,
                GLenum drawTypeIA = GL_STATIC_DRAW,
                bool useVertexArrayObject = true
                );

    inline void enableVertexArrayAttribs(bool enableUnusedArraysToo=false)
    {
        glEnableVertexAttribArray(vertsAttributeIndex);
        if (colorsOffsetInBytes>0 || enableUnusedArraysToo)      glEnableVertexAttribArray(colorsAttributeIndex);
        if (normsOffsetInBytes>0 || enableUnusedArraysToo)       glEnableVertexAttribArray(normsAttributeIndex);
        if (tangsOffsetInBytes>0 || enableUnusedArraysToo)       glEnableVertexAttribArray(tangsAttributeIndex);
        if (texCoordsOffsetInBytes>0 || enableUnusedArraysToo)   glEnableVertexAttribArray(texCoordsAttributeIndex);
        if (boneIndsOffsetInBytes>0 || enableUnusedArraysToo)   glEnableVertexAttribArray(boneIndsAttributeIndex);
        if (boneWeightsOffsetInBytes>0 || enableUnusedArraysToo)   glEnableVertexAttribArray(boneWeightsAttributeIndex);
    }
    inline bool bindBuffers(bool autoEnableVertexArrayAttribs = true)
    {
        if (!vbo || !vboIA) return false;

        if (vao) glBindVertexArray(vao);
        else    {
            glBindBuffer(GL_ARRAY_BUFFER,vbo);

            glVertexAttribPointer(vertsAttributeIndex, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
            if (colorsOffsetInBytes>0)      glVertexAttribPointer(colorsAttributeIndex, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(colorsOffsetInBytes));
            if (normsOffsetInBytes>0)       glVertexAttribPointer(normsAttributeIndex, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(normsOffsetInBytes));
            if (tangsOffsetInBytes>0)       glVertexAttribPointer(tangsAttributeIndex, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(tangsOffsetInBytes));
            if (texCoordsOffsetInBytes>0)   glVertexAttribPointer(texCoordsAttributeIndex, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(texCoordsOffsetInBytes));
            if (boneIndsOffsetInBytes>0)       {
#               ifdef GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
                glVertexAttribIPointer(boneIndsAttributeIndex, 4, GL_INT, 0, BUFFER_OFFSET(boneIndsOffsetInBytes));
#               else //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
                glVertexAttribIPointer(boneIndsAttributeIndex, 4, GL_FLOAT, 0, BUFFER_OFFSET(boneIndsOffsetInBytes));
#               endif //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
            }
            if (boneWeightsOffsetInBytes>0)   glVertexAttribPointer(boneWeightsAttributeIndex, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(boneWeightsOffsetInBytes));
            if (autoEnableVertexArrayAttribs)   enableVertexArrayAttribs();

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,vboIA);
        }
        return true;
    }
    inline void drawGL(GLint startIndex=0,GLsizei indexCount=-1,bool autoBindAndUnbindBuffers=true,bool autoEnableDisableVertexArrayAttribs=true)   {
        if (autoBindAndUnbindBuffers) bindBuffers(autoEnableDisableVertexArrayAttribs);
        else if (autoEnableDisableVertexArrayAttribs) enableVertexArrayAttribs();
        glDrawElements(primitive,indexCount==-1 ? gpuElementArrayIndsSize : indexCount,indexTypeSizeInBytes==2 ? GL_UNSIGNED_SHORT : indexTypeSizeInBytes==1 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_INT,BUFFER_OFFSET(startIndex*indexTypeSizeInBytes));
        if (autoBindAndUnbindBuffers) unbindBuffers(autoEnableDisableVertexArrayAttribs);
        else if (autoEnableDisableVertexArrayAttribs) disableVertexArrayAttribs();
    }
    inline void drawGL(bool autoBindAndUnbindBuffers,bool autoEnableDisableVertexArrayAttribs=true)   {
        drawGL(0,gpuArrayVertsSize,autoBindAndUnbindBuffers,autoEnableDisableVertexArrayAttribs);
    }
    inline bool unbindBuffers(bool autoEnableVertexArrayAttribs=true)
    {
        if (!vbo || !vboIA) return false;

        if (vao) glBindVertexArray(0);
        else    {
            // branch WITHOUT vao:
            glBindBuffer(GL_ARRAY_BUFFER,0);                                  
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);                          
            if (autoEnableVertexArrayAttribs)   disableVertexArrayAttribs();
        }
        return true;
    }
    inline void disableVertexArrayAttribs(bool disableUnusedArraysToo=false)
    {
        glDisableVertexAttribArray(vertsAttributeIndex);
        if (colorsOffsetInBytes>0 || disableUnusedArraysToo)      glDisableVertexAttribArray(colorsAttributeIndex);
        if (normsOffsetInBytes>0 || disableUnusedArraysToo)       glDisableVertexAttribArray(normsAttributeIndex);
        if (tangsOffsetInBytes>0 || disableUnusedArraysToo)       glDisableVertexAttribArray(tangsAttributeIndex);
        if (texCoordsOffsetInBytes>0 || disableUnusedArraysToo)   glDisableVertexAttribArray(texCoordsAttributeIndex);
        if (boneIndsOffsetInBytes>0 || disableUnusedArraysToo)    glDisableVertexAttribArray(boneIndsAttributeIndex);
        if (boneWeightsOffsetInBytes>0 || disableUnusedArraysToo) glDisableVertexAttribArray(boneWeightsAttributeIndex);
    }
    inline void setPrimitiveType(GLenum mode) {primitive = mode;}
    inline void displayAttributeIndices() const {
        printf("GLMiniTriangleBatch::vertsAttributeIndex = %d\n",vertsAttributeIndex);
        printf("GLMiniTriangleBatch::colorsAttributeIndex = %d\n",colorsAttributeIndex);
        printf("GLMiniTriangleBatch::normsAttributeIndex = %d\n",normsAttributeIndex);
        printf("GLMiniTriangleBatch::tangsAttributeIndex = %d\n",tangsAttributeIndex);
        printf("GLMiniTriangleBatch::texCoordsAttributeIndex = %d\n",texCoordsAttributeIndex);
        printf("GLMiniTriangleBatch::boneIndsAttributeIndex = %d\n",boneIndsAttributeIndex);
        printf("GLMiniTriangleBatch::boneWeightsAttributeIndex = %d\n",boneWeightsAttributeIndex);
    }
    inline void displayInfo() const {
        printf("GLMiniTriangleBatch::gpuArrayVertsSize = %zu;   gpuArraySizeInBytes = %zu\n", gpuArrayVertsSize,gpuArraySizeInBytes);
        printf("GLMiniTriangleBatch::vertsAttributeIndex = %d;\n",vertsAttributeIndex);
        printf("GLMiniTriangleBatch::colorsAttributeIndex = %d;  colorsOffsetInBytes = %zu\n",colorsAttributeIndex,colorsOffsetInBytes);
        printf("GLMiniTriangleBatch::normsAttributeIndex = %d;  normsOffsetInBytes = %zu\n",normsAttributeIndex,normsOffsetInBytes);
        printf("GLMiniTriangleBatch::tangsAttributeIndex = %d;  tangsOffsetInBytes = %zu\n",tangsAttributeIndex,tangsOffsetInBytes);
        printf("GLMiniTriangleBatch::texCoordsAttributeIndex = %d;  texCoordsOffsetInBytes = %zu\n",texCoordsAttributeIndex,texCoordsOffsetInBytes);
        printf("GLMiniTriangleBatch::boneIndsAttributeIndex = %d;  boneIndsOffsetInBytes = %zu\n",boneIndsAttributeIndex,boneIndsOffsetInBytes);
        printf("GLMiniTriangleBatch::boneWeightsAttributeIndex = %d;  boneWeightsOffsetInBytes = %zu\n",boneWeightsAttributeIndex,boneWeightsOffsetInBytes);
        printf("GLMiniTriangleBatch::gpuElementArrayIndsSize = %zu;   gpuElementArraySizeInBytes = %zu\n", gpuElementArrayIndsSize,gpuElementArraySizeInBytes);
    }

protected:
    // Internal method:
    template <typename T> inline static void _UpdateSingleArrayBuffer(
                    GLuint vbo,
                    const T* pVector,
                    const size_t size,
                    const size_t strideInGLfloatUnits,
                    const size_t numComponents,
                    const GLbyte* pMapBufferData,
                    bool useVao,
                    const GLint attributeIndex,
                    const size_t gpuArrayVertsSize,
                    size_t& curOffsetInBytes,
                    size_t* pAtrributeOffsetInBytes = NULL,
                    GLenum type = GL_FLOAT
                    );

public:
    // Helper methods (not used):
    template < typename ElementTypeSingle > inline static ElementTypeSingle Sqrt(const ElementTypeSingle v)
    {
        return static_cast < ElementTypeSingle > (btSqrt(v));
    }
    template < typename ElementTypeSingle > inline static ElementTypeSingle InvSqrt(const ElementTypeSingle v)
    {
    #ifdef MAX_FLOAT
        #define BIG_FLOAT MAX_FLOAT
    #else
        #define BIG_FLOAT 10000000000000000000.0f
    #endif
    #ifndef SIMD_EPSILON
     #ifdef MIN_FLOAT
        #define SIMD_EPSILON MIN_FLOAT
    #else
        #define SIMD_EPSILON 0.000000000001f
    #endif
    #endif //SIMD_EPSILON
        return static_cast < ElementTypeSingle > (v>SIMD_EPSILON ? (ElementTypeSingle(1.0)/sqrt(v)) : ElementTypeSingle(BIG_FLOAT));
    #undef BIG_FLOAT
    }
    template < typename ElementTypeVector,typename ElementTypeSingle > inline static ElementTypeSingle Length2(const ElementTypeVector& v)
    {
        return static_cast < ElementTypeSingle > (v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    }
    template < typename ElementTypeVector,typename ElementTypeSingle > inline static ElementTypeSingle Length(const ElementTypeVector& v)
    {
        return static_cast < ElementTypeSingle > (Sqrt(Length2<ElementTypeVector,ElementTypeSingle>(v)));
    }
    template < typename ElementTypeVector,typename ElementTypeSingle > inline static void Normalize(ElementTypeVector& v)
    {
        v*=InvSqrt(Length2<ElementTypeVector,ElementTypeSingle>(v));
    }
    template < typename ElementTypeVector,typename ElementTypeSingle > inline static ElementTypeVector Normalized(const ElementTypeVector& v)
    {
        return v*InvSqrt<ElementTypeSingle>(Length2<ElementTypeVector,ElementTypeSingle>(v));
    }
    template < typename ElementTypeVector > inline static ElementTypeVector CrossProduct(const ElementTypeVector& T0,const ElementTypeVector& T1)   {
        return ElementTypeVector(    // cross product
                               T0[1] * T1[2] - T0[2] * T1[1],
                               T0[2] * T1[0] - T0[0] * T1[2],
                               T0[0] * T1[1] - T0[1] * T1[0]);
    }
public:
    // Hp) normsOut must be large enough to contain vertsInSize elements
    template < typename ElementType,typename IndexElementType > static void CalculateNormalsAndTangents(   const ElementType* vertsIn,const size_t vertsInSize,
                                                                                                const IndexElementType* indsIn,const size_t indsSize,
                                                                                                ElementType* normsInOut,bool calculateNormals = true,
                                                                                                const GLfloat* texCoordsIn=NULL,size_t texCoordsInStrideInGLfloatUnits=2,
                                                                                                GLfloat* tangentsOut=NULL,size_t numTangentsComponents=4,size_t tangentsStrideInGLfloatUnits=4,
                                                                                                ElementType* bitangentsOut=NULL,
                                                                                                GLenum primitive = GL_TRIANGLES
                                                                                                );

    template < typename IndexElementType > static void CalculateNormals(const std::vector < glm::vec3 >& vertsIn,
                                                                        const std::vector < IndexElementType >& indsIn,
                                                                        std::vector < glm::vec3 >& normsOut,
                                                                        GLenum primitive = GL_TRIANGLES
                                                                        )
    {
        CalculateNormalsAndTangents<IndexElementType>(vertsIn,indsIn,normsOut[0],true,NULL,NULL,NULL,primitive);
    }

    template < typename IndexElementType > static void CalculateNormalsAndTangents(const std::vector < glm::vec3 >& vertsIn,
                                                                        const std::vector < IndexElementType >& indsIn,
                                                                        std::vector < glm::vec3 >& normsInOut,bool calculateNormals = true,
                                                                        const std::vector < glm::vec2 >* ptexCoordsIn=NULL,
                                                                        std::vector < glm::vec4 >* ptangentsOut=NULL,
                                                                        std::vector < glm::vec3 >* pbitangentsOut=NULL,
                                                                        GLenum primitive = GL_TRIANGLES
                                                                        )
    {
        const size_t vertsSize = vertsIn.size();
        if (vertsSize==0) return;
        if (normsInOut.size()<vertsSize) {normsInOut.resize(vertsSize);calculateNormals=true;}
        if (ptexCoordsIn) {
            if (ptangentsOut && ptangentsOut->size()<vertsSize) ptangentsOut->resize(vertsSize);
            if (pbitangentsOut && pbitangentsOut->size()<vertsSize) pbitangentsOut->resize(vertsSize);
        }
        CalculateNormalsAndTangents<glm::vec3,IndexElementType>((const glm::vec3*) glm::value_ptr(vertsIn[0]),vertsSize,(const IndexElementType*) &indsIn[0],indsIn.size(),(glm::vec3*) glm::value_ptr(normsInOut[0]),calculateNormals,
                                                     ptexCoordsIn ? (const GLfloat*)glm::value_ptr((*ptexCoordsIn)[0]) : (const GLfloat*)NULL,sizeof(glm::vec2)/sizeof(GLfloat),
                                                     (ptexCoordsIn && ptangentsOut) ? (GLfloat*) glm::value_ptr((*ptangentsOut)[0]) : (GLfloat*)NULL,4,sizeof(glm::vec4)/sizeof(GLfloat),
                                                     (ptexCoordsIn && pbitangentsOut) ? (glm::vec3*) glm::value_ptr((*pbitangentsOut)[0]) : (glm::vec3*)NULL,
                                                     primitive
                                                     );
    }

    template < typename ElementType > static ElementType CalculateAabbExtents(const ElementType* vertsIn,const size_t vertsInSize,ElementType* pOptionalCenterOut=NULL);

    template < typename ElementType > static ElementType CalculateAabbHalfExtents(const ElementType* vertsIn,const size_t vertsInSize,ElementType* pOptionalCenterOut=NULL)  {
        return CalculateAabbExtents<ElementType>(vertsIn,vertsInSize,pOptionalCenterOut)*0.5f;
    }

    template < typename ElementType > static ElementType CalculateAabbCenter(const ElementType* vertsIn,const size_t vertsInSize,ElementType* pOptionalAabbExtentsOut=NULL)  {
        ElementType center;
        const ElementType aabbExtents = CalculateAabbExtents<ElementType>(vertsIn,vertsInSize,&center);
        if (pOptionalAabbExtentsOut) *pOptionalAabbExtentsOut = aabbExtents;
        return center;
    }

    static glm::vec3 CalculateAabbExtents(const std::vector < glm::vec3 >& vertsIn,glm::vec3* pOptionalCenterOut=NULL) {
        return CalculateAabbExtents<glm::vec3>(vertsIn.size()>0 ? &vertsIn[0] : NULL,vertsIn.size(),pOptionalCenterOut);
    }

    static glm::vec3 CalculateHalfAabbExtents(const std::vector < glm::vec3 >& vertsIn,glm::vec3* pOptionalCenterOut=NULL) {
        return CalculateAabbHalfExtents<glm::vec3>(vertsIn.size()>0 ? &vertsIn[0] : NULL,vertsIn.size(),pOptionalCenterOut);
    }

    static glm::vec3 CalculateAabbCenter(const std::vector < glm::vec3 >& vertsIn,glm::vec3* pOptionalAabbExtentsOut=NULL) {
        return CalculateAabbCenter<glm::vec3>(vertsIn.size()>0 ? &vertsIn[0] : NULL,vertsIn.size(),pOptionalAabbExtentsOut);
    }


protected:


    GLint vertsAttributeIndex;
    GLint colorsAttributeIndex;size_t colorsOffsetInBytes;
    GLint normsAttributeIndex;size_t normsOffsetInBytes;
    GLint tangsAttributeIndex;size_t tangsOffsetInBytes;
    GLint texCoordsAttributeIndex;size_t texCoordsOffsetInBytes;
    GLint boneIndsAttributeIndex;size_t boneIndsOffsetInBytes;
    GLint boneWeightsAttributeIndex;size_t boneWeightsOffsetInBytes;

    GLuint vboIA;          // "Serialized" index array
    GLenum primitive;           // GL_TRIANGLES
    GLenum drawTypeIA;          //GL_STATIC_DRAW;
    size_t gpuElementArrayIndsSize;    // == inds.size(), but cpu vectors might be cleared
    size_t gpuElementArraySizeInBytes; // In Bytes. Can be bigger than needed, if an old vboIA gpu space is reused.
    size_t indexTypeSizeInBytes;    // Must be 1,2 or 4

    GLuint vbo;            // "Serialized" vertex array
    GLenum drawType;            // GL_STATIC_DRAW
    size_t gpuArrayVertsSize;   // == verts.size(), but cpu vectors might be cleared
    size_t gpuArraySizeInBytes; // In Bytes (all fields included). Can be bigger than all fields, if an old vbo gpu space is reused.

    GLuint vao;

};


// (Very long) implementation of the template methods (AFAIK they must stay in the .h file) --------------------------------------------------------------------------------------------
template < typename ElementType > ElementType GLMiniTriangleBatch::CalculateAabbExtents(const ElementType* vertsIn,const size_t vertsInSize,ElementType* pOptionalCenterOut)
{
    ElementType exts(0,0,0);
    if (pOptionalCenterOut) *pOptionalCenterOut = ElementType(0,0,0);
    if (!vertsIn || vertsInSize==0) return exts;
    ElementType minb(vertsIn[0]),maxb(vertsIn[0]);
    for (size_t i=1;i<vertsInSize;i++)
    {
        const ElementType& V = vertsIn[i];
        if      (minb[0] > V[0]) minb[0] = V[0];
        else if (maxb[0] < V[0]) maxb[0] = V[0];
        if      (minb[1] > V[1]) minb[1] = V[1];
        else if (maxb[1] < V[1]) maxb[1] = V[1];
        if      (minb[2] > V[2]) minb[2] = V[2];
        else if (maxb[2] < V[2]) maxb[2] = V[2];
    }
    exts = maxb - minb;
    if (pOptionalCenterOut) *pOptionalCenterOut = (maxb+minb)*0.5f;
    return exts;
}

template < typename ElementType,typename IndexElementType > void GLMiniTriangleBatch::CalculateNormalsAndTangents(   const ElementType* vertsIn,const size_t vertsInSize,
                                                                                            const IndexElementType* indsIn,const size_t indsSize,
                                                                                            ElementType* normsInOut,bool calculateNormals,
                                                                                            const GLfloat* texCoordsIn,size_t texCoordsInStrideInGLfloatUnits,
                                                                                            GLfloat* tangentsOut,size_t numTangentsComponents,size_t tangentsStrideInGLfloatUnits,
                                                                                            ElementType* bitangentsOut,GLenum primitive
                                                                                            )
{
    size_t numIndicesPerFace = 3;   // Must be 3 or 4 (mandatory)
    if (primitive==GL_QUADS || primitive==GL_QUAD_STRIP) numIndicesPerFace = 4;
    if (primitive!=GL_TRIANGLES && primitive!=GL_QUADS && primitive!=GL_TRIANGLE_STRIP && primitive!=GL_QUAD_STRIP) {
        printf("GLMiniTriangleBatch::CalculateNormalsAndTangents(...): only GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_QUADS and GL_QUAD_STRIPS primitives are supported\n");
        return;
    }
    const bool facesOf4verts = (numIndicesPerFace == 4);
    const bool isTriangleStrip = !facesOf4verts && primitive==GL_TRIANGLE_STRIP;
    const bool isQuadStrip = facesOf4verts && primitive==GL_QUAD_STRIP;
    const bool isNotAStrip = !isTriangleStrip && !isQuadStrip;

    const size_t vertsSize = vertsInSize;
    if (vertsSize==0 || indsSize==0) return;
    if (texCoordsIn==NULL)  {tangentsOut=NULL;bitangentsOut=NULL;}
    if (texCoordsInStrideInGLfloatUnits<2) texCoordsInStrideInGLfloatUnits = 2;
    if (numTangentsComponents<3) numTangentsComponents = 3;
    if (tangentsStrideInGLfloatUnits<numTangentsComponents) tangentsStrideInGLfloatUnits=numTangentsComponents;

    const ElementType zv(0,0,0);
    const ElementType defN(0,0,1);
    const ElementType defTg(1,0,0);
    const ElementType defBtg(0,1,0);
    const GLfloat epsilon = SIMD_EPSILON;   //Or the smallest GLfloat available for you (e.g. 1e-6f)

    ElementType* pNorms = normsInOut;
    GLfloat* pTangs = tangentsOut;
    ElementType* pBitangs = bitangentsOut;
    std::vector < ElementType > bitangentsTemp;
    bool useBitangentsTemp = false;
    if (tangentsOut && !bitangentsOut) {
        bitangentsTemp.resize(vertsInSize);
        pBitangs=&bitangentsTemp[0];
        useBitangentsTemp = true;
    }
    for(size_t i = 0;i<vertsSize;i++) {
        if (calculateNormals)   *pNorms++ = zv;
        if (tangentsOut) {
            for (size_t c=0;c<numTangentsComponents;c++) pTangs[c]=0.f;
            pTangs+=tangentsStrideInGLfloatUnits;
        }
        if (bitangentsOut || useBitangentsTemp) *pBitangs++ = zv;
    }
    pNorms = normsInOut;
    if (tangentsOut) pTangs = tangentsOut;
    if (bitangentsOut) pBitangs = bitangentsOut;
    else if (useBitangentsTemp) pBitangs=&bitangentsTemp[0];

    size_t i3;ElementType n,T0,T1,tg,btg;GLfloat len,coeff,s1,s2,t1,t2,handedness;
    std::vector < IndexElementType > I;I.resize(numIndicesPerFace);
    size_t startIndex = 0;bool invertCrossProduct = false;
    size_t increment = 1;
    size_t numFaces;
    if (isTriangleStrip) {
        numIndicesPerFace = 1;
        startIndex = 2;
        numFaces = indsSize-2;
    }
    else if (isQuadStrip)   {
        numIndicesPerFace = 2;
        startIndex = 3;
        increment = 2;
        numFaces = (indsSize-2)/2;
    }
    else numFaces = indsSize/numIndicesPerFace;
    for(size_t i = startIndex;i<numFaces;i+=increment)
    {                
        if (isNotAStrip)   {
            i3 = numIndicesPerFace*i;
            I[0] = indsIn[i3];
            I[1] = indsIn[i3+1];
            I[2] = indsIn[i3+2];
            if (facesOf4verts) I[3] = indsIn[i3+3];
        }
        else if (isTriangleStrip)    {
            I[0] = indsIn[i-2];
            I[1] = invertCrossProduct ? indsIn[i] : indsIn[i-1];
            I[2] = invertCrossProduct ? indsIn[i-1] : indsIn[i];
            invertCrossProduct = !invertCrossProduct;
        }
        else if (isQuadStrip)   {
            I[0] = indsIn[i-3];
            I[1] = invertCrossProduct ? indsIn[i-1] : indsIn[i-2];
            I[2] = indsIn[i];
            I[3] = invertCrossProduct ? indsIn[i-2] : indsIn[i-1];
            invertCrossProduct = !invertCrossProduct;
        }

        if (calculateNormals)
        {
            const ElementType& V0 = vertsIn[I[0]];
            const ElementType& V1 = vertsIn[I[1]];
            const ElementType& V2 = vertsIn[I[2]];

            T0 = V1 - V0;
            T1 = V2 - V0;
            n = CrossProduct<ElementType>(T0,T1);
            //Normalize<ElementType,GLfloat>(n);

            normsInOut[I[0]]+=n;
            normsInOut[I[1]]+=n;
            normsInOut[I[2]]+=n;
            if (facesOf4verts) normsInOut[I[3]]+=n;

            //printf("1 %d/%d\n",i,sz);

        }

        if (tangentsOut)    {
            const GLfloat* TC0 = &texCoordsIn[I[0]*texCoordsInStrideInGLfloatUnits];
            const GLfloat* TC1 = &texCoordsIn[I[1]*texCoordsInStrideInGLfloatUnits];
            const GLfloat* TC2 = &texCoordsIn[I[2]*texCoordsInStrideInGLfloatUnits];

            GLfloat* TG0 = &tangentsOut[I[0]*tangentsStrideInGLfloatUnits];
            GLfloat* TG1 = &tangentsOut[I[1]*tangentsStrideInGLfloatUnits];
            GLfloat* TG2 = &tangentsOut[I[2]*tangentsStrideInGLfloatUnits];
            GLfloat* TG3 = NULL;
            if (facesOf4verts) TG3 = &tangentsOut[I[3]*tangentsStrideInGLfloatUnits];

            s1 = TC1[0] - TC0[0];
            s2 = TC2[0] - TC0[0];
            t1 = TC1[1] - TC0[1];
            t2 = TC2[1] - TC0[1];

            /*
    texEdge1[0] = pVertex1->texCoord[0] - pVertex0->texCoord[0];    //s1
    texEdge1[1] = pVertex1->texCoord[1] - pVertex0->texCoord[1];    //s2

    texEdge2[0] = pVertex2->texCoord[0] - pVertex0->texCoord[0];    //t1
    texEdge2[1] = pVertex2->texCoord[1] - pVertex0->texCoord[1];    //t2

    det = texEdge1[0] * texEdge2[1] - texEdge2[0] * texEdge1[1];    //coeff
            */

            coeff = s1 * t2 - s2 * t1;  // (st1.u * st2.v - st2.u * st1.v);
            if (fabs(coeff)<epsilon) {
                tg = defTg;

                TG0[0]+=tg[0];TG0[1]+=tg[1];TG0[2]+=tg[2];
                TG1[0]+=tg[0];TG1[1]+=tg[1];TG1[2]+=tg[2];
                TG2[0]+=tg[0];TG2[1]+=tg[1];TG2[2]+=tg[2];
                if (TG3) {TG3[0]+=tg[0];TG3[1]+=tg[1];TG3[2]+=tg[2];}

                if (pBitangs && numTangentsComponents>3)  {

                    btg = defBtg;

                    pBitangs[I[0]]+=btg;
                    pBitangs[I[1]]+=btg;
                    pBitangs[I[2]]+=btg;
                    if (facesOf4verts) pBitangs[I[3]]+=btg;
                }
                continue;
            }
            coeff = 1.0f / coeff;

            tg = ElementType(coeff * ((T0[0] * t2)  - (T1[0] *  t1)),
                             coeff * ((T0[1] * t2)  - (T1[1] *  t1)),
                             coeff * ((T0[2] * t2)  - (T1[2] *  t1)));
/*
        tangent[0] = (texEdge2[1] * edge1[0] - texEdge1[1] * edge2[0]) * det;
        tangent[1] = (texEdge2[1] * edge1[1] - texEdge1[1] * edge2[1]) * det;
        tangent[2] = (texEdge2[1] * edge1[2] - texEdge1[1] * edge2[2]) * det;

        bitangent[0] = (-texEdge2[0] * edge1[0] + texEdge1[0] * edge2[0]) * det;
        bitangent[1] = (-texEdge2[0] * edge1[1] + texEdge1[0] * edge2[1]) * det;
        bitangent[2] = (-texEdge2[0] * edge1[2] + texEdge1[0] * edge2[2]) * det;
*/

            TG0[0]+=tg[0];TG0[1]+=tg[1];TG0[2]+=tg[2];
            TG1[0]+=tg[0];TG1[1]+=tg[1];TG1[2]+=tg[2];
            TG2[0]+=tg[0];TG2[1]+=tg[1];TG2[2]+=tg[2];
            if (TG3) {TG3[0]+=tg[0];TG3[1]+=tg[1];TG3[2]+=tg[2];}

            if (pBitangs && numTangentsComponents>3)  {

                btg = //CrossProduct<ElementType>(n,tg);
                        ElementType(coeff * ((T0[0] * (-s2))  + (T1[0] *  s1)),
                                    coeff * ((T0[1] * (-s2))  + (T1[1] *  s1)),
                                    coeff * ((T0[2] * (-s2))  + (T1[2] *  s1)));

                pBitangs[I[0]]+=btg;
                pBitangs[I[1]]+=btg;
                pBitangs[I[2]]+=btg;
                if (facesOf4verts) pBitangs[I[3]]+=btg;

            }
        }
    }


    for(size_t i = 0;i<vertsSize;i++) {
        ElementType& N = normsInOut[i];
        if (calculateNormals)   {
            //Normalize<ElementType,GLfloat>(N);

            len = Length2<ElementType,GLfloat>(N);
            if (len>epsilon) N*=InvSqrt<GLfloat>(len);
            else N = defN;
            //printf("2 %d/%d\n",i,vertsSize);

        }
        if (tangentsOut)    {
            GLfloat* T = &tangentsOut[i*tangentsStrideInGLfloatUnits];

            coeff = N[0]*T[0]+N[1]*T[1]+N[2]*T[2];  // dot product N*T

            T[0]-=N[0]*coeff;
            T[1]-=N[1]*coeff;
            T[2]-=N[2]*coeff;

            //Normalize<ElementType>(T);    // Maybe it's better to check before doing this:
            len = T[0]*T[0]+T[1]*T[1]+T[2]*T[2];
            if (len>epsilon) {
                coeff = InvSqrt<GLfloat>(len);
                T[0]*=coeff;
                T[1]*=coeff;
                T[2]*=coeff;
            }
            else {
                T[0] = defTg[0];
                T[1] = defTg[1];
                T[2] = defTg[2];
            }

            if (numTangentsComponents>3) T[3]=1.0f;

            if (pBitangs)  {
                ElementType& BT = pBitangs[i];

                btg = CrossProduct<ElementType>(N,ElementType(T[0],T[1],T[2]));   // already normalized if N and T are normalized

                coeff = btg[0] * BT[0] + btg[1] * BT[1] + btg[2] * BT[2];   // Dot product btg*BT
                handedness =  (coeff < 0.0f) ? -1.0f : 1.0f;   // handedness of the local tangent space. Some add this to BT[3] when available...
                if (numTangentsComponents>3)     T[3] = handedness;

                if (!useBitangentsTemp) {
                    btg*=handedness;

                    BT[0] = btg[0];
                    BT[1] = btg[1];
                    BT[2] = btg[2];
                }

                /*
                //Normalize<ElementType>(BT);    // Maybe it's better to check before doing this:
                len = Length2<ElementType,GLfloat>(BT);
                if (len>epsilon) BT*=InvSqrt<GLfloat>(len);
                else BT = defBtg;
                */

                //Explanation of handedess:
                /*
    // The bitangent vector is the cross product between the triangle face
    // normal vector and the calculated tangent vector. The resulting
    // bitangent vector should be the same as the bitangent vector
    // calculated from the set of linear equations above. If they point in
    // different directions then we need to invert the cross product
    // calculated bitangent vector. We store this scalar multiplier in the
    // tangent vector's 'w' component so that the correct bitangent vector
    // can be generated in the normal mapping shader's vertex shader.
    //
    // Normal maps have a left handed coordinate system with the origin
    // located at the top left of the normal map texture. The x coordinates
    // run horizontally from left to right. The y coordinates run
    // vertically from top to bottom. The z coordinates run out of the
    // normal map texture towards the viewer. Our handedness calculations
    // must take this fact into account as well so that the normal mapping
    // shader's vertex shader will generate the correct bitangent vectors.
    // Some normal map authoring tools such as Crazybump
    // (http://www.crazybump.com/) includes options to allow you to control
    // the orientation of the normal map normal's y-axis.
                */
            }
        }
    }

}



template <typename T> void GLMiniTriangleBatch::_UpdateSingleArrayBuffer(
                GLuint vbo,
                const T* pVector,
                const size_t size,
                const size_t strideInGLfloatUnits,
                const size_t numComponents,
                const GLbyte* pMapBufferData,
                bool useVao,
                const GLint attributeIndex,
                const size_t gpuArrayVertsSize,
                size_t& curOffsetInBytes,
                size_t* pAtrributeOffsetInBytes,
                GLenum type //GL_FLOAT
                )
{
    if (pVector)
    {
        if (size>0)
        {
            const T* pV = pVector;
            const size_t stride = strideInGLfloatUnits;
            const size_t strideInBytes = stride * sizeof(T);
            const size_t sizeOfElementInBytes = numComponents*sizeof(T);
            const size_t totalSizeInBytes = sizeOfElementInBytes * size;
            if (pMapBufferData) {
                if (stride==numComponents) memcpy((void*)&pMapBufferData[curOffsetInBytes],  pV, totalSizeInBytes);
                else    {
                    GLbyte* pGpuBuffer = (GLbyte*) &(pMapBufferData[curOffsetInBytes]);
                    GLbyte* pCpuBuffer = (GLbyte*) pV;
                    for (size_t i=0;i<size;i++)
                    {
                        memcpy(pGpuBuffer,  pCpuBuffer, sizeOfElementInBytes);
                        pGpuBuffer+=sizeOfElementInBytes;
                        pCpuBuffer+=strideInBytes;
                    }
                }
            }
            else if (stride==numComponents) glBufferSubData(GL_ARRAY_BUFFER, curOffsetInBytes, totalSizeInBytes, pV);
            else    {
                std::vector<T> V(numComponents*size); //Temp array
                {
                    GLbyte* pMyBuffer = (GLbyte*) &V[0];
                    GLbyte* pCpuBuffer = (GLbyte*) pV;
                    for (size_t i=0;i<size;i++)   {
                        memcpy(pMyBuffer,  pCpuBuffer, sizeOfElementInBytes);
                        pMyBuffer+=sizeOfElementInBytes;
                        pCpuBuffer+=strideInBytes;
                    }
                }
                glBufferSubData(GL_ARRAY_BUFFER, curOffsetInBytes, totalSizeInBytes, (GLbyte*) &V[0]);
            }
            if (useVao)    {
                if (type == GL_INT || type == GL_UNSIGNED_INT)  glVertexAttribIPointer(attributeIndex, (GLint) numComponents, type, 0, BUFFER_OFFSET(curOffsetInBytes));
                else glVertexAttribPointer(attributeIndex, (GLint) numComponents, type, GL_FALSE, 0, BUFFER_OFFSET(curOffsetInBytes));
                glEnableVertexAttribArray(attributeIndex);
            }
            if (pAtrributeOffsetInBytes) *pAtrributeOffsetInBytes = curOffsetInBytes;
            curOffsetInBytes+=totalSizeInBytes;
        }
        else if (pAtrributeOffsetInBytes) *pAtrributeOffsetInBytes = 0;
    }
    else {
        if (pAtrributeOffsetInBytes && *pAtrributeOffsetInBytes>0) {
            // we're probably updating some other part of the buffer, leaving this intact
            if (useVao)    {// I'm not too sure if this branch is mandatory...
                if (type == GL_INT || type == GL_UNSIGNED_INT)  glVertexAttribIPointer(attributeIndex, (GLint) numComponents, type, 0, BUFFER_OFFSET(curOffsetInBytes));
                else glVertexAttribPointer(attributeIndex, (GLint) numComponents, type, GL_FALSE, 0, BUFFER_OFFSET(curOffsetInBytes));
                glEnableVertexAttribArray(attributeIndex);
            }
            curOffsetInBytes+= numComponents*(sizeof(T))*gpuArrayVertsSize; // mandatory for sure
        }
    }

}

template < typename GLMiniTriangleBatchIndexType > void GLMiniTriangleBatch::upload(
        const GLMiniTriangleBatchIndexType* pinds,
        size_t numInds,
        const GLfloat* pverts,
        size_t numVerts,
        size_t vertsStrideInGLfloatUnits,
        const GLfloat* pcolors,
        size_t numColors,
        size_t colorsStrideInGLfloatUnits,
        const GLfloat* pnorms,
        size_t numNorms,
        size_t normsStrideInGLfloatUnits,
        const GLfloat* ptangs,  // if present must have 4 components
        size_t numTangs,
        size_t tangsStrideInGLfloatUnits,
        const GLfloat* ptexCoords,
        size_t numTexCoords,
        size_t texCoordsStrideInGLfloatUnits,
        const BoneIndsComponentType* pboneInds,
        size_t numBoneInds,
        size_t boneIndsStrideInGLuintUnits,
        const GLfloat* pboneWeights,
        size_t numBoneWeights,
        size_t boneWeightsStrideInGLfloatUnits,
        GLenum drawType,
        GLenum drawTypeIA,
        bool useVertexArrayObject
        )
{
    //---------- Check arguments correctness----------------------
    if (!pverts) numVerts=0;
    else if (numVerts==0) pverts=NULL;
    if (!pcolors) numColors=0;
    else if (numColors==0) pcolors=NULL;
    if (!pnorms) numNorms=0;
    else if (numNorms==0) pnorms=NULL;
    if (!ptangs) numTangs=0;
    else if (numTangs==0) ptangs=NULL;
    if (!ptexCoords) numTexCoords=0;
    else if (numTexCoords==0) ptexCoords=NULL;
    if (!pboneInds) numBoneInds=0;
    else if (numBoneInds==0) pboneInds=NULL;
    if (!pboneWeights) numBoneWeights=0;
    else if (numBoneWeights==0) pboneWeights=NULL;

    if (vertsStrideInGLfloatUnits<3)    vertsStrideInGLfloatUnits   = 3;
    if (normsStrideInGLfloatUnits<3)    normsStrideInGLfloatUnits   = 3;
    if (tangsStrideInGLfloatUnits<4)    {
        if (tangsStrideInGLfloatUnits!=0) {
            ptangs = NULL;
            numTangs = 0;
        }
        tangsStrideInGLfloatUnits = 4;
    }
    if (colorsStrideInGLfloatUnits<4)    {
        if (colorsStrideInGLfloatUnits!=0) {
            pcolors = NULL;
            numColors = 0;
        }
        colorsStrideInGLfloatUnits = 4;
    }
    if (texCoordsStrideInGLfloatUnits<2) texCoordsStrideInGLfloatUnits = 2;
    //if (boneIndsStrideInGLuintUnits<4) boneIndsStrideInGLuintUnits = 4;
    //if (boneWeightsStrideInGLfloatUnits<4) boneWeightsStrideInGLfloatUnits = 4;

    const size_t vertsSize = pverts ? numVerts : gpuArrayVertsSize;
    const size_t colorsSize = pcolors ? numColors : (colorsOffsetInBytes==0 ? 0 : gpuArrayVertsSize);
    const size_t normsSize = pnorms ? numNorms : (normsOffsetInBytes==0 ? 0 : gpuArrayVertsSize);
    const size_t tangsSize = ptangs ? numTangs : (tangsOffsetInBytes==0 ? 0 : gpuArrayVertsSize);
    const size_t texCoordsSize = ptexCoords ? numTexCoords : (texCoordsOffsetInBytes==0 ? 0 : gpuArrayVertsSize);
    const size_t boneIndsSize = pboneInds ? numBoneInds : (boneIndsOffsetInBytes==0 ? 0 : gpuArrayVertsSize);
    const size_t boneWeightsSize = pboneWeights ? numBoneWeights : (boneWeightsOffsetInBytes==0 ? 0 : gpuArrayVertsSize);
    bool error = false;
    if (colorsSize!=0 && colorsSize!=vertsSize) {error=true;printf("GLMiniTriangleBatch::upload(...) Error: vertices and colors must be both %zu.\n",vertsSize);}
    if (normsSize!=0 && normsSize!=vertsSize) {error=true;printf("GLMiniTriangleBatch::upload(...) Error: vertices and normals must be both %zu.\n",vertsSize);}
    if (tangsSize!=0 && tangsSize!=vertsSize) {error=true;printf("GLMiniTriangleBatch::upload(...) Error: vertices and tangents must be both %zu.\n",vertsSize);}
    if (texCoordsSize!=0 && texCoordsSize!=vertsSize) {error=true;printf("GLMiniTriangleBatch::upload(...) Error: vertices and texcoords must be both %zu.\n",vertsSize);}
    if (boneIndsSize!=0 && boneIndsSize!=vertsSize) {error=true;printf("GLMiniTriangleBatch::upload(...) Error: vertices and boneInds must be both %zu.\n",vertsSize);}
    if (boneWeightsSize!=0 && boneWeightsSize!=vertsSize) {error=true;printf("GLMiniTriangleBatch::upload(...) Error: vertices and boneWeights must be both %zu.\n",vertsSize);}
    //------------------------------------------------------------
    const size_t newVertsSize = pverts ? numVerts : 0;
    const bool allFieldsPresent = pinds && pverts && (colorsOffsetInBytes==0 ? true : pcolors!=NULL) && (normsOffsetInBytes==0 ? true : pnorms!=NULL) && (tangsOffsetInBytes==0 ? true : ptangs!=NULL) && (texCoordsOffsetInBytes==0 ? true : ptexCoords!=NULL)
             && (boneIndsOffsetInBytes==0 ? true : pboneInds!=NULL)  && (boneWeightsOffsetInBytes==0 ? true : pboneWeights!=NULL);
    bool updateMode = false;
    if (!vboIA || !vbo)
    {
        if (!pinds || !pverts) {
			if (vbo) {glDeleteBuffers(1,&vbo);vbo=0;}
			if (vboIA) {glDeleteBuffers(1,&vboIA);vboIA=0;}
            gpuArrayVertsSize = gpuArraySizeInBytes = gpuElementArrayIndsSize = gpuElementArraySizeInBytes = 0;
            colorsOffsetInBytes = normsOffsetInBytes = tangsOffsetInBytes = texCoordsOffsetInBytes = boneIndsOffsetInBytes = boneWeightsOffsetInBytes = 0;
            return;
        }
    }
    else updateMode = true;

    const bool useVao = useVertexArrayObject;
    if (!useVao)    {
        if (vao) {glDeleteVertexArrays(1,&vao);vao=0;}
    }
    else if (!vao) glGenVertexArrays(1,&vao);
    if (useVao) glBindVertexArray(vao);

    static const size_t vec4SizeInBytes = 4*sizeof(GLfloat);
    static const size_t vec3SizeInBytes = 3*sizeof(GLfloat);
    static const size_t vec2SizeInBytes = 2*sizeof(GLfloat);
    static const size_t uvec4SizeInBytes = 4*sizeof(BoneIndsComponentType); // Originally sizeof(GLint);

    const size_t gpuNewArraySizeInBytes =   vec4SizeInBytes  * (
                                                                (pcolors ? newVertsSize : (colorsOffsetInBytes==0 ? 0 : gpuArrayVertsSize))  +
                                                                (ptangs ? newVertsSize : (tangsOffsetInBytes==0 ? 0 : gpuArrayVertsSize)) +
                                                                (pboneWeights ? newVertsSize : (boneWeightsOffsetInBytes==0 ? 0 : gpuArrayVertsSize))
                                                                )   +
                                            vec3SizeInBytes  * (
                                                                (pverts ? newVertsSize : gpuArrayVertsSize) +
                                                                (pnorms ? newVertsSize : (normsOffsetInBytes==0 ? 0 : gpuArrayVertsSize))
                                                                ) +
                                            vec2SizeInBytes * (
                                                                (ptexCoords ? newVertsSize : (texCoordsOffsetInBytes==0 ? 0 : gpuArrayVertsSize))
                                                                ) +
                                            uvec4SizeInBytes * (
                                                                (pboneInds ? newVertsSize : (boneIndsOffsetInBytes==0 ? 0 : gpuArrayVertsSize))
                                                               );
    bool reuseExistingGpuArray = false;
    gpuArrayVertsSize = vertsSize;
    if (updateMode && gpuArraySizeInBytes >= gpuNewArraySizeInBytes && drawType==this->drawType && drawType!=GL_STATIC_DRAW) reuseExistingGpuArray = true;
#   ifndef __EMSCRIPTEN__
    const bool useMapBuffer = (drawType == GL_STREAM_DRAW) && (gpuArraySizeInBytes > 32 * 1024);
#   else
    const bool useMapBuffer = false;
#   endif
    if (useMapBuffer && allFieldsPresent) reuseExistingGpuArray = false;

    if (!vbo) glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    if (!reuseExistingGpuArray) {
        this->drawType = drawType;
        this->gpuArraySizeInBytes = gpuNewArraySizeInBytes;
        if (!updateMode || allFieldsPresent) glBufferData(GL_ARRAY_BUFFER,gpuArraySizeInBytes, 0, this->drawType);
    }
    size_t curOffsetInBytes = 0;

    GLbyte* pMapBufferData = NULL;
    if (useMapBuffer) pMapBufferData = (GLbyte*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

    // Verts
    _UpdateSingleArrayBuffer<GLfloat>(vbo,pverts,newVertsSize,vertsStrideInGLfloatUnits,3,pMapBufferData,useVao,vertsAttributeIndex,gpuArrayVertsSize,curOffsetInBytes,NULL);

    // Colors
    _UpdateSingleArrayBuffer<GLfloat>(vbo,pcolors,numColors,colorsStrideInGLfloatUnits,4,pMapBufferData,useVao,colorsAttributeIndex,gpuArrayVertsSize,curOffsetInBytes,&colorsOffsetInBytes);

    // Norms
    _UpdateSingleArrayBuffer<GLfloat>(vbo,pnorms,numNorms,normsStrideInGLfloatUnits,3,pMapBufferData,useVao,normsAttributeIndex,gpuArrayVertsSize,curOffsetInBytes,&normsOffsetInBytes);

    // Tangs
    _UpdateSingleArrayBuffer<GLfloat>(vbo,ptangs,numTangs,tangsStrideInGLfloatUnits,4,pMapBufferData,useVao,tangsAttributeIndex,gpuArrayVertsSize,curOffsetInBytes,&tangsOffsetInBytes);

    // TexCoords
    _UpdateSingleArrayBuffer<GLfloat>(vbo,ptexCoords,numTexCoords,texCoordsStrideInGLfloatUnits,2,pMapBufferData,useVao,texCoordsAttributeIndex,gpuArrayVertsSize,curOffsetInBytes,&texCoordsOffsetInBytes);

    // BoneInds
#   ifdef GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
    const GLenum boneIndsEnum = GL_INT;
#   else //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
    const GLenum boneIndsEnum = GL_FLOAT;
#   endif //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
    _UpdateSingleArrayBuffer<BoneIndsComponentType>(vbo,pboneInds,numBoneInds,boneIndsStrideInGLuintUnits,4,pMapBufferData,useVao,boneIndsAttributeIndex,gpuArrayVertsSize,curOffsetInBytes,&boneIndsOffsetInBytes,boneIndsEnum);

    // BoneWeights
    _UpdateSingleArrayBuffer<GLfloat>(vbo,pboneWeights,numBoneWeights,boneWeightsStrideInGLfloatUnits,4,pMapBufferData,useVao,boneWeightsAttributeIndex,gpuArrayVertsSize,curOffsetInBytes,&boneWeightsOffsetInBytes);


    if (pMapBufferData) glUnmapBuffer(GL_ARRAY_BUFFER);

    // inds
    if (pinds)
    {
        const size_t indsSize = numInds;
        const size_t newIndexTypeSizeInBytes = sizeof(GLMiniTriangleBatchIndexType);
        const size_t newElementArraySizeInBytes = indsSize * newIndexTypeSizeInBytes;
        bool updateModePossible = false;
        if (!vboIA) glGenBuffers(1,&vboIA);
        else if (gpuElementArraySizeInBytes>0) updateModePossible = true;
        const bool reuseGpuIndexArray = updateModePossible && this->drawTypeIA==drawTypeIA && this->indexTypeSizeInBytes == newIndexTypeSizeInBytes && gpuElementArraySizeInBytes >= newElementArraySizeInBytes;
#       ifndef __EMSCRIPTEN__
        const bool useMapBuffer = updateModePossible && newElementArraySizeInBytes >= 32 * 1024;
#       else
        const bool useMapBuffer = false;
#       endif
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,vboIA);
        gpuElementArrayIndsSize = indsSize;
        if (!reuseGpuIndexArray)
        {
            gpuElementArraySizeInBytes = newElementArraySizeInBytes;
            indexTypeSizeInBytes = newIndexTypeSizeInBytes;
            this->drawTypeIA = drawTypeIA;
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,gpuElementArraySizeInBytes, pinds, this->drawTypeIA);
        }
        else    {
            if (useMapBuffer)   {
                gpuElementArraySizeInBytes = newElementArraySizeInBytes;
                indexTypeSizeInBytes = newIndexTypeSizeInBytes;
                this->drawTypeIA = drawTypeIA;
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,gpuElementArraySizeInBytes, 0, this->drawTypeIA);

                GLbyte* pMapBufferData = (GLbyte*) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
                if (pMapBufferData) {
                    memcpy(pMapBufferData,  pinds, gpuElementArraySizeInBytes);
                    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
                }
            }
            else glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,newElementArraySizeInBytes,pinds);
        }

    }

    glBindBuffer(GL_ARRAY_BUFFER,0);
    if (useVao) {
		glBindVertexArray(0);
        glDisableVertexAttribArray(vertsAttributeIndex);
        if (colorsOffsetInBytes>0) glDisableVertexAttribArray(colorsAttributeIndex);
        if (normsOffsetInBytes>0) glDisableVertexAttribArray(normsAttributeIndex);
        if (tangsOffsetInBytes>0) glDisableVertexAttribArray(tangsAttributeIndex);
        if (texCoordsOffsetInBytes>0) glDisableVertexAttribArray(texCoordsAttributeIndex);
        if (boneIndsOffsetInBytes>0) glDisableVertexAttribArray(boneIndsAttributeIndex);
        if (boneWeightsOffsetInBytes>0) glDisableVertexAttribArray(boneWeightsAttributeIndex);

    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
}
// End implementation of the template methods (AFAIK they must stay in the .h file) -----------------------------------------------------------------------------------------------



// String And Path Helper Structs -------------------------------------------------------------------------------------
class String {
    typedef std::string string;
    typedef char Char;
    typedef size_t Size_t;
    static const string::size_type Npos = string::npos;
    public:
    inline static string ToLower(const string& s) {
        string rv(s);
        for (Size_t i=0,sz=rv.size();i<sz;i++) {
            Char& c = rv[i];
            c = tolower(c);
        }
        return rv;
    }
    inline static Size_t IndexOf(const string& s,const string& part) {
        return s.find(part);
    }
    inline static bool Contains(const string& s,const string& part) {
        return IndexOf(s,part)!=Npos;//string::npos;
    }

    inline static int ConvertToInt(const string& s) {
        int rv = 0;sscanf(s.c_str(),"%d",&rv);return rv;
        //return boost::lexical_cast<int>(s);
    }
    inline static bool StartsWith(const string& s,const string& part) {
        const Size_t partsize = part.size();
        const Size_t ssize = s.size();
        if (partsize>ssize) return false;
        for (Size_t i=0;i<partsize;i++) {
            if (s[i]!=part[i]) return false;
        }
        return true;
    }
    inline static bool EndsWith(const string& s,const string& part) {
        const Size_t partsize = part.size();
        const Size_t ssize = s.size();
        if (partsize>ssize) return false;
        for (Size_t i=0;i<partsize;i++) {
            if (s[ssize-partsize+i]!=part[i]) return false;
        }
        return true;
    }

    protected:
    String() {}
    void operator=(const String& ) {}

};

class Path {
    typedef std::string string;
    typedef char Char;
    typedef size_t Size_t;
    static const string::size_type Npos = string::npos;

    public:
    inline static bool FileExists(const Char* path) {
        FILE* f = fopen(path,"r");
        if (f) {
            fclose(f);
            return true;
        }
        return false;
    }
    inline static bool FileExists(const string& path) {
        return FileExists(path.c_str());
    }
    inline static string GetDirectoryName(const string& filePath)
    {
        if (filePath[filePath.size()-1]==':' || filePath=="/") return filePath;
        Size_t beg=filePath.find_last_of('\\');
        Size_t beg2=filePath.find_last_of('/');
        if (beg==Npos) {
            if (beg2==Npos) return "";
            return filePath.substr(0,beg2);
        }
        if (beg2==Npos) return filePath.substr(0,beg);
        beg=(beg>beg2?beg:beg2);
        return filePath.substr(0,beg);
    }
    inline static string GetFileName(const std::string& filePath)
    {
        Size_t beg=filePath.find_last_of('\\');
        Size_t beg2=filePath.find_last_of('/');
        if (beg==Npos) {
            if (beg2==Npos) return filePath;
            return filePath.substr(beg2+1);
        }
        if (beg2==Npos) return filePath.substr(beg+1);
        beg=(beg>beg2?beg:beg2);
        return filePath.substr(beg+1);
    }
    inline static string GetExtension(const std::string& filePath)
    {
        Size_t beg=filePath.find_last_of('.');
        if (beg!=Npos)  return String::ToLower(filePath.substr(beg));
        return "";
    }
    inline static string GetFilePathWithoutExtension(const std::string& filePath)
    {
        Size_t beg=filePath.find_last_of('\\');
        Size_t beg2=filePath.find_last_of('/');
        Size_t beg3=filePath.find_last_of('.');
        if (beg==Npos) {
            if (beg2==Npos) {
                if (beg3==Npos) return filePath;
                return filePath.substr(0,beg3);
            }
            if (beg3==Npos || beg3<=beg2) return filePath;
            else return filePath.substr(0,beg3);
        }
        if (beg2==Npos) {
            if (beg3==Npos) return filePath;
            else return filePath.substr(0,beg3);
        }
        beg=(beg>beg2?beg:beg2);
        if (beg3==Npos || beg3<=beg) return filePath;
        else return filePath.substr(0,beg3);
    }
    inline static string GetFileNameWithoutExtension(const std::string& filePath)
    {
        Size_t beg=filePath.find_last_of('\\');
        Size_t beg2=filePath.find_last_of('/');
        Size_t beg3=filePath.find_last_of('.');
        if (beg==Npos) {
            if (beg2==Npos) {
                if (beg3==Npos) return filePath;
                return filePath.substr(0,beg3);
            }
            if (beg3==Npos || beg3<=beg2) return filePath.substr(beg2+1);
            else return filePath.substr(beg2+1,beg3-(beg2+1));
        }
        if (beg2==Npos) {
            if (beg3==Npos) return filePath.substr(beg+1);
            else return filePath.substr(beg+1,beg3-(beg+1));
        }
        beg=(beg>beg2?beg:beg2);
        if (beg3==Npos || beg3<=beg) return filePath.substr(beg+1);
        else return filePath.substr(beg+1,beg3-(beg+1));
    }
    inline static string Combine(const string& directory,const string& fileName)
    {
        string ret=directory;
        Size_t size=ret.size();
        if (size==0) return fileName;
        if (ret[size-1]!='\\' && ret[size-1]!='/') ret+="/"+fileName;
        else ret+=fileName;
        return ret;
    }
    inline static string ChangeExtension(const string& filePath,const string& newExtension)
    {
        return GetFilePathWithoutExtension(filePath)+newExtension;
    }

    protected:
    Path() {}
    void operator=(const Path& ) {}
};
//---------------------------------------------------------------------------------------------------------------------


