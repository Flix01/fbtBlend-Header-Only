#include "mesh.h"


#define ALLOW_SEPARETED_PARTS_PARENTED_TO_A_SINGLE_BONE // There could be an optimization in the shader used to render these parts (they depend on a single bone matrix). However we still use boneIndices and weights here, so that we can reuse the same shader.
#define ALLOW_GLOBAL_OBJECT_TRANSFORM


#define FBTBLEND_IMPLEMENTATION
#include <fbtBlend.h>


//#include "StringAndPathHelper.h"

#include <map> // std::map
#include <algorithm> // std::sort std::find

/*// This are defined in openGLTextureShaderAndBufferHelpers.h
extern void OpenGLFlipTexturesVerticallyOnLoad(bool flag_true_if_should_flip);
extern GLuint OpenGLLoadTexture(const char* filename,int req_comp,bool useMipmapsIfPossible,bool wraps,bool wrapt);
extern GLuint OpenGLLoadTextureFromMemory(const unsigned char* filenameInMemory, int filenameInMemorySize, int req_comp, bool useMipmapsIfPossible,bool wraps,bool wrapt);
*/

//extern GLuint LoadTextureFromFile(const char* filename,bool oneChannelIsAlpha=false,bool flipImageVertically=false,GLenum GL_KIND_MIN=GL_LINEAR_MIPMAP_LINEAR,GLenum GL_KIND_MAG=GL_LINEAR,GLenum GL_KIND_WRAP_S=GL_REPEAT,GLenum GL_KIND_WRAP_T=GL_REPEAT);
//extern GLuint LoadTextureFromMemory(const unsigned char* buffer,int len,bool oneChannelIsAlpha=false,bool flipImageVertically=false,GLenum GL_KIND_MIN=GL_LINEAR_MIPMAP_LINEAR,GLenum GL_KIND_MAG=GL_LINEAR,GLenum GL_KIND_WRAP_S=GL_REPEAT,GLenum GL_KIND_WRAP_T=GL_REPEAT);
struct OpenGLTextureFlipperScope {
    OpenGLTextureFlipperScope() {OpenGLFlipTexturesVerticallyOnLoad(true);}
    ~OpenGLTextureFlipperScope() {OpenGLFlipTexturesVerticallyOnLoad(false);}
};


