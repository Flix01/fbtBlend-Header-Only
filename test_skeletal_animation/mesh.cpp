#include "mesh.h"
#include <assert.h>

#define MAX_NUMBER_SUBMESHES 50

#ifdef TEST_SOFTWARE_SKINNING
//-------------------------------------------------------------------------------------------------------------
//#define REQUIRE_GL_NORMALIZE  // Tweakable (needs glEnable(GL_NORMALIZE),but I'm not sure which is faster)
//-------------------------------------------------------------------------------------------------------------
#endif // TEST_SOFTWARE_SKINNING


//-------------------- Texture Loading Global Methods -----------------------------
typedef MapStringUnsigned TexturePath2IdMapType;
//typedef map<std::string,GLuint> TexturePath2IdMapType;        // More compatible but slightly slower

static TexturePath2IdMapType texturePath2IdMap; // This avoids loading the same image path twice
static vector<GLuint> textureGarbageCollector;  // This is used by DeleteAllLoadedTextures()

void DeleteAllLoadedTextures() {
    for (vector<GLuint>::iterator it = textureGarbageCollector.begin();it!=textureGarbageCollector.end();++it) {
        const GLuint id = *it;
        if (id) glDeleteTextures(1,&id);
    }
    textureGarbageCollector.clear();
    texturePath2IdMap.clear();
}

GLuint Mesh::WhiteTextureId = 0;
//---------------------------------------------------------------------------------


 //region Material
static void clampFloatIn01(float& f)	{
	f = (f<0 ? 0 : (f>1 ? 1 : f));
}
Material::Material()	{
	amb[0]=amb[1]=amb[2]=0.25f;amb[3]=1;
	dif[0]=dif[1]=dif[2]=0.85f;dif[3]=1;
	spe[0]=spe[1]=spe[2]=0.0f;spe[3]=1;
    shi = 0.f;//40.f/128.f;
	emi[0]=emi[1]=emi[2]=0.0f;emi[3]=1;
}
Material::Material(float r,float g,float b,bool useSpecularColor,float ambFraction,float speAddOn,float shininess)	{
	amb[0]=r*ambFraction;amb[1]=g*ambFraction;amb[2]=b*ambFraction;amb[3]=1;
	dif[0]=r;dif[1]=g;dif[2]=b;dif[3]=1;
	spe[3]=1;
	if (useSpecularColor)	{
		spe[0] = r + speAddOn;	clampFloatIn01(spe[0]);
		spe[1] = g + speAddOn;	clampFloatIn01(spe[1]);
		spe[2] = b + speAddOn;	clampFloatIn01(spe[2]);
		shi = shininess;		
	}
	else	{
		spe[0]=spe[1]=spe[2]=0;
		shi = 0.f;
	}
	emi[0]=emi[1]=emi[2]=0.0f;emi[3]=1;	
}	
void Material::reset() {*this = Material();}
void Material::bind() const	{
	#ifdef TAKE_IT_EASY
	glColor3fv(dif);
	#undef TAKE_IT_EASY
	#else //TAKE_IT_EASY
	#define GL_MY_MODE GL_FRONT
	//#define GL_MY_MODE GL_FRONT_AND_BACK
	
	glMaterialfv(GL_MY_MODE, GL_AMBIENT, amb);
	glMaterialfv(GL_MY_MODE, GL_DIFFUSE, dif);
	glMaterialfv(GL_MY_MODE, GL_SPECULAR, spe);
	glMaterialf(GL_MY_MODE, GL_SHININESS, (shi * 128.0f));
	glMaterialfv(GL_MY_MODE, GL_EMISSION, emi);
	
	#undef GL_MY_MODE
	#endif //TAKE_IT_EASY
}
void Material::displayDebugInfo() const	{
	printf("amb{%1.2f,%1.2f,%1.2f,%1.2f};\n",amb[0],amb[1],amb[2],amb[3]);
	printf("dif{%1.2f,%1.2f,%1.2f,%1.2f};\n",dif[0],dif[1],dif[2],dif[3]);
	printf("spe{%1.2f,%1.2f,%1.2f,%1.2f};\n",spe[0],spe[1],spe[2],spe[3]);
	printf("shi: %1.4f in[0,1];\n",shi);	
	printf("emi{%1.2f,%1.2f,%1.2f,%1.2f};\n",emi[0],emi[1],emi[2],emi[3]);
}
void Material::lighten(float q)	{
	for (int i=0;i<3;i++)	{
		amb[i]+=q;
		dif[i]+=q;
		spe[i]+=q;			
	}
	clamp();
}
void Material::darken(float q)	{
	for (int i=0;i<3;i++)	{
		amb[i]-=q;
		dif[i]-=q;
		spe[i]-=q;			
	}
	clamp();	
}
void Material::clamp()	{
	for (int i=0;i<4;i++)	{
		clampFloatIn01(amb[i]);
		clampFloatIn01(dif[i]);
		clampFloatIn01(spe[i]);
		clampFloatIn01(emi[i]);		
	}
	clampFloatIn01(shi);
}
//endregion

 //region MeshInstance
shared_ptr < MeshInstance > MeshInstance::CreateFrom(shared_ptr < Mesh > meshPtr)	{
	shared_ptr < MeshInstance > p;
	if (!meshPtr) return p;
	p = shared_ptr < MeshInstance >(new MeshInstance(meshPtr));
	if (p)	{
		p->translationAndRotationOffset = glm::mat4(1);
		p->scalingOffset = glm::vec3(1);
        p->mMatrix = glm::mat4(1);
		const size_t numValidBones = meshPtr->getNumBones();
		//printf("numValidBones = %d\n",numValidBones);		
		p->currentPose.resize(numValidBones);
		//for (size_t i=0;i<numValidBones;i++) p->currentPose[i] = p->translationAndRotationOffset;	//glm::mat(1)
		meshPtr->getBindingPoseBoneTransforms(p->currentPose);
		p->boneFootprints.resize(numValidBones);
        #ifdef TEST_SOFTWARE_SKINNING
        p->meshPartFootprints.resize(meshPtr->getNumMeshParts());
        for (size_t i=0,isz=p->meshPartFootprints.size();i<isz;i++) {
            const Mesh::Part& m = meshPtr->meshParts[i];
            MeshPartFootprint& f = p->meshPartFootprints[i];
            #ifdef USE_SOFWARE_RENDER_PARENT_BONE_ID_OPTIMIZATION
            #ifdef REQUIRE_GL_NORMALIZE
            if (m.parentBoneId>=0) continue;    // we'll handle this case later
            #endif //REQUIRE_GL_NORMALIZE
            #endif //USE_SOFWARE_RENDER_PARENT_BONE_ID_OPTIMIZATION

            f.verts = m.verts;
            f.norms = m.norms;
        }
        #endif //TEST_SOFTWARE_SKINNING
    }
	return p;
}
MeshInstance::MeshInstance(shared_ptr < class Mesh > _parentMesh) : parentMesh(_parentMesh) {}
void MeshInstance::setPose(unsigned animationIndex,float TimeInSeconds,float additionalPoseTransitionTime)	{
	//parentMesh->getBoneTransforms(animationIndex,TimeInSeconds,currentPose);
	parentMesh->getBoneTransforms(animationIndex,TimeInSeconds,additionalPoseTransitionTime,*this);
}	
void MeshInstance::resetPose(unsigned animationIndex,int rotationKeyFrameNumber,int translationKeyFrameNumber,int scalingKeyFrameNumber)	{
	parentMesh->getBoneTransformsForPose(animationIndex,rotationKeyFrameNumber,translationKeyFrameNumber,scalingKeyFrameNumber,*this);
}
void MeshInstance::setBindingPose()	{
	parentMesh->getBindingPoseBoneTransforms(currentPose);
}
void MeshInstance::manuallySetPoseToBone(unsigned boneIndex,const glm::mat4& bonePose,bool applyPoseOverCurrentBonePose,bool bonePoseIsInLocalSpace)	{
	parentMesh->manuallySetPoseToBone(boneIndex,bonePose,currentPose,applyPoseOverCurrentBonePose,bonePoseIsInLocalSpace);
}	
void MeshInstance::displaySkeleton(const glm::mat4 &vpMatrix) const	{
    parentMesh->displaySkeleton(vpMatrix*mMatrix,&currentPose,&translationAndRotationOffset,&scalingOffset);
}
void MeshInstance::shift(const Vector3& v)	{
    translationAndRotationOffset[3][0]+= scalingOffset[0]*v[0];
    translationAndRotationOffset[3][1]+= scalingOffset[1]*v[1];
    translationAndRotationOffset[3][2]+= scalingOffset[2]*v[2];
}
void MeshInstance::scale(const Vector3& s)	{
    translationAndRotationOffset[3][0]*= s[0];
    translationAndRotationOffset[3][1]*= s[1];
    translationAndRotationOffset[3][2]*= s[2];
	scalingOffset*=s;
}
void MeshInstance::rotate(Scalar degAngle,const glm::vec3& axis)	{
    translationAndRotationOffset = glm::rotate(glm::mat4(1),glm::radians(degAngle),axis)
            * translationAndRotationOffset;
}
void MeshInstance::resetShiftScaleRotate()  {
    translationAndRotationOffset = glm::mat4(1);
    scalingOffset = glm::vec3(1,1,1);
}
void MeshInstance::resetBoneFootprints() {
	for (size_t i=0,sz = boneFootprints.size();i<sz;i++) boneFootprints[i].reset();
}
/*
void MeshInstance::rotate(const glm::mat4& m)	{
	translationAndRotationOffset = m * translationAndRotationOffset;
}
*/

//#ifndef TEST_SOFTWARE_SKINNING
void MeshInstance::render(const OpenGLSkinningShaderData &p, const GLfloat *vMatrix, int meshPartIndex) const  {
    if (vMatrix) {
        const glm::mat4& _vMatrix = *((const glm::mat4*) vMatrix);    //Weird and dangerous hack..
        glUniformMatrix4fv(p.mvMatrixUnifLoc,1,GL_FALSE,glm::value_ptr(_vMatrix*mMatrix*translationAndRotationOffset));
    }
    glUniform3fv(p.mMatrixScalingUnifLoc,1,glm::value_ptr(scalingOffset));
    glUniformMatrix4fv(p.boneMatricesUnifLoc, getCurrentPoseTransforms().size(), GL_FALSE, glm::value_ptr(getCurrentPoseTransforms()[0]));
    parentMesh->render(p,meshPartIndex,&materialOverrides,&textureOverrides,&visibilityOverrides);
}
//#endif //TEST_SOFTWARE_SKINNING
#ifdef TEST_SOFTWARE_SKINNING
void MeshInstance::softwareRender(const GLfloat *vMatrix, int meshPartIndex,bool autoUpdateVertsAndNorms) const  {
    if (autoUpdateVertsAndNorms) const_cast<MeshInstance*>(this)->softwareUpdateVertsAndNorms();
    glPushMatrix();
    if (vMatrix) {
        const glm::mat4& _vMatrix = *((const glm::mat4*) vMatrix);    //Weird and dangerous hack..
        glMultMatrixf(glm::value_ptr(_vMatrix*mMatrix*translationAndRotationOffset));
    }
    else glMultMatrixf(glm::value_ptr(mMatrix*translationAndRotationOffset));
    #ifdef REQUIRE_GL_NORMALIZE
    glScalef(scalingOffset[0],scalingOffset[1],scalingOffset[2]);
    #endif //DONT_REQUIRE_GL_NORMALIZE

    parentMesh->softwareRender(meshPartIndex,*this);
    glPopMatrix();
}
void MeshInstance::softwareUpdateVertsAndNorms()    {
    parentMesh->softwareUpdateVertsAndNorms(*this
    //#ifndef REQUIRE_GL_NORMALIZE
    ,scalingOffset
    //#endif //REQUIRE_GL_NORMALIZE
    );
}
#endif //TEST_SOFTWARE_SKINNING

Vector3 MeshInstance::center()	{
	Vector3 center = parentMesh->getCenterPoint();
    const Vector3 oldCenter = center + glm::vec3(translationAndRotationOffset[3][0],translationAndRotationOffset[3][1],translationAndRotationOffset[3][2]);
	center/=scalingOffset;
	shift(-center);	
	return oldCenter;
}
Vector3 MeshInstance::setHeight(Scalar value)	{
    Vector3 ext = glm::mat3_cast(translationAndRotationOffset) * (parentMesh->getAabbHalfExtents() * Scalar(2.)/scalingOffset);
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[1]<checkZero) ext[1]=checkZero;
	const Scalar uniformScalingFactor = value/ext[1];
	Vector3 scalingFactor(uniformScalingFactor,uniformScalingFactor,uniformScalingFactor);
	scale(scalingFactor);
	return scalingFactor;
}
Vector3 MeshInstance::setWidth(Scalar value)	{
    Vector3 ext = glm::mat3_cast(translationAndRotationOffset) * parentMesh->getAabbHalfExtents() * Scalar(2.)/scalingOffset;
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[0]<checkZero) ext[0]=checkZero;
	const Scalar uniformScalingFactor = value/ext[0];
	Vector3 scalingFactor(uniformScalingFactor,uniformScalingFactor,uniformScalingFactor);
	scale(scalingFactor);
	return scalingFactor;
}
Vector3 MeshInstance::setLength(Scalar value)	{
    Vector3 ext = glm::mat3_cast(translationAndRotationOffset) * parentMesh->getAabbHalfExtents() * Scalar(2.)/scalingOffset;
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[2]<checkZero) ext[2]=checkZero;
	const Scalar uniformScalingFactor = value/ext[2];
	Vector3 scalingFactor(uniformScalingFactor,uniformScalingFactor,uniformScalingFactor);
	scale(scalingFactor);
	return scalingFactor;
}
Vector3 MeshInstance::setFullExtents(const Vector3& extents)	{
    Vector3 ext = glm::mat3_cast(translationAndRotationOffset) * parentMesh->getAabbFullExtents()/scalingOffset;
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[0]<checkZero) 	ext[0]=checkZero;
	if (ext[1]<checkZero) 	ext[1]=checkZero;
	if (ext[2]<checkZero) 	ext[2]=checkZero;
	Vector3 scalingFactor(extents[0]/ext[0],extents[1]/ext[1],extents[2]/ext[2]);
	scale(scalingFactor);
	return scalingFactor;
}
Vector3 MeshInstance::setHalfExtents(const Vector3& extents)	{
    Vector3 ext = glm::mat3_cast(translationAndRotationOffset) * parentMesh->getAabbHalfExtents()/scalingOffset;
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[0]<checkZero) 	ext[0]=checkZero;
	if (ext[1]<checkZero) 	ext[1]=checkZero;
	if (ext[2]<checkZero) 	ext[2]=checkZero;
	Vector3 scalingFactor(extents[0]/ext[0],extents[1]/ext[1],extents[2]/ext[2]);
	scale(scalingFactor);
	return scalingFactor;
}
Vector3 MeshInstance::getHalfExtents() const	{
	return parentMesh->getAabbHalfExtents()/scalingOffset;
}
Vector3 MeshInstance::getHalfFullExtents() const	{
	return parentMesh->getAabbFullExtents()/scalingOffset;
}
Vector3 MeshInstance::getCenterPoint()	const {
    return (parentMesh->getCenterPoint()+glm::vec3(translationAndRotationOffset[3][0],translationAndRotationOffset[3][1],translationAndRotationOffset[3][2]));
}
size_t MeshInstance::getNumMeshParts() const {return parentMesh->getNumMeshParts();}
//endregion
/*
 //region MeshBase::AnimationSetter
AnimationSetter::AnimationSetter(unsigned _animationIndex,float _animationTransitionTime,float _animationSpeed,int boneIndex)
:	animationIndex(_animationIndex),animationTransitionTime(_animationTransitionTime),animationSpeed(_animationSpeed),boneIndex(_boneIndex),
	AnimationStartTimeInTicks(-1.f)
{}
//endregion 
*/
shared_ptr < Mesh > Mesh::Create(/*bool _uselocalTextureMapCollector*/)	{
    shared_ptr < Mesh > p(new Mesh(/*_uselocalTextureMapCollector*/));
	return p;
}

