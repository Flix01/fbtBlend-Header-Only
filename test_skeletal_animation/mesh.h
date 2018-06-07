#ifndef MESH_H__
#define MESH_H__
#pragma once


//#include <glm/glm.hpp>
//#include <glm/ext.hpp>                  // glm::perspective(...), glm::value_ptr(...),etc.
//#include <glmMissingMethods.hpp>        // plain glm is not enough (Ogre Math would probably be enough...)

#include <math_helper.hpp>

// GL types (and calls) are be included by this header-----
#include "opengl_includer.h"
#include "openGLTextureShaderAndBufferHelpers.h"

#include <vector>
using std::vector;

/*#include <memory>       // shared_ptr needs C++11 (is there some way to avoid it?)
using std::shared_ptr;*/  // Probably without C++11 we can't use "using" with template classes (but this issue can be solved using multiple statements)
#include "shared_ptr.hpp"   // stand-alone version (no boost or C++11 required)
//#define MESH_H_STD_MAP std::unordered_map
//#include <unordered_map>      // Faster, but needs C++11
#define MESH_H_STD_MAP std::map
#include <map>                  // Slower, but no C++11
typedef MESH_H_STD_MAP<std::string,unsigned > MapStringUnsigned;


struct Material	{
	float amb[4];
	float dif[4];
	float spe[4];
	float shi;	// In [0,1], not in [0,128]
	float emi[4];
	Material();	
	Material(float r,float g,float b,bool useSpecularColor = true,float ambFraction=0.4f,float speAddon=0.4f,float shininessIn01=(4.0f/128.0f));	
	// TODO: add ctrs for known colors
	void bind() const;	// openGL method: btTriangleMesh.h/cpp shouldn't include gl stuff, we'll define it in another .cpp file.		
	void lighten(float q=0.2f);
	void darken(float q=0.2f);
	void clamp();	// all in [0,1]
	void reset();
	//Uses printf
	void displayDebugInfo() const;
};



typedef glm::vec4 Vector4;
typedef vector<Vector4> Vector4Array;
typedef glm::vec3 Vector3;
typedef vector<Vector3> Vector3Array;
typedef glm::vec2 Vector2;
typedef vector<Vector2> Vector2Array;
#ifndef __EMSCRIPTEN__
typedef unsigned int IndexType;
#else //__EMSCRIPTEN__
typedef unsigned short IndexType;
#endif //__EMSCRIPTEN__
typedef vector<IndexType> IndexArray;

typedef glm::quat Quaternion;
typedef glm::mat3 Matrix3;
typedef glm::mat4 Matrix;
typedef float Scalar;

#ifdef GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
typedef glm::uvec4 BoneIdsType;
#else //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
typedef glm::vec4 BoneIdsType;
#endif //GLMINITRIANGLEBATCH_INTEGER_BONE_INDICES
typedef glm::vec4 BoneWeightsType;

typedef glm::mat4 Mat4;
typedef glm::mat3 Mat3;

#include <string>
typedef std::string string;

class Mesh;
class MeshInstance	{
	public:
	static shared_ptr < MeshInstance > CreateFrom(shared_ptr < Mesh > meshPtr);
	shared_ptr <MeshInstance> clone() const {shared_ptr<MeshInstance> p(new MeshInstance(parentMesh));*p=*this; return p;}	

	void shift(const Vector3& v);						// Rotation agnostic
	void scale(const Vector3& s);						// Rotation agnostic
	void rotate(Scalar degAngle,const glm::vec3& axis);	
    void resetShiftScaleRotate();

	// These methods are ROTATION AGNOSTIC: i.e. are not affected by any rotation you make
	Vector3 center();
	Vector3 getHalfExtents() const;
	Vector3 getHalfFullExtents() const;
	Vector3 getCenterPoint() const;
	// These methods are ROTATION AWARE: i.e. y is always the vertical axis (it can be changed by a previous rotation)
	Vector3 setHeight(Scalar value);
	Vector3 setWidth(Scalar value);
	Vector3 setLength(Scalar value);
	Vector3 setFullExtents(const Vector3& extents);
	Vector3 setHalfExtents(const Vector3& extents);
	//--------------------------------------------------------------------------------------------