struct BlenderTexture {
    typedef glm::vec2 vec2;

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
    float difffac,norfac,specfac,alphafac,dispfac;  // Not used
    enum BlendType  {   // No idea on where this struct comes from right now :)
        BT_BLEND = 0,
        BT_MUL = 1,     // The only one we support...
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
    BlenderTexture() : id(0),repX(1),repY(1),offX(0),offY(0),mapping(MAP_COLOR),blendtype(BT_BLEND),alphafac(1),dispfac(0.2f),difffac(1),norfac(1),specfac(1) {}
    void transformTexCoords(float uin,float vin,vec2& uvout)	const {
        float u = offX + repX*uin;
        float v = offY + repY*vin;
        if (repX!=1) u+=0.5f;   // Of course there's no reason for this... (except some bad empirical hack)
        if (repY!=1) v+=0.5f;
        uvout[0]=u;uvout[1]=v;
    }
    void reset() {*this = BlenderTexture();}
};
static bool GetMaterialFromBlender(const Blender::Material* ma,::Material& mOut) {
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
    static const float dif2ambFactor = 0.6f;	// MUST BE <=1. If no ambient color is provided, obtain one by: amb = dif * dif2ambFactor;
    static const float correctTooDimAmbientLightDimThreshold = 0.15f;  //must be bigger than every component to trigger correction
    static const bool cutTooBigCorrectedAmbientLight = true;

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
static GLuint GetTextureFromBlender(const Blender::Image* im,const std::string& blendModelParentFolder="",std::map<uintptr_t,GLuint>* pPackedFileMap=NULL,std::string* pOptionalTextureFilePathOut=NULL) {
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
    if (String::StartsWith(texturePath,"//")) texturePath = texturePath.substr(2);
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

    return texId;
}
static bool GetTextureFromBlender(Blender::MTex* const* mtexture,BlenderTexture& btexOut,const std::string& blendModelParentFolder="",std::map<uintptr_t,GLuint>* pPackedFileMap=NULL,BlenderTexture::Mapping mapping = BlenderTexture::MAP_COLOR,std::string* pOptionalTextureFilePathOut=NULL) {
    if (pOptionalTextureFilePathOut) *pOptionalTextureFilePathOut = "";
    btexOut.reset();
    if (!mtexture) return false;

    int i = -1;
    while (mtexture[++i] != 0)	{        
        const Blender::MTex* mtex = mtexture[i];
        if (mtex)	{

            if ((mtex->mapto&((short) mapping))==0) {
                //printf("mtex->mapto(%d)&((short) mapping)=%d)==0\n",mtex->mapto,(int)mapping);
                continue;
            }
            const Blender::Tex* tex = mtex->tex;
            if (tex)	{
                if (tex->type!=8) { // image texture
                    //printf("tex->type(%d)!=8\n",tex->type);
                    continue;
                }
                const Blender::Image* im = tex->ima;
                if (!im) continue;
                btexOut.id = GetTextureFromBlender(im,blendModelParentFolder,pPackedFileMap,pOptionalTextureFilePathOut);
                if (btexOut.id==0) continue;

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

                break;
            }
        }
    }

    return btexOut.id>0;
}

#ifdef ALLOW_SEPARETED_PARTS_PARENTED_TO_A_SINGLE_BONE
#ifndef ALLOW_GLOBAL_OBJECT_TRANSFORM
#define ALLOW_GLOBAL_OBJECT_TRANSFORM
#endif //ALLOW_GLOBAL_OBJECT_TRANSFORM
#endif //ALLOW_SEPARETED_PARTS_PARENTED_TO_A_SINGLE_BONE


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

struct BlenderModifierMirror {
    typedef glm::vec2 vec2;
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
   BlenderModifierMirror() :
   enabled(false) {}

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
   inline vec2 mirrorUV(const vec2& tc) const {
       vec2 rv;
       if (mirrorU) rv[0] = -tc[0];
       else rv[0] = tc[0];
       if (mirrorV) rv[1] = -tc[1];
       else rv[1] = tc[1];
       return rv;
   }
};
/*
void btTriangleMeshEx::Bone::displayDebugInfo(bool displayAdditionalBoneData) const {
    printf("%d) %s  mirror:%d   parent:%d",id,name.c_str(),mirrorBoneId,parentBoneId);
    const int numChildBones = childBoneIds.size();
    if (numChildBones>0) {
        printf("    childBones:");
        for (int i=0;i<numChildBones;i++) printf(" %d",childBoneIds[i]);
    }
    printf("\n");
    if (displayAdditionalBoneData)  {
    printf("roll:%1.3f head:(%1.3f %1.3f %1.3f) tail:(%1.3f %1.3f %1.3f) flag:%d bone_mat:\n",btDegrees(roll),head[0],head[1],head[2],tail[0],tail[1],tail[2],flag);
    for (int i=0;i<3;i++) {for (int j=0;j<3;j++) printf("    %1.3f",bone_mat[i][j]);printf("\n");}
    printf("arm_roll:%1.3f arm_head:(%1.3f %1.3f %1.3f) arm_tail:(%1.3f %1.3f %1.3f) arm_mat:\n",btDegrees(arm_roll),arm_head[0],arm_head[1],arm_head[2],arm_tail[0],arm_tail[1],arm_tail[2]);
    for (int i=0;i<4;i++) {for (int j=0;j<4;j++) printf("    %1.3f",arm_mat[i][j]);printf("\n");}
    printf("dist:%1.3f weight:%1.3f xwidth:%1.3f length:%1.3f zwidth:%1.3f ease1:%1.3f ease2:%1.3f rad_head:%1.3f rad_tail:%1.3f \n",
    dist,weight,xwidth,length,zwidth,ease1,ease2,rad_head,rad_tail);
    printf("size:(%1.3f %1.3f %1.3f) layer:%d segments:%d\n",size[0],size[1],size[2],layer,segments);
    }

}
void btTriangleMeshEx::Armature::displayDebugInfo(bool displayAdditionalBoneData) const {
    printf("%d) armature name:%s\n",id,name.c_str());
    const int numBones = size();
    if (numBones>0) {
        for (int i=0;i<numBones;i++) (*this)[i].displayDebugInfo(displayAdditionalBoneData);
    }
    const int numRootBones = rootBoneIds.size();
    if (numRootBones>0) {
        printf("ROOT BONE IDS: ");
        for (int i=0;i<numRootBones;i++) printf(" %d",rootBoneIds[i]);
    }
    printf("\n");
}
*/

class BlenderHelper {
    public:
    typedef glm::vec2 vec2;
    typedef glm::vec3 vec3;
    typedef glm::mat3 mat3;
    typedef glm::mat4 mat4;
    typedef glm::quat quat;

    static const Blender::bArmature* GetArmature(const Blender::Object* ob) {return ( (ob && ob->type == (short) BL_OBTYPE_ARMATURE) ? (const Blender::bArmature*) ob->data : NULL);}
    static void Display(const Blender::bArmature* me) {
            if (!me) return;

            fbtPrintf("\tArmature: %s\n", me->id.name);

            fbtPrintf("\tflag: %1d\n", me->flag);					//
            fbtPrintf("\tdrawtype: %1d\n", me->drawtype);			//
            fbtPrintf("\tdeformflag: %1d\n", me->deformflag);		//
            fbtPrintf("\tpathflag: %1d\n", me->pathflag);			//
            fbtPrintf("\tlayer_used: %1d\n", me->layer_used);		//
            fbtPrintf("\tlayer: %1d\n", me->layer);					//
            fbtPrintf("\tlayer_protected: %1d\n", me->layer_protected);					//
            fbtPrintf("\tghostep: %1d\n", me->ghostep);				//
            fbtPrintf("\tghostsize: %1d\n", me->ghostsize);			//
            fbtPrintf("\tghosttype: %1d\n", me->ghosttype);			//
            fbtPrintf("\tpathsize: %1d\n", me->pathsize);			//
            fbtPrintf("\tghostsf: %1d\n", me->ghostsf);				//
            fbtPrintf("\tghostef: %1d\n", me->ghostef);				//
            fbtPrintf("\tpathsf: %1d\n", me->pathsf);				//
            fbtPrintf("\tpathef: %1d\n", me->pathef);				//
            fbtPrintf("\tpathbc: %1d\n", me->pathbc);				//
            fbtPrintf("\tpathac: %1d\n", me->pathac);				//

    }
    static void Display(const Blender::Bone* b) {
        if (!b) return;
        fbtPrintf("\n\t\tBONE: %s\n", b->name);
        if (b->parent) fbtPrintf("\t\tparent: %s\n", b->parent->name);
        else fbtPrintf("\t\tparent: none (ROOT BONE)\n");
        const Blender::Bone* c = (const Blender::Bone*) b->childbase.first;
        if (c) fbtPrintf("\t\tchild bones: ");
        while (c)	{
            fbtPrintf("%s ", c->name);
            c = c->next;
        }
        if (b->childbase.first) fbtPrintf("\n");
        fbtPrintf("\t\troll: %1.4f\n", b->roll);
        fbtPrintf("\t\thead: (%1.4f,%1.4f,%1.4f)\n", b->head[0],b->head[1],b->head[2]);
        fbtPrintf("\t\ttail: (%1.4f,%1.4f,%1.4f)\n", b->tail[0],b->tail[1],b->tail[2]);
        fbtPrintf("\t\tbone_mat:	(%1.4f,%1.4f,%1.4f)\n\t\t	(%1.4f,%1.4f,%1.4f)\n\t\t	(%1.4f,%1.4f,%1.4f)\n", b->bone_mat[0][0],b->bone_mat[0][1],b->bone_mat[0][2],b->bone_mat[1][0],b->bone_mat[1][1],b->bone_mat[1][2],b->bone_mat[2][0],b->bone_mat[2][1],b->bone_mat[2][2]);
        fbtPrintf("\t\tflag: %1d\n", b->flag);
        fbtPrintf("\t\tarm_head: (%1.4f,%1.4f,%1.4f)\n", b->arm_head[0],b->arm_head[1],b->arm_head[2]);
        fbtPrintf("\t\tarm_tail: (%1.4f,%1.4f,%1.4f)\n", b->arm_tail[0],b->arm_tail[1],b->arm_tail[2]);
        fbtPrintf("\t\tarm_mat:	(%1.4f,%1.4f,%1.4f,%1.4f)\n\t\t	(%1.4f,%1.4f,%1.4f,%1.4f)\n\t\t	(%1.4f,%1.4f,%1.4f,%1.4f)\n\t\t	(%1.4f,%1.4f,%1.4f,%1.4f)\n", b->arm_mat[0][0],b->arm_mat[0][1],b->arm_mat[0][2],b->arm_mat[0][3], b->arm_mat[1][0],b->arm_mat[1][1],b->arm_mat[1][2],b->arm_mat[1][3], b->arm_mat[2][0],b->arm_mat[2][1],b->arm_mat[2][2],b->arm_mat[2][3], b->arm_mat[3][0],b->arm_mat[3][1],b->arm_mat[3][2],b->arm_mat[3][3]);
        fbtPrintf("\t\tarm_roll: %1.4f\n", b->arm_roll);
        fbtPrintf("\t\tdist: %1.4f\n", b->dist);
        fbtPrintf("\t\tweight: %1.4f\n", b->weight);
        fbtPrintf("\t\txwidth: %1.4f\n", b->xwidth);
        fbtPrintf("\t\tlength: %1.4f\n", b->length);
        fbtPrintf("\t\tzwidth: %1.4f\n", b->zwidth);
        fbtPrintf("\t\tease1: %1.4f\n", b->ease1);
        fbtPrintf("\t\tease2: %1.4f\n", b->ease2);
        fbtPrintf("\t\trad_head: %1.4f\n", b->rad_head);
        fbtPrintf("\t\trad_tail: %1.4f\n", b->rad_tail);
        fbtPrintf("\t\tsize: (%1.4f,%1.4f,%1.4f)\n", b->size[0],b->size[1],b->size[2]);
        fbtPrintf("\t\tlayer: %1d\n", b->layer);
        fbtPrintf("\t\tsegments: %1d\n", b->segments);
        fbtPrintf("\t\tpad1: %1d\n", b->pad1);

        c = (const Blender::Bone*) b->childbase.first;
        while (c)	{
            Display(c);
            c = c->next;
        }
    }

    static void DisplayPoseChannelBriefly(const Blender::bPoseChannel* p) {
        if (!p) return;
        printf("bPoseChannel %s rotmode:%d eul:{%1.4f %1.4f %1.4f};\n",p->name,p->rotmode,p->eul[0],p->eul[1],p->eul[2]);
        //TODO: constraints are here too
    }
    // TEST STUFF -------------------------------------------------------
    #ifdef NOPE
    enum ActiononstraintType {
        ACT_CONST_LOCX  =                         1,
        ACT_CONST_LOCY  =                         2,
        ACT_CONST_LOCZ  =                         4,
        ACT_CONST_ROTX  =                         8,
        ACT_CONST_ROTY  =                         16,
        ACT_CONST_ROTZ  =                         32,
        ACT_CONST_NORMAL =                        64,
        ACT_CONST_MATERIAL =                      128,
        ACT_CONST_PERMANENT =                     256,
        ACT_CONST_DISTANCE =                      512,
        ACT_CONST_LOCAL =                         1024,
        ACT_CONST_DOROTFH =                       2048
    };
    enum ActionChannelType {
        AC_LOC_X =                                1,
        AC_LOC_Y =                                2,
        AC_LOC_Z =                                3,
        AC_SIZE_X =                               13,
        AC_SIZE_Y =                               14,
        AC_SIZE_Z =                               15,
        AC_EUL_X  =                               16,
        AC_EUL_Y  =                               17,
        AC_EUL_Z  =                               18,
        AC_QUAT_W =                               25,
        AC_QUAT_X =                               26,
        AC_QUAT_Y =                               27,
        AC_QUAT_Z =                               28
    };
    /*
    enum ActionChannelType {
        AC_LOC_X =                                1,
        AC_LOC_Y =                                2,
        AC_LOC_Z =                                4,
        AC_SIZE_X =                               8,
        AC_SIZE_Y =                               16,
        AC_SIZE_Z =                               32,
        AC_EUL_X  =                               64,
        AC_EUL_Y  =                               128,
        AC_EUL_Z  =                               256,
        AC_QUAT_W =                               512,
        AC_QUAT_X =                               1024,
        AC_QUAT_Y =                               2048,
        AC_QUAT_Z =                               4096
    };*/
    inline static void DisplayActionChannelType(int i) {
        string rv = "";
        if (i&AC_LOC_X) rv+="locX";
        if (i&AC_LOC_Y) rv+="locY";
        if (i&AC_LOC_Z) rv+="locZ";
        if (i&AC_SIZE_X) rv+="scaX";
        if (i&AC_SIZE_Y) rv+="scaY";
        if (i&AC_SIZE_Z) rv+="scaZ";
        if (i&AC_EUL_X) rv+="eulX";
        if (i&AC_EUL_Y) rv+="eulY";
        if (i&AC_EUL_Z) rv+="eulZ";
        if (i&AC_QUAT_W) rv+="quaW";
        if (i&AC_QUAT_X) rv+="quaX";
        if (i&AC_QUAT_Y) rv+="quaY";
        if (i&AC_QUAT_Z) rv+="quaZ";
        fbtPrintf("%s",rv.c_str());

   }
   #endif //NOPE
    static void Display(const Blender::bPoseChannel* p,int cnt=0)	{
        if (!p) return;
        /*
struct bPoseChannel
{
    bPoseChannel *next;
    bPoseChannel *prev;
    IDProperty *prop;
    ListBase constraints;
    char name[32];
    short flag;
    short constflag;
    short ikflag;
    short selectflag;
    short protectflag;
    short agrp_index;
    int pathlen;
    int pathsf;
    int pathef;
    Bone *bone;
    bPoseChannel *parent;
    bPoseChannel *child;
    ListBase iktree;
    bMotionPath *mpath;
    Object *custom;
    bPoseChannel *custom_tx;
    float loc[3];
    float size[3];
    float eul[3];
    float quat[4];
    float rotAxis[3];
    float rotAngle;
    short rotmode;
    short pad;
    float chan_mat[4][4];
    float pose_mat[4][4];
    float constinv[4][4];
    float pose_head[3];
    float pose_tail[3];
    float limitmin[3];
    float limitmax[3];
    float stiffness[3];
    float ikstretch;
    float ikrotweight;
    float iklinweight;
    float *path;
};
    */
        fbtPrintf("\tSTART POSECHANNEL %1d) %s:\n",cnt,p->name);
        if (p->prop) fbtPrintf("\tprop present\n");
        if (p->constraints.first) fbtPrintf("\tconstraints present\n");
        if (p->bone) fbtPrintf("\tbone: %s",p->bone->name);
        if (p->parent) fbtPrintf(" parent channel: %s\n",p->parent->name);
        if (p->child) {
            const Blender::bPoseChannel* c = p->child;
            fbtPrintf("\tchild channels: %s ",c->name);
            while ((c=c->next)) fbtPrintf("%s ",c->name);
            fbtPrintf("\n");
        }
        fbtPrintf("\tflag:%1d", p->flag);			//
        fbtPrintf(" constflag:%1d", p->constflag);			//
        fbtPrintf(" ikflag:%1d", p->ikflag);			//
        fbtPrintf(" selectflag:%1d\n", p->selectflag);			//
        fbtPrintf("\tagrp_index:%1d", p->agrp_index);			//
        //fbtPrintf(" pathlen:%1d", p->pathlen);			//
        //fbtPrintf(" pathsf:%1d", p->pathsf);			//
        //fbtPrintf(" pathef:%1d\n", p->pathef);			//
        if (p->iktree.first) fbtPrintf("\tiktree present\n");
        if (p->custom) fbtPrintf("\tcustom object present\n");
        if (p->custom_tx) fbtPrintf("\tcustom_tx pose channel present\n");

        fbtPrintf("\tloc: (%1.4f,%1.4f,%1.4f)\n", p->loc[0],p->loc[1],p->loc[2]);
        fbtPrintf("\tsize: (%1.4f,%1.4f,%1.4f)\n", p->size[0],p->size[1],p->size[2]);
        fbtPrintf("\teul: (%1.4f,%1.4f,%1.4f)\n", p->eul[0],p->eul[1],p->eul[2]);
        fbtPrintf("\tquat: (%1.4f,%1.4f,%1.4f,%1.4f)\n", p->quat[0],p->quat[1],p->quat[2],p->quat[3]);
        fbtPrintf("\trotAxis: (%1.4f,%1.4f,%1.4f)\n", p->rotAxis[0],p->rotAxis[1],p->rotAxis[2]);
        fbtPrintf("\trotAngle:%1.4f", p->rotAngle);			//
        fbtPrintf(" rotmode:%1d", p->rotmode);			//
        fbtPrintf(" pad:%1d\n", p->pad);			//

        fbtPrintf("\tchan_mat:	(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n", p->chan_mat[0][0],p->chan_mat[0][1],p->chan_mat[0][2],p->chan_mat[0][3], p->chan_mat[1][0],p->chan_mat[1][1],p->chan_mat[1][2],p->chan_mat[1][3], p->chan_mat[2][0],p->chan_mat[2][1],p->chan_mat[2][2],p->chan_mat[2][3], p->chan_mat[3][0],p->chan_mat[3][1],p->chan_mat[3][2],p->chan_mat[3][3]);
        fbtPrintf("\tpose_mat:	(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n", p->pose_mat[0][0],p->pose_mat[0][1],p->pose_mat[0][2],p->pose_mat[0][3], p->pose_mat[1][0],p->pose_mat[1][1],p->pose_mat[1][2],p->pose_mat[1][3], p->pose_mat[2][0],p->pose_mat[2][1],p->pose_mat[2][2],p->pose_mat[2][3], p->pose_mat[3][0],p->pose_mat[3][1],p->pose_mat[3][2],p->pose_mat[3][3]);
        fbtPrintf("\tconstinv:	(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n", p->constinv[0][0],p->constinv[0][1],p->constinv[0][2],p->constinv[0][3], p->constinv[1][0],p->constinv[1][1],p->constinv[1][2],p->constinv[1][3], p->constinv[2][0],p->constinv[2][1],p->constinv[2][2],p->constinv[2][3], p->constinv[3][0],p->constinv[3][1],p->constinv[3][2],p->constinv[3][3]);
        //fbtPrintf("\t:	(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n\t		(%1.4f,%1.4f,%1.4f,%1.4f)\n", p->[0][0],p->[0][1],p->[0][2],p->[0][3], p->[1][0],p->[1][1],p->[1][2],p->[1][3], p->[2][0],p->[2][1],p->[2][2],p->[2][3], p->[3][0],p->[3][1],p->[3][2],p->[3][3]);


        fbtPrintf("\tpose_head: (%1.4f,%1.4f,%1.4f)\n", p->pose_head[0],p->pose_head[1],p->pose_head[2]);
        fbtPrintf("\tpose_tail: (%1.4f,%1.4f,%1.4f)\n", p->pose_tail[0],p->pose_tail[1],p->pose_tail[2]);
        fbtPrintf("\tlimitmin: (%1.4f,%1.4f,%1.4f)\n", p->limitmin[0],p->limitmin[1],p->limitmin[2]);
        fbtPrintf("\tlimitmax: (%1.4f,%1.4f,%1.4f)\n", p->limitmax[0],p->limitmax[1],p->limitmax[2]);
        fbtPrintf("\tstiffness: (%1.4f,%1.4f,%1.4f)\n", p->stiffness[0],p->stiffness[1],p->stiffness[2]);


        fbtPrintf("\tikstretch:%1.4f", p->ikstretch);			//
        fbtPrintf(" ikrotweight:%1.4f", p->ikrotweight);			//
        fbtPrintf(" iklinweight:%1.4f\n", p->iklinweight);			//
        //if (p->path) fbtPrintf("\t: %1.4f\n", p->path);			//

        fbtPrintf("\tEND POSECHANNEL %1d) %s\n",cnt, p->name);

        //DisplayPoseChannels(p->child,cnt+1);
        Display(p->next,cnt+1);
    }
    static void Display(const Blender::AnimData* an)	{
        if (!an) return;
        const Blender::AnimData& a = *an;
        /*
struct AnimData
{
    bAction *action;
    bAction *tmpact;
    AnimMapper *remap;
    ListBase nla_tracks;
    NlaStrip *actstrip;
    ListBase drivers;
    ListBase overrides;
    int flag;
    int recalc;
    short act_blendmode;
    short act_extendmode;
    float act_influence;
};
    */
        fbtPrintf("\tSTART ANIMDATA:\n");

        if (a.action) {
            fbtPrintf("\taction present:\n");
            Display(a.action);
        }
        if (a.tmpact) {
            fbtPrintf("\ttmpact present:\n");
            Display(a.tmpact);
        }
        if (a.remap) fbtPrintf("\tremap present\n");

        if (a.nla_tracks.first) fbtPrintf("\tnla_tracks present\n");
        if (a.actstrip) fbtPrintf("\tactstrip present\n");
        if (a.drivers.first) fbtPrintf("\tdrivers present\n");
        if (a.overrides.first) fbtPrintf("\toverrides present\n");

        fbtPrintf("\tflag: %1d\n", a.flag);
        fbtPrintf("\trecalc: %1d\n", a.recalc);
        fbtPrintf("\tact_blendmode: %1d\n", a.act_blendmode);
        fbtPrintf("\tact_extendmode: %1d\n", a.act_extendmode);
        fbtPrintf("\tact_influence: %1.4f\n", a.act_influence);			//

        fbtPrintf("\tEND ANIMDATA\n");
    }
    static void Display(const Blender::bAction* a)	{
        if (!a) return;
        /*
struct bAction
{
    ID id;
    ListBase curves;
    ListBase chanbase;
    ListBase groups;
    ListBase markers;
    int flag;
    int active_marker;
};
*/
        /* Action - reusable F-Curve 'bag'  (act)
 *
 * This contains F-Curves that may affect settings from more than one ID blocktype and/or
 * datablock (i.e. sub-data linked/used directly to the ID block that the animation data is linked to),
 * but with the restriction that the other unrelated data (i.e. data that is not directly used or linked to
 * by the source ID block).
 *
 * It serves as a 'unit' of reusable animation information (i.e. keyframes/motion data), that
 * affects a group of related settings (as defined by the user).
 */
        /*
typedef struct bAction {
    ID 	id;				// ID-serialisation for relinking

    ListBase curves;	// function-curves (FCurve)
    ListBase chanbase;	// legacy data - Action Channels (bActionChannel) in pre-2.5 animation system
    ListBase groups;	// groups of function-curves (bActionGroup)
    ListBase markers;	// markers local to the Action (used to provide Pose-Libraries)

    int flag;			// settings for this action
    int active_marker;	// index of the active marker

    int idroot;			// type of ID-blocks that action can be assigned to (if 0, will be set to whatever ID first evaluates it)
    int pad;
} bAction;
    */
        fbtPrintf("\t\tSTART ACTION: %s\n",a->id.name);

        if (a->curves.first) {
            fbtPrintf("\t\tcurves present:\n");int cnt=0;
            Display((const Blender::FCurve*)a->curves.first,cnt);
        }
        if (a->chanbase.first) fbtPrintf("\t\tchanbase present\n");
        if (a->groups.first) {
            fbtPrintf("\t\tgroups present:\n");
            Display((const Blender::bActionGroup*)a->groups.first);
        }
        if (a->markers.first) fbtPrintf("\t\tmarkers present\n");

        fbtPrintf("\t\tflag: %1d\n", a->flag);
        fbtPrintf("\t\tactive_marker: %1d\n", a->active_marker);

        fbtPrintf("\t\tEND ACTION: %s\n",a->id.name);
    }
    static void Display(const Blender::bActionChannel* a)	{
        if (!a) return;
        /*
struct bActionChannel
{
    bActionChannel *next;
    bActionChannel *prev;
    bActionGroup *grp;
    Ipo *ipo;
    ListBase constraintChannels;
    int flag;
    char name[32];
    int temp;
};
*/
        /* Legacy Data */
        /* WARNING: Action Channels are now depreceated... they were part of the old animation system!
 * 		  (ONLY USED FOR DO_VERSIONS...)
 *
 * Action Channels belong to Actions. They are linked with an IPO block, and can also own
 * Constraint Channels in certain situations.
 *
 * Action-Channels can only belong to one group at a time, but they still live the Action's
 * list of achans (to preserve backwards compatability, and also minimise the code
 * that would need to be recoded). Grouped achans are stored at the start of the list, according
 * to the position of the group in the list, and their position within the group.
 */
        /*
typedef struct bActionChannel {
    struct bActionChannel	*next, *prev;
    bActionGroup 			*grp;					// Action Group this Action Channel belongs to

    struct Ipo				*ipo;					// IPO block this action channel references
    ListBase				constraintChannels;		// Constraint Channels (when Action Channel represents an Object or Bone)

    int		flag;			// settings accessed via bitmapping
    char	name[32];		// channel name
    int		temp;			// temporary setting - may be used to indicate group that channel belongs to during syncing
} bActionChannel;
    */


        if (strlen(a->name)>0) {
            fbtPrintf("\t\t\t\tSTART ACTIONCHANNEL: %s\n",a->name);

            //if (a->grp) fbtPrintf("\t\t\t\tgrp: %s\n",a->grp->name);
            if (a->ipo) {
                fbtPrintf("\t\t\t\tipo present:\n");
                Display(a->ipo);
            }
            if (a->constraintChannels.first) {
                fbtPrintf("\t\t\t\tconstraintChannels present\n");
            }

            fbtPrintf("\t\t\t\tflag: %1d\n", a->flag);
            fbtPrintf("\t\t\t\ttemp: %1d\n", a->temp);

            fbtPrintf("\t\t\t\tEND ACTIONCHANNEL: %s\n",a->name);
        }
        else {
            if (a->temp!=0) fbtPrintf("\t\t\t\tActionChannel: flag: %d temp: %1d\n",a->flag,a->temp);
            else {
                fbtPrintf("\t\t\t\tActionChannel: flag: %d\n",a->flag);
                /* //NOPE
                fbtPrintf("\t\t\t\tActionChannel: flag: ");
                DisplayActionChannelType(a->flag);
                fbtPrintf("\n");
                */
            }
        }

        if (a->next) Display(a->next);

    }
    static void Display(const Blender::bActionGroup* a)	{
        if (!a) return;
        /*
struct bActionGroup
{
    bActionGroup *next;
    bActionGroup *prev;
    ListBase channels;
    int flag;
    int customCol;
    char name[64];
    ThemeWireColor cs;
};
    */
        fbtPrintf("\t\t\tSTART ACTIONGROUP: %s\n",a->name);

        if (a->channels.first) {
            fbtPrintf("\t\t\tchannels present:\n");
            Display((const Blender::bActionChannel*)a->channels.first);

            //DisplayPoseChannelBriefly((const Blender::bPoseChannel*)a->channels.first);//nope
        }

        if (a->flag!=0) {
            /*
#define ACT_GROUP_PLAY                           0
#define ACT_GROUP_PINGPONG                       1
#define ACT_GROUP_FLIPPER                        2
#define ACT_GROUP_LOOP_STOP                      3
#define ACT_GROUP_LOOP_END                       4
#define ACT_GROUP_FROM_PROP                      5
#define ACT_GROUP_SET                            6

#define ACT_ACTION_PLAY                          0
#define ACT_ACTION_PINGPONG                      1
#define ACT_ACTION_FLIPPER                       2
#define ACT_ACTION_LOOP_STOP                     3
#define ACT_ACTION_LOOP_END                      4
#define ACT_ACTION_KEY2KEY                       5
#define ACT_ACTION_FROM_PROP                     6
#define ACT_ACTION_MOTION                        7

#define ACT_IPO_PLAY                             0
#define ACT_IPO_PINGPONG                         1
#define ACT_IPO_FLIPPER                          2
#define ACT_IPO_LOOP_STOP                        3
#define ACT_IPO_LOOP_END                         4
#define ACT_IPO_KEY2KEY                          5
#define ACT_IPO_FROM_PROP                        6
            */
            fbtPrintf("\t\t\tflag: %1d\n", a->flag);
        }
        if (a->customCol!=0) fbtPrintf("\t\t\tcustomCol: %1d\n", a->customCol);

        fbtPrintf("\t\t\tEND ACTIONGROUP: %s\n",a->name);

        if (a->next) Display(a->next);
    }
    static void Display(const Blender::FCurve* a,int num=0,const char* indent="\t\t")	{
        if (!a) return;
        /*
    struct FCurve
{
    FCurve *next;
    FCurve *prev;
    bActionGroup *grp;
    ChannelDriver *driver;
    ListBase modifiers;
    BezTriple *bezt;
    FPoint *fpt;
    int totvert;
    float curval;
    short flag;
    short extend;
    int array_index;
    char *rna_path;
    int color_mode;
    float color[3];
};
    */
        /* 'Function-Curve' - defines values over time for a given setting (fcu) */
        //typedef struct FCurve {
        //	struct FCurve *next, *prev;

        /* group */
        //	bActionGroup *grp;		/* group that F-Curve belongs to */

        /* driver settings */
        //	ChannelDriver *driver;	/* only valid for drivers (i.e. stored in AnimData not Actions) */
        /* evaluation settings */
        //	ListBase modifiers;		/* FCurve Modifiers */

        /* motion data */
        //	BezTriple *bezt;		/* user-editable keyframes (array) */
        //	FPoint *fpt;			/* 'baked/imported' motion samples (array) */
        //	unsigned int totvert;	/* total number of points which define the curve (i.e. size of arrays in FPoints) */	// It's the size of bezt too and the number of keyframes (but where is the time associated to each keyframe ?

        /* value cache + settings */
        //	float curval;			/* value stored from last time curve was evaluated */
        //	short flag;				/* user-editable settings for this curve */
        //	short extend;			/* value-extending mode for this curve (does not cover  */

        /* RNA - data link */
        //	int array_index;		/* if applicable, the index of the RNA-array item to get */	// For quaternions the order should be (W,X,Y,Z), but where is this single number inside each bezt (9 numbers) ?
        //	char *rna_path;			/* RNA-path to resolve data-access */

        /* curve coloring (for editor) */
        //	int color_mode;			/* coloring method to use (eFCurve_Coloring) */
        //	float color[3];			/* the last-color this curve took */
        //} FCurve;

        fbtPrintf("%sSTART FCURVE: %1d\n",indent,		num);
        fbtPrintf("%srna_path: %s[%1d]\n",indent,		a->rna_path,a->array_index);
        //if (a->grp) fbtPrintf("%sgrp: %s\n",indent,a->grp->name);
        if (a->driver) fbtPrintf("%schannel driver present\n",indent);
        if (a->modifiers.first) fbtPrintf("%smodifiers driver present\n",indent);
        // IMPORTANT -----------------------------------------------------
        if (a->bezt && a->totvert) {
            for (int i=0;i<a->totvert;i++)	{
                Display(a->bezt[i],indent,i);
            }
        }
        // ----------------------------------------------------------------
        if (a->fpt && a->totvert) {
            /*
        struct FPoint
{
    float vec[2];
    int flag;
    int pad;
}
        */
            for (int i=0;i<a->totvert;i++)	{
                fbtPrintf("%sfpt[%1d]: (%1.4f,%1.4f) flag:%1d pad:%1d\n",indent,i,a->fpt[i].vec[0],a->fpt[i].vec[1],a->fpt[i].flag,a->fpt[i].pad);
            }

        }

        /*
        if (a->flag!=35 || a->extend!=0)	{
            fbtPrintf("%stotvert:%1d",indent, 			a->totvert);
            fbtPrintf(" curval:%1.4f",	 		a->curval);
            fbtPrintf(" flag:%1d", 			a->flag);
            if (a->extend!=0) fbtPrintf(" extend: %1d",	a->extend);
            fbtPrintf("\n");
        }
        if (a->color_mode!=0 || a->color[0]!=0 || a->color[1]!=0 || a->color[2]!=0)	{
            fbtPrintf("%scolor_mode: %1d",indent, 			a->color_mode);
            fbtPrintf(" color:(%1.2f,%1.2f,%1.2f)\n",a->color[0],a->color[1],a->color[2]);
        }
        */
        fbtPrintf("%sEND FCURVE: %1d\n",indent,		num);
        Display(a->next,num+1,indent);
    }
    static void Display(const Blender::BezTriple& p,const char* indent="\t\t\t",int num=0)	{
        /*
                    struct BezTriple	{
                        float vec[3][3];
                        float alfa;
                        float weight;
                        float radius;
                        short ipo;
                        char h1;
                        char h2;
                        char f1;
                        char f2;
                        char f3;
                        char hide;
                    };
                    */
        /* Keyframes on F-Curves (allows code reuse of Bezier eval code) and
 * Points on Bezier Curves/Paths are generally BezTriples
 */
        /* note: alfa location in struct is abused by Key system */				// FOR QUATERNION KEYFRAMES IN FCURVES:
        /* vec in BezTriple looks like this: vec[?][0] => frame of the key, vec[?][1] => actual value, vec[?][2] == 0
    vec[0][0]=x location of handle 1									// This should be the actual value
    vec[0][1]=y location of handle 1									// Unknown
    vec[0][2]=z location of handle 1 (not used for FCurve Points(2d))	// This is zero
    vec[1][0]=x location of control point								// Number of frame of the key
    vec[1][1]=y location of control point								// Unknown
    vec[1][2]=z location of control point								// This is zero
    vec[2][0]=x location of handle 2									// Unknown
    vec[2][1]=y location of handle 2									// Unknown
    vec[2][2]=z location of handle 2 (not used for FCurve Points(2d))	// This is zero
*/
        // typedef struct BezTriple {
        //	float vec[3][3];
        //	float alfa, weight, radius;	/* alfa: tilt in 3D View, weight: used for softbody goal weight, radius: for bevel tapering */
        //	short ipo;					/* ipo: interpolation mode for segment from this BezTriple to the next */
        //	char h1, h2; 				/* h1, h2: the handle type of the two handles */
        //	char f1, f2, f3;			/* f1, f2, f3: used for selection status */
        //	char hide;					/* hide: used to indicate whether BezTriple is hidden (3D), type of keyframe (eBezTriple_KeyframeTypes) */
        // } BezTriple;
        //
        fbtPrintf("%sBT[%1d]:",indent,num);
        //fbtPrintf("(%1.2f,%1.2f,%1.2f,%1.2f,%1.2f,%1.2f,%1.2f,%1.2f,%1.2f)\n",p.vec[0][0],p.vec[0][1],p.vec[0][2],p.vec[1][0],p.vec[1][1],p.vec[1][2],p.vec[2][0],p.vec[2][1],p.vec[2][2]);

        fbtPrintf("(frame:%1.2f: %1.2f)\n",p.vec[1][0],p.vec[2][1]);


        char newIndent[25];newIndent[0]='\0';char tab[2];tab[0]='\t';tab[1]='\0';strcat(newIndent,indent);strcat(newIndent,tab);
        if (p.alfa!=0 || p.weight!=0 || p.radius!=0)	{
            fbtPrintf("%salfa: %1.4f",newIndent, p.alfa);
            fbtPrintf("	weight: %1.4f", p.weight);
            fbtPrintf("	radius: %1.4f\n", p.radius);
        }
        if (p.ipo!=2 || p.h1!=1 || p.h2!=1 || p.f1!=1 || p.f2!=1 || p.f3!=1 || p.hide!=0)	{
            fbtPrintf("%sipo:%1d",newIndent, p.ipo);
            fbtPrintf(" h1:%1d", p.h1);
            fbtPrintf(" h2:%1d", p.h2);
            fbtPrintf(" f1:%1d", p.f1);
            fbtPrintf(" f2:%1d", p.f2);
            fbtPrintf(" f3:%1d", p.f3);
            fbtPrintf(" hide:%1d\n", p.hide);
        }

        //fbtPrintf("%sEND BEZTRIPLE\n",indent);
    }
    static void Display(const Blender::IpoCurve* c,int& iccnt)	{
        if (!c) return;
        fbtPrintf("\t\t\tSTART IPO CURVE %1d\n", iccnt);
        if (c->bp)	{
            const Blender::BPoint& p = *(c->bp);
            /*
                    struct BPoint	{
                        float vec[4];
                        float alfa;
                        float weight;
                        short f1;
                        short hide;
                        float radius;
                        float pad;
                    };
                    */
            fbtPrintf("\t\t\tBPOINT\n");
            fbtPrintf("\t\t\tvec: (%1.2f,%1.2f,%1.2f)\n", p.vec[0],p.vec[1],p.vec[2]);

            fbtPrintf("\t\t\talfa: %1.4f\n", p.alfa);
            fbtPrintf("\t\t\tweight: %1.4f\n", p.weight);

            fbtPrintf("\t\t\tf1: %1d\n", p.f1);
            fbtPrintf("\t\t\thide: %1d\n", p.hide);

            fbtPrintf("\t\t\tradius: %1.4f\n", p.radius);
            fbtPrintf("\t\t\tpad: %1.4f\n", p.pad);
            fbtPrintf("\t\t\tEND BPOINT\n");
        }
        if (c->bezt)	{
            Display(*c->bezt);
        }

        fbtPrintf("\t\t\tmaxrct: (%1.2f,%1.2f,%1.2f,%1.2f)\n", c->maxrct.xmin,c->maxrct.ymin,c->maxrct.xmax,c->maxrct.ymax);
        fbtPrintf("\t\t\ttotrct: (%1.2f,%1.2f,%1.2f,%1.2f)\n", c->totrct.xmin,c->totrct.ymin,c->totrct.xmax,c->totrct.ymax);

        fbtPrintf("\t\t\tblocktype: %1d\n", c->blocktype);
        fbtPrintf("\t\t\tadrcode: %1d\n", c->adrcode);
        fbtPrintf("\t\t\tvartype: %1d\n", c->vartype);
        fbtPrintf("\t\t\ttotvert: %1d\n", c->totvert);
        fbtPrintf("\t\t\tipo: %1d\n", c->ipo);
        fbtPrintf("\t\t\textrap: %1d\n", c->extrap);
        fbtPrintf("\t\t\tflag: %1d\n", c->flag);
        fbtPrintf("\t\t\trt: %1d\n", c->rt);
        fbtPrintf("\t\t\tymin: %1.4f\n", c->ymin);
        fbtPrintf("\t\t\tymax: %1.4f\n", c->ymax);
        fbtPrintf("\t\t\tbitmask: %1d\n", c->bitmask);
        fbtPrintf("\t\t\tslide_min: %1.4f\n", c->slide_min);
        fbtPrintf("\t\t\tslide_max: %1.4f\n", c->slide_max);
        fbtPrintf("\t\t\tcurval: %1.4f\n", c->curval);

        if (c->driver)	{
            const Blender::IpoDriver& d = *(c->driver);
            /*
                    struct IpoDriver	{
                        Object *ob;
                        short blocktype;
                        short adrcode;
                        short type;
                        short flag;
                        char name[128];
                    };
                    */
            fbtPrintf("\t\t\tIPODRIVER\n");
            if (d.ob) fbtPrintf("\t\t\tob: %s\n",d.ob->id.name);
            fbtPrintf("\t\t\tblocktype: %1d\n", d.blocktype);
            fbtPrintf("\t\t\tadrcode: %1d\n", d.adrcode);
            fbtPrintf("\t\t\ttype: %1d\n", d.type);
            fbtPrintf("\t\t\tflag: %1d\n", d.flag);
            fbtPrintf("\t\t\tname: %s\n",d.name);
            fbtPrintf("\t\t\tEND IPODRIVER\n");

        }

        fbtPrintf("\t\t\tEND IPO CURVE %1d\n", ++iccnt);
        c = c->next;
        Display(c,++iccnt);
    }
    static void Display(const Blender::Ipo* i)	{
        if (!i) return;
        /*
struct Ipo
{
    ID id;
    ListBase curve;
    rctf cur;
    short blocktype;
    short showkey;
    short muteipo;
    short pad;
};
struct IpoCurve
{
    IpoCurve *next;
    IpoCurve *prev;
    BPoint *bp;
    BezTriple *bezt;
    rctf maxrct;
    rctf totrct;
    short blocktype;
    short adrcode;
    short vartype;
    short totvert;
    short ipo;
    short extrap;
    short flag;
    short rt;
    float ymin;
    float ymax;
    int bitmask;
    float slide_min;
    float slide_max;
    float curval;
    IpoDriver *driver;
};

                */
        fbtPrintf("\tIPO: %s\n", i->id.name);

        fbtPrintf("\t\tcur (rect): (%1.2f,%1.2f,%1.2f,%1.2f)\n", i->cur.xmin,i->cur.ymin,i->cur.xmax,i->cur.ymax);
        fbtPrintf("\t\tblocktype: %1d\n", i->blocktype);
        fbtPrintf("\t\tshowkey: %1d\n", i->showkey);
        fbtPrintf("\t\tmuteipo: %1d\n", i->muteipo);
        fbtPrintf("\t\tpad: %1d\n", i->pad);

        const Blender::IpoCurve* c = (const Blender::IpoCurve*) i->curve.first;
        int iccnt=0;
        Display(c,iccnt);

        fbtPrintf("\tEND IPO: %s\n", i->id.name);
    }
    static void Display(const Blender::Key* key)	{
        if (!key) return;
        const Blender::Key& k = *key;
        /*
                struct Key	{
                    ID id;
                    AnimData *adt;
                    KeyBlock *refkey;
                    char elemstr[32];
                    int elemsize;
                    float curval;
                    ListBase block;
                    Ipo *ipo;
                    ID *from;
                    short type;
                    short totkey;
                    short slurph;
                    short flag;
                };
                */
        fbtPrintf("\tKEY: %s\n", k.id.name);
        if (k.adt) fbtPrintf("\tAnimation data present\n");	//TODO...
        if (k.refkey) fbtPrintf("\trefkey present\n");		//TODO...
        fbtPrintf("\telemstr: %s\n", k.elemstr);
        fbtPrintf("\telemsize: %1d\n", k.elemsize);
        //fbtPrintf("\tcurval: %1.4f\n", k.curval);
        if (k.block.first) fbtPrintf("\tblocks present\n"); //TODO...
        Display(k.ipo);
        if (k.from) fbtPrintf("\tfrom: %s\n", k.from->name);
        fbtPrintf("\ttype: %1d\n", k.type);
        fbtPrintf("\ttotkey: %1d\n", k.totkey);
        //fbtPrintf("\tslurph: %1d\n", k.slurph);
        fbtPrintf("\tflag: %1d\n", k.flag);

        fbtPrintf("\tEND KEY: %s\n", k.id.name);
    }
    enum ConstraintType {
        CONSTRAINT_SPACE_LOCAL			=		1,
        CONSTRAINT_TYPE_ROTLIMIT        =		5,
        CONSTRAINT_TYPE_LOCLIMIT		=		6,
        CONSTRAINT_TYPE_RIGIDBODYJOINT	=		17
    };
    enum ConstraintLimitRotationType  {
        LIMIT_XROT =							0x01,
        LIMIT_YROT =							0x02,
        LIMIT_ZROT =							0x04
    };
    enum ConstraintLimitLocationType {
        LIMIT_XMIN =							0x01,
        LIMIT_XMAX =							0x02,
        LIMIT_YMIN =							0x04,
        LIMIT_YMAX =							0x08,
        LIMIT_ZMIN =							0x10,
        LIMIT_ZMAX =							0x20
    };
    static void Display(const Blender::bConstraint* c) {
        if (!c) return;
        if (c->type==(int)CONSTRAINT_TYPE_ROTLIMIT && c->data) {
            const Blender::bRotLimitConstraint* rlc = (const Blender::bRotLimitConstraint*) c->data;
            fbtPrintf("bRotationLimitConstraint: ");
            if (strlen(c->name)>0) fbtPrintf("'%s' ",c->name);
            fbtPrintf("\tmin:{%1.4f,%1.4f,%1.4f}\t\tmax:{%1.4f,%1.4f,%1.4f}\t",glm::degrees(rlc->xmin),glm::degrees(rlc->ymin),glm::degrees(rlc->zmin),glm::degrees(rlc->xmax),glm::degrees(rlc->ymax),glm::degrees(rlc->zmax));
            if (rlc->flag&LIMIT_XROT) fbtPrintf("LIM_XROT ");
            if (rlc->flag&LIMIT_YROT) fbtPrintf("LIM_YROT ");
            if (rlc->flag&LIMIT_ZROT) fbtPrintf("LIM_ZROT ");
            if (rlc->flag>7) fbtPrintf("flag: %d ",rlc->flag);
            /*
            Flag2:
            0 nothing
            2 For transform
            */
            fbtPrintf("flag2: %d\n",rlc->flag2);
        }
        else {
            if (strlen(c->name)>0) fbtPrintf("bConstraint:'%s' flags:%d type:%d\n",c->name,c->flag,c->type);
            else fbtPrintf("bConstraint: flags:%d type:%d\n",c->flag,c->type);
        }
        if (c->ipo) Display(c->ipo);
    }
    static void Display(const Blender::bConstraintChannel* c) {
        if (!c) return;
        if (strlen(c->name)>0) fbtPrintf("bConstraintChannel:'%s' flags:%d\n",c->name,c->flag);
        else fbtPrintf("bConstraintChannel: flags:%d\n",c->flag);
        if (c->ipo) Display(c->ipo);
    }
    static void Display(const Blender::bConstraintTarget* c) {
        if (!c) return;
        if (strlen(c->subtarget)>0) fbtPrintf("bConstraintTarget:'%s' flags:%d type:%d\n",c->subtarget,c->flag,c->type);
        else fbtPrintf("bConstraintTarget: flags:%d type:%d\n",c->flag,c->type);
    }
    static void Display(const Blender::NlaStrip* s)
    {
        if (!s) return;
        if (strlen(s->name)>0) fbtPrintf("NlaStrip:'%s' flags:%d type:%d\n",s->name,s->flag,s->type);
        else fbtPrintf("NlaStrip: flags:%d type:%d\n",s->flag,s->type);


    }
    static void Display(const Blender::Scene* s) {
        if (!s) return;
        if (strlen(s->id.name)>0) fbtPrintf("Scene:'%s' flags:%d type:%d\n",s->id.name,s->flag);
        fbtPrintf("Scene-Renderer: flag:%d framapto:%d\n framelen:%1.4f frame_step:%d freqplay:%d frs_sec:%d frs_sec_base:%1.4f\n",
        s->r.flag,s->r.framapto,s->r.framelen,s->r.frame_step,
        s->r.freqplay,s->r.frs_sec,s->r.frs_sec_base);
        if (s->clip) fbtPrintf("s->clip present\n");
        if (s->fps_info) fbtPrintf("s->fps_info present\n");
    }
    static void Display(const Blender::bProperty* p) {
        if (!p) return;
        fbtPrintf("bProperty: ");
        if (strlen(p->name)>0) fbtPrintf("'%s'",p->name);
        fbtPrintf(" flag:%d data:%d type%d\n",p->flag,p->data,p->type);
    }
    /*static void Display(const Blender::bNode* p) {
        if (!p) return;
        fbtPrintf("bNode: ");
        if (p->name && strlen(p->name)>0) fbtPrintf("'%s'",p->name);
        fbtPrintf(" flag:%d data:%d type%d\n",p->flag,p->data,p->type);
    }*/
    static void Display(const Blender::bPose* p) {
        if (!p) return;
        printf("bPose ctime: %1.4f\n",p->ctime);
        CycledCall<const Blender::bPoseChannel>(p->chanbase,DisplayPoseChannelBriefly);
    }
    /*static void Display(const Blender::Key* sk) {
        if (!sk) return;
        printf("ShapeKey: '%s' ctime:%1.4f\n",sk->id.name,sk->ctime);
        CycledCall<const Blender::KeyBlock>(sk->block,Display);
    }*/
    static void Display(const Blender::KeyBlock* b) {
        if (!b) return;
        // b->type == KEY_RELATIVE
        printf("ShapeKeyBlock: '%s' b->type:%d relative:%d min:%1.4f max:%1.4f\n",b->name,b->type,b->relative,b->slidermin,b->slidermax);
    }
    /*
#define KEY_NORMAL                               0
#define KEY_RELATIVE                             1
void akMeshLoaderUtils_getShapeKeys(Blender::Mesh* bmesh, utArray<utString>& shapes)
{
    Blender::Key* bk = bmesh->key;
    if(bk)
    {
        Blender::KeyBlock* bkb = (Blender::KeyBlock*)bk->block.first;

        // skip first shape key (basis)
        if(bkb) bkb = bkb->next;
        while(bkb)
        {
            if(bkb->type == KEY_RELATIVE)
            {
                Blender::KeyBlock* basis = (Blender::KeyBlock*)bk->block.first;
                for(int i=0; basis && i<bkb->relative; i++)
                    basis = basis->next;

                if(basis)
                    shapes.push_back(bkb->name);
            }
            bkb = bkb->next;
        }
    }
}

void akMeshLoaderUtils_getShapeKeysNormals(Blender::Mesh* bmesh, UTuint32 numshape, utArray<btAlignedObjectArray<akVector3> >& shapenormals)
{
    Blender::Key* bk = bmesh->key;
    if(bk)
    {

        shapenormals.resize(numshape);
        Blender::KeyBlock* bkb = (Blender::KeyBlock*)bk->block.first;

        // skip first shape key (basis)
        UTuint32 shape=0;
        if(bkb) bkb = bkb->next;
        while(bkb)
        {
            if(bkb->type == KEY_RELATIVE)
            {
                Blender::KeyBlock* basis = (Blender::KeyBlock*)bk->block.first;
                for(int i=0; basis && i<bkb->relative; i++)
                    basis = basis->next;

                if(basis)
                {
                    float* pos = (float*)bkb->data;
                    shapenormals[shape].resize(bmesh->totface);
                    for(int i=0; i<bmesh->totface; i++)
                    {
                        const Blender::MFace& bface = bmesh->mface[i];
                        akVector3 normal = akMeshLoaderUtils_calcMorphNormal(bface, pos);
                        shapenormals[shape][i]=normal;
                    }
                    shape++;
                }
            }
            bkb = bkb->next;
        }
    }
}
    */
    //-------------------------------------------------------------------



    static void DisplayDeformGroups(const Blender::Mesh* me) {
            if (me && me->dvert)	{
                /*
                struct MDeformWeight	{
                    int def_nr;
                    float weight;
                };
                struct MDeformVert	{
                    MDeformWeight *dw;
                    int totweight;
                    int flag;
                };
                */
                fbtPrintf("\tMDEFORMVERT (Vertex Weights for each bone vertex group):\n");
                for (int i = 0;i<me->totvert;i++)	{
                    const Blender::MDeformVert& d = me->dvert[i];
                    fbtPrintf("\t(%1d) ",i);
                    if (d.dw &&  d.totweight>0) {
                        for (int j = 0;j<d.totweight;j++)	{
                            const Blender::MDeformWeight& dw = d.dw[j];
                            fbtPrintf(" w[%1d]=%1.4f", dw.def_nr,dw.weight);
                        }
                    }
                    fbtPrintf("\ttotweight=%1d", d.totweight);
                    if (d.flag!=0) fbtPrintf("\tflag=%1d", d.flag);
                    fbtPrintf("\t");
                }
                fbtPrintf("\n\tEND MDEFORMVERT\n");

            }
    }    
    inline static void FillDeformGroups(const Blender::MDeformVert& d,BoneIdsType& idsOut,BoneWeightsType& weightsOut,std::map<int,int>* pDeformGroupBoneIdMap,int maxNumBonesPerVertex=4) {
        idsOut[0]=idsOut[1]=idsOut[2]=idsOut[3]=0;
        weightsOut[0]=weightsOut[1]=weightsOut[2]=weightsOut[3]=0;
        static std::vector<DeformVertStruct> dvs;
        static std::map<int,int>::const_iterator it;
        if (d.dw &&  d.totweight>0) {
            dvs.resize(d.totweight);
            for (int j = 0;j<d.totweight;j++)	{
                const Blender::MDeformWeight& dw = d.dw[j];
                DeformVertStruct& ds = dvs[j];
                if (pDeformGroupBoneIdMap) {
                    if ((it = pDeformGroupBoneIdMap->find(dw.def_nr))!=pDeformGroupBoneIdMap->end()) ds.id = it->second;
                    else {
                        fprintf(stderr,"Error: Deform Group Index: %d not found in pDeformGroupBoneIdMap\n",dw.def_nr);
                        ds.id = -1;
                    }
                }
                else ds.id = dw.def_nr;
                ds.w = dw.weight;
            }
            std::sort(dvs.begin(),dvs.end(),DeformVertStructCmp());
            maxNumBonesPerVertex = maxNumBonesPerVertex>4 ? 4 : maxNumBonesPerVertex;
            dvs.resize(maxNumBonesPerVertex);float totWeight=0;
            for (int i=0;i<maxNumBonesPerVertex;i++) {
                const DeformVertStruct& ds = dvs[i];
                idsOut[i] = ds.id;
                weightsOut[i] = ds.w;
                totWeight+=ds.w;
            }
            if (totWeight>0) {
                for (int i=0;i<maxNumBonesPerVertex;i++) {
                    weightsOut[i] /= totWeight;
                }
            }
        }
        //if (d.flag!=0) fbtPrintf("\tflag=%1d", d.flag);
    }
    // Checks parentBone too:
    static const Blender::Bone* FindBoneFromNameInsideDescendants(const Blender::Bone* parentBone,const char* boneName) {
        if (strcmp(parentBone->name,boneName)==0) return parentBone;
        parentBone = (const Blender::Bone*) parentBone->childbase.first;
        const Blender::Bone* rv = NULL;
        while (parentBone) {
            rv = FindBoneFromNameInsideDescendants(parentBone,boneName);
            if (rv) return rv;
            parentBone=parentBone->next;
        }
        return rv;
    }
    static const Blender::Bone* FindBoneFromName(const Blender::bArmature* armature,const char* boneName) {
        const Blender::Bone* rv = NULL;
        if (!armature || ! boneName || strlen(boneName)==0) return rv;
            Blender::ListBase rootBones = armature->bonebase;   // These seems to be the root bones only...
            const Blender::Bone* b = (const Blender::Bone*) rootBones.first;
            while (b) {
                rv = FindBoneFromNameInsideDescendants(b,boneName);
                if (rv) return rv;
                b=b->next;
            }
        return rv;
    }


    inline static vec3 ToVector3(const float v[3],bool invertYZ=false) {
        return (invertYZ ? GetInvYZMatrix3()*vec3(v[0],v[1],v[2]) : vec3(v[0],v[1],v[2]));
    }
    inline static void ToMatrix3(mat3& mOut,const float m[3][3],bool invertYZ=false) {
        for (int i=0;i<3;i++) for (int j=0;j<3;j++) mOut[i][j] = (float) m[i][j];
        if (invertYZ) mOut = GetInvYZMatrix3()* mOut;
    }
    inline static void ToMatrix4(mat4& mOut,const float m[4][4],bool invertYZ=false) {
        if (invertYZ) {
            mat3 tmp;
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) tmp[i][j] = (float) m[i][j];
            tmp = GetInvYZMatrix3()*tmp;
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) mOut[i][j] = tmp[i][j];

            vec3 orig = GetInvYZMatrix3()*vec3(m[3][0],m[3][1],m[3][2]);
            for (int i=0;i<4;i++) mOut[i][3] = 0;//(btScalar) m[i][3];
            //for (int j=0;j<4;j++) mOut[3][j] = 0;//(btScalar) m[3][j];
            for (int j=0;j<3;j++) mOut[3][j] = orig[j];
            mOut[3][3] = 1;
        }
        else for (int i=0;i<4;i++) for (int j=0;j<4;j++) mOut[i][j] = (float) m[i][j];
    }
    inline static void ToMatrix4(mat4& mOut,const float m[3][3],const vec3& translation,bool invertYZ=false) {
        if (invertYZ) {
            mat4 tmp(1);
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) tmp[i][j] = (float) m[i][j];
            tmp = GetInvYZMatrix4()*tmp;
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) mOut[i][j] = tmp[i][j];