Mesh::Part::Part()	{reset();}
Mesh::Part::Part(int _useVertexArrayInPartIndex)	{
	reset();
	useVertexArrayInPartIndex = _useVertexArrayInPartIndex;
}
Mesh::Part::~Part() {reset();}
void Mesh::Part::reset()	{
			verts.resize(0);
			norms.resize(0);
			texcoords.resize(0);
			inds.resize(0);
			boneIds.resize(0);
			boneWeights.resize(0);
			useVertexArrayInPartIndex = -1;
            parentBoneId = -1;

            meshName = "";
            //textureFilePath = "";
            //texture.reset();
            textureId = 0;
            textureFilePath = "";
			material = Material();
			materialName = "";
			
			batch.reset();
			/*
			vbo.reset();
			ibo.reset();
			vao.reset();
			*/
			visible = true;
}
void Mesh::Part::normalizeBoneWeights()	{
	assert(verts.size()==boneIds.size() && verts.size()==boneWeights.size());
	for (size_t v=0,sz=verts.size();v<sz;v++)	{	
		//BoneIdsType& bId = boneIds[v];
		BoneWeightsType& bWeights = boneWeights[v];
		    		    
		    GLfloat total(0);
    		for (unsigned char i=0;i<4;i++) total+=bWeights[i];
    		if (total>0 && total!=1)	{
    			const GLfloat mult = 1.0/total;
    			for (unsigned char i=0;i<4;i++) bWeights[i]*=mult;
    		}
	}    		
}
bool Mesh::Part::addBoneWeights(size_t vertsId,unsigned int boneId,GLfloat boneWeight)	{
	assert(verts.size()==boneIds.size() && verts.size()==boneWeights.size() && vertsId<boneIds.size());
	BoneIdsType& bIds = boneIds[vertsId];
	BoneWeightsType& bWeights = boneWeights[vertsId];
	
	for (unsigned char i=0;i<4;i++)	{
		if (bWeights[i]==0) {
			bIds[i] = boneId;
			bWeights[i] = boneWeight;
			return true;
		}
	}
	
	unsigned char minIndex = 0;GLfloat minWeight=100000000000;bool found = false;
	for (unsigned char i=0;i<4;i++)	{
		if (bWeights[i]<minWeight) {minWeight = bWeights[i];minIndex=i;found=true;}
	}
	if (found) {bIds[minIndex]=boneId;bWeights[minIndex]=boneWeight;}
	return found;
}

Mesh::Mesh(/*bool _uselocalTextureMapCollector*/) /* : uselocalTextureMapCollector(_uselocalTextureMapCollector)*/	{
	meshParts.reserve(MAX_NUMBER_SUBMESHES);	// MANDATORY: prevents pointers to Parts verts and inds to move in resizing operations (200 is probably too big anyway).
}
	
Mesh::~Mesh()	{
	reset();
}

Mesh::Mesh(const Mesh& c)	{
	*this = c;
}
const Mesh& Mesh::operator=(const Mesh& c)	{
	this->meshParts = *(const_cast < MeshPartVector* > (&c.meshParts));	//very big memory copy...
	return *this;
}
	
void Mesh::reset()	{
	meshParts.clear();
    //localTextureMapCollector.clear();
	m_boneIndexMap.clear();
	m_boneInfos.resize(0);
	numValidBones = 0;
	m_rootBoneInfos.resize(0);
	m_animationIndexMap.clear();
	m_animationInfos.resize(0);

    m_playAnimationsAgainWhenOvertime = false;
}

int Mesh::getNumMeshParts(int optionalPartIndex) const	{
	if (optionalPartIndex<0) return meshParts.size();
	int meshCnt=-1;bool meshTracked = false;int basePartIndex = -1;
	int numParts=0;
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		const Part& m = meshParts[part];
		if (m.verts.size()>0) {
			++meshCnt;
			if (optionalPartIndex == meshCnt) {basePartIndex = part;meshTracked = true;++numParts;}
			else if (optionalPartIndex > meshCnt) meshTracked = false;
			continue;
		}	
		if ((basePartIndex>=0 && m.useVertexArrayInPartIndex==basePartIndex) || (m.useVertexArrayInPartIndex==-1 && meshTracked)) ++numParts;	
	}
	return numParts;
}
int Mesh::getBaseMeshPartIndex(int optionalPartIndex) const	{
	if (optionalPartIndex<0) return -1;//meshParts.size() > 0 ? 0 : -1;
	int meshCnt=-1;
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		const Part& m = meshParts[part];
		if (m.verts.size()>0) {
			++meshCnt;
			if (optionalPartIndex == meshCnt) return part;
		}	
	}
	return meshParts.size();	
}
vector< int > Mesh::getMeshPartIndices(int optionalPartIndex) const	{
	vector< int > rv;
	rv.reserve(meshParts.size());
	if (optionalPartIndex<0) {
		for (int part = 0,parts = meshParts.size();part<parts;part++)	{
			rv.push_back(part);
		}
		return rv;
	}	
	int meshCnt=-1;bool meshTracked = false;int basePartIndex = -1;
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		const Part& m = meshParts[part];
		if (m.verts.size()>0) {
			++meshCnt;
			if (optionalPartIndex == meshCnt) {rv.push_back(part);basePartIndex = part;meshTracked = true;}
			else if (optionalPartIndex > meshCnt) meshTracked = false;
			continue;
		}	
		if ((basePartIndex>=0 && m.useVertexArrayInPartIndex==basePartIndex) || (m.useVertexArrayInPartIndex==-1 && meshTracked))rv.push_back(part);	
	}
	return rv;
}


Vector3 Mesh::GetAabbHalfExtents(Vector3* pOptionalCenterPointOut,int _optionalPartIndex) const	{	
	Vector3 aabbHalfExtents(0,0,0);
	if (pOptionalCenterPointOut) *pOptionalCenterPointOut = Vector3(0,0,0);
	
	Vector3 cmax(0,0,0);
	Vector3 cmin(0,0,0);
	
	bool noStartingVertex = true;
	const int optionalPartIndex = getBaseMeshPartIndex(_optionalPartIndex);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		if (optionalPartIndex>=0 && optionalPartIndex!=part) continue;	 
		const Part& m = meshParts[part];
		for (int i=0,sz=m.verts.size();i<sz;i++)	{
			const Vector3& v = m.verts[i];
			if (noStartingVertex)	{
				noStartingVertex = false;
				cmin = cmax = v;
			}
			else	{
					if 		(cmax[0]<v[0]) 		cmax[0] = v[0];
					else if (cmin[0]>v[0])		cmin[0] = v[0];
					if 		(cmax[1]<v[1]) 		cmax[1] = v[1];
					else if (cmin[1]>v[1])		cmin[1] = v[1];
					if 		(cmax[2]<v[2]) 		cmax[2] = v[2];
					else if (cmin[2]>v[2])		cmin[2] = v[2];																			
			}			
		}
	}	
	
	aabbHalfExtents = (cmax - cmin)*Scalar(0.5);
	if (pOptionalCenterPointOut) {
		*pOptionalCenterPointOut =  (cmax + cmin)*Scalar(0.5);
	}	
	return aabbHalfExtents;
}
void Mesh::GetAabbEdgePoints(Vector3& exts,Vector3& centerPoint,vector<Vector3>& aabbEdge6Vertices,const Vector3* pAabbToScan,int _optionalPartIndex) const	{	
	Vector3 aabbScanMin,aabbScanMax;
	if (pAabbToScan) {
		aabbScanMin = centerPoint - (*pAabbToScan)*Scalar(0.5f);
		aabbScanMax = centerPoint + (*pAabbToScan)*Scalar(0.5f);
	}
	else centerPoint=Vector3(0,0,0);
	exts=Vector3(0,0,0);		
	aabbEdge6Vertices.resize(6);	
	unsigned aabbEdge6Weights[6]={1,1,1,1,1,1};
	for (int i=0;i<6;i++) aabbEdge6Vertices[i]=centerPoint;
	
	
	Vector3 cmax(0,0,0);
	Vector3 cmin(0,0,0);
	
	bool noStartingVertex = true;
	const int optionalPartIndex = getBaseMeshPartIndex(_optionalPartIndex);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		if (optionalPartIndex>=0 && optionalPartIndex!=part) continue;	 
		const Part& m = meshParts[part];
		for (int i=0,sz=m.verts.size();i<sz;i++)	{
			const Vector3& v = m.verts[i];
			if (pAabbToScan)	{
				if ( 
					v[0]<aabbScanMin[0] || v[0]>aabbScanMax[0] ||
					v[1]<aabbScanMin[1] || v[1]>aabbScanMax[1] ||
					v[2]<aabbScanMin[2] || v[2]>aabbScanMax[2]
				) continue;
			}				
			if (noStartingVertex)	{
				noStartingVertex = false;
				cmin = cmax = v;
				for (int i=0;i<6;i++) aabbEdge6Vertices[i]=v;
			}
			else	{
					if 		(cmax[0]<v[0]) 		{cmax[0] = v[0];aabbEdge6Vertices[1]+=cmax[0];++(aabbEdge6Weights[1]);}
					else if (cmin[0]>v[0])		{cmin[0] = v[0];aabbEdge6Vertices[0]+=cmin[0];++(aabbEdge6Weights[0]);}
					if 		(cmax[1]<v[1]) 		{cmax[1] = v[1];aabbEdge6Vertices[3]+=cmax[1];++(aabbEdge6Weights[3]);}
					else if (cmin[1]>v[1])		{cmin[1] = v[1];aabbEdge6Vertices[2]+=cmin[1];++(aabbEdge6Weights[2]);}
					if 		(cmax[2]<v[2]) 		{cmax[2] = v[2];aabbEdge6Vertices[5]+=cmax[2];++(aabbEdge6Weights[5]);}
					else if (cmin[2]>v[2])		{cmin[2] = v[2];aabbEdge6Vertices[4]+=cmin[2];++(aabbEdge6Weights[4]);}																			
			}			
		}
	}	
	
	exts = (cmax - cmin);
	centerPoint = (cmax + cmin)*Scalar(0.5);
	for (int i=0;i<6;i++) aabbEdge6Vertices[i]/=(Scalar)aabbEdge6Weights[i];

}
const Vector3 Mesh::getAabbHalfExtents(int optionalPartIndex) const	{
	return GetAabbHalfExtents(NULL,optionalPartIndex);
}
const Vector3 Mesh::getAabbFullExtents(int optionalPartIndex) const	{
	return GetAabbHalfExtents(NULL,optionalPartIndex)*Scalar(2.);
}
const Vector3 Mesh::getCenterPoint(int optionalPartIndex) const	{
	Vector3 cp;
	GetAabbHalfExtents(&cp,optionalPartIndex);
	return cp;
}
int Mesh::getIndexOfBoneContainingName(const string& name) const {
    const string nameLower = String::ToLower(name);
	for (size_t i=0,sz=m_boneInfos.size();i<sz;i++)	{
        const string nl = String::ToLower(m_boneInfos[i].boneName);
        if (String::Contains(nl,nameLower)) return (int) i;
    }
	//printf("text: '%s' not found in Bone Hierarchy.\n",name.c_str());
	return -1;
}
bool Mesh::getVerticesBelongingToBone(unsigned boneIndex,vector<Vector3>& vertsOut,int _optionalPartIndex) const	{
	vertsOut.clear();
	if (boneIndex>=m_boneInfos.size()) return false;
	Scalar w;char boneFoundIndex = -1;
	const int optionalPartIndex = getBaseMeshPartIndex(_optionalPartIndex);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		if (optionalPartIndex>=0 && optionalPartIndex!=part) continue;	 
		const Part& m = meshParts[part];
		assert(m.boneIds.size()==m.boneWeights.size());
		for (size_t i=0,sz=m.boneIds.size();i<sz;i++)	{
			const BoneIdsType& ids = m.boneIds[i];
			const BoneWeightsType& ws = m.boneWeights[i];
			boneFoundIndex = -1;
			for (char l = 0;l < 4; l++) {
				if (ids[l]==boneIndex)	{
					w = ws[l];boneFoundIndex=l;break;
				}
			}
			if (boneFoundIndex==-1) continue;
			for (char l = 0;l < 4; l++) {
				if (l==boneFoundIndex) continue;
				if (w<ws[l])	{
					boneFoundIndex = -1;break;
				}
			}
			if (boneFoundIndex!=-1) vertsOut.push_back(m.verts[i]);			
		}
	}	
	//printf("vertices belonging to bone '%s': %d\n",	m_boneInfos[boneIndex].boneName.c_str(),vertsOut.size());		
	return true;
}


void Mesh::shift(const Vector3& shiftFactor,int _optionalPartIndex)	{
//if (!isSkeletalAnimated())	{
	const int optionalPartIndex = getBaseMeshPartIndex(_optionalPartIndex);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		if (optionalPartIndex>=0 && optionalPartIndex!=part) continue;	 
		Part& m = meshParts[part];
		for (int i=0,sz=m.verts.size();i<sz;i++)	{
			Vector3& v = m.verts[i];
			v+=shiftFactor;
		}
	}		
/*
}	
else	{
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& b = m_boneInfos[i];
		//b.transform = glm::translate(b.transform,shiftFactor);
		//if (b.preTransformIsPresent) b.preTransform = glm::translate(b.preTransform,shiftFactor);
		//b.boneOffset = glm::translate(b.boneOffset,shiftFactor);
		//b.boneOffsetInverse = glm::inverse(b.boneOffset);
		b.boneOffsetInverse = glm::translate(b.boneOffsetInverse,shiftFactor);
		b.boneOffset = glm::inverse(b.boneOffsetInverse);		
	}	
}
*/	
}
void Mesh::scale(const Vector3& scaling,int _optionalPartIndex)	{
if (!isSkeletalAnimated())	{
	const int optionalPartIndex = getBaseMeshPartIndex(_optionalPartIndex);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		if (optionalPartIndex>=0 && optionalPartIndex!=part) continue;	 
		Part& m = meshParts[part];
		for (int i=0,sz=m.verts.size();i<sz;i++)	{
			Vector3& v = m.verts[i];
			v*=scaling;
		}
	}
}
else	{
/*	
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& ni = m_boneInfos[i];
		ni.transform =  glm::scale(ni.transform,scaling);
	}					
*/
}