	// Just fills the currentPose member:
	void setPose(unsigned animationIndex,float TimeInSeconds,float additionalPoseTransitionTime = 1.0f);		
	void resetPose(unsigned animationIndex,int rotationKeyFrameNumber=0,int transitionKeyFrameNumber=0,int scalingKeyFrameNumber=0);
	void setBindingPose();
	void manuallySetPoseToBone(unsigned boneIndex,const glm::mat4& bonePose,bool applyPoseOverCurrentBonePose=false,bool bonePoseIsInLocalSpace=true);
    void displaySkeleton(const glm::mat4& vpMatrix) const;

    //#ifndef TEST_SOFTWARE_SKINNING
    void render(const OpenGLSkinningShaderData& p, const GLfloat *vMatrix=NULL, int meshPartIndex=-1) const;
    inline void render(const OpenGLSkinningShaderData& p, const glm::mat4& vMatrix, int meshPartIndex=-1) const {render(p,glm::value_ptr(vMatrix),meshPartIndex);}
    //#endif //TEST_SOFTWARE_SKINNING
    #ifdef TEST_SOFTWARE_SKINNING
    void softwareRender(const GLfloat *vMatrix=NULL, int meshPartIndex=-1,bool autoUpdateVertsAndNorms=true) const;
    inline void softwareRender(const glm::mat4& vMatrix,int meshPartIndex=-1,bool autoUpdateVertsAndNorms=true) const {softwareRender(glm::value_ptr(vMatrix),meshPartIndex,autoUpdateVertsAndNorms);}
    protected:
    void softwareUpdateVertsAndNorms(); // exposed through 'autoUpdateVertsAndNorms' in the methods above this.
    public:
    #endif //TEST_SOFTWARE_SKINNING


	// use it to feed the shader:
	const vector < glm::mat4 >& getCurrentPoseTransforms() const {return currentPose;}

	// The following 2 members are here for convenience: the user should set up a
	// shader environment to properly use them (e.g. modelT *= translationAndRotationOffset, and pass scalingOffset as uniform if scaling is supported)
    // Update: when using the render(...) method with the valid shader these 2 are set automatically
    const glm::mat4& getTranslationAndRotationOffset() const {return translationAndRotationOffset;}
    const glm::vec3& getScalingOffset() const {return scalingOffset;}	// available only for shaders that supports it (shader with separeted uniform value scaling).

    // position
    const GLfloat* glModelMatrixGet() const {return glm::value_ptr(mMatrix);}
    GLfloat* glModelMatrixGet() {return glm::value_ptr(mMatrix);}
    const glm::mat4& getModelMatrix() const {return mMatrix;}
    glm::mat4& getModelMatrix() {return mMatrix;}

    // Look customization:
    vector < Material* >& getMaterialOverrides() {return materialOverrides;}    // overridden materials must be persistent
    vector < GLuint >& getTextureOverrides() {return textureOverrides;}
    vector<bool>&   getVisibilityOverrides() {return visibilityOverrides;}