            vec3 orig = GetInvYZMatrix3()*translation;
            for (int i=0;i<3;i++) mOut[i][3] = 0;//orig[i];
            //for (int j=0;j<4;j++) mOut[3][j] = 0;//(btScalar) m[3][j];
            for (int j=0;j<3;j++) mOut[3][j] = orig[j];
            mOut[3][3] = 1;
        }
        else {
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) mOut[i][j] = (float) m[i][j];
            for (int i=0;i<3;i++) mOut[i][3] = 0;//translation[i];
            //for (int j=0;j<4;j++) mOut[3][j] = 0;//(btScalar) m[3][j];
            for (int j=0;j<3;j++) mOut[3][j] = translation[j];
            mOut[3][3] = 1;
        }
    }
    inline static const quat& GetInvYZQuat() {
#ifdef KEEP_Z_FORWARD
        static quat q1;
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            glm::fromAngleAxis(q1,-glm::HALF_PI<float>(),vec3(1,0,0));
        }
        return q1;
#undef KEEP_Z_FORWARD
#else //KEEP_Z_FORWARD
        static quat q3;
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            quat q1,q2;
            glm::fromAngleAxis(q1,(float)M_PI,vec3(0,1,0));
            glm::fromAngleAxis(q2,(float)M_HALF_PI,vec3(1,0,0));
            q3 = q1* q2;
        }
        return q3;