#ifdef NEVER
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& bi = m_boneInfos[i];
		//bi.boneOffset = glm::scale(bi.boneOffset,scaling);
	}
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& ni = m_boneInfos[i];
		ni.transform =  glm::scale(ni.transform,scaling);
		/*
		for (unordered_map<unsigned,BoneAnimationInfo>::iterator it = ni.nodeAnimationInfoMap.begin();it!=ni.nodeAnimationInfoMap.end();++it)	{
			BoneAnimationInfo& nai = it->second;
			for (unsigned k=0,ksz=nai.translationKeys.size();k<ksz;k++)	{
				glm::vec3& key = nai.translationKeys[k].value;
				key*=scaling;
			}	
					
			for (unsigned k=0,ksz=nai.scalingKeys.size();k<ksz;k++)	{
				glm::vec3& key = nai.scalingKeys[k].value;
				key*=scaling;
			}
			
		}
		*/		
	}
#endif //NEVER	
	
}
void Mesh::rotate(const Matrix3& mt,int _optionalPartIndex)	{
if (!isSkeletalAnimated())	{
	const int optionalPartIndex = getBaseMeshPartIndex(_optionalPartIndex);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		if (optionalPartIndex>=0 && optionalPartIndex!=part) continue;	 
		Part& m = meshParts[part];
		for (int i=0,sz=m.verts.size();i<sz;i++)	{
			Vector3& v = m.verts[i];
			v = mt*v;
		}
		for (int i=0,sz=m.norms.size();i<sz;i++)	{
			Vector3& n = m.norms[i];
			n = mt*n;
		}
	}
}	else
if (isSkeletalAnimated())	{
    //const glm::mat3 mtinv = glm::inverse(mt);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		Part& m = meshParts[part];
		for (int i=0,sz=m.verts.size();i<sz;i++)	{
			Vector3& v = m.verts[i];
			v = mt*v;
		}
		for (int i=0,sz=m.norms.size();i<sz;i++)	{
			Vector3& n = m.norms[i];
			n = mt*n;
		}
	}

	
	const glm::mat4 mt4(mt);
	/*
	for (unsigned i = 0,sz = m_rootBoneInfos.size();i<sz;i++)	{
		BoneInfo& bi = *(m_rootBoneInfos[i]);
		bi.preTransform = mt4 * bi.preTransform;
		bi.preTransformIsPresent = true;
		//bi.transform = m * bi.transform;
	}
	*/
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& bi = m_boneInfos[i];
		//bi.boneOffset = mt4 * bi.boneOffset;
		//bi.boneOffsetInverse = glm::inverse(bi.boneOffset);

		bi.boneOffsetInverse = mt4 * bi.boneOffsetInverse;
		bi.boneOffset = glm::inverse(bi.boneOffsetInverse);
	
	}

/*
//TODO: glm:: is not reliable here, we should get a more valid math library...
	glm::mat4 mt4(mt);mt4[3][3]=1;
	
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& ni = m_boneInfos[i];
		//ni.transform*=  mt4;
		for (unordered_map<unsigned,BoneAnimationInfo>::iterator it = ni.nodeAnimationInfoMap.begin();it!=ni.nodeAnimationInfoMap.end();++it)	{
			BoneAnimationInfo& nai = it->second;
			for (unsigned k=0,ksz=nai.translationKeys.size();k<ksz;k++)	{
				glm::vec3& key = nai.translationKeys[k].value;
				key=mt * key;
			}	
					
			//for (unsigned k=0,ksz=nai.scalingKeys.size();k<ksz;k++)	{
				//glm::vec3& key = nai.scalingKeys[k].value;
				//key*=scaling;
			//}
			
		}			
	}		
*/		
	/*
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& bi = m_boneInfos[i];
		bi.boneOffset = mt4 * bi.boneOffset;
	}	
	*/
		
}
		
} 

void Mesh::rotate(const glm::quat& q,int optionalPartIndex)	{
	const 
	Matrix3 m = glm::mat3_cast(q);//glm::toMat3(q);
	//Matrix3 m = glm::toMat3(q);
	rotate(m,optionalPartIndex);
	
	//rotate(m,optionalPartIndex);
}


void Mesh::transform(const glm::mat4& T,int _optionalPartIndex)	{
	const int optionalPartIndex = getBaseMeshPartIndex(_optionalPartIndex);
	for (int part = 0,parts = meshParts.size();part<parts;part++)	{
		if (optionalPartIndex>=0 && optionalPartIndex!=part) continue;	 
		Part& m = meshParts[part];
		for (int i=0,sz=m.verts.size();i<sz;i++)	{
			Vector3& v = m.verts[i];
			//v = T.getOrigin() + T.getBasis()*v;
			v = glm::vec3(T * glm::vec4(v,1));
		}
		for (int i=0,sz=m.norms.size();i<sz;i++)	{
			Vector3& n = m.norms[i];
			//n = T.getBasis()*n;
			n = glm::vec3(T * glm::vec4(n,0));
		}		
	}
/*
	//const vector < BoneInfos >& boneVector = m_rootBoneInfos;
	const vector < BoneInfos >& boneVector = m_boneInfos;
	for (unsigned i = 0,sz = boneVector.size();i<sz;i++)	{
		BoneInfo& bi = *(boneVector[i]);
		bi.preTransform = T * bi.preTransform;
		bi.preTransformIsPresent = true;
		//bi.transform = T * bi.transform;
	}
	
	for (unsigned i = 0,sz = m_boneInfos.size();i<sz;++i)	{
		BoneInfo& bi = m_boneInfos[i];
		//bi.boneOffset = T * bi.boneOffset;
		//bi.boneOffsetInverse = glm::inverse(bi.boneOffset);

		bi.boneOffsetInverse = T * bi.boneOffsetInverse;
		bi.boneOffset = glm::inverse(bi.boneOffsetInverse);
	
	}	
	*/	
} 

Vector3 Mesh::center(int  optionalPartIndex)	{
	Vector3 center;
	GetAabbHalfExtents(&center,optionalPartIndex);
	shift(-center,optionalPartIndex);
	return center;	
}
Vector3 Mesh::setHeight(Scalar height,int  optionalPartIndex)	{
	Vector3 ext = GetAabbHalfExtents(NULL,optionalPartIndex) * Scalar(2.);
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[1]<checkZero) ext[1]=checkZero;
	const Scalar uniformScalingFactor = height/ext[1];
	Vector3 scalingFactor(uniformScalingFactor,uniformScalingFactor,uniformScalingFactor);
	scale(scalingFactor,optionalPartIndex);
	return scalingFactor;
}
Vector3 Mesh::setWidth(Scalar width,int  optionalPartIndex)	{
	Vector3 ext = GetAabbHalfExtents(NULL,optionalPartIndex) * Scalar(2.);
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[0]<checkZero) ext[0]=checkZero;
	const Scalar uniformScalingFactor = width/ext[0];
	Vector3 scalingFactor(uniformScalingFactor,uniformScalingFactor,uniformScalingFactor);
	scale(scalingFactor,optionalPartIndex);	
	return scalingFactor;
}
Vector3 Mesh::setLength(Scalar length,int  optionalPartIndex)	{
	Vector3 ext = GetAabbHalfExtents(NULL,optionalPartIndex) * Scalar(2.);
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[2]<checkZero) ext[2]=checkZero;
	const Scalar uniformScalingFactor = length/ext[2];
	Vector3 scalingFactor(uniformScalingFactor,uniformScalingFactor,uniformScalingFactor);
	scale(scalingFactor,optionalPartIndex);
	return scalingFactor;
}
Vector3 Mesh::setFullExtents(const Vector3& extents,int  optionalPartIndex)	{
	Vector3 ext = GetAabbHalfExtents(NULL,optionalPartIndex) * Scalar(2.);
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[0]<checkZero) 	ext[0]=checkZero;
	if (ext[1]<checkZero) 	ext[1]=checkZero;
	if (ext[2]<checkZero) 	ext[2]=checkZero;
	Vector3 scalingFactor(extents[0]/ext[0],extents[1]/ext[1],extents[2]/ext[2]);
	scale(scalingFactor,optionalPartIndex);
	return scalingFactor;
}
Vector3 Mesh::setHalfExtents(const Vector3& extents,int  optionalPartIndex)	{
	Vector3 ext = GetAabbHalfExtents(NULL,optionalPartIndex);
	const Scalar checkZero(SIMD_EPSILON);
	if (ext[0]<checkZero) 	ext[0]=checkZero;
	if (ext[1]<checkZero) 	ext[1]=checkZero;
	if (ext[2]<checkZero) 	ext[2]=checkZero;
	Vector3 scalingFactor(extents[0]/ext[0],extents[1]/ext[1],extents[2]/ext[2]);
	scale(scalingFactor,optionalPartIndex);
	return scalingFactor;
}

#include <stdio.h>

inline static void Display(const glm::mat4& m,const std::string& indent="\t") {
    for (int i=0;i<4;i++)   {
        printf("%s[\t",indent.c_str());
        for (int j=0;j<4;j++)   {
            printf("%1.4f\t",m[i][j]);
        }
        printf("]\n");
    }
}
inline static void Display(const glm::mat3& m,const std::string& indent="\t") {
    for (int i=0;i<3;i++)   {
        printf("%s[\t",indent.c_str());
        for (int j=0;j<3;j++)   {
            printf("%1.4f\t",m[i][j]);
        }
        printf("]\n");
    }
}
inline static void Display(const glm::vec3& v) {
    printf("{%1.4f %1.4f %1.4f};",v[0],v[1],v[2]);
}