    size_t getNumMeshParts() const;

/*	
	struct AnimationSetter {
		float animationTransitionTime;	// the time (seconds) needed to go from currentPose to currentAnimationKeys[0].value is: animationTransitionTime + currentAnimationKeys[0].time
										// this increases the effective currentAnimationTime of "animationTransitionTime" seconds.
		float animationSpeed;
		unsigned animationIndex;
		float AnimationStartTimeInTicks;
		int boneIndex;	// -1 == all bones
		public:
		AnimationSetter(unsigned _animationIndex=0,float _animationTransitionTime=float(1.0),float _animationSpeed=float(1.0),int boneIndex=-1);
		void setAnimationTransitionTime(float value) {animationTransitionTime = value;}
		const float& getAnimationTransitionTime() const {return animationTransitionTime;}
		float& getAnimationTransitionTime() {return animationTransitionTime;}
		void setBoneIndex(int _boneIndex=-1)	{boneIndex=_boneIndex;}
		const int getBoneIndex() const {return boneIndex;}
		friend class MeshInstance;
		friend class Mesh;
	};    

	void setPose(AnimationSetter& as,float TimeInSeconds);		
*/
	/*
	void setPose(AnimationSetter& as,float TimeInSeconds);	
	void startAnimation(AnimationSetter& as,float TimeInSeconds);
	void pauseAnimation(AnimationSetter& as,float TimeInSeconds);
	void stopAnimation(AnimationSetter& as);
	void resetAnimation(MeshInstancePtr mi);
	*/
	protected:	
	MeshInstance(shared_ptr < class Mesh > _parentMesh);
	// The following 2 members are here for convenience: the user should set up a
	// shader environment to properly use them (e.g. modelT *= translationAndRotationOffset, and pass scalingOffset as uniform if scaling is supported)
    glm::mat4 mMatrix;
    glm::mat4 translationAndRotationOffset;	// must be orthonormal
	glm::vec3 scalingOffset;				
	// --------------------------------------------------------------------------------	
	
	vector < glm::mat4 > currentPose;		
	shared_ptr < class Mesh > parentMesh;
	struct BoneFootprint{
		//------- Current Bone Pose ------------------------------------------------
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scaling;
		//--------------------------------------------------------------------------
        BoneFootprint()	: translation(0,0,0),rotation(1,0,0,0),scaling(1,1,1),
		currentAnimationIndex(0),startTimeInTicksOfCurrentAnimationIndex(-1.0f),
        translationPoseTransitionTime(0),rotationPoseTransitionTime(0),scalingPoseTransitionTime(0)
        //,translationAtStart(0,0,0),rotationAtStart(1,0,0,0),scalingAtStart(1,1,1)
		 {}
		void reset() {*this = BoneFootprint();}	
		//--------------------------------------------------------------------------
		int currentAnimationIndex;							// unused
        float startTimeInTicksOfCurrentAnimationIndex;		// unused
		//--------------------------------------------------------------------------	
        float translationPoseTransitionTime,rotationPoseTransitionTime,scalingPoseTransitionTime;   // to remove
        // TODO:for fixing the initial 'blending' time-----------------------------------
        /*glm::vec3 translationAtStart;
        glm::quat rotationAtStart;
        glm::vec3 scalingAtStart;
        void initializeInitialBlending() {
            translationAtStart = translation;
            rotationAtStart = rotation;
            scalingAtStart = scaling;
        }*/
	};	
	vector < BoneFootprint > boneFootprints;	//This should be transparent to the user
	void resetBoneFootprints();
	
    // look customization:
    vector < Material* > materialOverrides;     // overridden materials must be persistent
    vector < GLuint >   textureOverrides;
    vector<bool>        visibilityOverrides;


    #ifdef TEST_SOFTWARE_SKINNING
    struct MeshPartFootprint    {
        Vector3Array verts;
        Vector3Array norms;
    };
    vector<MeshPartFootprint> meshPartFootprints;
    // TODO: vertex array, VBO or VAO here...
    #endif //TEST_SOFTWARE_SKINNING

	friend class Mesh;
};
typedef shared_ptr < MeshInstance > MeshInstancePtr;

class Mesh	{
	public:
	struct Part {
		Vector3Array verts;	// if size()==0, inds refers to useVertexArrayInPartIndex if != -1 ot to the last "valid" verts in the previous subparts.
		Vector3Array norms;		// valid only if verts.size()==norms.size()
		Vector2Array texcoords;	// valid only if verts.size()==texcoords.size()
		vector< BoneIdsType > boneIds;
    	vector< BoneWeightsType > boneWeights;
        string meshName;    //optional