#endif //KEEP_Z_FORWARD
    }
    inline static const mat3& GetInvYZMatrix3() {
        static mat3 mInvYZ;
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            mInvYZ = glm::mat3_cast(GetInvYZQuat());
        }
        return mInvYZ;
    }
    inline static const mat4& GetInvYZMatrix4() {
        static mat4 mInvYZ;
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            mInvYZ = glm::mat4_cast(GetInvYZQuat());
        }
        return mInvYZ;
    }
    inline static void GetSubMat3(mat3& mOut,const float m[4][4],bool invertYZ=false) {
        if (invertYZ) {
            mat3 tmp(1);
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) tmp[i][j] = (float) m[i][j];
            tmp = GetInvYZMatrix3()*tmp;
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) mOut[i][j] = tmp[i][j];
        }
        else {
            for (int i=0;i<3;i++) for (int j=0;j<3;j++) mOut[i][j] = (float) m[i][j];
        }
    }

    inline static bool IsEqual(float a,float b) {
        static const float eps = 0.00001;
        return (a>b) ? a-b<eps : b-a<eps;
    }
    inline static bool IsEqual(const Vector2& a,const Vector2& b) {
        return IsEqual(a[0],b[0]) && IsEqual(a[1],b[1]);
    }
    inline static int AddTexCoord(int id,const Vector2& tc,Mesh::Part& mesh,std::vector<int>& numTexCoordAssignments,std::multimap<int,int>& texCoordsSingleVertsVertsMultiMap)
    {
        std::vector < Vector2 >& texCoords = mesh.texcoords;

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
            mesh.boneIds.push_back(mesh.boneIds[id]);
            mesh.boneWeights.push_back(mesh.boneWeights[id]);

            texCoordsSingleVertsVertsMultiMap.insert(std::make_pair(id, id2));
            //printf("New! %d = %d\n",id,id2);
            return id2;
        }
        //printf("OK! %d = %d\n",id,id);
        return id;
    }

    template<typename ObjectType> static void CycledCall(fbtList& list,void (*fnc)(ObjectType*)) {
        for (ObjectType* ob = (ObjectType*)list.first; ob; ob = (ObjectType*)ob->id.next)	{
            if (!ob) break;
            fnc(ob);
        }
    }
    template<typename ObjectType> static void CycledCall(const fbtList& list,void (*fnc)(const ObjectType*)) {
        for (const ObjectType* ob = (const ObjectType*)list.first; ob; ob = (const ObjectType*)ob->id.next)	{
            if (!ob) break;
            fnc(ob);
        }
    }
    template<typename ObjectType> static void CycledCall(Blender::ListBase& list,void (*fnc)(ObjectType*)) {
        for (ObjectType* ob = (ObjectType*)list.first; ob; ob = (ObjectType*)ob->next)	{
            if (!ob) break;
            fnc(ob);
        }
    }
    template<typename ObjectType> static void CycledCall(const Blender::ListBase& list,void (*fnc)(const ObjectType*)) {
        for (const ObjectType* ob = (const ObjectType*)list.first; ob; ob = (const ObjectType*)ob->next)	{
            if (!ob) break;
            fnc(ob);
        }
    }

    protected:
    BlenderHelper() {}
    void operator=(const BlenderHelper&) {}
    struct DeformVertStruct {
        int id;
        float w;
        DeformVertStruct(int _id=0,float _w=0) : id(_id),w(_w) {}
    };
    struct DeformVertStructCmp {
        inline bool operator()(const DeformVertStruct& a,const DeformVertStruct& b) const {
            return a.w>b.w;
        }
    };
};