void Mesh::displayDebugInfo(bool vertices,bool materials,bool bones,bool animations) const	{
    printf("Num meshParts: %d",(int)meshParts.size());
    if (m_boneInfos.size()>0) printf("	m_boneInfos.size(): %d numValidBones: %d",(int)m_boneInfos.size(),(int)numValidBones);
	//if (m_boneIndexMap.size()>0) printf(" m_boneIndexMap.size(): %1d ",m_boneIndexMap.size());
	printf("\n");
    for (int i=0;i<(int)meshParts.size();i++)	{
		printf("meshPart: %1d. ",i);
		const Part& m = meshParts[i];
        if (m.meshName.size()>0) printf("%s\t\t",m.meshName.c_str());
		if (vertices)	{
        if (m.verts.size()>0) printf("numVerts: %d ",(int)m.verts.size());
		if (m.norms.size()>0) {
			if (m.norms.size()==m.verts.size()) printf("normals ok. ");
            else printf("\nError: numNorms: %1d != numVerts\n",(int)m.norms.size());
		}			
		if (m.texcoords.size()>0) {
			if (m.texcoords.size()==m.verts.size()) printf("texcoords ok.");
            else printf("\nError: numTexcoords: %1d != numVerts\n",(int)m.texcoords.size());
		}		
		if (m.boneIds.size()>0) {
			if (m.boneIds.size()==m.verts.size()) printf("boneIndices ok. ");
            else printf("\nError: numBoneIndices: %1d != numVerts\n",(int)m.boneIds.size());
		}
		if (m.boneWeights.size()>0) {
			if (m.boneWeights.size()==m.verts.size()) printf("boneWeights ok.");
            else printf("\nError: numBoneWeights: %1d != numVerts\n",(int)m.boneWeights.size());
		}
        if (m.parentBoneId>=0) {
            if ((int)numValidBones>m.parentBoneId) {
                const string parentBoneName = m_boneInfos[m.parentBoneId].boneName;
                printf("parentBone: '%s'",parentBoneName.c_str());
            }
            else printf("\nError: parentBoneId: %1d >= numValidBones (%d)\n",m.parentBoneId,(int)numValidBones);
        }
        if (m.verts.size()>0) printf("\t");
																				
        printf("numTri: %d ",(int)m.inds.size()/3);
		if (m.verts.size()==0 && m.useVertexArrayInPartIndex>=0) printf("referring to verts in meshPart %1d ",m.useVertexArrayInPartIndex);			
		printf("\n");		
		}
		if (materials)	{
        //if (m.texture && m.texture->isValid()) printf("\ttexId=%d	path: \"%s\"\n",static_cast<GLuint>(*m.texture),m.textureFilePath/*.file_string()*/.c_str());
        if (m.textureId>0) printf("\ttexId=%d textureFilePath=%s\n",m.textureId,m.textureFilePath.c_str());
        if (m.materialName.size()>0) printf("\tmaterialName: '%s'\n",m.materialName.c_str());
		m.material.displayDebugInfo();
		}
		
		if (bones)	{
			const int numBoneInfluencesToDisplay = 0;	//15;
			if (numBoneInfluencesToDisplay>0)	{
				if (m.boneIds.size()==m.verts.size() && m.boneWeights.size()==m.verts.size())	{
					const int maxNumElementaToDisplay = numBoneInfluencesToDisplay;
                    const int end = (maxNumElementaToDisplay>(int)m.verts.size()?(int)m.verts.size():maxNumElementaToDisplay);
					for (int j=0;j<end;j++)	{
						printf("	boneInfluence[%d] = ",j);
						for (int h=0;h<4;h++) {
							const GLuint boneIndex = m.boneIds[j][h];
							const BoneInfo* bi = boneIndex < m_boneInfos.size() ? &m_boneInfos[boneIndex] : NULL;
							if (!bi) {printf("Error ");continue;}
							const GLfloat boneWeight = m.boneWeights[j][h];
							if (boneWeight==0) continue;
							if (h>0 && h<3) printf(" + ");
							printf("%1.2f*%s", boneWeight,bi->boneName.c_str());
						}	
					printf(";\n");				
					}
				}				
			}
		}
	}
	if (bones) {
        for (int i=0;i<(int)m_boneInfos.size();i++)	{
			const BoneInfo& bi = m_boneInfos[i];
			printf("bone[%d].boneName = '%s'",i,bi.boneName.c_str());	
			//printf( "ai.size()=%d",bi.boneAnimationInfoMap.size());
			if (bi.preTransformIsPresent) printf(" (preTransform present)");
			if (bi.isDummyBone) printf(" (DUMMY BONE)");		
            if (bi.isUseless) printf(" (USELESS BONE)");

            printf("\t\t");
            if (bi.indexMirror!=bi.index) printf("\tmirror:'%s'",m_boneInfos[bi.indexMirror].boneName.c_str());
            if (!bi.parentBoneInfo)	printf("\tROOT");
            else printf("\tparent:'%s'",bi.parentBoneInfo->boneName.c_str());
            if (bi.childBoneInfos.size()>0) {
                printf("\tchild:");
                for (int ci=0,cisz=bi.childBoneInfos.size();ci<cisz;ci++) {
                    printf(" '%s'",bi.childBoneInfos[ci]->boneName.c_str());
                }
            }

            //#define DISPLAY_TRANSFORMS_TOO
            #ifdef DISPLAY_TRANSFORMS_TOO
            printf("\n\tboneOffset:\n");Display(bi.boneOffset,"\t\t");
            //printf("\tboneOffsetInverse:\n");Display(bi.boneOffsetInverse,"\t\t");
            printf("\ttransform:\n");Display(bi.transform,"\t\t");
            if (bi.preTransformIsPresent)  {printf("\tpreTransform:\n");Display(bi.preTransform,"\t\t");}
            if (bi.preAnimationTransformIsPresent)  {printf("\tpreAnimationTransform:\n");Display(bi.preAnimationTransform,"\t\t");}
            if (bi.preTransformIsPresent && bi.preAnimationTransformIsPresent && bi.multPreTransformPreAnimationTransform!=bi.preTransform*bi.preAnimationTransform)  {printf("\tmultPreTransformPreAnimationTransform is WRONG\n");}
            if (bi.postAnimationTransformIsPresent)  {printf("\tpostAnimationTransform:\n");Display(bi.postAnimationTransform,"\t\t");}
            #undef DISPLAY_TRANSFORMS_TOO
            #endif //DISPLAY_TRANSFORMS_TOO

            printf("\n");
		}
	}
	if (animations)	{
		printf("NumAnimations: %d\n",getNumAnimations());
		for (size_t i=0,sz=m_animationInfos.size();i<sz;i++)	{
			const AnimationInfo& ai = m_animationInfos[i];
            printf("animation[%d] = '%s'	dur:%1.2f tps:%1.2f nt:%1.2f\n",(int)i,ai.name.c_str(),ai.duration,ai.ticksPerSecond,ai.numTicks);
		}

        for (size_t i=0,sz=m_animationInfos.size();i<sz;i++)  {
            const AnimationInfo& ai = m_animationInfos[i];
            printf("\nKeyframes of animation[%d] = '%s'	dur:%1.2f tps:%1.2f nt:%1.2f:\n",(int)i,ai.name.c_str(),ai.duration,ai.ticksPerSecond,ai.numTicks);
            BoneInfo::MapUnsignedBoneAnimationInfo::const_iterator it;
            for (size_t j=0;j<numValidBones;j++) {
                const BoneInfo& bi = m_boneInfos[j];
                if ((it = bi.boneAnimationInfoMap.find(i))!=bi.boneAnimationInfoMap.end())  {
                    const BoneAnimationInfo& bai = it->second;
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
        }

	}
}


bool Mesh::AllDescendantsAreDummy(const Mesh::BoneInfo& bi)	{
	if (bi.childBoneInfos.size()==0) return true;
	for (vector<Mesh::BoneInfo*>::const_iterator it = bi.childBoneInfos.begin(); it != bi.childBoneInfos.end(); ++it)	{
		if ((*it) && !(*it)->isDummyBone) return false;
		if (!AllDescendantsAreDummy(*(*it))) return false;
	}
	return true;
}


 //region Animation Stuff
// Methods that use the Mesh class directly (good for developers)
unsigned Mesh::CalcInterpolatedPosition(glm::vec3& Out, float AnimationTime, const BoneAnimationInfo& nasi, unsigned* pAlreadyKnownPositionIndex)
{
	const vector <Vector3Key>& keys = nasi.translationKeys;
	const size_t sz = keys.size();
	if (sz==0) {Out[0]=Out[1]=Out[2]=0;return 0;}
	unsigned PositionIndex;
	if (!pAlreadyKnownPositionIndex)	{
    	if (sz == 1 || AnimationTime < keys[0].time) {Out = keys[0].value;return 0;}    
    	PositionIndex = 0;bool found=false;
    	for (unsigned i = 0 ; i < sz - 1 ; i++) {
        	if (AnimationTime < keys[i + 1].time) {PositionIndex = i;found=true;break;}
    	}    
    	if (!found) {Out = keys[sz-1].value;return sz - 1;}
    }
    else {
    	PositionIndex = *pAlreadyKnownPositionIndex;
    	if (PositionIndex >= sz - 1) {Out = keys[sz-1].value;return PositionIndex;}
	}    
	if (PositionIndex==0 && AnimationTime < keys[0].time)	{
		const glm::vec3& Start = keys[sz-1].value;
		const glm::vec3& End = keys[0].value; 
		float Factor = AnimationTime / keys[0].time;
        Out = Start + (End - Start) * Factor;
    	return PositionIndex;
	}		
    const unsigned NextPositionIndex = (PositionIndex + 1);
    assert(NextPositionIndex < sz);
    float DeltaTime = keys[NextPositionIndex].time - keys[PositionIndex].time;
    float Factor = (AnimationTime - keys[PositionIndex].time) / DeltaTime;
    /*if (Factor < 0.0 || Factor > 1.0) printf("PositionIndex:%d NextPositionIndex:%d sz:%d keys[NextPositionIndex].time=%1.4f keys[PositionIndex].time=%1.4f AnimationTime=%1.4f\n",PositionIndex,NextPositionIndex,sz,
    										keys[NextPositionIndex].time,keys[PositionIndex].time,AnimationTime);
    										*/
    assert(Factor >= 0.0 && Factor <= 1.0);
    const glm::vec3& Start = keys[PositionIndex].value;
    const glm::vec3& End = keys[NextPositionIndex].value;
    Out = Start + (End - Start) * Factor;
    return PositionIndex;
}
unsigned Mesh::CalcInterpolatedRotation(glm::quat& Out, float AnimationTime, const BoneAnimationInfo& nasi, unsigned* pAlreadyKnownRotationIndex)
{
	static const bool normalizeAfterLerp = false;
    static const bool useLerp = false;
    static const Scalar eps = 0.001f;

	const vector <QuaternionKey>& keys = nasi.rotationKeys;
	const size_t sz = keys.size();
	if (sz==0) {Out.x=Out.y=Out.z=0;Out.w=1;return 0;}
	unsigned RotationIndex;
	if (!pAlreadyKnownRotationIndex)	{    
    	if (sz == 1 || AnimationTime < keys[0].time) {Out = keys[0].value;return 0;}
    	RotationIndex = 0;bool found=false;
    	for (unsigned i = 0 ; i < sz - 1 ; i++) {
        	if (AnimationTime < keys[i + 1].time) {RotationIndex = i;found=true;break;}
    	}    
    	if (!found) {Out = keys[sz-1].value;return sz - 1;}
	}
	else {
		RotationIndex = *pAlreadyKnownRotationIndex;  
		if (RotationIndex >= sz - 1) {Out = keys[sz-1].value;return RotationIndex;}	 	
	}	
	if (RotationIndex==0 && AnimationTime < keys[0].time)	{
		const glm::quat& StartRotationQ = keys[sz-1].value; 
		const glm::quat& EndRotationQ   = keys[0].value; 
		float Factor = AnimationTime / keys[0].time;
		if (!useLerp) glm::quatSlerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp,eps);
    	else glm::quatLerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp);	// Fastest possible = Lerp with "false"
    	return RotationIndex;
	} 		  	
   	
		
    unsigned NextRotationIndex = (RotationIndex + 1);
    assert(RotationIndex<sz && NextRotationIndex < sz);
    float DeltaTime = keys[NextRotationIndex].time - keys[RotationIndex].time;
    float Factor = (AnimationTime - keys[RotationIndex].time) / DeltaTime;
    
    //if (!(Factor >= 0.0f && Factor <= 1.0f)) printf("Factor= %1.4f RotationIndex = %d AnimationTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,RotationIndex,AnimationTime,NextRotationIndex,keys[NextRotationIndex].time,RotationIndex,keys[RotationIndex].time,sz);
    
    assert(Factor >= 0.0f && Factor <= 1.0f);
    const glm::quat& StartRotationQ = keys[RotationIndex].value;
    const glm::quat& EndRotationQ   = keys[NextRotationIndex].value; 
    // Default glm methods fail here:
    /*Out = //glm::normalize(
    		glm::mix(StartRotationQ, EndRotationQ, Factor)	// glm::slerp on older versions of glm probably
    	  //)
    	  ; 
	*/    	   	
	// thus I've added some based on the asset import library code in mesh.h (I guess Ogre::Quaternion slerp would have worked too)
	if (!useLerp) glm::quatSlerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp,eps);
    else glm::quatLerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp);	// Fastest possible = Lerp with "false"
    /*	  
	printf("StartRotationQ=(%1.2f,%1.2f,%1.2f,%1.2f);EndRotationQ=(%1.2f,%1.2f,%1.2f,%1.2f);Slerp=(%1.2f,%1.2f,%1.2f,%1.2f);\n",
    		StartRotationQ.x,StartRotationQ.y,StartRotationQ.z,StartRotationQ.w,
    		EndRotationQ.x,EndRotationQ.y,EndRotationQ.z,EndRotationQ.w,
    		Out.x,Out.y,Out.z,Out.w
    		);   
    */ 
    return RotationIndex;   	  
}
unsigned Mesh::CalcInterpolatedScaling(glm::vec3& Out, float AnimationTime, const BoneAnimationInfo& nasi, unsigned* pAlreadyKnownScalingIndex)
{
	const vector <Vector3Key>& keys = nasi.scalingKeys;
	const size_t sz = keys.size();
	if (sz==0) {Out[0]=Out[1]=Out[2]=1;return 0;}
	unsigned ScalingIndex;
	if (!pAlreadyKnownScalingIndex)	{        
    	if (sz == 1 || AnimationTime < keys[0].time) {Out = keys[0].value;return 0;}
    	ScalingIndex = 0;bool found=false;
    	for (unsigned i = 0 ; i < sz - 1 ; i++) {
        	if (AnimationTime < keys[i + 1].time) {ScalingIndex = i;found=true;break;}
    	}    
    	if (!found) {Out = keys[sz-1].value;return sz - 1;}
    }
    else {
    	ScalingIndex = *pAlreadyKnownScalingIndex;
    	if (ScalingIndex >= sz - 1) {Out = keys[sz-1].value;return ScalingIndex;}
	}    
	if (ScalingIndex==0 && AnimationTime < keys[0].time)	{
		const glm::vec3& Start = keys[sz-1].value; 
		const glm::vec3& End = keys[0].value; 
		float Factor = AnimationTime / keys[0].time;
        Out = Start + (End - Start) * Factor;
    	return ScalingIndex;
	}	 	
    unsigned NextScalingIndex = (ScalingIndex + 1);
    assert(NextScalingIndex < sz);
    float DeltaTime = keys[NextScalingIndex].time - keys[ScalingIndex].time;
    float Factor = (AnimationTime - keys[ScalingIndex].time) / DeltaTime;
    assert(Factor >= 0.0f && Factor <= 1.0f);
    const glm::vec3& Start = keys[ScalingIndex].value;
    const glm::vec3& End   = keys[NextScalingIndex].value;
    Out = Start + (End - Start) * Factor;
    return ScalingIndex;
}
void Mesh::ReadBoneHeirarchy(unsigned animationIndex,float AnimationTime, BoneInfo& bi, const glm::mat4& ParentTransform){    
	if (bi.isUseless) return;	// Exit early
    Mat4 GlobalTransformation = ParentTransform;         
   
    const BoneAnimationInfo* pbai = bi.getBoneAnimationInfo(animationIndex);
    if (pbai) {
		const BoneAnimationInfo& bai = *pbai;		
		if (bi.preAnimationTransformIsPresent) 	GlobalTransformation*=bi.multPreTransformPreAnimationTransform;

		const bool hasTranslationKeys 	= bai.translationKeys.size()>0;
       	const bool hasRotationKeys 		= bai.rotationKeys.size()>0;
       	const bool hasScalingKeys 		= bai.scalingKeys.size()>0;
       	unsigned translationIndex(0),rotationIndex(0);
       	
        if (hasTranslationKeys)	{
        	glm::vec3 Translation(0,0,0);
        	translationIndex = CalcInterpolatedPosition(Translation, AnimationTime, bai);	// Interpolate translation and generate translation transformation matrix        
			GlobalTransformation = glm::translate(GlobalTransformation,Translation);
		}        	
   		if (bi.postAnimationTransformIsPresent) GlobalTransformation*=bi.postAnimationTransform;
		if (hasRotationKeys) 	{               
            glm::quat RotationQ(1,0,0,0);
        	rotationIndex = CalcInterpolatedRotation(RotationQ, AnimationTime, bai, (bai.tkAndRkHaveUniqueTimePerKey && hasTranslationKeys) ? &translationIndex : NULL);  // Interpolate rotation and generate rotation transformation matrix      
            GlobalTransformation*=glm::mat4_cast(RotationQ);//glm::mat4_cast(RotationQ);
		}
		if (hasScalingKeys)	{
        	glm::vec3 Scaling(1,1,1);
        	if (hasScalingKeys) CalcInterpolatedScaling(Scaling, AnimationTime, bai, 	(bai.tkAndSkHaveUniqueTimePerKey && hasTranslationKeys) ? &translationIndex : 
        																				(bai.rkAndSkHaveUniqueTimePerKey && hasRotationKeys) ? &rotationIndex :
        																				NULL);	// Interpolate scaling and generate scaling transformation matrix
        	GlobalTransformation = glm::scale(GlobalTransformation,Scaling);
		}        																						       							        					        						
   		
    }
    else {
    	if (bi.preTransformIsPresent) GlobalTransformation*=bi.preTransform;
    	GlobalTransformation *= bi.transform;
	}    	
	
	if (!bi.isDummyBone) bi.finalTransformation = GlobalTransformation * bi.boneOffset;	 				
	else bi.finalTransformation = GlobalTransformation;	//bi.boneOffset==mat(1)
        
    for (unsigned i = 0,sz = bi.childBoneInfos.size() ; i < sz ; i++) {
        ReadBoneHeirarchy(animationIndex,AnimationTime, *(bi.childBoneInfos[i]), GlobalTransformation);
    }
} 
void Mesh::getBoneTransforms(unsigned animationIndex,float TimeInSeconds, vector<glm::mat4>& Transforms)	{
	if (animationIndex>=m_animationInfos.size() || m_boneInfos.size()==0) return;
	const AnimationInfo& animationInfo = m_animationInfos[animationIndex];
	
    glm::mat4 Identity;
    
    float TimeInTicks = TimeInSeconds * animationInfo.ticksPerSecond;
    float AnimationTime = fmod(TimeInTicks, animationInfo.numTicks);

	for (unsigned i = 0,sz = m_rootBoneInfos.size() ; i < sz ; i++) {
		ReadBoneHeirarchy(animationIndex,AnimationTime, *(m_rootBoneInfos[i]), Identity);       	
    }

    Transforms.resize(numValidBones);//m_boneInfos.size());

    for (unsigned i = 0,sz = numValidBones/*m_boneInfos.size()*/ ; i < sz ; i++) {
        Transforms[i] = m_boneInfos[i].finalTransformation;
    }	
}