		IndexArray inds;
		int useVertexArrayInPartIndex;		// Used only if verts.size()==0, useVertexArrayInPartIndex must be lesser than the current subpart index
        int parentBoneId;                   // (.blend only) -1 by default. When set it means that all the verts are transformed only by this single Bone (just as a possible future optimization, since boneIds and bone Weights are still present and used instead)

		Material material;
        string materialName;        //optional
        //bf::GLTexturePtr textureId;
        GLuint textureId;
        string textureFilePath; //optional
        //fs::path textureFilePath;	//optional

        GLMiniTriangleBatchPtr batch;
		/*
		bf::GLBufferPtr vbo;
		bf::GLBufferPtr ibo;
		
		bf::GLArrayBuffer vao;
		*/
        void generateVertexArrays(const Mesh& parentMesh);	// Not actually used with TEST_SOFTWARE_SKINNING
		bool visible;
		
		Part();
		Part(int _useVertexArrayInPartIndex);
		~Part();
		protected:		
		void reset();
		void normalizeBoneWeights();	
    	bool addBoneWeights(size_t vertsId,unsigned int boneId,GLfloat boneWeight);	
    	friend class Mesh;	
	};	
	typedef vector < Part > MeshPartVector;
	
    static shared_ptr < Mesh > Create(/*bool _uselocalTextureMapCollector=false*/);
	
	protected:
    Mesh(/*bool _uselocalTextureMapCollector=false*/);
	public:
	virtual ~Mesh();
	void reset();

    bool loadFromAnimatedBlendFile(const char* filePath,int flags=0);
    bool loadFromFile(const char* filePath, int flags=0);
	void generateVertexArrays(int meshPartIndex=-1);
    //#ifndef TEST_SOFTWARE_SKINNING
	void render(int meshPartIndex=-1) const;
    void render(const OpenGLSkinningShaderData& p, int meshPartIndex, const vector<Material*> *pOptionalMaterialOverrides=NULL, const vector<GLuint> *pOptionalTextureOverrides=NULL,const vector<bool>* pOptionalVisibilityOverrides=NULL) const;
    //#endif //TEST_SOFTWARE_SKINNING
	// The following actions are not persistent, but instantaneous.
	// optionalSubpartIndex!=-1 has undefined results if meshes[optionalSubpartIndex] does not own his own vertex array
	Vector3 center(int  optionalMeshPartIndex=-1);	// (returns the old center)
	Vector3 setHeight(Scalar height,int  optionalMeshPartIndex=-1);		// Y axis (returns the applied scaling factor)
	Vector3 setWidth(Scalar width,int  optionalMeshPartIndex=-1);		// X axis (returns the applied scaling factor)
	Vector3 setLength(Scalar length,int  optionalMeshPartIndex=-1);		// Z axis (returns the applied scaling factor)	
	Vector3 setFullExtents(const Scalar& extents,int  optionalMeshPartIndex=-1); // (returns the applied scaling factor)
	Vector3 setHalfExtents(const Scalar& extents,int  optionalMeshPartIndex=-1); // (returns the applied scaling factor)
	void shift(const Vector3& shiftFactor,int optionalMeshPartIndex=-1);
	void scale(const Vector3& scaling,int optionalMeshPartIndex=-1);
	void rotate(const Matrix3& m,int optionalMeshPartIndex=-1);
	void rotate(const glm::quat& q,int optionalMeshPartIndex=-1);
	void transform(const glm::mat4& T,int optionalMeshPartIndex=-1); 
	
	const Vector3 getAabbHalfExtents(int optionalMeshPartIndex=-1) const;
	const Vector3 getAabbFullExtents(int optionalMeshPartIndex=-1) const;
	const Vector3 getCenterPoint(int optionalMeshPartIndex=-1) const;
	int getIndexOfBoneContainingName(const string& name) const;
	bool getVerticesBelongingToBone(unsigned boneIndex,vector<Vector3>& vertsOut,int optionalMeshPartIndex=-1) const;