class MeshArmatureSetter {
protected:
inline static bool Exists(const string& name,std::vector<string>& v,bool addIfNonExistent) {
    const std::vector<string>::const_iterator it = std::find(v.begin(),v.end(),name);
    const bool exists =  (it!=v.end());
    if (addIfNonExistent && !exists) v.push_back(name);
    return exists;
}
inline static int GetNumChildBones(const Blender::Bone* b) {
    int rv=0;
    if (!b) return rv;
    b = (const Blender::Bone*) b->childbase.first;
    while (b) {
        ++rv;
        b=b->next;
    }
    return rv;
}
inline static bool IsGoodBone(const Blender::Bone* b) {
    return b && (b->parent || GetNumChildBones(b)>0);
}
public:


static void _GetNumBonesFromTheFirstRootBone(const Blender::Bone* b,int& numBonesOut,std::vector<string>* palreadyAddedBones=NULL) {
        if (!b) return;
        if (palreadyAddedBones && Exists(string(b->name),*palreadyAddedBones,true)) return;
        if (IsGoodBone(b)) ++numBonesOut;
        if (!b->parent) {
            // add siblings
            const Blender::Bone* s = b->next;
            while (s)	{
                _GetNumBonesFromTheFirstRootBone(s,numBonesOut,palreadyAddedBones);
                s = s->next;
            }
        }
        // add children
        const Blender::Bone* c = (const Blender::Bone*) b->childbase.first;
        while (c)	{
            _GetNumBonesFromTheFirstRootBone(c,numBonesOut,palreadyAddedBones);
            c = c->next;
        }
}

inline static void Display(const glm::mat4& m,const std::string& indent="\t") {
    for (int i=0;i<4;i++)   {
        printf("%s[\t",indent.c_str());
        for (int j=0;j<4;j++)   {
            printf("%1.4f\t",m[i][j]);
        }
        printf("]\n");
    }
}

inline static bool IsDummyBone(const Blender::Bone* b)  {
    //return false;   // We DON'T support Dummy Bones with .blend files...
    //return (b && b->flag == 12303);  // TODO: check better ( my dummy 'root' flag:12303 Other non-dummy values: 8208,8192,8320,8336 ))
    const string name = String::ToLower(string(b->name));
    //if (!name.find("def")==string::npos) return true;   // to remove
    if (name.find(".ik.")!=string::npos) return true;
    if (name.find(".link.")!=string::npos) return true;
    if (name.find(".rev.")!=string::npos) return true;
    return false;
}

// Important: ar must be resized to blank 'numBones' elements before the call
static void _AddAllBonesFromTheFirstRootBone(std::vector<Mesh::BoneInfo>& ar,const Blender::Bone* b,std::map<const Blender::Bone*,int>* pInternalBoneMap,unsigned& numNextValidBoneInsideBoneInfos,unsigned& numDummyBones,bool invertYZ=false,std::vector<string>* palreadyAddedBones=NULL) {
        if (!b) return;
        if (palreadyAddedBones && Exists(string(b->name),*palreadyAddedBones,true)) return;
        if (IsGoodBone(b))     {
        const bool isDummyBone = IsDummyBone(b);

        #define USE_SAFETY_CHECKS
        #ifdef USE_SAFETY_CHECKS
        if (ar.size()==0) {
            fprintf(stderr,"Error: _AddAllBonesFromTheFirstRootBone(...) in 'meshBlender.cpp' must be called with ar.size()>0.\n");
            return;
        }
        if (isDummyBone) {
            if (ar.size()-1 < numDummyBones) {
            fprintf(stderr,"Error: _AddAllBonesFromTheFirstRootBone(...) in 'meshBlender.cpp' has: ar.size()-1(=%d) < numDummyBones(%d) for bone: '%s'.\n",(int)ar.size()-1,(int)numDummyBones,b->name);
            return;
            }
        }
        else if (ar.size()<=numNextValidBoneInsideBoneInfos) {
            fprintf(stderr,"Error: _AddAllBonesFromTheFirstRootBone(...) in 'meshBlender.cpp' has: ar.size()(=%d) <= numNextValidBoneInsideBoneInfos(%d) for bone: '%s'.\n",(int)ar.size(),(int)numNextValidBoneInsideBoneInfos,b->name);
            return;
        }
        #undef USE_SAFETY_CHECKS
        #endif //USE_SAFETY_CHECKS

        const unsigned boneIndexToFillInsideAr = isDummyBone ? (ar.size() - (++numDummyBones)) : (numNextValidBoneInsideBoneInfos++);//ar.size();
        //ar.push_back(Mesh::BoneInfo());
        if (pInternalBoneMap) (*pInternalBoneMap)[b] = boneIndexToFillInsideAr;

        Mesh::BoneInfo& bone = ar[boneIndexToFillInsideAr];
        bone.index = bone.indexMirror = boneIndexToFillInsideAr;
        if (strlen(b->name)>0) bone.boneName = string(b->name);
        //else bone.boneName="";
        bone.eulerRotationMode = glm::EULER_XYZ;//0;//glm::EULER_YZX;//b->layer;//b->flag;       // It will be overwritten later (if this info is available), and this variable won't break quaternion-based keyframes: so I think I can set it here to the most common Euler mode (or not?)
        //printf("boneName: %s        bone.eulerRotationMode: %d\n",bone.boneName.c_str(),/*bone.eulerRotationMode,*/b->flag);
        bone.isDummyBone = isDummyBone;
        bone.isUseless = false;         // we don't use it for Blend files (or maybe we'll set these later)

        //if (bone.isDummyBone)  bone.boneOffsetInverse = bone.boneOffset = glm::mat4(1);    // Not used in dummy bones
        //else
        {
            BlenderHelper::ToMatrix4(bone.boneOffsetInverse,b->arm_mat,invertYZ);
            bone.boneOffset = glm::inverse(bone.boneOffsetInverse);
        }

        bone.preTransform = glm::mat4(1);
        bone.preTransformIsPresent = false;
        //bone.preAnimationTransform = glm::mat4(1);                                                              // [*] Used only when animation is present
        //bone.preAnimationTransformIsPresent = false;

        /*
        bone->arm_mat is (bonemat(b)+head(b))*arm_mat(b-1), so it is in object_space.

        bone_mat is the rotation derived from head/tail/roll
        pose_mat(b) = pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b) , therefore pose_mat is object space.
        */
        /*
        # parent head to tail, and parent tail to child head
        bone_translation = Matrix.Translation(Vector((0, bone.parent.length, 0)) + bone.head)
        # parent armature space matrix, translation, child bone rotation
        bone.matrix_local == bone.parent.matrix_local * bone_translation * bone.matrix.to_4x4()
        */

        //== THIS SEEMS TO WORK =================================================================
        // For bones without any animation keyframe (or using manual animation)
        glm::vec3 boneTranslation(0,0,0);
        if (b->parent) {
            //if (!IsDummyBone(b->parent))
                 boneTranslation[1]+=b->parent->length;
        }
        boneTranslation += BlenderHelper::ToVector3(b->head);

        BlenderHelper::ToMatrix4(bone.transform,b->bone_mat,boneTranslation,invertYZ);

        // Now for animations:
        BlenderHelper::ToMatrix4(bone.preAnimationTransform,b->bone_mat,boneTranslation,invertYZ);
        bone.preAnimationTransformIsPresent = true;
        //=======================================================================================


        bone.multPreTransformPreAnimationTransform = bone.preTransform * bone.preAnimationTransform;
        bone.postAnimationTransform = glm::mat4(1);                                                             // [*] Used only when animation is present (after translation key)
        bone.postAnimationTransformIsPresent = false;

        // For reference: all the data in the Blender::Bone struct--------------
        /*
        bone.roll = b->roll;
        bone.head = BlenderHelper::ToVector3(b->head,invertYZ);            // (*)
        bone.tail = BlenderHelper::ToVector3(b->tail,invertYZ);            // (*)
        BlenderHelper::ToBtMatrix3x3(bone.bone_mat,b->bone_mat,invertYZ);    // (*)
        bone.flag = b->flag;
        bone.arm_roll = b->arm_roll;
        bone.arm_head = BlenderHelper::ToVector3(b->arm_head,invertYZ);    // (*)
        bone.arm_tail = BlenderHelper::ToVector3(b->arm_tail,invertYZ);    // (*)
        BlenderHelper::ToBtMatrix4x4(bone.arm_mat,b->arm_mat,invertYZ);      // (*)
        bone.dist = (btScalar) b->dist;
        bone.weight = (btScalar) b->weight;
        bone.xwidth = (btScalar) b->xwidth;
        bone.length = (btScalar) b->length;
        bone.zwidth = (btScalar) b->zwidth;
        bone.ease1 = (btScalar) b->ease1;
        bone.ease2 = (btScalar) b->ease2;
        bone.rad_head = (btScalar) b->rad_head;
        bone.rad_tail = (btScalar) b->rad_tail;
        BlenderHelper::ToVector3(b->size,invertYZ);                        // (*)
        bone.layer = b->layer;
        bone.segments = b->segments;
        */
        //BlenderHelper::Display(b);
        //------------------------------------------------------------------------
        }
        if (!b->parent) {
            // add siblings
            const Blender::Bone* s = b->next;
            while (s)	{
                _AddAllBonesFromTheFirstRootBone(ar,s,pInternalBoneMap,numNextValidBoneInsideBoneInfos,numDummyBones,invertYZ,palreadyAddedBones);
                s = s->next;
            }
        }
        const Blender::Bone* c = (const Blender::Bone*) b->childbase.first;
        while (c)	{
            _AddAllBonesFromTheFirstRootBone(ar,c,pInternalBoneMap,numNextValidBoneInsideBoneInfos,numDummyBones,invertYZ,palreadyAddedBones);
            c = c->next;
        }
}


static void LoadArmature(Mesh& mesh,const Blender::Object* armatureObject,bool invertYZ=false)   {
    const Blender::bArmature* a = BlenderHelper::GetArmature(armatureObject);
    if (!a) return;
    std::map<const Blender::Bone*,int> internalBoneMap;
    std::vector<Mesh::BoneInfo>& ar = mesh.m_boneInfos;

    //load bones here:-------------------------------------------------
    const Blender::ListBase rootBones = a->bonebase;   // These seems to be the root bones only...
    const Blender::Bone* b = (const Blender::Bone*) rootBones.first;
    std::vector<string> alreadyAddedBones;
    int numBones=0;_GetNumBonesFromTheFirstRootBone(b,numBones,&alreadyAddedBones);ar.resize(numBones);alreadyAddedBones.clear();//printf("Resized %d bones.\n",numBones);
    unsigned numDummyBones = 0;
    // After this call dummy bones are all at the end, and 'mesh.numValidBones' and 'numDummyBones' should be set (but parent/child relations are still missing)
    _AddAllBonesFromTheFirstRootBone(ar,b,&internalBoneMap,mesh.numValidBones,numDummyBones,invertYZ,&alreadyAddedBones);
    //printf("Filled %d bones. numValidBones=%d numDummyBones=%d\n",(int)ar.size(),(int) mesh.numValidBones,(int) numDummyBones);

    // Fill mesh.m_boneIndexMap now:
    mesh.m_boneIndexMap.clear();
    for (unsigned i=0,sz=mesh.m_boneInfos.size();i<sz;i++) {
        const Mesh::BoneInfo& bone = mesh.m_boneInfos[i];
        //if (i < mesh.numValidBones)   // Nope: better leave the full map
            mesh.m_boneIndexMap[bone.boneName] = bone.index;
        //printf("boneInfo[%d].boneName='%s'\n",bone.index,bone.boneName.c_str());
    }

    // Second Pass to fill the remaining data (parent/child/mirror fields)-

    // For mirrorBoneId:
    static std::string sym1[] = {".L",".Left",".left","_L","_Left","_left","-L","-Left","-left"         ,"Left","left"      // The last 2 could be commented out...
                                };
    static std::string sym2[] = {".R",".Right",".right","_R","_Right","_right","-R","-Right","-right"   ,"Right","right"    // The last 2 could be commented out...
                                };
    static const int symSize = sizeof(sym1)/sizeof(sym1[0]);
    if (sizeof(sym1)/sizeof(sym1[0])!=sizeof(sym2)/sizeof(sym2[0])) fprintf(stderr,"Error in LoadArmature(...): sizeof(sym1)/sizeof(sym1[0])!=sizeof(sym2)/sizeof(sym2[0])");

    for (std::map<const Blender::Bone*,int>::const_iterator it = internalBoneMap.begin();it!=internalBoneMap.end();++it) {
        b = it->first;
        Mesh::BoneInfo& bone = ar[it->second];
        //---------------------
        if (b->parent) {
            bone.parentBoneInfo = (Mesh::BoneInfo*) mesh.getBoneInfoFromName(string(b->parent->name));
            /*
            if (bone.parentBoneInfo)   {
                // This might happen if it's a dummy bone
                // TODO: however I should probably multiply some matrices in between...
                const Blender::Bone* bb = b->parent;
                while ((bb=bb->parent)!=NULL)   {
                    bone.parentBoneInfo = (Mesh::BoneInfo*) mesh.getBoneInfoFromName(string(bb->name));
                    if (bone.parentBoneInfo) break;
                }
            }
            */
        }
        else bone.parentBoneInfo = NULL;
        if (bone.parentBoneInfo==NULL) {
            mesh.m_rootBoneInfos.push_back(&bone);
            //printf("root bone.name=%s\n",bone.boneName.c_str());
        }


        //printf("bone.name=%s b->name=%s parentId=%d\n",bone.name.c_str(),b->name,bone.parentBoneId);
        const Blender::Bone* c = (const Blender::Bone*) b->childbase.first;
        while (c) {
            //printf("c->name=%s\n",c->name);
            Mesh::BoneInfo* childBoneInfo = (Mesh::BoneInfo*) mesh.getBoneInfoFromName(string(c->name));
            if (!childBoneInfo) {
                fprintf(stderr,"Error: Wrong bone child id of bone %s: -> %s\n",bone.boneName.c_str(),c->name);
                // TODO: This might happen if it's a dummy bone
                // TODO: however I should probably multiply some matrices in between...
            }
            else bone.childBoneInfos.push_back(childBoneInfo);
            //---------------------------------
            c=c->next;
        }
        // Mirror boneId:----------------------
        const std::string& name = bone.boneName;
        std::string symName = "";
        for (int j=0;j<symSize;j++) {
            if (String::EndsWith(name,sym1[j])) {
                symName = name.substr(0,name.size()-sym1[j].size())+sym2[j];
                break;
            }
            if (String::EndsWith(name,sym2[j])) {
                symName = name.substr(0,name.size()-sym2[j].size())+sym1[j];
                break;
            }
        }
        if (symName.size()>0) {
            const Mesh::BoneInfo* morrorBoneInfo = mesh.getBoneInfoFromName(string(symName.c_str()));
            if (morrorBoneInfo) bone.indexMirror = morrorBoneInfo->index;
            else fprintf(stderr,"Error: mirror of bone %s should be: %s, but I can't find a bone named like that.\n",name.c_str(),symName.c_str());
        }
        //-----------------------------------------------------------
    }

    //#define SKIP_THIS_PART  // This marks some 'isDummy' bone to 'isUseless', but needs child relations
    #ifndef SKIP_THIS_PART
    // Now we see if we can remove dummy bones:---- OPTIONAL STEP (comment it out in case of problems) ----------------------------------------
    bool m_boneInfosModified = false;
    if (mesh.m_boneInfos.size()>mesh.numValidBones)	{
        for (unsigned i = mesh.numValidBones;i<mesh.m_boneInfos.size();i++)	{
            Mesh::BoneInfo& bi = mesh.m_boneInfos[i];
            if (bi.isDummyBone)	// && (!bi.parentBoneInfo || !bi.parentBoneInfo->isDummyBone))
            {
                if (Mesh::AllDescendantsAreDummy(bi))	{
                    bi.isUseless = true;
                    continue;
                }
            }
        }
    }
    //------------------------------------------------------------------------------------------------------------------------------------------
    #else //SKIP_THIS_PART
    #undef SKIP_THIS_PART
    #endif //SKIP_THIS_PART

    #define PERFORM_SAFETY_CHECK
    #ifdef 	PERFORM_SAFETY_CHECK
    {
        mesh.m_boneIndexMap.clear();
        for (unsigned i=0,sz=mesh.m_boneInfos.size();i<sz;i++)	{
            Mesh::BoneInfo& bi = mesh.m_boneInfos[i];
            if (bi.index!=i)	{
                printf("Safety check: m_boneInfos[%d].index = %d != %d ->corrected ('%s').\n",i,bi.index,i,bi.boneName.c_str());
                bi.index = i;
            }
            if (i<mesh.numValidBones)
            {
                MapStringUnsigned::const_iterator it = mesh.m_boneIndexMap.find(bi.boneName);
                if (it!=mesh.m_boneIndexMap.end()) {
                    printf("Safety check ERROR: m_boneInfos[%d].boneName = m_boneInfos[%d].boneName = '%s'\n",i,it->second,bi.boneName.c_str());
                    //TODO: replace bi.boneName here to a unique name
                }
                mesh.m_boneIndexMap[bi.boneName] = i;
            }
        }
    }
    #undef PERFORM_SAFETY_CHECK
    #endif //PERFORM_SAFETY_CHECK

    // Now we fill the m_boneInfos::eulerRotationMode-------------
    const Blender::bPose* pose = armatureObject->pose;
    if (pose)   {
        MapStringUnsigned::const_iterator it;
        const Blender::bPoseChannel* pc = (const Blender::bPoseChannel*) pose->chanbase.first;
        while (pc) {
            if (strlen(pc->name)>0) {
                const std::string name = pc->name;
                if ((it = mesh.m_boneIndexMap.find(name))!=mesh.m_boneIndexMap.end())    {
                    Mesh::BoneInfo& bone = mesh.m_boneInfos[it->second];
                    bone.eulerRotationMode = (int)pc->rotmode;
                    //printf("set m_boneInfos[%s] = %d\n",bone.boneName.c_str(),bone.eulerRotationMode);

                    //#define TEST_BONE_CONSTRAINTS
                    #ifdef TEST_BONE_CONSTRAINTS
                    fprintf(stderr,"m_boneInfos[%s] = %d\n",bone.boneName.c_str(),bone.eulerRotationMode);
                    BlenderHelper::CycledCall<Blender::bConstraint>(pc->constraints,&BlenderHelper::Display);
                    #undef TEST_BONE_CONSTRAINTS
                    #endif //TEST_BONE_CONSTRAINTS

                }
            }



            //------------
            pc = pc->next;
        }
    }
    //------------------------------------------------------------

    // TODO: test armatureObject->poselib-------------------------
    /*
    const Blender::bAction* poselib = armatureObject->poselib;
    if (poselib)   {
        BlenderHelper::Display(poselib);    // Actually poseLib is stored as a simple action!
    }
    */
    //------------------------------------------------------------


    // TESTING STUFF----------------------------------------------
    /* // CRASHES!
    const Blender::Object* o = (const Blender::Object*) a;  // Is this legal ?
    if (o->constraints.first) {
        fbtPrintf("\tarmature->constraints present\n");
        const Blender::bConstraintTarget* c = (const Blender::bConstraintTarget*) o->constraints.first;
        while (c) {
            BlenderHelper::Display(c);
            c=c->next;
        }
    }
    if (o->nlastrips.first) {
        fbtPrintf("\tarmature->nlastrips present\n");
        const Blender::NlaStrip* s = (const Blender::NlaStrip*) o->nlastrips.first;
        while (s) {
            BlenderHelper::Display(s);
            s=s->next;
        }
    }
    */
    //------------------------------------------------------------

}

inline static void GetTimeAndValue(const Blender::BezTriple& bt,float& time,float& value) {
     /* vec in BezTriple looks like this: vec[?][0] => frame of the key, vec[?][1] => actual value, vec[?][2] == 0
                   vec[0][0]=x location of handle 1									// This should be the actual value
                   vec[0][1]=y location of handle 1									// Unknown
                   vec[0][2]=z location of handle 1 (not used for FCurve Points(2d))	// This is zero
                   vec[1][0]=x location of control point								// Number of frame of the key
                   vec[1][1]=y location of control point								// Unknown
                   vec[1][2]=z location of control point								// This is zero
                   vec[2][0]=x location of handle 2									// Unknown
                   vec[2][1]=y location of handle 2									// Unknown
                   vec[2][2]=z location of handle 2 (not used for FCurve Points(2d))	// This is zero
                   // typedef struct BezTriple {
                   //	float vec[3][3];
                   //	float alfa, weight, radius;	// alfa: tilt in 3D View, weight: used for softbody goal weight, radius: for bevel tapering
                   //	short ipo;					// ipo: interpolation mode for segment from this BezTriple to the next
                   //	char h1, h2; 				// h1, h2: the handle type of the two handles
                   //	char f1, f2, f3;			// f1, f2, f3: used for selection status
                   //	char hide;					// hide: used to indicate whether BezTriple is hidden (3D), type of keyframe (eBezTriple_KeyframeTypes)
                   // } BezTriple;
                */
               time  =  bt.vec[1][0];   //[2][0] or [3][0]
               value =  bt.vec[2][1];

}

static void LoadAction(Mesh& mesh,const Blender::bAction* act,float frs_sec=24.f) {
    if (!act) return;
    string name = act->id.name;
    if (String::StartsWith(name,"AC")) name = name.substr(2);

    if (name.size()>4 && name.substr(name.size()-4,3)==".00") return;   // Optional (We exclude actions that sometimes blender clones for apparently no reason)

    if (mesh.getAnimationIndex(name)>=0) return;

    const unsigned index = mesh.m_animationInfos.size();

    //const Blender::bPoseChannel* c = (const Blender::bPoseChannel*) act->chanbase.first;
    //BlenderHelper::CycledCall<const Blender::bPoseChannel>(act->chanbase,&BlenderHelper::DisplayPoseChannelBriefly);
    //BlenderHelper::CycledCall<const Blender::bActionChannel>(act->chanbase,&BlenderHelper::Display);
    //BlenderHelper::CycledCall<const Blender::bActionGroup>(act->groups,&BlenderHelper::Display);
    //bPose has chanbase too

    bool animationOk = false;

    std::vector <Mesh::BoneInfo::BoneAnimationInfo::Vector3Key> translationKeys;
    std::vector <Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey> rotationKeys;
    std::vector <Mesh::BoneInfo::BoneAnimationInfo::Vector3Key> scalingKeys;
    float animationTime = 0;    // number of key frames of the action

    Mesh::BoneInfo* bi = NULL;bool newBone = false;glm::EulerRotationMode eulerRotationMode = glm::EULER_XYZ;
    const Blender::FCurve* c = (const Blender::FCurve*) act->curves.first;
    while (c) {
        string rna = c->rna_path;
        //Now rna is something like: pose.bones["pelvis_body"].rotation_quaternion
        if (!String::StartsWith(rna,"pose.bones[\"")) {c=c->next;continue;}
        rna = rna.substr(12);
        size_t beg = rna.find('\"');
        if (beg==string::npos) {c=c->next;continue;}
        const string boneName = rna.substr(0,beg);

        Mesh::BoneInfo* bi2 = (Mesh::BoneInfo*)mesh.getBoneInfoFromName(boneName);
        if (!bi2)    {
            fprintf(stderr,"Error: animation '%s' has a FCurve on a bone named: '%s', that does not exist in the (single) supported armature.\n",name.c_str(),boneName.c_str());
            {c=c->next;continue;}
        }
        if (bi!=bi2) {
            bi = bi2;
            newBone = true;
            translationKeys.clear();rotationKeys.clear();scalingKeys.clear();
            translationKeys.reserve(c->totvert/3);rotationKeys.reserve(c->totvert/4);scalingKeys.reserve(c->totvert/3);
        }
        else newBone = false;

        rna = rna.substr(beg+1);

        beg = rna.find('.');
        if (beg==string::npos) {c=c->next;continue;}

        const string animationTypeString = rna.substr(beg+1);
        const int animationTypeEnum = // 0 = location, 1 = scaling, 2 = rotation_euler, 3 = rotation_axis_angle, 4 = rotation_quaternion, -1 = invalid
        animationTypeString=="rotation_quaternion" ? 4 :
        animationTypeString=="rotation_axis_angle" ? 3 :
        animationTypeString=="rotation_euler" ? 2 :
        animationTypeString=="location" ? 0 :
        (animationTypeString=="scaling" || animationTypeString=="scale") ? 1 :
        -1;
        if (animationTypeEnum<0) {
            c=c->next;
            fprintf(stderr,"Error: can't decode animationTypeString = %s\n",animationTypeString.c_str());
            continue;
        }
        else if (animationTypeEnum==2) {
            eulerRotationMode = (bi->eulerRotationMode>0 && bi->eulerRotationMode<7) ? (glm::EulerRotationMode)bi->eulerRotationMode : glm::EULER_XYZ;
        }

        const int animationTypeIndex = c->array_index;
        const bool isLastComponent = (animationTypeIndex==2 && animationTypeEnum<3) || (animationTypeIndex==3 && animationTypeEnum>=3);
        //const int numKeys = c->totvert; // Warning: it's the number of keys of EVERY single-value component!
        //printf("boneName: '%s' animationTypeString: '%s' animationTypeIndex: '%d' numKeys: %d\n",boneName.c_str(),animationTypeString.c_str(),animationTypeIndex,numKeys);

        //-----------------------------------

        if (c->bezt && c->totvert) {
            float time,value;
            Mesh::BoneInfo::BoneAnimationInfo::Vector3Key* lastTranslationKey=NULL;
            Mesh::BoneInfo::BoneAnimationInfo::Vector3Key* lastScalingKey=NULL;
            Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey* lastQuaternionKey=NULL;

            int tki=0,ski=0,rki=0;


            if (!newBone) {
                if (translationKeys.size()>0) lastTranslationKey = &translationKeys[0];
                if (scalingKeys.size()>0) lastScalingKey = &scalingKeys[0];
                if (rotationKeys.size()>0) lastQuaternionKey = &rotationKeys[0];
            }

            for (int i=0;i<c->totvert;i++)	{
               GetTimeAndValue(c->bezt[i],time,value);
               //printf("\t%s: key[%d] = {%1.2f:  %1.4f};\n",bi->boneName.c_str(),i,time,value);

               switch (animationTypeEnum) {
               case 0: {
                   if (animationTypeIndex==0) {
                       const size_t id = translationKeys.size();
                       translationKeys.push_back(Mesh::BoneInfo::BoneAnimationInfo::Vector3Key());
                       lastTranslationKey = &translationKeys[id];
                       lastTranslationKey->time = time;
                   }
                   if (!lastTranslationKey) {
                       fprintf(stderr,"An error has occurred while parsing an animation ('%s') with a 'loc' key for bone ('%s'): location[0] MUST be the first key in the .blend file.\n",name.c_str(),boneName.c_str());
                       break;
                   }
                   Mesh::BoneInfo::BoneAnimationInfo::Vector3Key& loc = *lastTranslationKey;
                   loc.value[animationTypeIndex] = value;
                   if (isLastComponent) {
                       if (animationTime<loc.time) animationTime=loc.time;
                       Mesh::BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
                       if (!bai) {
                           animationOk = true;
                           bai = &(bi->boneAnimationInfoMap[index] = Mesh::BoneAnimationInfo());
                           bai->reset();
                       }
                       bai->translationKeys.push_back(loc);
                   }
                   if (tki+1 <= (int)translationKeys.size()) lastTranslationKey = &translationKeys[++tki];
               }
               break;
               case 1:  {
                   if (animationTypeIndex==0) {
                       const size_t id = scalingKeys.size();
                       scalingKeys.push_back(Mesh::BoneInfo::BoneAnimationInfo::Vector3Key());
                       lastScalingKey = &scalingKeys[id];
                       lastScalingKey->time = time;
                   }
                   if (!lastScalingKey) {
                       fprintf(stderr,"An error has occurred while parsing an animation ('%s') with a 'sca' key for bone ('%s'): scaling[0] MUST be the first key in the .blend file.\n",name.c_str(),boneName.c_str());
                       break;
                   }
                   Mesh::BoneInfo::BoneAnimationInfo::Vector3Key& sca = *lastScalingKey;
                   sca.value[animationTypeIndex] = value;
                   if (isLastComponent) {
                       if (animationTime<sca.time) animationTime=sca.time;
                       Mesh::BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
                       if (!bai) {
                           animationOk = true;
                           bai = &(bi->boneAnimationInfoMap[index] = Mesh::BoneAnimationInfo());
                           bai->reset();
                       }
                       bai->scalingKeys.push_back(sca);
                   }
                   if (ski+1 <= (int)scalingKeys.size()) lastScalingKey = &scalingKeys[++ski];
               }
               break;
               case 2:
               case 3:  {
                   // rotation_euler (2) or rotation_axis_angle (3) [the latter to be tested]
                   if (animationTypeIndex==0) {
                       const size_t id = rotationKeys.size();
                       rotationKeys.push_back(Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey());
                       lastQuaternionKey = &rotationKeys[id];
                       lastQuaternionKey->time = time;
                   }
                   if (!lastQuaternionKey) {
                       fprintf(stderr,"An error has occurred while parsing an animation ('%s') with a 'rot' key for bone ('%s'): rotation_quaternion[0] MUST be the first key in the .blend file.\n",name.c_str(),boneName.c_str());
                       break;
                   }
                   Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey& qua = *lastQuaternionKey;
                   qua.value[animationTypeIndex] = value;

                   if (isLastComponent) {
                       if (animationTypeEnum == 2)  {
                           const Vector3 eul(qua.value.x,qua.value.y,qua.value.z);

                           // Convert euler to quaternion: eul to qua-------------------------
                           /*
                           //const bool useAxisConventionOpenGL = true;
                           static Matrix3 m;
                           glm::mat3::FromEuler(m,eul[0],eul[1],eul[2],eulerRotationMode);//,useAxisConventionOpenGL);
                           //fprintf(stderr,"From Euler animationTypeEnum=%d\n\n",animationTypeEnum);
                           qua.value = glm::quat_cast(m);
                           qua.value.x = -qua.value.x;qua.value.y = -qua.value.y;qua.value.z = -qua.value.z;qua.value.w = -qua.value.w;  // useless (but matches blender in my test-case
                           qua.value.w = -qua.value.w;  //why???? Because glm::mat3::FromEuler(...) is wrong... in fact code below does not need it:
                           */

                           const glm::quat qx(eul.x,glm::vec3(1.0,0.0,0.0));
                           const glm::quat qy(eul.y,glm::vec3(0.0,1.0,0.0));
                           const glm::quat qz(eul.z,glm::vec3(0.0,0.0,1.0));
                           switch (eulerRotationMode) {
                           case glm::EULER_XYZ: qua.value = qz*qy*qx;break;
                           case glm::EULER_XZY: qua.value = qy*qz*qx;break;
                           case glm::EULER_YXZ: qua.value = qz*qx*qy;break;
                           case glm::EULER_YZX: qua.value = qx*qz*qy;break;
                           case glm::EULER_ZXY: qua.value = qy*qx*qz;break;
                           case glm::EULER_ZYX: qua.value = qx*qy*qz;break;
                           default: qua.value = qz*qy*qx;break;
                           }
                           // is the code above wrapped in this line or not?
                           //qua.value = glm::quat_FromEuler(eul[0],eul[1],eul[2],eulerRotationMode);
                           qua.value.x = -qua.value.x;qua.value.y = -qua.value.y;qua.value.z = -qua.value.z;qua.value.w = -qua.value.w;

                           /*static bool firstTime = true;if (firstTime) {firstTime=false;
                               printf("%s:       {%1.4f   %1.4f   %1.4f}    =>  {%1.4f  %1.4f   %1.4f   %1.4f};\n",bi->boneName.c_str(),glm::degrees(eul[0]),glm::degrees(eul[1]),glm::degrees(eul[2]),qua.value.w,qua.value.x,qua.value.y,qua.value.z);
                           }*/

                       }
                       else {
                           // rotation_angle_axis NEVER TESTED
                           const Vector3 axis(qua.value.x,qua.value.y,qua.value.z);
                           const float angle = qua.value.w;
                           glm::fromAngleAxis(qua.value,angle,axis);
                           //qua.value = -qua.value;  // useless (but matches blender in my test-case
                           //qua.value.w = -qua.value.w;  //why????

                           /*
                           static bool firstTime = true;if (firstTime) {firstTime=false;
                               printf("%s:       {%1.4f ; (%1.4f   %1.4f   %1.4f)}    =>  {%1.4f  %1.4f   %1.4f   %1.4f};\n",bi->boneName.c_str(),glm::degrees(angle),axis[0],axis[1],axis[2],qua.value.w,qua.value.x,qua.value.y,qua.value.z);
                           }
                           */
                       }
                       //-----------------------------------------------------------------
                       if (animationTime<qua.time) animationTime=qua.time;
                       //glm::normalizeInPlace(qua.value);
                       Mesh::BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
                       if (!bai) {
                           animationOk = true;
                           bai = &(bi->boneAnimationInfoMap[index] = Mesh::BoneAnimationInfo());
                           bai->reset();
                       }
                       bai->rotationKeys.push_back(qua);
                   }
                   if (rki+1 <= (int)rotationKeys.size()) lastQuaternionKey = &rotationKeys[++rki];
               }
               break;
               case 4:  {
                   if (animationTypeIndex==0) {
                       const size_t id = rotationKeys.size();
                       rotationKeys.push_back(Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey());
                       lastQuaternionKey = &rotationKeys[id];
                       lastQuaternionKey->time = time;
                   }
                   if (!lastQuaternionKey) {
                       fprintf(stderr,"An error has occurred while parsing an animation ('%s') with a 'rot' key for bone ('%s'): rotation_quaternion[0] MUST be the first key in the .blend file.\n",name.c_str(),boneName.c_str());
                       break;
                   }
                   Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey& qua = *lastQuaternionKey;
                   //qua.value[animationTypeIndex] = value; // Nope, we need more clearness:
                   #define QUA_W_IS_FIRST_COMPONENT
                   #ifndef QUA_W_IS_FIRST_COMPONENT
                   switch (animationTypeIndex) {
                   case 0: qua.value.x = value;break;
                   case 1: qua.value.y = value;break;
                   case 2: qua.value.z = value;break;
                   case 3: qua.value.w = value;break;
                   default: break;
                   }
                   #else //QUA_W_IS_FIRST_COMPONENT
                   switch (animationTypeIndex) {
                   case 0: qua.value.w = value;break;
                   case 1: qua.value.x = value;break;
                   case 2: qua.value.y = value;break;
                   case 3: qua.value.z = value;break;
                   default: break;
                   }
                   #undef QUA_W_IS_FIRST_COMPONENT
                   #endif //QUA_W_IS_FIRST_COMPONENT
                   if (isLastComponent) {
                       if (animationTime<qua.time) animationTime=qua.time;
                       //glm::normalizeInPlace(qua.value);
                       Mesh::BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
                       if (!bai) {
                           animationOk = true;
                           bai = &(bi->boneAnimationInfoMap[index] = Mesh::BoneAnimationInfo());
                           bai->reset();
                       }
                       bai->rotationKeys.push_back(qua);                       
                   }
                   if (rki+1 <= (int)rotationKeys.size()) lastQuaternionKey = &rotationKeys[++rki];
               }
               break;
               default:
               break;
               }

            }
        }

        //-----------------------------------

        c = c->next;
    }




    // second pass:-----------------------------------------
    const bool displayDebug = false;//true;//false;
    const bool mergeFrameKeys = false;  // The worst framekey decimator I've ever seen...
    const float mergeTimeEps = 3.01f;   // Twickable
    size_t maxNumFrames = 0;
    const glm::vec3 zeroVector3(0,0,0);
    const glm::vec3 oneVector3(1,1,1);
    const glm::quat zeroQuat(1,0,0,0);
    for (size_t i=0,sz = mesh.m_boneInfos.size();i<sz;i++) {
        Mesh::BoneInfo& bi = mesh.m_boneInfos[i];
        Mesh::BoneInfo::BoneAnimationInfo* pbai = bi.getBoneAnimationInfo(index);
        if (!pbai) continue;
        Mesh::BoneInfo::BoneAnimationInfo& bai = *pbai;

        // Testing only (to remove)
        bai.postState = Mesh::BoneInfo::BoneAnimationInfo::AB_REPEAT;

        #define PERFORM_KEYS_SIMPLIFICATION
        #ifdef PERFORM_KEYS_SIMPLIFICATION
        {
            const Scalar eps = 0.00001f;//0.0000001f;
            size_t sz = bai.translationKeys.size();unsigned cnt=0;
            if (sz>0)	{
                glm::vec3 oldValue = bai.translationKeys[0].value;
                glm::vec3 newValue;
                for (unsigned k = 1; k < sz ; k++)	{
                    newValue = bai.translationKeys[k].value;
                    if (glm::isEqual(newValue,oldValue,eps)) {
                        ++cnt;
                        //printf("	%d) newvalue==oldValue for T\n",k);
                    }
                    oldValue=newValue;
                }
                if (cnt >= sz - 1) {
                    //printf("	ALL TRANSLATION KEYS ARE: (%1.4f,%1.4f,%1.4f)\n",bai.translationKeys[0].value[0],bai.translationKeys[0].value[1],bai.translationKeys[0].value[2]);
                    bai.translationKeys.resize(1);
                    if (glm::isEqual(bai.translationKeys[0].value,zeroVector3)) bai.translationKeys.resize(0);
                }
            }
            sz = bai.rotationKeys.size();cnt=0;
            if (sz>0)	{
                glm::quat oldValue = bai.rotationKeys[0].value;
                glm::quat newValue;
                for (unsigned k = 1; k < sz ; k++)	{
                    newValue = bai.rotationKeys[k].value;
                    if (glm::isEqual(newValue,oldValue,eps)) {
                        ++cnt;
                        //printf("	%d) newvalue==oldValue for R\n",k);
                    }
                    oldValue=newValue;
                }
                if (cnt >= sz - 1) {
                    //printf("	ALL ROTATION KEYS ARE: (%1.3f,%1.3f,%1.3f,%1.3f)\n",bai.rotationKeys[0].value[0],bai.rotationKeys[0].value[1],bai.rotationKeys[0].value[2],bai.rotationKeys[0].value[3]);
                    bai.rotationKeys.resize(1);
                    if (glm::isEqual(bai.rotationKeys[0].value,zeroQuat)) bai.rotationKeys.resize(0);
                }
            }
            sz = bai.scalingKeys.size();cnt=0;
            if (sz>0)	{
                glm::vec3 oldValue = bai.scalingKeys[0].value;
                glm::vec3 newValue;
                for (unsigned k = 1; k < sz ; k++)	{
                    newValue = bai.scalingKeys[k].value;
                    if (glm::isEqual(newValue,oldValue,eps)) {
                        ++cnt;
                        //printf("	%d) newvalue==oldValue for S\n",k);
                    }
                    oldValue=newValue;
                }
                if (cnt >= sz - 1) {
                    //printf("	ALL SCALING KEYS ARE: (%1.4f,%1.4f,%1.4f)\n",bai.scalingKeys[0].value[0],bai.scalingKeys[0].value[1],bai.scalingKeys[0].value[2]);
                    bai.scalingKeys.resize(1);
                    if (glm::isEqual(bai.scalingKeys[0].value,oneVector3)) bai.scalingKeys.resize(0);
                }
            }
        }
        #endif //PERFORM_KEYS_SIMPLIFICATION


        if (mergeFrameKeys) {
            //---------------------------
            if (bai.translationKeys.size()>0) {
                typedef Mesh::BoneInfo::BoneAnimationInfo::Vector3Key KeyType;
                std::vector<KeyType>& v = bai.translationKeys;
                int i = v.size()-1;
                while (v.size()>1 && i>0) {
                    if (v[i].time-v[i-1].time<mergeTimeEps) {
                        if (i!=1)
                        v[i-1] = v[i];    // if (i!=1)  => preserves key[0] as it is
                        for (size_t j=i,jsz=v.size()-1;j<jsz;j++) v[j]=v[j+1];v.resize(v.size()-1);// erase element at i
                    }
                    --i;
                }
            }
            //-----------------------------
             if (bai.rotationKeys.size()>0) {
                typedef Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey KeyType;
                std::vector<KeyType>& v = bai.rotationKeys;
                int i = v.size()-1;
                while (v.size()>1 && i>0) {
                    if (v[i].time-v[i-1].time<mergeTimeEps) {
                        if (i!=1)
                        v[i-1] = v[i];    // if (i!=1)  => preserves key[0] as it is
                        for (size_t j=i,jsz=v.size()-1;j<jsz;j++) v[j]=v[j+1];v.resize(v.size()-1);// erase element at i
                    }
                    --i;
                }
            }
            //-----------------------------
            if (bai.scalingKeys.size()>0) {
                typedef Mesh::BoneInfo::BoneAnimationInfo::Vector3Key KeyType;
                std::vector<KeyType>& v = bai.scalingKeys;
                int i = v.size()-1;
                while (v.size()>1 && i>0) {
                    if (v[i].time-v[i-1].time<mergeTimeEps) {
                        if (i!=1)
                            v[i-1] = v[i];    // if (i!=1)  => preserves key[0] as it is
                        for (size_t j=i,jsz=v.size()-1;j<jsz;j++) v[j]=v[j+1];v.resize(v.size()-1);// erase element at i
                    }
                    --i;
                }
            }
            //-----------------------------
       }


        // Setting bai.tkAndRkHaveUniqueTimePerKey and similiar---------
        const unsigned numPositonKeys = bai.translationKeys.size();
        const unsigned numRotationKeys = bai.rotationKeys.size();
        const unsigned numScalingKeys = bai.scalingKeys.size();
        const unsigned numKeys = numPositonKeys>0 ? numPositonKeys : numRotationKeys>0 ? numRotationKeys : numScalingKeys>0 ? numScalingKeys : 0;
        if (numKeys!=0) {
            bool ok = true &&
                    (numPositonKeys==0 || numKeys==numPositonKeys)	&&
                    (numRotationKeys==0 || numKeys==numRotationKeys)  &&
                    (numScalingKeys==0  || numKeys==numScalingKeys);
            if (ok)	{
                if (numPositonKeys>0)	{
                    if (numRotationKeys>0)	{
                        // Check equality P=R
                        for (unsigned k = 0 ; k < numKeys ; k++)	{
                            if (bai.translationKeys[k].time != bai.rotationKeys[k].time)	{ok=false;break;}
                        }
                        if (ok) bai.tkAndRkHaveUniqueTimePerKey = true;
                        ok = true;
                    }
                    if (numScalingKeys>0)	{
                        // Check equality P=S
                        for (unsigned k = 0 ; k < numKeys ; k++)	{
                            if (bai.translationKeys[k].time != bai.scalingKeys[k].time)	{ok=false;break;}
                        }
                        if (ok) bai.tkAndSkHaveUniqueTimePerKey = true;
                        ok = true;
                    }
                }
                if (numRotationKeys>0)	{
                    if (numScalingKeys>0)	{
                        // Check equality R=S
                        for (unsigned k = 0 ; k < numKeys ; k++)	{
                            if (bai.rotationKeys[k].time != bai.scalingKeys[k].time)	{ok=false;break;}
                        }
                        if (ok) bai.rkAndSkHaveUniqueTimePerKey = true;
                        ok = true;
                    }
                }
            }
        }

        //------------------------------------------------
        //#define DISPLAY_SYNCHRO_INFO
        #ifdef DISPLAY_SYNCHRO_INFO
        printf("Bone[%d]['%s']Animation[%d]['%s']: NumTK:%d NumRK:%d NumSK:%d ",bi.index,bi.boneName.c_str(),animIndex,getAnimationName(animIndex).c_str(),bai.translationKeys.size(),bai.rotationKeys.size(),bai.scalingKeys.size());
        if (bai.tkAndRkHaveUniqueTimePerKey && bai.tkAndRkHaveUniqueTimePerKey==bai.tkAndSkHaveUniqueTimePerKey && bai.tkAndSkHaveUniqueTimePerKey==bai.rkAndSkHaveUniqueTimePerKey)	{
            printf("hasSynchroKeys");
        }
        else	{
            if (bai.tkAndRkHaveUniqueTimePerKey) printf("tkRkTime ");
            if (bai.tkAndSkHaveUniqueTimePerKey) printf("tkSkTime ");
            if (bai.rkAndSkHaveUniqueTimePerKey) printf("rkSkTime ");
        }
        printf("\n");
        #undef DISPLAY_SYNCHRO_INFO
        #endif //DISPLAY_SYNCHRO_INFO
        //----------------------------------------------------------------

        maxNumFrames = numPositonKeys;
        if (numRotationKeys>0 && (!bai.tkAndRkHaveUniqueTimePerKey || maxNumFrames==0)) {
            maxNumFrames+=numRotationKeys;   // wrong
            if (numScalingKeys>0 && !bai.tkAndSkHaveUniqueTimePerKey && !bai.rkAndSkHaveUniqueTimePerKey)  maxNumFrames+=numScalingKeys;   // wrong
        }
        else if (numScalingKeys>0 && ((!bai.tkAndSkHaveUniqueTimePerKey || (numRotationKeys>0 && !bai.rkAndSkHaveUniqueTimePerKey))|| maxNumFrames==0))
                   maxNumFrames+=numScalingKeys;   // wrong

        if (displayDebug) {
            printf("bone: '%s':\n",bi.boneName.c_str());
            for (size_t j=0;j<bai.translationKeys.size();j++) {
                const Mesh::BoneInfo::BoneAnimationInfo::Vector3Key& k = bai.translationKeys[j];
                printf("\ttra[%d: %1.2f] = {%1.4f %1.4f %1.4f};\n",(int)j,k.time,k.value[0],k.value[1],k.value[2]);
            }
            for (size_t j=0;j<bai.scalingKeys.size();j++) {
                const Mesh::BoneInfo::BoneAnimationInfo::Vector3Key& k = bai.scalingKeys[j];
                printf("\tsca[%d: %1.2f] = {%1.4f %1.4f %1.4f};\n",(int)j,k.time,k.value[0],k.value[1],k.value[2]);
            }
            for (size_t j=0;j<bai.rotationKeys.size();j++) {
                const Mesh::BoneInfo::BoneAnimationInfo::QuaternionKey& k = bai.rotationKeys[j];
                printf("\tqua[%d: %1.2f] = {%1.4f %1.4f %1.4f,%1.4f};\n",(int)j,k.time,k.value.w,k.value.x,k.value.y,k.value.z);
            }
        }
    }
    //------------------------------------------------------


    // Setting global Mesh::AnimationInfo
    if (animationOk)    {
        mesh.m_animationInfos.push_back(Mesh::AnimationInfo());
        mesh.m_animationIndexMap[name] = index;
        Mesh::AnimationInfo& ai = mesh.m_animationInfos[index];

        ai.name = name;

        //fbtPrintf("animationName:'%s' animationTime:%1.4f maxNumFrames:%d frs_sec:%1.4f\n",name.c_str(),animationTime,(int)maxNumFrames,frs_sec)
;       ai.numTicks = animationTime;
        ai.ticksPerSecond = frs_sec > 0 ? frs_sec : 24;//maxNumFrames > 0 ? (float)maxNumFrames : 24.0;
        ai.duration = ai.numTicks/ai.ticksPerSecond;    // mmmh: this should be animationTime!!!
    }


}

static void LoadAnimations(Mesh& mesh,const Blender::AnimData* a,float frs_sec=24.f) {
    if (!a) return;
    const Blender::bAction* action = a->action;
    if (action) LoadAction(mesh,action,frs_sec);    // Just 1 action ?
}

};