void Mesh::ReadBoneHeirarchyForBindingPose(BoneInfo& bi, const glm::mat4& ParentTransform)	{
	if (bi.isUseless) return;	// Exit early
	Mat4 GlobalTransformation = ParentTransform;
	if (bi.preTransformIsPresent) GlobalTransformation*=bi.preTransform;
    GlobalTransformation*=bi.transform;
    bi.finalTransformation = GlobalTransformation * bi.boneOffset;;
        
    for (unsigned i = 0,sz = bi.childBoneInfos.size() ; i < sz ; i++) {
        ReadBoneHeirarchyForBindingPose(*(bi.childBoneInfos[i]), GlobalTransformation);
    }	
}
void Mesh::getBindingPoseBoneTransforms(vector<glm::mat4>& Transforms)	{
	glm::mat4 Identity;
	Transforms.resize(numValidBones);//m_boneInfos.size());    
    //#define SIMPLY_RESET_TRANSFORMS
	#ifdef SIMPLY_RESET_TRANSFORMS	
	for (unsigned i = 0,sz = numValidBones/*bi.childBoneInfos.size()*/ ; i < sz ; i++)	{
		Transforms[i] = Identity;
	}
	#undef SIMPLY_RESET_TRANSFORMS
	#else //SIMPLY_RESET_TRANSFORMS	
	for (unsigned i = 0,sz = m_rootBoneInfos.size() ; i < sz ; i++) {
       	ReadBoneHeirarchyForBindingPose(*(m_rootBoneInfos[i]), Identity);
    }
	for (unsigned i = 0,sz = numValidBones/*m_boneInfos.size()*/ ; i < sz ; i++) {
        Transforms[i] = m_boneInfos[i].finalTransformation;
    }
    #endif //SIMPLY_RESET_TRANSFORMS		
}