	//very big memory copy... slow
	Mesh(const Mesh& c);
	//very big memory copy... slow
	const Mesh& operator=(const Mesh& c);

	//Uses printf
	void displayDebugInfo(bool vertices=true,bool materials=true,bool bones=true,bool animations=true) const;
	
	
		
	const MeshPartVector& getMeshParts() const {return meshParts;}	
    const Part* getMeshPart(int i) const {if (i>=0 && i<(int)meshParts.size()) return &meshParts[i];else return NULL;}
    const Material* getMeshPartMaterial(int i) const {if (i>=0 && i<(int)meshParts.size()) return &meshParts[i].material;else return NULL;}
    Material* getMeshPartMaterial(int i) {if (i>=0 && i<(int)meshParts.size()) return &meshParts[i].material;else return NULL;}

    void setMeshPartMaterial(int i,const Material& mat) {if (i>=0 && i<(int)meshParts.size())  meshParts[i].material = mat;}
    GLuint getMeshPartTexture(int i) const {if (i>=0 && i<(int)meshParts.size()) return meshParts[i].textureId;else return 0;}
    void setMeshPartTexture(int i,GLuint textureId) {if (i>=0 && i<(int)meshParts.size())  meshParts[i].textureId = textureId;}
/*
    GLuint getMeshPartTextureId(int i) const {return ((i>=0 && i<meshParts.size() && meshParts[i].texture) ? (GLuint)(*(meshParts[i].texture)) : 0);}
    bf::GLTexturePtr getMeshPartTexture(int i) const {return ((i>=0 && i<meshParts.size()) ? meshParts[i].texture : bf::GLTexturePtr());}
    void setMeshPartTexture(int i,bf::GLTexturePtr texture) {if (i>=0 && i<meshParts.size())  meshParts[i].texture = texture;}
*/
    int getNumMeshParts() const {return (int)meshParts.size();}	// Equivalent to getNumMeshParts(-1);
	
	
	int getNumMeshParts(int optionalPartIndex) const;
    int getBaseMeshPartIndex(int optionalPartIndex) const	;
	vector< int > getMeshPartIndices(int optionalPartIndex) const;
	Vector3 GetAabbHalfExtents(Vector3* pOptionalCenterPointOut,int optionalPartIndex) const;
	Vector3 setFullExtents(const Vector3& extents,int  optionalPartIndex);
	Vector3 setHalfExtents(const Vector3& extents,int  optionalPartIndex);
	void GetAabbEdgePoints(Vector3& fullExts,Vector3& centerPoint,vector<Vector3>& aabbEdge6Vertices,const Vector3* pAabbFullExtsToScan=NULL,int _optionalPartIndex=-1) const;	// If pAabbToScan!=NULL, centerPoint ia an In-Out argument.
	
	void getBoneTransforms(unsigned animationIndex,float TimeInSeconds, vector<glm::mat4>& Transforms);		
	void getBindingPoseBoneTransforms(vector<glm::mat4>& Transforms);
	void getBoneTransforms(unsigned animationIndex,float TimeInSeconds,float additionalPoseTransitionTime, MeshInstance& mi);
	void getBoneTransformsForPose(unsigned animationIndex,int rotationKeyFrameNumber,int translationKeyFrameNumber,int scalingKeyFrameNumber, MeshInstance& mi);
	