#ifdef ALLOW_GLOBAL_OBJECT_TRANSFORM
// T contains translation and rotation only
static inline void GetObjectTransform(Blender::Object* ob,glm::mat3& rot,glm::vec3& tra,glm::vec3& scale,const char* parentSubstr=NULL,bool initT=true) {
    using glm::mat4;
    using glm::mat3;
    using glm::vec3;
    using glm::quat;
    using namespace glm;

    if (initT) {rot = glm::mat3(1);tra[0]=tra[1]=tra[2]=0;scale[0]=scale[1]=scale[2]=1;}
    if (!ob) return;

    //printf("%s: rotationMode:%d\n",ob->id.name,(int)ob->rotmode);
    /*printf("loc:[%1.4f,%1.4f,%1.4f]\tdloc:[%1.4f,%1.4f,%1.4f]\torig:[%1.4f,%1.4f,%1.4f]\n",ob->loc[0],ob->loc[1],ob->loc[2],ob->dloc[0],ob->dloc[1],ob->dloc[2],ob->orig[0],ob->orig[1],ob->orig[2]);
    printf("rotAngle:%1.4f\tdrotAngle:%1.4f\trotAxis:[%1.4f,%1.4f,%1.4f]\tdrotAxis:[%1.4f,%1.4f,%1.4f]\n",ob->rotAngle,ob->drotAngle,ob->rotAxis[0],ob->rotAxis[1],ob->rotAxis[2],ob->drotAxis[0],ob->drotAxis[1],ob->drotAxis[2]);
    printf("size:[%1.4f,%1.4f,%1.4f]\tdsize:[%1.4f,%1.4f,%1.4f]\n",ob->size[0],ob->size[1],ob->size[2],ob->dsize[0],ob->dsize[1],ob->dsize[2]);
    */

    mat3 obR;vec3 obT;vec3 obS;

    if (ob->rotmode>0 && ob->rotmode<7) {
        //printf("[%1.4f %1.4f %1.4f] %d\n",btDegrees(ob->rot[0]),btDegrees(ob->rot[1]),btDegrees(ob->rot[2]),(int)ob->rotmode);
        glm::mat3::FromEuler(obR,ob->rot[0],ob->rot[1],ob->rot[2],(glm::EulerRotationMode) ob->rotmode);//,false);
        //fprintf(stderr,"From Euler ob->rotmode=%d\n\n",ob->rotmode);
    }
    else if (ob->rotmode==0)    {
        // Quaternion WXYZ:
        //printf("ob->quat(WXYZ):[%1.4f,%1.4f,%1.4f,%1.4f]\n",ob->quat[0],ob->quat[1],ob->quat[2],ob->quat[3]);
        quat q(ob->quat[0],ob->quat[1],ob->quat[2],ob->quat[3]);    // Warning: ob->quat[0] is the W component!
        obR = mat3_cast(q);
    }
    else  {
        // Axis Angle:
        quat q;
        glm::fromAngleAxis(q,ob->rotAngle,vec3(ob->rotAxis[0],ob->rotAxis[1],ob->rotAxis[2]));
        obR = mat3_cast(q);
    }
    obT =  vec3(ob->loc[0],ob->loc[1],ob->loc[2]);
    obS =  vec3(ob->size[0],ob->size[1],ob->size[2]);

    // Is bone parent?
    if (ob->type==(short)BL_OBTYPE_ARMATURE && parentSubstr && strlen(parentSubstr)>0)  {
        mat3 bR(1);vec3 bT(0,0,0);vec3 bS(1,1,1);
        //printf("An object is parented to bone '%s'' in armature '%s'. TODO: Handle this case\n",parentSubstr,ob->id.name);
        const Blender::bArmature* armature = BlenderHelper::GetArmature(ob);
        if (armature)   {
             // This displays everything
            /*
            {
                BlenderHelper::Display(armature);
                Blender::ListBase rootBones = armature->bonebase;   // These seems to be the root bones only...
                const Blender::Bone* b = (const Blender::Bone*) rootBones.first;
                while (b) {
                    BlenderHelper::Display(b);
                    b=b->next;
                }
            }
            */
            const Blender::Bone* b = BlenderHelper::FindBoneFromName(armature,parentSubstr);
            if (b) {
                //BlenderHelper::Display(b);
                BlenderHelper::GetSubMat3(bR,b->arm_mat,false);
                //glm::FromMatrixf4x4(b->arm_mat,bR,false);    // This works ok for the rotation part only
                bT =  vec3(b->arm_tail[0],b->arm_tail[1],b->arm_tail[2]); // This is OK for the position
                // What about scaling ? There's b->size, but what if scaling is included in b->arm_mat too ?
                //bT.getOrigin()*=obS;

            }

            //--------------------------------------------------------------------
            // WARNING: This part is wrong if the armature or one of its parent objects has a scaling applied:
            //--------------------------------------------------------------------
            obS*=bS;    // bS is not used

            //obTransform*=bTransform;  // good, or: (bS is always one)
            obT += obR*(bT*bS);
            obR  = obR*bR;
            //---------------------------------------------------------------------
            /*
            //-----------------------------------------------------------------------
            scale*=obS;
            //T.getOrigin() = obT.getOrigin() + obT.getBasis()*(T.getOrigin()*scale);   //good but not enough
            T.getOrigin() = obT.getOrigin() + obT.getBasis()*(T.getOrigin()*obS);
            T.getBasis()  = obT.getBasis()*T.getBasis();
            //------------------------------------------------------------------------
            */
        }
    }
    //else {
    //-----------------------------------------------------------------------
    scale*=obS;
    //T.getOrigin() = obT.getOrigin() + obT.getBasis()*(T.getOrigin()*scale);   //good but not enough
    tra = obT + obR*(tra*obS);
    rot = obR*rot;
    //------------------------------------------------------------------------
    //}

    GetObjectTransform(ob->parent,rot,tra,scale,ob->parsubstr,false);

}
#endif //ALLOW_GLOBAL_OBJECT_TRANSFORM