// Methods that take a MeshInstance& argument:
unsigned Mesh::CalcInterpolatedPosition(float AnimationTime,float additionalPoseTransitionTime, const BoneAnimationInfo& nasi, MeshInstance::BoneFootprint& bf,unsigned* pAlreadyKnownPositionIndex,float* pAlreadyKnownFactor,float* pFactorOut,int looping,const AnimationInfo& ai)
{
    glm::vec3& Out = bf.translation;
	const vector <Vector3Key>& keys = nasi.translationKeys;
	const size_t sz = keys.size();
	if (sz==0) {/*Out[0]=Out[1]=Out[2]=0;*/return 0;}
    const bool mustRepeatAnimation = (nasi.postState == BoneAnimationInfo::AB_REPEAT) && sz>1;
    if (!mustRepeatAnimation && looping!=0 && !m_playAnimationsAgainWhenOvertime) {
        Out = keys[sz-1].value;
        return sz - 1;
    }
    const bool isFirstLoop = mustRepeatAnimation && looping==0; // used only when mustRepeatAnimation == true;
    unsigned PositionIndex;
	if (!pAlreadyKnownPositionIndex)	{
        PositionIndex=0; bool found = false;
        if (!mustRepeatAnimation || isFirstLoop)   {    // All cases with sz==1 should en here too (to check)
            const float cleanAnimationTime = AnimationTime - additionalPoseTransitionTime;
            if (sz == 1 || cleanAnimationTime < keys[0].time) {
                PositionIndex=0;found = true;
            }
            else {
                for (unsigned i = 0; i < sz - 1 ; i++) {
                    if (cleanAnimationTime < keys[i + 1].time) {PositionIndex = i;found=true;break;}
                }
            }
            if (!found) {
                if (!mustRepeatAnimation)   {
                    Out = keys[sz-1].value;
                    return sz - 1;
                }
                else PositionIndex=sz-1;
            }
        }
        else {
            if (AnimationTime < keys[0].time) {
                PositionIndex=sz-1;found = true;            // This happens after ai.numTicks in the figure.
            }
            else {
                for (unsigned i = 0; i < sz - 1 ; i++) {
                    if (AnimationTime < keys[i + 1].time) {PositionIndex = i;found=true;break;}
                }
            }
            if (!found) {
                PositionIndex=sz-1;                         // This happens between 1 and ai.numTicks in the figure.
            }
        }
    }
    else PositionIndex = *pAlreadyKnownPositionIndex; 
    if ((!mustRepeatAnimation || isFirstLoop) && PositionIndex==0 && AnimationTime < (keys[0].time+additionalPoseTransitionTime))	{
		const glm::vec3& Start = Out;//keys[sz-1].value;
		const glm::vec3& End = keys[0].value; 
		//if (bf.translationPoseTransitionTime<0) bf.translationPoseTransitionTime=0;
		float deltaTime = keys[0].time+additionalPoseTransitionTime - bf.translationPoseTransitionTime;
		float Factor = (AnimationTime - bf.translationPoseTransitionTime) / deltaTime;
		bf.translationPoseTransitionTime = AnimationTime;
		// Factor???
        Out = Start + (End - Start) * Factor;
    	return PositionIndex;
	}
	else {
		bf.translationPoseTransitionTime = 0.f;
        if ((!mustRepeatAnimation && PositionIndex >= sz - 1) || sz<2) {return PositionIndex;}
	}		
    unsigned NextPositionIndex = (PositionIndex + 1);
    if (mustRepeatAnimation && NextPositionIndex>=sz) NextPositionIndex = 0;   //
    else assert(NextPositionIndex < sz);
    assert(NextPositionIndex<sz && NextPositionIndex < sz);
    float Factor;
    if (!pAlreadyKnownFactor)	{
        if (NextPositionIndex==0) {          //
            assert(mustRepeatAnimation);
            // PERFECT!
            if (isFirstLoop)    {
                float DeltaTime = keys[NextPositionIndex].time+ ai.numTicks - keys[PositionIndex].time;
                float cleanAnimationTime = AnimationTime - (keys[PositionIndex].time + additionalPoseTransitionTime);
                Factor =  cleanAnimationTime / DeltaTime;
                //if (sz==2 && (Factor<0 || Factor>1)) printf("1) Factor= %1.4f looping:%d PositionIndex = %d AnimationTime = %1.4f additionalPoseTransitionTime=%1.4f ai.numTicks = %1.4f cleanAnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,looping,PositionIndex,AnimationTime,additionalPoseTransitionTime,ai.numTicks,cleanAnimationTime,DeltaTime,NextPositionIndex,keys[NextPositionIndex].time,PositionIndex,keys[PositionIndex].time,sz);
            }
            else {
                const bool newLoop = AnimationTime < keys[0].time;
                float DeltaTime = keys[NextPositionIndex].time + ai.numTicks - keys[PositionIndex].time;
                float cleanAnimationTime = newLoop ? (AnimationTime + ai.numTicks - keys[PositionIndex].time) :
                                                     (AnimationTime - keys[PositionIndex].time);
                Factor =  cleanAnimationTime / DeltaTime;
                //if (sz==2 && (Factor<0 || Factor>1)) printf("3) Factor= %1.4f looping:%d PositionIndex = %d AnimationTime = %1.4f additionalPoseTransitionTime=%1.4f ai.numTicks = %1.4f cleanAnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,looping,PositionIndex,AnimationTime,additionalPoseTransitionTime,ai.numTicks,cleanAnimationTime,DeltaTime,NextPositionIndex,keys[NextPositionIndex].time,PositionIndex,keys[PositionIndex].time,sz);
            }
        }
        else {
            float DeltaTime = keys[NextPositionIndex].time - keys[PositionIndex].time;
            Factor = (AnimationTime - (keys[PositionIndex].time+additionalPoseTransitionTime)) / DeltaTime;
            //if (mustRepeatAnimation && sz==2) printf("2) Factor= %1.4f PositionIndex = %d AnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,PositionIndex,AnimationTime,DeltaTime,NextPositionIndex,keys[NextPositionIndex].time,PositionIndex,keys[PositionIndex].time,sz);
        }
        //if (!(Factor >= 0.0f && Factor <= 1.0f)) printf("Factor= %1.4f PositionIndex = %d AnimationTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d looping = %d additionalPoseTransitionTime = %1.4f\n",Factor,PositionIndex,AnimationTime,NextPositionIndex,keys[NextPositionIndex].time,PositionIndex,keys[PositionIndex].time,sz,looping,additionalPoseTransitionTime);
        assert(Factor >= 0.0f && Factor <= 1.0f);
    }
    else Factor = *pAlreadyKnownFactor;
    if (pFactorOut) *pFactorOut = Factor;
    const glm::vec3& Start = keys[PositionIndex].value;
    const glm::vec3& End = keys[NextPositionIndex].value;
    Out = Start + (End - Start) * Factor;
    return PositionIndex;
}
unsigned Mesh::CalcInterpolatedRotation(float AnimationTime,float additionalPoseTransitionTime, const BoneAnimationInfo& nasi, MeshInstance::BoneFootprint& bf,unsigned* pAlreadyKnownRotationIndex,float* pAlreadyKnownFactor,float* pFactorOut,int looping,const AnimationInfo& ai)
{
/*
 *      LOOPING IMPLEMENTATION NOTES: ( figure with keys.size()=2 )
 *
 * keys:                              0                    1                                         0                      1                                             0
 *          |-------------------------|--------------------|-------------------|---------------------|----------------------|----------------------|----------------------|
 * ticks:   0                   firstLoopAdditional                         ai.numTicks              |                                         ai.numTicks              looping = 2
 *                              PoseTransitionTime                                                   |                                                                  AnimationTime = 0 again
 *                                                                                          looping = 1
 *                                                                                          AnimationTime = 0 again
 *
 */

    glm::quat& Out = bf.rotation;
	static const bool normalizeAfterLerp = false;
    static const bool useLerp = false;
    static const Scalar eps = 0.001f;

    const vector <QuaternionKey>& keys = nasi.rotationKeys;
    const size_t sz = keys.size();
    if (sz==0) {/*Out.x=Out.y=Out.z=0;Out.w=1;*/return 0;}
    const bool mustRepeatAnimation = (nasi.postState == BoneAnimationInfo::AB_REPEAT) && sz>1;
    if (!mustRepeatAnimation && looping!=0 && !m_playAnimationsAgainWhenOvertime) {
        Out = keys[sz-1].value;
        return sz - 1;
    }
    const bool isFirstLoop = mustRepeatAnimation && looping==0; // used only when mustRepeatAnimation == true;
    unsigned RotationIndex;
    if (!pAlreadyKnownRotationIndex)	{
    	RotationIndex = 0;bool found=false;
        if (!mustRepeatAnimation || isFirstLoop)   {    // All cases with sz==1 should en here too (to check)
            const float cleanAnimationTime = AnimationTime - additionalPoseTransitionTime;
            if (sz == 1 || cleanAnimationTime < keys[0].time) {
                RotationIndex=0;found = true;
            }
            else {
                for (unsigned i = 0; i < sz - 1 ; i++) {
                    if (cleanAnimationTime < keys[i + 1].time) {RotationIndex = i;found=true;break;}
                }
            }
            if (!found) {
                if (!mustRepeatAnimation)   {
                    Out = keys[sz-1].value;
                    return sz - 1;
                }
                else RotationIndex=sz-1;
            }
        }
        else {
            if (AnimationTime < keys[0].time) {
                RotationIndex=sz-1;found = true;            // This happens after ai.numTicks in the figure.
            }
            else {
                for (unsigned i = 0; i < sz - 1 ; i++) {
                    if (AnimationTime < keys[i + 1].time) {RotationIndex = i;found=true;break;}
                }
            }
            if (!found) {
                RotationIndex=sz-1;                         // This happens between 1 and ai.numTicks in the figure.
            }
        }
    }
	else RotationIndex = *pAlreadyKnownRotationIndex;  

    if ((!mustRepeatAnimation || isFirstLoop) && RotationIndex==0 && AnimationTime < (keys[0].time+additionalPoseTransitionTime))	{
        const glm::quat& StartRotationQ = Out;//keys[sz-1].value;   //TODO: we need the starting pose as a quaternion (how to get it?)
		const glm::quat& EndRotationQ   = keys[0].value; 
		//if (bf.rotationPoseTransitionTime<0) bf.rotationPoseTransitionTime=0;
        float deltaTime = keys[0].time+additionalPoseTransitionTime - bf.rotationPoseTransitionTime;
        float Factor = (AnimationTime - bf.rotationPoseTransitionTime) / deltaTime;
        bf.rotationPoseTransitionTime = AnimationTime;
		
		
		// Factor ???
		if (!useLerp) glm::quatSlerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp,eps);
    	else glm::quatLerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp);	// Fastest possible = Lerp with "false"
        return RotationIndex;
	} 	
	else {
		bf.rotationPoseTransitionTime = 0.f;
        if ((!mustRepeatAnimation && RotationIndex >= sz - 1) || sz<2) {return RotationIndex;}    //
	}		
   	
		
    unsigned NextRotationIndex = (RotationIndex + 1);
    if (mustRepeatAnimation && NextRotationIndex>=sz) NextRotationIndex = 0;   //
    else assert(NextRotationIndex < sz);
    assert(RotationIndex<sz && NextRotationIndex < sz);
    float Factor;
    if (!pAlreadyKnownFactor)	{
        if (NextRotationIndex==0) {          //
            assert(mustRepeatAnimation);
            // PERFECT!
            if (isFirstLoop)    {
                float DeltaTime = keys[NextRotationIndex].time+ ai.numTicks - keys[RotationIndex].time;
                float cleanAnimationTime = AnimationTime - (keys[RotationIndex].time + additionalPoseTransitionTime);
                Factor =  cleanAnimationTime / DeltaTime;
                //if (sz==2 && (Factor<0 || Factor>1)) printf("1) Factor= %1.4f looping:%d RotationIndex = %d AnimationTime = %1.4f additionalPoseTransitionTime=%1.4f ai.numTicks = %1.4f cleanAnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,looping,RotationIndex,AnimationTime,additionalPoseTransitionTime,ai.numTicks,cleanAnimationTime,DeltaTime,NextRotationIndex,keys[NextRotationIndex].time,RotationIndex,keys[RotationIndex].time,sz);
            }
            else {
                const bool newLoop = AnimationTime < keys[0].time;
                float DeltaTime = keys[NextRotationIndex].time + ai.numTicks - keys[RotationIndex].time;
                float cleanAnimationTime = newLoop ? (AnimationTime + ai.numTicks - keys[RotationIndex].time) :
                                                     (AnimationTime - keys[RotationIndex].time);
                Factor =  cleanAnimationTime / DeltaTime;
                //if (sz==2 && (Factor<0 || Factor>1)) printf("3) Factor= %1.4f looping:%d RotationIndex = %d AnimationTime = %1.4f additionalPoseTransitionTime=%1.4f ai.numTicks = %1.4f cleanAnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,looping,RotationIndex,AnimationTime,additionalPoseTransitionTime,ai.numTicks,cleanAnimationTime,DeltaTime,NextRotationIndex,keys[NextRotationIndex].time,RotationIndex,keys[RotationIndex].time,sz);
            }
        }
        else {
            float DeltaTime = keys[NextRotationIndex].time - keys[RotationIndex].time;
            Factor = (AnimationTime - (keys[RotationIndex].time+additionalPoseTransitionTime)) / DeltaTime;
            //if (mustRepeatAnimation && sz==2) printf("2) Factor= %1.4f RotationIndex = %d AnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,RotationIndex,AnimationTime,DeltaTime,NextRotationIndex,keys[NextRotationIndex].time,RotationIndex,keys[RotationIndex].time,sz);
        }
        //if (!(Factor >= 0.0f && Factor <= 1.0f)) printf("Factor= %1.4f RotationIndex = %d AnimationTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d looping = %d additionalPoseTransitionTime = %1.4f\n",Factor,RotationIndex,AnimationTime,NextRotationIndex,keys[NextRotationIndex].time,RotationIndex,keys[RotationIndex].time,sz,looping,additionalPoseTransitionTime);
    	assert(Factor >= 0.0f && Factor <= 1.0f);
    }
    else Factor = *pAlreadyKnownFactor;
    if (pFactorOut) *pFactorOut = Factor;
    const glm::quat& StartRotationQ = keys[RotationIndex].value;
    const glm::quat& EndRotationQ   = keys[NextRotationIndex].value; 
    // Default glm methods fail here:
    /*Out = //glm::normalize(
    		glm::mix(StartRotationQ, EndRotationQ, Factor)	// glm::slerp on older versions of glm probably
    	  //)
    	  ; 
	*/    	   	
	// thus I've added some based on the asset import library code in mesh.h (I guess Ogre::Quaternion slerp would have worked too)
	if (!useLerp) glm::quatSlerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp,eps);
    else glm::quatLerp(Out,Factor,StartRotationQ,EndRotationQ,normalizeAfterLerp);	// Fastest possible = Lerp with "false"
    /*	  
	printf("StartRotationQ=(%1.2f,%1.2f,%1.2f,%1.2f);EndRotationQ=(%1.2f,%1.2f,%1.2f,%1.2f);Slerp=(%1.2f,%1.2f,%1.2f,%1.2f);\n",
    		StartRotationQ.x,StartRotationQ.y,StartRotationQ.z,StartRotationQ.w,
    		EndRotationQ.x,EndRotationQ.y,EndRotationQ.z,EndRotationQ.w,
    		Out.x,Out.y,Out.z,Out.w
    		);   
    */ 
    return RotationIndex;   	  
}
unsigned Mesh::CalcInterpolatedScaling(float AnimationTime,float additionalPoseTransitionTime, const BoneAnimationInfo& nasi, MeshInstance::BoneFootprint& bf, unsigned* pAlreadyKnownScalingIndex,float* pAlreadyKnownFactor,float* pFactorOut,int looping,const AnimationInfo& ai)
{
	glm::vec3& Out = bf.scaling;
	const vector <Vector3Key>& keys = nasi.scalingKeys;
	const size_t sz = keys.size();
	if (sz==0) {/*Out[0]=Out[1]=Out[2]=1;*/return 0;}
    const bool mustRepeatAnimation = (nasi.postState == BoneAnimationInfo::AB_REPEAT) && sz>1;
    if (!mustRepeatAnimation && looping!=0 && !m_playAnimationsAgainWhenOvertime) {
        Out = keys[sz-1].value;
        return sz - 1;
    }
    const bool isFirstLoop = mustRepeatAnimation && looping==0; // used only when mustRepeatAnimation == true;
    unsigned ScalingIndex;
    if (!pAlreadyKnownScalingIndex)	{
        ScalingIndex = 0;bool found = false;
        if (!mustRepeatAnimation || isFirstLoop)   {    // All cases with sz==1 should en here too (to check)
            const float cleanAnimationTime = AnimationTime - additionalPoseTransitionTime;
            if (sz == 1 || cleanAnimationTime < keys[0].time) {
                ScalingIndex=0;found = true;
            }
            else {
                for (unsigned i = 0; i < sz - 1 ; i++) {
                    if (cleanAnimationTime < keys[i + 1].time) {ScalingIndex = i;found=true;break;}
                }
            }
            if (!found) {
                if (!mustRepeatAnimation)   {
                    Out = keys[sz-1].value;
                    return sz - 1;
                }
                else ScalingIndex=sz-1;
            }
        }
        else {
            if (AnimationTime < keys[0].time) {
                ScalingIndex=sz-1;found = true;            // This happens after ai.numTicks in the figure.
            }
            else {
                for (unsigned i = 0; i < sz - 1 ; i++) {
                    if (AnimationTime < keys[i + 1].time) {ScalingIndex = i;found=true;break;}
                }
            }
            if (!found) {
                ScalingIndex=sz-1;                         // This happens between 1 and ai.numTicks in the figure.
            }
        }
    }
    else ScalingIndex = *pAlreadyKnownScalingIndex;    	
    if ((!mustRepeatAnimation || isFirstLoop) && ScalingIndex==0 && AnimationTime < keys[0].time+additionalPoseTransitionTime)	{
		const glm::vec3& Start = Out;//keys[sz-1].value; 
		const glm::vec3& End = keys[0].value; 
		//if (bf.scalingPoseTransitionTime<0) bf.scalingPoseTransitionTime=0;
		float deltaTime = keys[0].time+additionalPoseTransitionTime - bf.scalingPoseTransitionTime;
		float Factor = (AnimationTime - bf.scalingPoseTransitionTime) / deltaTime;
		bf.scalingPoseTransitionTime = AnimationTime;
		///Factor???
        Out = Start +(End - Start) * Factor;
    	return ScalingIndex;
	}
	else {
		bf.scalingPoseTransitionTime = 0.f;
        if ((!mustRepeatAnimation && ScalingIndex >= sz - 1) || sz<2) {return ScalingIndex;}
	}		
    unsigned NextScalingIndex = (ScalingIndex + 1);
    if (mustRepeatAnimation && NextScalingIndex>=sz) NextScalingIndex = 0;   //
    else assert(NextScalingIndex < sz);
    assert(ScalingIndex<sz && NextScalingIndex < sz);
    float Factor;
    if (!pAlreadyKnownFactor)	{
        if (NextScalingIndex==0) {          //
            assert(mustRepeatAnimation);
            // PERFECT!
            if (isFirstLoop)    {
                float DeltaTime = keys[NextScalingIndex].time+ ai.numTicks - keys[ScalingIndex].time;
                float cleanAnimationTime = AnimationTime - (keys[ScalingIndex].time + additionalPoseTransitionTime);
                Factor =  cleanAnimationTime / DeltaTime;
                //if (sz==2 && (Factor<0 || Factor>1)) printf("1) Factor= %1.4f looping:%d ScalingIndex = %d AnimationTime = %1.4f additionalPoseTransitionTime=%1.4f ai.numTicks = %1.4f cleanAnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,looping,ScalingIndex,AnimationTime,additionalPoseTransitionTime,ai.numTicks,cleanAnimationTime,DeltaTime,NextScalingIndex,keys[NextScalingIndex].time,ScalingIndex,keys[ScalingIndex].time,sz);
            }
            else {
                const bool newLoop = AnimationTime < keys[0].time;
                float DeltaTime = keys[NextScalingIndex].time + ai.numTicks - keys[ScalingIndex].time;
                float cleanAnimationTime = newLoop ? (AnimationTime + ai.numTicks - keys[ScalingIndex].time) :
                                                     (AnimationTime - keys[ScalingIndex].time);
                Factor =  cleanAnimationTime / DeltaTime;
                //if (sz==2 && (Factor<0 || Factor>1)) printf("3) Factor= %1.4f looping:%d ScalingIndex = %d AnimationTime = %1.4f additionalPoseTransitionTime=%1.4f ai.numTicks = %1.4f cleanAnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,looping,ScalingIndex,AnimationTime,additionalPoseTransitionTime,ai.numTicks,cleanAnimationTime,DeltaTime,NextScalingIndex,keys[NextScalingIndex].time,ScalingIndex,keys[ScalingIndex].time,sz);
            }
        }
        else {
            float DeltaTime = keys[NextScalingIndex].time - keys[ScalingIndex].time;
            Factor = (AnimationTime - (keys[ScalingIndex].time+additionalPoseTransitionTime)) / DeltaTime;
            //if (mustRepeatAnimation && sz==2) printf("2) Factor= %1.4f ScalingIndex = %d AnimationTime = %1.4f DeltaTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d\n",Factor,ScalingIndex,AnimationTime,DeltaTime,NextScalingIndex,keys[NextScalingIndex].time,ScalingIndex,keys[ScalingIndex].time,sz);
        }
        //if (!(Factor >= 0.0f && Factor <= 1.0f)) printf("Factor= %1.4f ScalingIndex = %d AnimationTime = %1.4f keys[%d].time = %1.4f keys[%d].time = %1.4f  numKeys = %d looping = %d additionalPoseTransitionTime = %1.4f\n",Factor,ScalingIndex,AnimationTime,NextScalingIndex,keys[NextScalingIndex].time,ScalingIndex,keys[ScalingIndex].time,sz,looping,additionalPoseTransitionTime);
        assert(Factor >= 0.0f && Factor <= 1.0f);
    }
    else Factor = *pAlreadyKnownFactor;
    if (pFactorOut) *pFactorOut = Factor;
    const glm::vec3& Start = keys[ScalingIndex].value;
    const glm::vec3& End   = keys[NextScalingIndex].value;
    Out = Start +(End - Start) * Factor;
    return ScalingIndex;
}
void Mesh::ReadBoneHeirarchy(unsigned animationIndex,float AnimationTime,float additionalPoseTransitionTime,const BoneInfo& bi, const glm::mat4& ParentTransform, MeshInstance& mi,int looping,const AnimationInfo& ai)	{
	if (bi.isUseless) return;	// Exit early
    Mat4 GlobalTransformation = ParentTransform;         
   
    const BoneAnimationInfo* pbai = bi.getBoneAnimationInfo(animationIndex);
    if (pbai) {
    	MeshInstance::BoneFootprint& bf = mi.boneFootprints[bi.index];
		const BoneAnimationInfo& bai = *pbai;		
		if (bi.preAnimationTransformIsPresent) 	GlobalTransformation*=bi.multPreTransformPreAnimationTransform;



		const bool hasTranslationKeys 	= bai.translationKeys.size()>0;
       	const bool hasRotationKeys 		= bai.rotationKeys.size()>0;
       	const bool hasScalingKeys 		= bai.scalingKeys.size()>0;
       	unsigned translationIndex(0),rotationIndex(0);
       	float translationFactor(0),rotationFactor(0);
       	
        if (hasTranslationKeys)	{
            translationIndex = CalcInterpolatedPosition(AnimationTime,additionalPoseTransitionTime, bai, bf, NULL,NULL,&translationFactor,looping,ai);	// Interpolate translation and generate translation transformation matrix
			GlobalTransformation = glm::translate(GlobalTransformation,bf.translation);
		}        	
   		if (bi.postAnimationTransformIsPresent) GlobalTransformation*=bi.postAnimationTransform;
		if (hasRotationKeys) 	{ 
			if (bai.tkAndRkHaveUniqueTimePerKey && hasTranslationKeys)              
                 rotationIndex = CalcInterpolatedRotation(AnimationTime,additionalPoseTransitionTime, bai, bf,&translationIndex,&translationFactor,&rotationFactor,looping,ai);  // Interpolate rotation and generate rotation transformation matrix
            else rotationIndex = CalcInterpolatedRotation(AnimationTime,additionalPoseTransitionTime, bai, bf, NULL,NULL,&rotationFactor,looping,ai);
            GlobalTransformation*=glm::mat4_cast(bf.rotation);
		}
		if (hasScalingKeys)	{
            if (bai.tkAndSkHaveUniqueTimePerKey && hasTranslationKeys)   CalcInterpolatedScaling(AnimationTime,additionalPoseTransitionTime, bai, bf,	&translationIndex, &translationFactor, NULL,looping,ai);
            else if (bai.rkAndSkHaveUniqueTimePerKey && hasRotationKeys) CalcInterpolatedScaling(AnimationTime,additionalPoseTransitionTime, bai, bf,	&rotationIndex, &rotationFactor, NULL,looping,ai);
            else 														 CalcInterpolatedScaling(AnimationTime,additionalPoseTransitionTime, bai, bf,	NULL, NULL, NULL,looping,ai);
        	GlobalTransformation = glm::scale(GlobalTransformation,bf.scaling);
		}        																						       							        					        						
   		
    }
    else {
    	if (bi.preTransformIsPresent) GlobalTransformation*=bi.preTransform;
    	GlobalTransformation *= bi.transform;
	}    	
	
	//if (!bi.isDummyBone) bi.finalTransformation = GlobalTransformation * bi.boneOffset;	 				
	//else bi.finalTransformation = GlobalTransformation;	//bi.boneOffset==mat(1)
    if (!bi.isDummyBone) mi.currentPose[bi.index] =  GlobalTransformation * bi.boneOffset;	//Are dummy bones all at the end ???
        
    for (unsigned i = 0,sz = bi.childBoneInfos.size() ; i < sz ; i++) {
        ReadBoneHeirarchy(animationIndex,AnimationTime,additionalPoseTransitionTime, *(bi.childBoneInfos[i]), GlobalTransformation, mi,looping,ai);
    }	
}
void Mesh::getBoneTransforms(unsigned animationIndex,float TimeInSeconds,float additionalPoseTransitionTime, MeshInstance& mi)	{
	if (animationIndex>=m_animationInfos.size() || m_boneInfos.size()==0) return;
	const AnimationInfo& animationInfo = m_animationInfos[animationIndex];
	
    glm::mat4 Identity;

    float AdditionalPoseTransitionTimeInTicks = additionalPoseTransitionTime * animationInfo.ticksPerSecond;

    float TotalTimeInSeconds = TimeInSeconds;
    float TotalTimeInTicks = TotalTimeInSeconds * animationInfo.ticksPerSecond;
    
    float AnimationTime;
    int looping = 0;

    // TODO: this first 'if' checks if the animation is over: we could decide to 'stop' it to a 'pose' passed as an argument
    if (TotalTimeInTicks > animationInfo.numTicks + AdditionalPoseTransitionTimeInTicks) {
        looping = (int)((TotalTimeInTicks-AdditionalPoseTransitionTimeInTicks)/animationInfo.numTicks);
        TotalTimeInTicks -= (animationInfo.numTicks + AdditionalPoseTransitionTimeInTicks);
		AdditionalPoseTransitionTimeInTicks = 0;
        AnimationTime = fmod(TotalTimeInTicks, animationInfo.numTicks);
        //printf("AdditionalPoseTransitionTimeInTicks = %1.4f\n",AdditionalPoseTransitionTimeInTicks);	//TODO: make it work, cause it doesn't
        //if (AnimationTime>animationInfo.numTicks) fprintf(stderr,"ERROR: AnimationTime(%1.4f) > animationInfo.numTicks(%1.4f)\n",AnimationTime,animationInfo.numTicks);
    }
	else  AnimationTime = TotalTimeInTicks;	
    //printf("AdditionalPoseTransitionTimeInTicks = %1.4f\n",AdditionalPoseTransitionTimeInTicks);

	if (mi.currentPose.size()<numValidBones) mi.currentPose.resize(numValidBones);
	if (mi.boneFootprints.size()<m_boneInfos.size()) mi.boneFootprints.resize(m_boneInfos.size());

	for (unsigned i = 0,sz = m_rootBoneInfos.size() ; i < sz ; i++) {
        ReadBoneHeirarchy(animationIndex,AnimationTime,AdditionalPoseTransitionTimeInTicks, *(m_rootBoneInfos[i]), Identity,mi,looping,animationInfo);
    }	
}