	void displaySkeleton(const glm::mat4& mvpMatrix,const vector<glm::mat4>* pTransforms=NULL,const glm::mat4* pOptionalTranslationAndRotationOffset=NULL,const glm::vec3* pOptionalScaling=NULL) const;
	void manuallySetPoseToBone(unsigned boneIndex,const glm::mat4& bonePose,vector<Mat4>& Transforms,bool applyPoseOverCurrentBonePose=false,bool bonePoseIsInLocalSpace=false) const;
	
	
	unsigned getNumBones() const {return numValidBones;/*m_boneInfos.size();*/}
	unsigned getNumExtraDummyBones() const {return m_boneInfos.size()-numValidBones;}		
    //unsigned getNumTotalBones() const {return m_boneInfos.size();}
    bool isSkeletalAnimated() const {return getNumBones()>0;}
	unsigned getNumAnimations() const {return m_animationInfos.size();}
	string getAnimationName(unsigned i) const {return i<m_animationInfos.size()?m_animationInfos[i].name:"";}
	float getAnimationLength(unsigned i) const {return i<m_animationInfos.size()?m_animationInfos[i].duration:0;}
	int getAnimationIndex(const string& name) const {
        MapStringUnsigned::const_iterator it=m_animationIndexMap.find(name);
		if (it!=m_animationIndexMap.end()) return (int)it->second;
		return -1;	
	}

    void setPlayAnimationAgainWhenOvertime(bool flag) {m_playAnimationsAgainWhenOvertime = flag;}
    bool getPlayAnimationsAgainWhenOvertime() {return m_playAnimationsAgainWhenOvertime;}

	protected:
	MeshPartVector meshParts;	// I should use <btMeshPart*>, but I want to keep things simple

	// Bone stuff
    MapStringUnsigned m_boneIndexMap; // maps a bone name to its index
    public:
    struct BoneInfo	{
        unsigned index;		// inside m_BoneInfos vector (bad design)
        unsigned indexMirror;   // .blend only (when not set is == index)
        Mat4 boneOffset;	//Problem: this depends on the mesh index (although I never experienced any problems so far)
        Mat4 boneOffsetInverse;	//Never used, but useful to display skeleton (from bone space to global space)
        //Mat4 globalSpaceBoneOffset;	//Never used
 		bool isDummyBone;	// Dummy bones have boneOffset =  boneOffsetInverse = mat4(1), and are not counted in getNumBones()
 		bool isUseless;		// Dummy nodes can be marked as useless they could be erased by m_boneInfos, but it's too difficult to do it (see the references stored below)
 		
 		//---new--------------------------
 		Mat4 preTransform;			// Must always be applied (if preTransformIsPresent == false) before all other transforms
 		bool preTransformIsPresent;	// When false preTransform = glm::mat(1)
        Mat4 preAnimationTransform;				// When animation keys are simplyfied some of them (usually translation only) are inglobed in this transform.
        bool preAnimationTransformIsPresent;
        Mat4 multPreTransformPreAnimationTransform;	// just to speed up things a bit               
        Mat4 postAnimationTransform;				// When animation keys are simplyfied some of them (usually rotation and scaling) are inglobed in this transform.
        bool postAnimationTransformIsPresent;
        
        enum EulerRotationMode {
        EULER_XYZ = 1,  // 1
        EULER_XZY = 2,  // 2
        EULER_YXZ = 3,  // 3
        EULER_YZX = 4,  // 4
        EULER_ZXY = 5,  // 5
        EULER_ZYX = 6  // 6
        //AXIS_ANGLE = -1       // (these are used by Blender too)
        //QUATERNION_WXYZ = 0
        };
        int eulerRotationMode;  // Optional (default=0);
        