bool Mesh::loadFromAnimatedBlendFile(const char* filePath/*const filesystem::path& filePath*/, int flags)    {
    using namespace Blender;
    const bool invertYZ = false; // true does NOT work! But it should be true (because Blender uses z-Axis as height, whereas OpenGL uses y as height)

    fbtBlend fp;
//    if (fp.parse(filePath,fbtFile::PM_READTOMEMORY) != fbtFile::FS_OK &&
//        fp.parse(filePath,fbtFile::PM_COMPRESSED) != fbtFile::FS_OK)
//        return false;
    if (fp.parse(filePath) != fbtFile::FS_OK) return false;
    const std::string parentFolderPath = Path::GetDirectoryName(std::string(filePath));

    const int startPartIndex = meshParts.size();
    int startObjectIndex;
    int currentPartIndex = -1;
    m_boneInfos.resize(0);
    numValidBones = 0;
    m_rootBoneInfos.resize(0);
    m_boneIndexMap.clear();
    m_animationInfos.resize(0);
    m_animationIndexMap.clear();


    float frs_sec = 24;
    float frs_base = 1.0f;
    if (fp.m_scene.first) {
        const Scene* s = (const Scene*) fp.m_scene.first;
        frs_sec = (float) s->r.frs_sec;
        frs_base = s->r.frs_sec_base;
        /*
        Scene-Renderer: flag:0 framapto:100
        framelen:1.0000 frame_step:1 freqplay:60 frs_sec:12 frs_sec_base:1.0000
        */
    }

    //#define TEST_TO_REMOVE
    #ifdef TEST_TO_REMOVE
    {
        //BlenderHelper::CycledCall<bArmature>(fp.m_armature,&BlenderHelper::Display);
        //BlenderHelper::CycledCall<bAction>(fp.m_action,&BlenderHelper::Display);        // HERE THERE CAN BE MORE THAN ONE ACTION!!! BUT HOW TO FIND THE PARENT ARMATURE FROM HERE ?
        // Here there's a lot of stuff to look for: just look at fp.m_XXX stuff...
        //BlenderHelper::CycledCall<Scene>(fp.m_scene,&BlenderHelper::Display);   // Here there is the frs_sec data:

        //BlenderHelper::CycledCall<bNodeTree>(fp.m_nodetree,&BlenderHelper::Display);
    }
    #undef TEST_TO_REMOVE
    #endif //TEST_TO_REMOVE

    std::map<uintptr_t,GLuint> packedFileMap;
    const Blender::bArmature* bArmature = NULL; // Only the meshes parented to the first armature found are loaded
    const Blender::Object* bArmatureObject = NULL;
    int numObjectsLoaded = 0;
    try {
        //=============================================================================================================================
        fbtList& objects = fp.m_object;
        for (Object* ob = (Object*)objects.first; ob; ob = (Object*)ob->id.next)	{
            if (!ob) break;
            if (!ob->data || ob->type != (short) BL_OBTYPE_MESH)	continue;
            // It's a Mesh

            string meshName = ob->id.name;  // it's actually objectName
            if (String::StartsWith(meshName,"OB")) meshName = meshName.substr(2);


            BlenderModifierMirror mirrorModifier;
            string boneParentName = "";
            bool mustLoadArmature = false;

            // 'hidden prop' is not here
            //printf("ob:'%s' state:%d flag:%d protectFlag:%d\n",ob->id.name,ob->state,ob->flag,ob->protectflag);    // To remove (looking for the hidden property)
            //BlenderHelper::CycledCall<Blender::bProperty>(ob->prop,&BlenderHelper::Display);                       // To remove (looking for the hidden property)
            //BlenderHelper::CycledCall<Blender::bProperty>(ob->prop,&BlenderHelper::Display);

            // Fill 'mirrorModifier' and 'bArmature' and skip mesh if not good or not skeletal animated
            {
                const Blender::bArmature* bArmatureOfThisMesh = NULL;
                const Blender::Object* bArmatureObjectOfThisMesh = NULL;
                const ModifierData* mdf = (const ModifierData*) ob->modifiers.first;
                while (mdf)    {
                    // mdf->type(5=mirror 8=armature)
                    //printf("Object '%s' had modifier: %s (type=%d mode=%d stackindex=%d)\n",ob->id.name,mdf->name,mdf->type,mdf->mode,mdf->stackindex);
                    if (mdf->error) {
                        //printf("Error: %s\n",mdf->error);
                    }
                    else {
                        if (mdf->type==5) {
                            //Mirror modifier:
                            const MirrorModifierData* md = (const MirrorModifierData*) mdf;
                            //printf("MirrorModifier: axis:%d flag:%d tolerance:%1.4f\n",md->axis,md->flag,md->tolerance);
                            //if (md->mirror_ob) printf("\nMirrorObj: %s\n",md->mirror_ob->id.name);
                            if (!mirrorModifier.enabled && !md->mirror_ob) {
                                mirrorModifier = BlenderModifierMirror(*md);
                                //mirrorModifier.display();
                            }
                        }
                        else if (mdf->type==8) {
                            //Armature Modifier:
                            const ArmatureModifierData* md = (const ArmatureModifierData*) mdf;
                            /*
                            printf("ArmatureModifier: deformflag:%d multi:%d\n",md->deformflag,md->multi);
                            if (md->object) printf("Armature Object: %s\n",md->object->id.name);
                            if (md->defgrp_name) printf("Deform Group Name: %s\n",md->defgrp_name);
                            */
                            bArmatureOfThisMesh = BlenderHelper::GetArmature(md->object);
                            if (!bArmatureOfThisMesh) fprintf(stderr,"Error: object %s has an armature modifier without name.\n",ob->id.name);
                            else bArmatureObjectOfThisMesh = md->object;
                        }
                        //else fprintf(stderr,"Detected unknown modifier: %d for object: %s\n",mdf->type,ob->id.name);
                    }
                    mdf = mdf->next;
                }
                if (!bArmatureOfThisMesh) {
                    bArmatureOfThisMesh = BlenderHelper::GetArmature(ob->parent);
                    if (bArmatureOfThisMesh) bArmatureObjectOfThisMesh = ob->parent;
                    if (strlen(ob->parsubstr)>0) boneParentName = ob->parsubstr;
                }
                if (!bArmatureOfThisMesh) continue; // We discard meshes without an armature here
                if (!bArmature) {
                    bArmature = bArmatureOfThisMesh;
                    bArmatureObject = bArmatureObjectOfThisMesh;
                    mustLoadArmature = true;
                }
                else if (bArmature!=bArmatureOfThisMesh) continue;  // We discard meshes parented to a different armature from the first one found here
            }
            //---------------------
            const Blender::Mesh* me = (const Blender::Mesh*)ob->data;

            //printf("me:'%s' drawflag:%d editflag:%d flag:%d\n",me->id.name,me->drawflag,me->editflag,me->flag);
            if (me->key) BlenderHelper::CycledCall<Blender::KeyBlock>(me->key->block,&BlenderHelper::Display);  // shape kes here!

            // VALIDATION-------------------------------------------
            // fbtPrintf("\tMesh: %s\n", me->id.name);
            const bool hasFaces = me->totface>0 && me->mface;
            const bool hasPolys = me->totpoly>0 && me->mpoly && me->totloop>0 && me->mloop;
            const bool hasVerts = me->totvert>0;
            const bool isValid = hasVerts && (hasFaces || hasPolys);
            if (!isValid) continue;
            //-------------------------------------------------------
            //printf("File: \"%s\": loading mesh %d: %s.\n",filePath,numObjectsLoaded,ob->id.name);

            // MATERIALS---------------------------------------------
            // Load Materials first
            const int numMaterials = me->totcol > 0 ? me->totcol : 1;
            std::vector< ::Material > materialVector(numMaterials);
            std::vector< BlenderTexture > textureVector(numMaterials);
            std::vector< string > materialNameVector(numMaterials);
            std::vector< string > textureFilePathVector(numMaterials);
            if (me->mat && *me->mat)	{
                for (int i = 0; i < me->totcol; ++i)	{
                    const Blender::Material* ma = me->mat[i];
                    if (!ma) continue;
                    GetMaterialFromBlender(ma,materialVector[i]);
                    materialNameVector[i] = string(ma->id.name);
                    GetTextureFromBlender(ma->mtex,textureVector[i],parentFolderPath,&packedFileMap,BlenderTexture::MAP_COLOR,&textureFilePathVector[i]);
                }
            }
            //-------------------------------------------------------

            // MESH -------------------------------------------------
            startObjectIndex = currentPartIndex = (int) meshParts.size();
            meshParts.resize(meshParts.size()+numMaterials);

            for (int i=0;i<numMaterials;i++) {
                Part& m = meshParts[startObjectIndex+i];
                m.meshName = meshName;
                m.material = materialVector[i]; m.materialName = materialNameVector[i];
                m.textureId = textureVector[i].id; if (m.textureId!=0) m.textureFilePath = textureFilePathVector[i];
                m.useVertexArrayInPartIndex = startObjectIndex;
            }

            Part& firstMesh = meshParts[startObjectIndex];
            std::vector < Vector3 >& verts = firstMesh.verts;
            std::vector < Vector3 >& norms = firstMesh.norms;
            std::vector < Vector2 >& texCoords = firstMesh.texcoords;
            bool hasTexCoords = false;

            // VERTS AND NORMS--(NOT MIRRORED)--------------------------
            int numSingleVerts = me->totvert;
            int numVerts = mirrorModifier.enabled ? numSingleVerts * mirrorModifier.getNumVertsMultiplier() : numSingleVerts;
            verts.reserve(numVerts);norms.reserve(numVerts);texCoords.reserve(numVerts);
            verts.resize(numSingleVerts);norms.resize(numSingleVerts);texCoords.resize(numSingleVerts);
            for (int i=0;i<numSingleVerts;i++) {
                const MVert& v = me->mvert[i];
                Vector3& V = verts[i];
                V[0]=v.co[0];V[1]=v.co[1];V[2]=v.co[2];
                Vector3& N = norms[i];
                N[0]=v.no[0];N[1]=v.no[1];N[2]=v.no[2];
                texCoords[i] = Vector2(0,0);
            }

            // ARMATURE --------------------------------------------------------
            #define LOAD_ANIMATIONS_FROM_SCENE  // Support for more than one animation, but all must belong to the same unique supported armature in the .blend file
            if (mustLoadArmature) {
                //fprintf(stderr,"Loading armature: '%s'\n",bArmature->id.name);
                MeshArmatureSetter::LoadArmature(*this,bArmatureObject,invertYZ);

                //printf("Armature:\n");
                //BlenderHelper::Display((const bPose*)bArmatureObject->pose);    // <=== OK!
                //BlenderHelper::Display((const bAction*)bArmatureObject->poselib);
                //BlenderHelper::CycledCall<const Blender::bAction>(bArmatureObject->poselib,&BlenderHelper::Display);


                #ifdef LOAD_ANIMATIONS_FROM_SCENE
                for (bAction* a = (bAction*)fp.m_action.first; a; a = (bAction*)a->id.next)	{
                    if (!a) continue;
                    MeshArmatureSetter::LoadAction(*this,a,frs_sec);
                }
                #endif //LOAD_ANIMATIONS_FROM_SCENE
            }

            //printf("Object:\n");
            //BlenderHelper::Display((const bPose*)ob->pose);
            //BlenderHelper::CycledCall<const Blender::bAction>(ob->poselib,&BlenderHelper::Display);


            // VERTEX DEFORM GROUP NAMES----------------------------------------
            if (boneParentName.size()==0) {
                std::vector<string> deformGroupNames;        // They should match the bone names
                std::map<int,int> deformGroupBoneIdMap;
                const bDeformGroup* dg = (const bDeformGroup*) ob->defbase.first;
                while (dg) {
                    const int dgId = deformGroupNames.size();
                    deformGroupNames.push_back(dg->name);
                    //printf("%d) %s\n",deformGroupNames.size()-1,deformGroupNames[deformGroupNames.size()-1].c_str());
                    if (m_boneInfos.size()>0) {
                        const BoneInfo* boneInfo = getBoneInfoFromName(dg->name);
                        if (boneInfo) {
                            deformGroupBoneIdMap[dgId] = (int) boneInfo->index;
                            //printf("deformGroupBoneIdMap[%d] = %d\n",dgId,(int) boneInfo->index);   // This line is good for debugging
                        }
                        else {
                            deformGroupBoneIdMap[dgId] = -1;
                            fprintf(stderr,"Error: Deform group: '%s' has no matching bone inside armature\n",dg->name);
                        }
                    }
                    dg = dg->next;
                }
                // VERTS DEFORM DATA (TODO: Mirror these when the mirror modifier is present)-
                if (me->dvert)  {
                    //BlenderHelper::DisplayDeformGroups(me);
                    std::vector < BoneIdsType >& boneIndices = firstMesh.boneIds;
                    std::vector < BoneWeightsType >& boneWeights = firstMesh.boneWeights;
                    boneIndices.resize(numSingleVerts);
                    boneWeights.resize(numSingleVerts);
                    //const int numSingleVerts = me->totvert;
                    for (int i = 0;i<numSingleVerts;i++)	{
                        const Blender::MDeformVert& d = me->dvert[i];
                        BlenderHelper::FillDeformGroups(d,boneIndices[i],boneWeights[i],&deformGroupBoneIdMap);
                        //printf("%d) [%d  %1.6f] [%d  %1.6f] [%d  %1.6f] [%d  %1.6f]\n",i,boneIndices[i][0],boneWeights[i][0],
                        //boneIndices[i][1],boneWeights[i][1],boneIndices[i][2],boneWeights[i][2],boneIndices[i][3],boneWeights[i][3]);
                        //#define DEBUG_BONE_INDICES_AND_WEIGHTS
#ifdef DEBUG_BONE_INDICES_AND_WEIGHTS
                        for (int j=0;j<4;j++)   {
                            if (boneWeights[i][j]>0)    {
                                if (j==0) printf("%d)   ",i);
                                printf("    [w(%d:%s) = %1.6f]",boneIndices[i][j],m_boneInfos[boneIndices[i][j]].boneName.c_str(),boneWeights[i][j]);
                            }
                            if (j==3) printf("\n");
                        }
#endif //DEBUG_BONE_INDICES_AND_WEIGHTS
                    }
                }
            }
            else if (m_boneInfos.size()>0)  {
                const BoneInfo* boneInfo = getBoneInfoFromName(boneParentName);
                if (boneInfo) {
                    firstMesh.parentBoneId = (int) boneInfo->index;
                    // Fill weights:
                    std::vector < BoneIdsType >& boneIndices = firstMesh.boneIds;
                    std::vector < BoneWeightsType >& boneWeights = firstMesh.boneWeights;
                    boneIndices.resize(numSingleVerts);
                    boneWeights.resize(numSingleVerts);
                    //const int numSingleVerts = me->totvert;
                    for (int i = 0;i<numSingleVerts;i++)	{
                        BoneIdsType& bi = boneIndices[i];
                        BoneWeightsType& bw = boneWeights[i];
                        bi[0] = boneInfo->index;    // (unsigned) parentBoneId;
                        bw[0] = 1.f;
                        for (int j=1;j<4;j++) {bi[j]=0;bw[j]=0.f;}
                    }
                }
                else fprintf(stderr,"Error: the mesh '%s' has a parent bone named: '%s', that does not exist inside current armature (we support a single armature).\n",ob->id.name,boneParentName.c_str());
            }
            //-----------------------------------------------------------

            // ANIMATIONS----------------------------------------------------------
            #ifndef  LOAD_ANIMATIONS_FROM_SCENE
            if (ob->adt) MeshArmatureSetter::LoadAnimations(*this,(const Blender::AnimData*) ob->adt,frs_sec);  // Ok but there's space for a single action here...
            #else // LOAD_ANIMATIONS_FROM_SCENE
            #undef LOAD_ANIMATIONS_FROM_SCENE
            #endif // LOAD_ANIMATIONS_FROM_SCENE
            //---------------------------------------------------------------------

            std::vector < IndexType >* pinds = NULL;
            const BlenderTexture* pBlenderTexture = NULL;

            Vector2 tc;int li0,li1,li2,li3,vi0,vi1,vi2,vi3;

            // INDS AND TEXCOORDS--(NOT MIRRORED)..........................
            std::vector<int> numTexCoordAssignments(numSingleVerts,0);
            std::multimap<int,int> texCoordsSingleVertsVertsMultiMap;
            if (hasFaces)
            {
                // NEVER TESTED
                pBlenderTexture = NULL;pinds = NULL;  // reset

                short mat_nr = me->mface[0].mat_nr;bool mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;int ic,ic2;
                //int smoothingGroup = (me->mface[0].flag&1) ? 1 : -1;
                //bool flatShading = (!(me->mface[0].flag&1)) ? true : false;
                bool isTriangle =  me->mface[0].v4==0 ? true : false;	// Here I suppose that 0 can't be a vertex index in the last point of a quad face (e.g. quad(0,1,2,0) is a triangle(0,1,2)). This assunction SHOULD BE WRONG, but I don't know how to spot if a face is a triangle or a quad...
                // TODO: also MTFace::mode can be: 512 (TF_TWOSIDE), 256 (TF_BILLBOARD), 4096 (TF_BILLBOARD2), 1024 (TF_INVISIBLE), 2 (TF_ALPHASORT), 4 (TF_TEX)
                // and MTFace::transp can be: 0 (TF_SOLID), 2 (TF_ALPHA)
                if (mat_nr_ok)  {
                    pBlenderTexture =           &textureVector[mat_nr];
                    pinds =                     &meshParts[startObjectIndex+mat_nr].inds;
                }

                for (int i=0;i<me->totface;i++) {
                    const MFace& mface = me->mface[i];
                    if (mat_nr!= mface.mat_nr)	{
                        mat_nr = mface.mat_nr;

                        pBlenderTexture = NULL;pinds = NULL;
                        mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;
                        // TODO: also MTFace::mode can be: 512 (TF_TWOSIDE), 256 (TF_BILLBOARD), 4096 (TF_BILLBOARD2), 1024 (TF_INVISIBLE), 2 (TF_ALPHASORT), 4 (TF_TEX)
                        // and MTFace::transp can be: 0 (TF_SOLID), 2 (TF_ALPHA)
                        if (mat_nr_ok)  {
                            pBlenderTexture =           &textureVector[mat_nr];
                            pinds =                     &meshParts[startObjectIndex+mat_nr].inds;
                        }
                    }
                    if (!pinds) continue;
                    //smoothingGroup = (mface.flag&1) ? 1 : -1;
                    //flatShading=!(mface.flag&1);
                    isTriangle=(mface.v4==0);
                    const MTFace* mtf = (pBlenderTexture->id>0 && me->mtface) ? &me->mtface[i] : NULL;

                    // Add (first) triangle:
                    pinds->push_back(mface.v1);
                    pinds->push_back(mface.v2);
                    pinds->push_back(mface.v3);

                    if (mtf) {
                        hasTexCoords = true;
                        pBlenderTexture->transformTexCoords(mtf->uv[0][0],mtf->uv[0][1],tc);
                        ic = mface.v1;
                        if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                            (*pinds)[pinds->size()-3] = ic2;

                        pBlenderTexture->transformTexCoords(mtf->uv[1][0],mtf->uv[1][1],tc);
                        ic = mface.v2;
                        if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                            (*pinds)[pinds->size()-2] = ic2;

                        pBlenderTexture->transformTexCoords(mtf->uv[2][0],mtf->uv[2][1],tc);
                        ic = mface.v3;
                        if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                            (*pinds)[pinds->size()-1] = ic2;
                    }

                    if (!isTriangle)	{
                        // Add second triangle:
                        pinds->push_back(mface.v3);
                        pinds->push_back(mface.v4);
                        pinds->push_back(mface.v1);


                        if (mtf) {
                            hasTexCoords = true;
                            pBlenderTexture->transformTexCoords(mtf->uv[2][0],mtf->uv[2][1],tc);
                            ic = mface.v3;
                            if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                (*pinds)[pinds->size()-3] = ic2;

                            pBlenderTexture->transformTexCoords(mtf->uv[3][0],mtf->uv[3][1],tc);
                            ic = mface.v4;
                            if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                (*pinds)[pinds->size()-2] = ic2;

                            pBlenderTexture->transformTexCoords(mtf->uv[0][0],mtf->uv[0][1],tc);
                            ic = mface.v1;
                            if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                (*pinds)[pinds->size()-1] = ic2;
                        }
                    }

                }

            }
            else if (hasPolys) {

                pBlenderTexture = NULL;pinds = NULL;   // reset

                short mat_nr = me->mpoly[0].mat_nr;bool mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;int ic,ic2;
                //bool flatShading = (!(me->mpoly[0].flag&1)) ? true : false;
                // TODO: also MTFace::mode can be: 512 (TF_TWOSIDE), 256 (TF_BILLBOARD), 4096 (TF_BILLBOARD2), 1024 (TF_INVISIBLE), 2 (TF_ALPHASORT), 4 (TF_TEX)
                // and MTFace::transp can be: 0 (TF_SOLID), 2 (TF_ALPHA)
                if (mat_nr_ok)  {
                    pBlenderTexture =           &textureVector[mat_nr];
                    pinds =                     &meshParts[startObjectIndex+mat_nr].inds;
                }

                for (int i=0;i<me->totpoly;i++) {
                    const MPoly& poly = me->mpoly[i];
                    if (mat_nr!= poly.mat_nr)	{
                        mat_nr = poly.mat_nr;

                        pBlenderTexture = NULL;pinds = NULL;

                        mat_nr_ok = mat_nr>=0 && mat_nr<numMaterials;
                        if (mat_nr_ok)  {
                            pBlenderTexture =           &textureVector[mat_nr];
                            pinds =                     &meshParts[startObjectIndex+mat_nr].inds;
                        }
                    }
                    if (!pinds) continue;

                    //smoothingGroup = (poly.flag&1) ? 1 : -1;
                    //printf("%d ",smoothingGroup);
                    //flatShading=!(poly.flag&1);
                    const int numLoops = poly.totloop;
                    const MTexPoly* mtp = (pBlenderTexture->id>0 && me->mtpoly && me->mloopuv) ? &me->mtpoly[i] : NULL;

                    {
                        // Is triangle or quad:
                        if (numLoops==3 || numLoops==4) {

                            // draw first triangle
                            if (poly.loopstart+2>=me->totloop) {
                                fbtPrintf("me->mtpoly ERROR: poly.loopstart+2(%d)>=me->totloop(%d)\n",poly.loopstart+2,me->totloop);
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


                            if (mtp) {
                                hasTexCoords = true;
                                const MLoopUV* uv = &me->mloopuv[li0];
                                pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                ic = vi0;
                                if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                    (*pinds)[pinds->size()-3] = ic2;


                                uv = &me->mloopuv[li1];
                                pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                ic = vi1;
                                if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                    (*pinds)[pinds->size()-2] = ic2;

                                uv = &me->mloopuv[li2];
                                pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                ic = vi2;
                                if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                    (*pinds)[pinds->size()-1] = ic2;
                            }

                            // isQuad
                            if (numLoops==4)	{
                                if (poly.loopstart+3>=me->totloop) {
                                    fbtPrintf("me->mtpoly ERROR: poly.loopstart+3(%d)>=me->totloop(%d)\n",poly.loopstart+3,me->totloop);
                                    continue;
                                }
                                li3 = poly.loopstart+3;
                                vi3 = me->mloop[li3].v;if (vi3 >= me->totvert) continue;

                                // Add second triangle ----------------
                                pinds->push_back(vi2);
                                pinds->push_back(vi3);
                                pinds->push_back(vi0);


                                if (mtp) {
                                    hasTexCoords = true;
                                    const MLoopUV* uv = &me->mloopuv[li2];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                    ic = vi2;
                                    if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-3] = ic2;

                                    uv = &me->mloopuv[li3];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                    ic = vi3;
                                    if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-2] = ic2;

                                    uv = &me->mloopuv[li0];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                    ic = vi0;
                                    if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
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
                            do {
                                isQuad = remainingFaceSize>=4;

                                if (poly.loopstart + currentLastVertPos>=me->totloop) {
                                    fbtPrintf("me->mtpoly ERROR: poly.loopstart + currentLastVertPos(%d)>=me->totloop(%d)\n",poly.loopstart + currentLastVertPos,me->totloop);
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


                                if (mtp) {
                                    hasTexCoords = true;
                                    const MLoopUV* uv = &me->mloopuv[li0];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                    ic = vi0;
                                    if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-3] = ic2;

                                    uv = &me->mloopuv[li1];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                    ic = vi1;
                                    if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-2] = ic2;

                                    uv = &me->mloopuv[li2];
                                    pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                    ic = vi2;
                                    if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                        (*pinds)[pinds->size()-1] = ic2;
                                }
                                // -----------------------------------------------------------------------

                                if (isQuad) {
                                    if (poly.loopstart + currentLastVertPos>=me->totloop) {
                                        fbtPrintf("me->mtpoly ERROR: poly.loopstart + currentLastVertPos(%d)>=me->totloop(%d)\n",poly.loopstart + currentLastVertPos,me->totloop);
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


                                    if (mtp) {
                                        hasTexCoords = true;
                                        const MLoopUV* uv = &me->mloopuv[li0];
                                        pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                        ic = vi0;
                                        if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                            (*pinds)[pinds->size()-3] = ic2;

                                        uv = &me->mloopuv[li1];
                                        pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                        ic = vi1;
                                        if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
                                            (*pinds)[pinds->size()-2] = ic2;

                                        uv = &me->mloopuv[li2];
                                        pBlenderTexture->transformTexCoords(uv->uv[0],uv->uv[1],tc);
                                        ic = vi2;
                                        if ( (ic2=BlenderHelper::AddTexCoord(ic,tc,firstMesh,numTexCoordAssignments,texCoordsSingleVertsVertsMultiMap))!=ic)
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
                std::vector < Vector2 > empty;
                texCoords.swap(empty);
            }
            //------------------------------------------------------------

            // TESTING STUFF----------------------------------------------
            //if (ob->adt) BlenderHelper::Display(ob->adt);   // Blender::AnimData
            //ob->data.shapeKeys ???
            /*
            I can access the shape key with mesh.shape_keys.key_blocks[x]. But how do i get it's data (the vertex locations) and how can i manipulate this data?  i only see data as an unkown type.
            data is what you looking for:

            >>> bpy.context.object.data.shape_keys.key_blocks['Key 1'].data[0].co
            Vector((0.7661620378494263, 0.16031043231487274, 0.7497548460960388))

            data index should equal the vertex index of the mesh.
            In other words. It isn't part of the API, but still works and is just a list of MeshVertex Objects.
            */
            //-----------
            #ifdef NEVER
            /*
            printf("me->edata.totsize: %d\n",me->edata.totsize);
            printf("me->fdata.totsize: %d\n",me->fdata.totsize);
            printf("me->ldata.totsize: %d\n",me->ldata.totsize);
            printf("me->vdata.totsize: %d\n",me->vdata.totsize);
            */
            if (ob->effect.first) printf("\tob->effect present\n");
            if (ob->hooks.first) printf("\tob->hooks present\n");
            if (ob->constraintChannels.first) fbtPrintf("\tob->constraintChannels present\n");
            if (ob->constraints.first) fbtPrintf("\tob->constraints present\n");
            if (ob->nlastrips.first) fbtPrintf("\tob->nlastrips present\n");
            if (ob->action) fbtPrintf("\tob->action present\n");
            if (ob->ipo) BlenderHelper::Display(ob->ipo);
            if (ob->poselib) fbtPrintf("\tob->poselib present\n");
            if (ob->pose) {
                fbtPrintf("\to.pose present\n");	// Important in armatures
                fbtPrintf("\tSTART POSE\n");
                /*
        struct bPose	{
            ListBase chanbase;
            void *chanhash;
            short flag;
            short pad;
            int proxy_layer;
            int pad1;
            float ctime;
            float stride_offset[3];
            float cyclic_offset[3];
            ListBase agroups;
            int active_group;
            int iksolver;
            void *ikdata;
            void *ikparam;
            bAnimVizSettings avs;
            char proxy_act_bone[32];
        };
        */
                const bPose& p = *ob->pose;
                if (p.chanhash) fbtPrintf("\tchanhash present\n");
                fbtPrintf("\tflag: %1d\n", p.flag);			//
                fbtPrintf("\tpad: %1d\n", p.pad);			//
                fbtPrintf("\tproxy_layer: %1d\n", p.proxy_layer);			//
                fbtPrintf("\tpad1: %1d\n", p.pad1);

                fbtPrintf("\tctime: %1.4f\n", p.ctime);			//
                fbtPrintf("\tstride_offset: (%1.4f,%1.4f,%1.4f)\n", p.stride_offset[0],p.stride_offset[1],p.stride_offset[2]);
                fbtPrintf("\tcyclic_offset: (%1.4f,%1.4f,%1.4f)\n", p.cyclic_offset[0],p.cyclic_offset[1],p.cyclic_offset[2]);

                fbtPrintf("\tactive_group: %1d\n", p.active_group);
                fbtPrintf("\tiksolver: %1d\n", p.iksolver);
                if (p.ikdata) fbtPrintf("\tikdata present\n");
                if (p.ikparam) fbtPrintf("\tikparam present\n");
                fbtPrintf("\tproxy_act_bone: %s\n", p.proxy_act_bone);			//
                if (p.chanbase.first) fbtPrintf("\tchanbase pose channels present\n", p.proxy_act_bone);
                int cnt=0;
                BlenderHelper::Display((const bPoseChannel*) p.chanbase.first,cnt);
                fbtPrintf("\tEND POSE\n");
            }
            #endif //NEVER
            //------------------------------------------------------------

            // APPLY MIRROR MODIFIER ----------------------------------
            if (mirrorModifier.enabled) {
                // VERTS ----------------------------------------------
                verts.resize(numVerts);
                norms.resize(numVerts);
                int startNumVerts = numSingleVerts;
                if (mirrorModifier.axisX) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vector3& v = verts[i];
                        verts[startNumVerts+i] = Vector3(-v[0],v[1],v[2]);
                        const Vector3& n = norms[i];
                        norms[startNumVerts+i] = Vector3(-n[0],n[1],n[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisY) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vector3& v = verts[i];
                        verts[startNumVerts+i]= Vector3(v[0],-v[1],v[2]);
                        const Vector3& n = norms[i];
                        norms[startNumVerts+i] = Vector3(n[0],-n[1],n[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vector3& v = verts[i];
                        verts[startNumVerts+i]=  Vector3(v[0],v[1],-v[2]);
                        const Vector3& n = norms[i];
                        norms[startNumVerts+i] = Vector3(n[0],n[1],-n[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisX && mirrorModifier.axisY) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vector3& v = verts[i];
                        verts[startNumVerts+i]=  Vector3(-v[0],-v[1],v[2]);
                        const Vector3& n = norms[i];
                        norms[startNumVerts+i] = Vector3(-n[0],-n[1],n[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisX && mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vector3& v = verts[i];
                        verts[startNumVerts+i]=  Vector3(-v[0],v[1],-v[2]);
                        const Vector3& n = norms[i];
                        norms[startNumVerts+i] = Vector3(-n[0],n[1],-n[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisY && mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vector3& v = verts[i];
                        verts[startNumVerts+i]=  Vector3(v[0],-v[1],-v[2]);
                        const Vector3& n = norms[i];
                        norms[startNumVerts+i] = Vector3(n[0],-n[1],-n[2]);
                    }
                    startNumVerts += numSingleVerts;
                }
                if (mirrorModifier.axisX && mirrorModifier.axisY && mirrorModifier.axisZ) {
                    for (int i=0;i<numSingleVerts;i++) {
                        const Vector3& v = verts[i];
                        verts[startNumVerts+i]=  Vector3(-v[0],-v[1],-v[2]);
                        const Vector3& n = norms[i];
                        norms[startNumVerts+i] = Vector3(-n[0],n[1],n[2]);
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
                            const Vector2& tc = texCoords[i];
                            if (invertFace) texCoords[startNumVerts+i] =  mirrorModifier.mirrorUV(tc);
                            else  texCoords[startNumVerts+i] =  tc;
                        }
                        startNumVerts += numSingleVerts;
                    }
                }

                // VERTEX DEFORM GROUPS--------------------------------
                if (numVerts == 2*numSingleVerts && m_boneInfos.size()>0) {
                    int startNumVerts = numSingleVerts;
                    std::vector < BoneIdsType >& boneIndices = firstMesh.boneIds;
                    std::vector < BoneWeightsType >& boneWeights = firstMesh.boneWeights;
                    boneIndices.resize(startNumVerts+numSingleVerts);
                    boneWeights.resize(startNumVerts+numSingleVerts);
                    for (int i=0;i<numSingleVerts;i++) {
                        const BoneIdsType& bi = boneIndices[i];
                        BoneIdsType& biNew = boneIndices[startNumVerts+i];
                        for (int j=0;j<4;j++) biNew[j] = m_boneInfos[bi[j]].indexMirror;
                        boneWeights[startNumVerts+i] = boneWeights[i];
#ifdef DEBUG_BONE_INDICES_AND_WEIGHTS
                        for (int j=0;j<4;j++)   {
                            if (boneWeights[startNumVerts+i][j]>0)    {
                                if (j==0) printf("%d)   ",startNumVerts+i);
                                printf("    [w(%d:%s) = %1.6f]",boneIndices[startNumVerts+i][j],m_boneInfos[boneIndices[startNumVerts+i][j]].boneName.c_str(),boneWeights[startNumVerts+i][j]);
                            }
                            if (j==3) printf("\n");
                        }
#undef DEBUG_BONE_INDICES_AND_WEIGHTS
#endif //DEBUG_BONE_INDICES_AND_WEIGHTS

                    }
                }
                else fprintf(stderr,"Error: the mirror modifier in a skinned object must mirror a single component only, otherwise I don't know how to match the right and left bones correctly.\n");
                // INDS -----------------------------------------------
                for (int i=startObjectIndex,sz = (int) meshParts.size();i<sz;i++)   {
                    Part& m = meshParts[i];
                    IndexArray& inds = m.inds;
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
                        int i0,i1,i2;
                        for (int i=0;i<numSingleInds;i+=3)
                        {
                            if (i+2>=numSingleInds) break;
                            if (invertFace) {i0=i;i1=i+2;i2=i+1;}
                            else            {i0=i;i1=i+1;i2=i+2;}

                            inds.push_back(startNumVerts + inds[i0]);
                            inds.push_back(startNumVerts + inds[i1]);
                            inds.push_back(startNumVerts + inds[i2]);

                        }
                        //startNumInds += numSingleInds;
                        startNumVerts +=numSingleVerts;
                    }
                }
                //-----------------------------------------------------
            }
            //---------------------------------------------------------

            // TRANSFORM VERTS -----------------------------------------
            Mat4 T(1);Mat3 rot(1);
            #ifdef ALLOW_GLOBAL_OBJECT_TRANSFORM
            Vector3 tra,sca;
            GetObjectTransform(ob,rot,tra,sca);
            glm::FromRotScaTra(T,rot,sca,tra);
//#           define DEBUG_HERE
#           ifdef DEBUG_HERE
            fprintf(stderr,"tra[%1.3f,%1.3f,%1.3f] sca[%1.3f,%1.3f,%1.3f] rot{[%1.3f,%1.3f,%1.3f],[%1.3f,%1.3f,%1.3f],[%1.3f,%1.3f,%1.3f]}\n",tra[0],tra[1],tra[2],sca[0],sca[1],sca[2],rot[0][0],rot[0][1],rot[0][2],rot[1][0],rot[1][1],rot[1][2],rot[2][0],rot[2][1],rot[2][2]);
            fprintf(stderr,"[%1.3f,%1.3f,%1.3f,%1.3f]\n",T[0][0],T[0][1],T[0][2],T[0][3]);
            fprintf(stderr,"[%1.3f,%1.3f,%1.3f,%1.3f]\n",T[1][0],T[1][1],T[1][2],T[1][3]);
            fprintf(stderr,"[%1.3f,%1.3f,%1.3f,%1.3f]\n",T[2][0],T[2][1],T[2][2],T[2][3]);
            fprintf(stderr,"[%1.3f,%1.3f,%1.3f,%1.3f]\n",T[3][0],T[3][1],T[3][2],T[3][3]);
#           undef DEBUG_HERE
#           endif //DEBUG_HERE
            #endif //ALLOW_GLOBAL_OBJECT_TRANSFORM
            if (invertYZ)
            {
                // We need to switch axis convention of the transform
                T = BlenderHelper::GetInvYZMatrix4() * T;
                rot = BlenderHelper::GetInvYZMatrix3() * rot;
            }

            // Here we apply the transform T
            for (int i=0;i<numVerts;i++) {
                Vector3& v = verts[i];
                v = Vector3(T*Vector4(v[0],v[1],v[2],1));
                Vector3& n = norms[i];
                n = rot * n;
            }
            //----------------------------------------------------------

            // delete unused mesh parts --------------------------------
            for (int i=startObjectIndex;i<(int)meshParts.size();i++)   {
                Part& m = meshParts[i];
                if (m.verts.size()==0 && m.inds.size()==0) {
                    for (int j = i;j<(int)meshParts.size()-1;j++)   {
                        meshParts[j] = meshParts[j+1];  // terribly slow and not-reliable...
                    }
                    i--;
                    meshParts.resize(meshParts.size()-1);
                }
            }
            //----------------------------------------------------------
            ++numObjectsLoaded;
        }
    }
    catch (...) {
        printf(".blend parsing error: can't load file %s Exception thrown\n\n",filePath);
        meshParts.resize(startPartIndex);
        return false;
    }

    if (numObjectsLoaded==0) {
        meshParts.resize(startPartIndex);
        return false;
    }

    return true;
}


