void Mesh::ReadBoneHeirarchyForPose(unsigned animationIndex,int rotationKeyFrameNumber,int translationKeyFrameNumber,int scalingKeyFrameNumber,const BoneInfo& bi, const glm::mat4& ParentTransform, MeshInstance& mi)	{
	if (bi.isUseless) return;	// Exit early
    Mat4 GlobalTransformation = ParentTransform;         
   
    const BoneAnimationInfo* pbai = bi.getBoneAnimationInfo(animationIndex);
    if (pbai) {
    	MeshInstance::BoneFootprint& bf = mi.boneFootprints[bi.index];
		const BoneAnimationInfo& bai = *pbai;		
		if (bi.preAnimationTransformIsPresent) 	GlobalTransformation*=bi.multPreTransformPreAnimationTransform;

        const bool hasTranslationKeys 	= (translationKeyFrameNumber>=0 && (int)bai.translationKeys.size()>translationKeyFrameNumber);
        const bool hasRotationKeys 		= (rotationKeyFrameNumber>=0 && (int)bai.rotationKeys.size()>rotationKeyFrameNumber);
        const bool hasScalingKeys 		= (scalingKeyFrameNumber>=0 && (int)bai.scalingKeys.size()>scalingKeyFrameNumber);
       	
        if (hasTranslationKeys)	{
        	bf.translation = bai.translationKeys[translationKeyFrameNumber].value;
			GlobalTransformation = glm::translate(GlobalTransformation,bf.translation);
		}        	
   		if (bi.postAnimationTransformIsPresent) GlobalTransformation*=bi.postAnimationTransform;
		if (hasRotationKeys) 	{ 
			bf.rotation = bai.rotationKeys[rotationKeyFrameNumber].value;
            GlobalTransformation*=glm::mat4_cast(bf.rotation);
		}
		if (hasScalingKeys)	{
        	bf.scaling = bai.scalingKeys[scalingKeyFrameNumber].value;
			GlobalTransformation = glm::scale(GlobalTransformation,bf.scaling);
		}        																						       							        					        						
   		
    }
    else {
    	if (bi.preTransformIsPresent) GlobalTransformation*=bi.preTransform;
    	GlobalTransformation *= bi.transform;
	}    	
	
	if (!bi.isDummyBone) mi.currentPose[bi.index] =  GlobalTransformation * bi.boneOffset;
        
    for (unsigned i = 0,sz = bi.childBoneInfos.size() ; i < sz ; i++) {
        ReadBoneHeirarchyForPose(animationIndex,rotationKeyFrameNumber,translationKeyFrameNumber,scalingKeyFrameNumber, *(bi.childBoneInfos[i]), GlobalTransformation, mi);
    }	
}
void Mesh::getBoneTransformsForPose(unsigned animationIndex,int rotationKeyFrameNumber,int translationKeyFrameNumber,int scalingKeyFrameNumber, MeshInstance& mi)	{
	if (animationIndex>=m_animationInfos.size() || m_boneInfos.size()==0) return;
    //const AnimationInfo& animationInfo = m_animationInfos[animationIndex];
	
    glm::mat4 Identity;
	if (mi.currentPose.size()<numValidBones) mi.currentPose.resize(numValidBones);
	if (mi.boneFootprints.size()<m_boneInfos.size()) mi.boneFootprints.resize(m_boneInfos.size());

	for (unsigned i = 0,sz = m_rootBoneInfos.size() ; i < sz ; i++) {
		ReadBoneHeirarchyForPose(animationIndex,rotationKeyFrameNumber,translationKeyFrameNumber,scalingKeyFrameNumber, *(m_rootBoneInfos[i]), Identity,mi);       	
    }
}


//endregion
//endregion

bool Mesh::loadFromFile(const char* filePath/*const filesystem::path& filePath*/,int flags)	{
    if (!filePath) return false;
    const string ext = Path::GetExtension(string(filePath));
    //fprintf(stderr,"ext=%s\n",ext.c_str());
    if (ext==".blend") return loadFromAnimatedBlendFile(filePath,flags);
    printf("Error while trying to load file: %s\n. File format is not supported.\n",filePath/*.file_string()*/);
	return false;
}

void Mesh::generateVertexArrays(int meshPartIndex) {
 for (size_t i=0;i<meshParts.size();i++)	{
     if (meshPartIndex>=0 && meshPartIndex!=(int)i) continue;
 	meshParts[i].generateVertexArrays(*this);
 }
}
void Mesh::Part::generateVertexArrays(const Mesh& parentMesh)	{
	batch.reset();
    batch = GLMiniTriangleBatch::Create();
    const Mesh::Part& part = (useVertexArrayInPartIndex<0 || useVertexArrayInPartIndex>=(int)parentMesh.meshParts.size()) ? (*this) : parentMesh.meshParts[useVertexArrayInPartIndex];

    /*
    if (part.norms.size()==0)	{
    	gl::GLMiniTriangleBatch::CalculateNormalsAndTangents(part.verts,inds,part.norms,false,&part.texCoords,&tangs,NULL,primitive);
    }
    */
    batch->upload< IndexType >(&inds,&part.verts,NULL,&part.norms,NULL/*&tangs*/,&part.texcoords,
                                &part.boneIds,&part.boneWeights		// commentable out
    						);

    //batch->displayInfo();
    //system("PAUSE");
        
	/*
	vao.reset();
	vao = bf::GLVertexArrayObject::Create();
	vao->bind();

	ibo.reset();
	if (inds.size()>0)	{	
		ibo = bf::GLArrayBuffer::Create(GL_ELEMENT_ARRAY_BUFFER);
    	ibo->bindBuffer(GL_ELEMENT_ARRAY_BUFFER);
    	ibo->bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(IndexType) * inds.size(), &inds[0], GL_STATIC_DRAW);
	}
	    
    vbo.reset();
    const Mesh::Part& part = (useVertexArrayInPartIndex<0 || useVertexArrayInPartIndex>=parentMesh.meshParts.size()) ? (*this) : parentMesh.meshParts[useVertexArrayInPartIndex];
    if (part.verts.size()>0)	{
    	vbo = bf::GLVertexBuffer::Create(GL_ARRAY_BUFFER);
    	vbo->bind(GL_ARRAY_BUFFER);
    	vbo->bufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * part.verts.size(), &part.verts[0], GL_STATIC_DRAW);
	}
	
	bf::GLVertexArrayObject::Unbind();
	*/

}
void Mesh::render(int meshPartIndex) const
{
/*
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
*/
	const size_t numMeshParts = meshParts.size();
    const size_t start = (meshPartIndex>=0 && meshPartIndex<(int)numMeshParts) ? meshPartIndex : 0;
    const size_t end   = (meshPartIndex>=0 && meshPartIndex<(int)numMeshParts) ? start+1 : numMeshParts;
	
    for (size_t i = start ; i < end ; i++) {
    	const Part& meshPart = meshParts[i];

        if (meshPart.visible && meshPart.batch)	{
            //if (meshPart.texture) meshPart.texture->bind();
            if (meshPart.textureId) glBindTexture(GL_TEXTURE_2D,meshPart.textureId);
            else glBindTexture(GL_TEXTURE_2D,Mesh::WhiteTextureId);


        	//mesh.material ?
        	//void drawGL(bool autoBindAndUnbindBuffers,bool autoEnableDisableVertexArrayAttribs=true)
        	//void drawGL(GLint startIndex=0,GLsizei indexCount=-1,bool autoBindAndUnbindBuffers=true,bool autoEnableDisableVertexArrayAttribs=true) 
        	meshPart.batch->drawGL();//bool autoBindAndUnbindBuffers,bool autoEnableDisableVertexArrayAttribs=true)
         //glDrawElements(GL_TRIANGLES, meshPart.inds.size(), GL_UNSIGNED_INT, 0);
        }
    }
/*
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
*/    
}
void Mesh::render(const OpenGLSkinningShaderData& p,int meshPartIndex,const vector<Material*>* pOptionalMaterialOverrides,const vector<GLuint>* pOptionalTextureOverrides,const vector<bool>* pOptionalVisibilityOverrides) const	{
	const size_t numMeshParts = meshParts.size();
    const size_t start = (meshPartIndex>=0 && meshPartIndex<(int)numMeshParts) ? meshPartIndex : 0;
    const size_t end   = (meshPartIndex>=0 && meshPartIndex<(int)numMeshParts) ? start+1 : numMeshParts;

    const size_t numOfMaterialOverrides = pOptionalMaterialOverrides ? pOptionalMaterialOverrides->size() : 0;
    const size_t numOfTextureOverrides = pOptionalTextureOverrides ? pOptionalTextureOverrides->size() : 0;
    const size_t numOfVisibilityOverrides = pOptionalVisibilityOverrides ? pOptionalVisibilityOverrides->size() : 0;

    GLuint textureId;
    bool visible;

    for (size_t i = start ; i < end ; i++) {
    	const Part& meshPart = meshParts[i];

        visible = numOfVisibilityOverrides>i ? (*pOptionalVisibilityOverrides)[i]  : meshPart.visible;
        if (visible && meshPart.batch)	{
            const Material& M = numOfMaterialOverrides>i ?  ( (*pOptionalMaterialOverrides)[i] ?  *(*pOptionalMaterialOverrides)[i] : meshPart.material) : meshPart.material;
            if (M.dif[3]==0.f) continue; // Skip alpha = 0.f

            //if (meshPart.texture) meshPart.texture->bind();
            textureId = numOfTextureOverrides>i ? ( (*pOptionalTextureOverrides)[i] && (*pOptionalTextureOverrides)[i]>0) : meshPart.textureId;
            if (textureId) glBindTexture(GL_TEXTURE_2D,textureId);
            else glBindTexture(GL_TEXTURE_2D,Mesh::WhiteTextureId);

            glUniform4fv(p.material_ambientUnifLoc,1,&M.amb[0]);
            glUniform4fv(p.material_diffuseUnifLoc,1,&M.dif[0]);
            glUniform4fv(p.material_emissionUnifLoc,1,&M.emi[0]);
            glUniform4fv(p.material_specularUnifLoc,1,&M.spe[0]);
            glUniform1f(p.material_shininessUnifLoc,M.shi);

        	//mesh.material ?
        	//void drawGL(bool autoBindAndUnbindBuffers,bool autoEnableDisableVertexArrayAttribs=true)
        	//void drawGL(GLint startIndex=0,GLsizei indexCount=-1,bool autoBindAndUnbindBuffers=true,bool autoEnableDisableVertexArrayAttribs=true) 
        	meshPart.batch->drawGL();//bool autoBindAndUnbindBuffers,bool autoEnableDisableVertexArrayAttribs=true)
         //glDrawElements(GL_TRIANGLES, meshPart.inds.size(), GL_UNSIGNED_INT, 0);
        }
    }	
}


#define DIFFERENT_DISPLAY_SKELETON_IMPLEMENTATION
#ifndef DIFFERENT_DISPLAY_SKELETON_IMPLEMENTATION
static void DisplaySkeletonLine(const Mesh::BoneInfo& bi,const vector<glm::mat4>& m_boneMatrices,const glm::vec3* pOptionalScaling)	{
		// Warning: m_boneMatrices.size() = numValidBones < m_boneIndos.size()!
		for (unsigned c=0,csz=bi.childBoneInfos.size();c<csz;c++)	{
			const Mesh::BoneInfo* pbic = bi.childBoneInfos[c];
			if (!pbic) continue;
			const Mesh::BoneInfo& bic = *pbic;
			if (!bi.isDummyBone && !bic.isDummyBone && bi.index<m_boneMatrices.size() && bic.index<m_boneMatrices.size())		// Mandatry to avoid crashes
			{
                const glm::mat4& mp = m_boneMatrices[bi.index];     // This does not take into account the distance between bone tail and bone head inside b, does it ?
				const glm::mat4& mc = m_boneMatrices[bic.index];
				if (pOptionalScaling)	{
					glVertex3fv(glm::value_ptr(glm::vec3(mp[3][0],mp[3][1],mp[3][2])*(*pOptionalScaling)));
					glVertex3fv(glm::value_ptr(glm::vec3(mc[3][0],mc[3][1],mc[3][2])*(*pOptionalScaling)));				
				}
				else
					{
					glVertex3f(mp[3][0],mp[3][1],mp[3][2]);
					glVertex3f(mc[3][0],mc[3][1],mc[3][2]);	
				}					
			}
			DisplaySkeletonLine(bic,m_boneMatrices,pOptionalScaling);
		}
}