        Mat4 transform;				// Must be used when no animation is present, and replaced by the BoneAnimation combined transform otherwise.
        struct BoneAnimationInfo	{
				struct Vector3Key	{
					glm::vec3 value;
					float time;
				};					
				struct QuaternionKey	{
					glm::quat value;
					float time;
				};	
				enum BehaviorEnum	{
					AB_DEFAULT = 0,		//The value from the default node transformation is taken. 
					AB_CONSTANT = 1,	//The nearest key value is used without interpolation. 
                    AB_LINEAR = 2,		//The value of the nearest two keys is linearly extrapolated for the current time value.
					AB_REPEAT = 3,		//The animation is repeated. 
										//If the animation key go from n to m and the current time is t, use the value at (t-n) % (|m-n|). 
					AB_COUNT
				};
				vector <Vector3Key> translationKeys;
				vector <QuaternionKey> rotationKeys;
				vector <Vector3Key> scalingKeys;	
				bool tkAndRkHaveUniqueTimePerKey;
				bool tkAndSkHaveUniqueTimePerKey;
				bool rkAndSkHaveUniqueTimePerKey;
				BehaviorEnum preState;	//Defines how the animation behaves before the first key is encountered. 
				BehaviorEnum postState;	//Defines how the animation behaves after the last key was processed. 
                void reset() {
                    translationKeys.clear();rotationKeys.clear();scalingKeys.clear();
                    tkAndRkHaveUniqueTimePerKey = tkAndSkHaveUniqueTimePerKey = rkAndSkHaveUniqueTimePerKey = false;
                    preState = postState = AB_DEFAULT;
                }
        };
        typedef MESH_H_STD_MAP<unsigned,  BoneAnimationInfo> MapUnsignedBoneAnimationInfo;
        MapUnsignedBoneAnimationInfo boneAnimationInfoMap;
		GLM_INLINE const BoneAnimationInfo* getBoneAnimationInfo(unsigned animationIndex) const	{
            MapUnsignedBoneAnimationInfo::const_iterator it = boneAnimationInfoMap.find(animationIndex);
			return (it!=boneAnimationInfoMap.end() ? &it->second : NULL);
		}
		GLM_INLINE  BoneAnimationInfo* getBoneAnimationInfo(unsigned animationIndex) {
            MapUnsignedBoneAnimationInfo::iterator it = boneAnimationInfoMap.find(animationIndex);
			return (it!=boneAnimationInfoMap.end() ? &it->second : NULL);
		}
        //---------------------------------
        
        // WARNING: Here we store POINTERS to other elements of the SAME vector m_bneInfos:
        // This mean that the vector must stay the same (no grow, and no deletions of elements at all) all the time
        BoneInfo* parentBoneInfo;
        vector < BoneInfo* > childBoneInfos;

        Mat4 finalTransformation;
        string boneName;
    };

	typedef BoneInfo::BoneAnimationInfo BoneAnimationInfo;
	typedef BoneAnimationInfo::Vector3Key Vector3Key;
	typedef BoneAnimationInfo::QuaternionKey QuaternionKey;
	
	//typedef BoneAnimationInfo BoneAnimationInfo;
	//typedef BoneInfo BoneInfo;
    	
	// name is both Bone and Node name:
	const BoneInfo*	getBoneInfoFromName(const string& name) const {
        MapStringUnsigned::const_iterator it=m_boneIndexMap.find(name);
		if (it!=m_boneIndexMap.end()) return &m_boneInfos[it->second];
		return NULL;	
	}
	const BoneInfo*	getBoneInfoFromBoneIndex(unsigned boneIndex) const {return boneIndex < m_boneInfos.size() ? &m_boneInfos[boneIndex] : NULL;	}			    			   
    
    protected:
    vector< BoneInfo > m_boneInfos; 
    unsigned numValidBones;	// m_boneInfos.size() - numDummyBones;	Dummy bones are appended after numValidBones in m_boneInfos.
    vector <BoneInfo*> m_rootBoneInfos;
    	

    MapStringUnsigned m_animationIndexMap;
	struct AnimationInfo	{
		string name;
		float duration;
		float ticksPerSecond;
		float numTicks;
	};  
	vector< AnimationInfo > m_animationInfos; 
	
#   ifdef LOAD_MESHES_FROM_ASSET_IMPORT_LIBRARY
    void fillBoneInfoStructs(const struct aiNode* pNode,const struct aiScene* m_pScene,Mesh::BoneInfo* pParentBone=NULL,const glm::mat4& preTransform=glm::mat4(1),bool preTransformIsPresent=false);
    void getNumAiNodesValidBonesAndDummyBones(const struct aiNode* pNode,unsigned& an,const vector< string >& boneNames,unsigned& vb,unsigned& db,const struct aiScene* m_pScene);
#   endif //LOAD_MESHES_FROM_ASSET_IMPORT_LIBRARY


    bool m_playAnimationsAgainWhenOvertime;   // false, might be useful in some sort of 'IDE' (but animation that needs 'repeat mode' are handled in a different way)

 	unsigned CalcInterpolatedPosition(glm::vec3& Out, float AnimationTime, const BoneAnimationInfo& nasi, unsigned* pAlreadyKnownPositionIndex = NULL);
	unsigned CalcInterpolatedRotation(glm::quat& Out, float AnimationTime, const BoneAnimationInfo& nasi, unsigned* pAlreadyKnownRotationIndex = NULL);
	unsigned CalcInterpolatedScaling(glm::vec3& Out, float AnimationTime, const BoneAnimationInfo& nasi, unsigned* pAlreadyKnownScalingIndex = NULL);
	void ReadBoneHeirarchy(unsigned animationIndex,float AnimationTime, BoneInfo& ni, const glm::mat4& ParentTransform);
	void ReadBoneHeirarchyForBindingPose(BoneInfo& ni, const glm::mat4& ParentTransform);

    unsigned CalcInterpolatedPosition(float AnimationTime,float additionalPoseTransitionTime, const BoneAnimationInfo& nasi, MeshInstance::BoneFootprint& bf,unsigned* pAlreadyKnownPositionIndex,float* pAlreadyKnownFactor,float* pFactorOut,int looping,const AnimationInfo& ai);
    unsigned CalcInterpolatedRotation(float AnimationTime,float additionalPoseTransitionTime, const BoneAnimationInfo& nasi, MeshInstance::BoneFootprint& bf,unsigned* pAlreadyKnownPositionIndex,float* pAlreadyKnownFactor,float* pFactorOut,int looping,const AnimationInfo& ai);
    unsigned CalcInterpolatedScaling(float AnimationTime,float additionalPoseTransitionTime, const BoneAnimationInfo& nasi, MeshInstance::BoneFootprint& bf,unsigned* pAlreadyKnownPositionIndex,float* pAlreadyKnownFactor,float* pFactorOut,int looping,const AnimationInfo& ai);
    void ReadBoneHeirarchy(unsigned animationIndex,float AnimationTime,float additionalPoseTransitionTime,const BoneInfo& ni, const glm::mat4& ParentTransform, MeshInstance& mi,int looping,const AnimationInfo& ai);
	void ReadBoneHeirarchyForPose(unsigned animationIndex,int rotationKeyFrameNumber,int translationKeyFrameNumber,int scalingKeyFrameNumber,const BoneInfo& bi, const glm::mat4& ParentTransform, MeshInstance& mi);

    static bool AllDescendantsAreDummy(const BoneInfo& bi);
    static void PremultiplyBoneDescendants(const BoneInfo& parentBone,const Mat4& T,vector<Mat4>& Transforms,const vector <BoneInfo> m_boneInfos);

    #ifdef TEST_SOFTWARE_SKINNING
    void softwareRender(int meshPartIndex,const MeshInstance& mi) const;
    void softwareUpdateVertsAndNorms(MeshInstance& mi,const Vector3 &scaling=Vector3(1,1,1)) const;
    #endif //TEST_SOFTWARE_SKINNING


    friend class MeshInstance;
    friend class MeshArmatureSetter;
};	
typedef vector < Mesh::Part > MeshPartVector;
typedef shared_ptr < Mesh > MeshPtr;

#undef MESH_H_STD_MAP

#endif //MESH_H__