void Mesh::displaySkeleton(const glm::mat4& mvpMatrix,const vector<glm::mat4>* pTransforms,const glm::mat4* pOptionalTranslationAndRotationOffset,const glm::vec3* pOptionalScaling) const	{
	if (pOptionalScaling) {assert((*pOptionalScaling)[0]!=0 && (*pOptionalScaling)[1]!=0 && (*pOptionalScaling)[2]!=0);}
	const glm::vec3 scalingInverse(glm::vec3(1)/(pOptionalScaling ? (*pOptionalScaling) : glm::vec3(1)));
	const GLfloat spheresVisualSize(0.12f);
	const GLfloat linesVisualSize(0.75f);
	//--------------------------------------------------
	
	if (m_boneInfos.size()==0) return;
	if (pTransforms && pTransforms->size()<numValidBones/*m_boneInfos.size()*/) pTransforms = NULL;
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(mvpMatrix));
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glColor3f(0.4f,0.6f,0.4f);
	static vector <glm::mat4> m_boneMatrices;m_boneMatrices.resize(numValidBones);//m_boneInfos.size());
	for (unsigned i=0,sz=numValidBones/*m_boneInfos.size()*/;i<sz;i++)	{
		const BoneInfo& bi = m_boneInfos[i];
		if (bi.isUseless) continue;
		glPushMatrix();	
			//if (pOptionalScaling) glScalef((*pOptionalScaling)[0],(*pOptionalScaling)[1],(*pOptionalScaling)[2]);			
			if (pOptionalTranslationAndRotationOffset)	glMultMatrixf(glm::value_ptr(*pOptionalTranslationAndRotationOffset));
			m_boneMatrices[i] = (pTransforms ? (*pTransforms)[i] : bi.finalTransformation) * bi.boneOffsetInverse;	
			if (pOptionalScaling) glScalef((*pOptionalScaling)[0],(*pOptionalScaling)[1],(*pOptionalScaling)[2]);	
			glMultMatrixf(glm::value_ptr(m_boneMatrices[i]));
			if (pOptionalScaling) glScalef(scalingInverse[0],scalingInverse[1],scalingInverse[2]);			
			glutSolidSphere(spheresVisualSize,6,6);
		glPopMatrix();
	}
	glColor3f(0.4f,0.4f,0.6f);
	glLineWidth(linesVisualSize);
	glPushMatrix();	
		//if (pOptionalScaling) glScalef((*pOptionalScaling)[0],(*pOptionalScaling)[1],(*pOptionalScaling)[2]);			
		if (pOptionalTranslationAndRotationOffset)	glMultMatrixf(glm::value_ptr(*pOptionalTranslationAndRotationOffset));
		glBegin(GL_LINES);
		for (unsigned i=0,sz=m_boneInfos.size();i<sz;i++)	{
			const BoneInfo& bi = m_boneInfos[i];
			if (bi.parentBoneInfo!=NULL) continue;
			DisplaySkeletonLine(bi,m_boneMatrices,pOptionalScaling);
		}				
		glEnd();
	glPopMatrix();
	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	
}
#else //DIFFERENT_DISPLAY_SKELETON_IMPLEMENTATION
void Mesh::displaySkeleton(const glm::mat4& mvpMatrix,const vector<glm::mat4>* pTransforms,const glm::mat4* pOptionalTranslationAndRotationOffset,const glm::vec3* pOptionalScaling) const	{
    if (pOptionalScaling) {assert((*pOptionalScaling)[0]!=0 && (*pOptionalScaling)[1]!=0 && (*pOptionalScaling)[2]!=0);}
    const glm::vec3 scalingInverse(glm::vec3(1)/(pOptionalScaling ? (*pOptionalScaling) : glm::vec3(1)));
    //const GLfloat spheresVisualSize(0.12f);
    const GLfloat linesVisualSize(1.25f);//(0.75f);
    //--------------------------------------------------

    if (m_boneInfos.size()==0) return;
    if (pTransforms && pTransforms->size()<numValidBones/*m_boneInfos.size()*/) pTransforms = NULL;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    std::vector<Vector3> boneHeadPoints;boneHeadPoints.resize(numValidBones);
    std::vector<Vector3> boneTailPoints;boneTailPoints.resize(numValidBones);   // WRONG! I'm not able to fix this

    glLoadMatrixf(glm::value_ptr(mvpMatrix));

    // bonePoints (fill and draw)
    glColor3f(0.4f,0.6f,0.4f);
    static vector <glm::mat4> m_boneMatrices;m_boneMatrices.resize(numValidBones);//m_boneInfos.size());
    for (unsigned i=0,sz=numValidBones/*m_boneInfos.size()*/;i<sz;i++)	{
        const BoneInfo& bi = m_boneInfos[i];
        if (bi.isUseless) continue;
        Mat4 tmp(1);
        glPushMatrix();
            //if (pOptionalScaling) {tmp = glm::scale(tmp,Vector3(*pOptionalScaling)[0],(*pOptionalScaling)[1],(*pOptionalScaling)[2]);}
            if (pOptionalTranslationAndRotationOffset)	tmp*=(*pOptionalTranslationAndRotationOffset);
            m_boneMatrices[i] = (pTransforms ? (*pTransforms)[i] : bi.finalTransformation) * bi.boneOffsetInverse;
            if (pOptionalScaling) tmp = glm::scale(tmp,*pOptionalScaling);
            tmp*=m_boneMatrices[i];
            if (pOptionalScaling) tmp = glm::scale(tmp,scalingInverse);
            glMultMatrixf(glm::value_ptr(tmp));
            glm::vec4 v(0,0,0,1);
            boneHeadPoints[i] = glm::vec3(tmp*(v));
            // WRONG PART--------------------------------------------------------------------------
            /*
            {
                const Mat4& tm = bi.transform;
                v[0] = tm[3][0];v[1] = tm[3][1];v[2] = tm[3][2];v[3] = tm[3][3];
                //v[0] = tm[0][3];v[1] = tm[1][3];v[2] = tm[2][3];v[3] = tm[3][3];
            }
            //boneTailPoints[i] = glm::vec3(tmp*(v)); //boneHeadPoints[i] + glm::vec3(bi.transform*v);    // THIS LINE IS WRONG!!!
            */
            boneTailPoints[i] = boneHeadPoints[i];  // wrong fallback (to remove if a solution is found)
            //--------------------------------------------------------------------------------------
            //glutSolidSphere(spheresVisualSize,6,6);   // We can't use it!!!
            fprintf(stderr,"Error: displaySkeleton(...) needs glutSolidSphere.\n");
        glPopMatrix();
    }

    //#define DEBUG_ME
    #ifdef DEBUG_ME
    static bool firstTime = true;
    {
        if (firstTime) {
            firstTime = false;
            for (unsigned i=0,sz=numValidBones;i<sz;i++)	{
                const Vector3& bhp = boneHeadPoints[i];
                const Vector3& btp = boneTailPoints[i];
                printf("boneHeadPoints[%d] = {%1.4f %1.4f   %1.4f};\t\tboneTailPoints[%d] = {%1.4f %1.4f   %1.4f};\n",i,bhp[0],bhp[1],bhp[2],btp[0],btp[1],btp[2]);
            }
        }
    }
    #undef DEBUG_ME
    #endif //DEBUG_ME

    // connect bonePoints together:
    glColor3f(0.4f,0.4f,0.6f);
    glLineWidth(linesVisualSize);
    glPushMatrix();
        glBegin(GL_LINES);
        for (unsigned i=0,sz=numValidBones;i<sz;i++)	{
            const BoneInfo& bi = m_boneInfos[i];
            if (bi.isUseless) continue;
            const Vector3& bhp = boneHeadPoints[bi.index];
            const Vector3& btp = boneTailPoints[bi.index];
            if (!bi.parentBoneInfo) {
                glVertex3f(bhp[0],bhp[1],bhp[2]);
                glVertex3f(btp[0],btp[1],btp[2]);
            }
            for (unsigned j=0,jsz = bi.childBoneInfos.size();j<jsz;j++) {
                const BoneInfo* bic = bi.childBoneInfos[j];
                if (!bic || bic->isDummyBone) continue;
                const Vector3& bhpc = boneHeadPoints[bic->index];
                glVertex3f(btp[0],btp[1],btp[2]);
                glVertex3f(bhpc[0],bhpc[1],bhpc[2]);
            }
        }
        glEnd();
    glPopMatrix();


    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

}

#undef DIFFERENT_DISPLAY_SKELETON_IMPLEMENTATION
#endif //DIFFERENT_DISPLAY_SKELETON_IMPLEMENTATION

void Mesh::PremultiplyBoneDescendants(const BoneInfo& parentBone,const Mat4& T,vector<Mat4>& Transforms,const vector <BoneInfo> m_boneInfos)	{
	for (unsigned i=0,sz=parentBone.childBoneInfos.size();i<sz;i++)	{
			const Mesh::BoneInfo* bi = parentBone.childBoneInfos[i];
			if (!bi) continue;
		 	if (!bi->isDummyBone)	{
		 		const unsigned index = bi->index;
		 		Transforms[index] = T * Transforms[index];
			}		 		
		 	PremultiplyBoneDescendants(*bi,T,Transforms,m_boneInfos);
	}
}

void Mesh::manuallySetPoseToBone(unsigned boneIndex,const glm::mat4& bonePose,vector<Mat4>& Transforms,bool applyPoseOverCurrentBonePose,bool bonePoseIsInLocalSpace) const	{
	const Mesh::BoneInfo* bi =  getBoneInfoFromBoneIndex(boneIndex);  
	if (!bi || bi->isDummyBone) return;  				        			
    const Mesh::BoneInfo* biParent = bi->parentBoneInfo;
	if (Transforms.size()<numValidBones/*m_boneInfos.size()*/) Transforms.resize(numValidBones);//m_boneInfos.size());      
    if (applyPoseOverCurrentBonePose) {
    	if (bonePoseIsInLocalSpace)	Transforms[bi->index] *= (biParent  ? (Transforms[biParent->index] * bi->boneOffsetInverse * bonePose * bi->boneOffset) : 
    																								     (bi->boneOffsetInverse * bonePose * bi->boneOffset) );
		else  Transforms[bi->index] *= (biParent  ? (Transforms[biParent->index] *  bonePose * bi->boneOffset) : 
    																				(bonePose * bi->boneOffset) );  																								     
	}    	
	else {
		if (bonePoseIsInLocalSpace)	Transforms[bi->index] = (biParent  ? (Transforms[biParent->index] * bi->boneOffsetInverse * bonePose * bi->boneOffset) : 
    							  																	     (bi->boneOffsetInverse * bonePose * bi->boneOffset) );
		else	Transforms[bi->index] = (biParent  ? (Transforms[biParent->index] * bonePose * bi->boneOffset) : 
    							  													(bonePose * bi->boneOffset) );
	}		    							  																	     	
	
	/* //	GENERAL THEORY (AFAIK):
    Transforms[bi->index] = Transforms[biParent->index] * 						// parent boneTransform *
        											bi->boneOffsetInverse  * 	// mesh vertices to bind pose		\ = global animation in model space (bone animation transforms stored in the asset import library are in global model space, not in bone space,i.e belong the this space)
        											localTransformToApply		// local animation in model space	/
        											* bi->boneOffset			// move to bone space         											
        											;
	*/
	// Now we must update all the descendants of bi, premultiplying per Transforms[index]
    PremultiplyBoneDescendants(*bi,Transforms[bi->index],Transforms,m_boneInfos);
	
}





#ifdef TEST_SOFTWARE_SKINNING
void Mesh::softwareRender(int meshPartIndex, const MeshInstance &mi) const {
    const size_t numMeshParts = meshParts.size();
    if (mi.meshPartFootprints.size()<numMeshParts) return;
    const size_t start = (meshPartIndex>=0 && (size_t)meshPartIndex<numMeshParts) ? (size_t)meshPartIndex : 0;
    const size_t end   = (meshPartIndex>=0 && (size_t)meshPartIndex<numMeshParts) ? start+1 : (size_t)numMeshParts;

    GLuint lastTextureId = 0;
    const Material* lastMaterial = NULL;

    const size_t numOfVisibilityOverrides = mi.visibilityOverrides.size();
    const size_t numOfMaterialOverrides = mi.materialOverrides.size();
    const size_t numOfTextureOverrides = mi.textureOverrides.size();

    int lmpvi=-1,mpvi;
    for (size_t partIndex = start ; partIndex < end ; partIndex++) {
        if (partIndex<numOfVisibilityOverrides && !mi.visibilityOverrides[partIndex]) continue;
        const Part& meshPart = meshParts[partIndex];
        mpvi = (meshPart.useVertexArrayInPartIndex>=0 && meshPart.useVertexArrayInPartIndex<(int)numMeshParts) ? meshPart.useVertexArrayInPartIndex : (int)partIndex;
        if (meshParts[mpvi].verts.size()==0) {
            if (lmpvi>=0) mpvi = lmpvi;
            else continue;
        }
        lmpvi = mpvi;

        const Part& effectiveMeshPart = meshParts[mpvi];
        const MeshInstance::MeshPartFootprint& effectiveMeshPartFootprint = mi.meshPartFootprints[mpvi];

        const Vector3Array& verts = effectiveMeshPartFootprint.verts; if (verts.size()==0) continue;
        const Vector3Array& norms = effectiveMeshPartFootprint.norms;
        const Vector2Array& texcoords = effectiveMeshPart.texcoords;
        const IndexArray& inds = meshPart.inds;
        GLuint textureId = (partIndex<numOfTextureOverrides && mi.textureOverrides[partIndex]!=0) ? mi.textureOverrides[partIndex] : meshPart.textureId;
        const Material& mat = (partIndex<numOfMaterialOverrides && mi.materialOverrides[partIndex]!=NULL) ? *mi.materialOverrides[partIndex] : meshPart.material;

        if (textureId!=lastTextureId || partIndex==start) {
            if (textureId) glBindTexture(GL_TEXTURE_2D,textureId);
            else glBindTexture(GL_TEXTURE_2D,Mesh::WhiteTextureId);
        }
        lastTextureId = textureId;const bool hasTexCoords = textureId!=0 && texcoords.size()>0;
        if (lastMaterial!=&mat) {
            const GLenum face = GL_FRONT;
            glMaterialfv(face,GL_AMBIENT,mat.amb);
            glMaterialfv(face,GL_DIFFUSE,mat.dif);
            glMaterialfv(face,GL_SPECULAR,mat.spe);
            glMaterialf(face,GL_SHININESS,mat.shi*128.0f);
            glMaterialfv(face,GL_EMISSION,mat.emi);
        }
        lastMaterial=&mat;


        // Here we'd better use vertex arrays (shorted mod) or VBO/VAO (code must be changed somewhere else too)
        // but how it's easier and shorter to just use immediate mode!
        glBegin(GL_TRIANGLES);
        for (size_t i=0,isz=inds.size();i<isz;i++)    {
            const IndexType ind = inds[i];
            glNormal3fv(glm::value_ptr(norms[ind]));
            if (hasTexCoords) glTexCoord2fv(glm::value_ptr(texcoords[ind]));
            glVertex3fv(glm::value_ptr(verts[ind]));
        }
        glEnd();


    }
}
void Mesh::softwareUpdateVertsAndNorms(MeshInstance& mi,const Vector3& scaling) const   {
    const size_t numMeshParts = meshParts.size();
    if (mi.meshPartFootprints.size()<numMeshParts) return;
    const Vector4 scaling4(scaling[0],scaling[1],scaling[2],1);
    const vector < glm::mat4 >& currentPose = mi.currentPose;
    Mat4 boneMatrix;
    for (size_t partIndex = 0 ; partIndex < numMeshParts ; partIndex++) {
        const Part& meshPart = meshParts[partIndex];
        const Vector3Array& verts = meshPart.verts;
        const Vector3Array& norms = meshPart.norms;
        if (verts.size()==0) continue;

        const vector< BoneIdsType >& boneIds = meshPart.boneIds;
        const vector< BoneWeightsType >& boneWeights = meshPart.boneWeights;    
        if (boneIds.size()<verts.size() || boneWeights.size()<verts.size()) continue;

        MeshInstance::MeshPartFootprint& meshFootprint = mi.meshPartFootprints[partIndex];
        Vector3Array& vertsOut = meshFootprint.verts;
        if (vertsOut.size()<verts.size()) continue;

        Vector3Array& normsOut = meshFootprint.norms;

        for (size_t vi=0,visz=verts.size();vi<visz;vi++) {
            const BoneIdsType& boneId = boneIds[vi];
            const BoneWeightsType& boneWeight = boneWeights[vi];
            if (boneWeight[0]>0) {
                short int bi=0;
                boneMatrix = currentPose[boneId[bi]]*boneWeight[bi];
                while (++bi<4 && boneWeight[bi]>0) boneMatrix += currentPose[boneId[bi]]*boneWeight[bi];
            }
            else boneMatrix = glm::mat4(1);


            const Vector3& v = verts[vi];
            #ifndef REQUIRE_GL_NORMALIZE
            vertsOut[vi] = glm::toVec3((boneMatrix * glm::vec4(v[0],v[1],v[2],1)) * scaling4);   // Optimizable
            #else //REQUIRE_GL_NORMALIZE
            vertsOut[vi] = glm::toVec3(boneMatrix * (glm::vec4(v[0],v[1],v[2],1)));   // ('scaling' is useless here) Optimizable
            #endif //REQUIRE_GL_NORMALIZE

            const Vector3& n = norms[vi];
            #ifndef REQUIRE_GL_NORMALIZE
            normsOut[vi] = glm::normalize(
                        glm::toVec3(boneMatrix * glm::vec4(n[0],n[1],n[2],0))               // Optimizable
                    );
            #else //REQUIRE_GL_NORMALIZE
            normsOut[vi] = glm::toVec3(boneMatrix * glm::vec4(n[0],n[1],n[2],0));               // Optimizable
            #undef REQUIRE_GL_NORMALIZE
            #endif //REQUIRE_GL_NORMALIZE
        }

    }

}
#endif //TEST_SOFTWARE_SKINNING


