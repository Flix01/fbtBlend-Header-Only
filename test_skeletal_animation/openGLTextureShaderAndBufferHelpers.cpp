#include "openGLTextureShaderAndBufferHelpers.h"
#include "opengl_includer.h"	// GL types and calls are included by this header


// All this is to safely include stb_image.h 
#ifndef STBI_INCLUDE_STB_IMAGE_H
#ifndef NO_STB_IMAGE_STATIC				// Tweakable
#define STB_IMAGE_STATIC
#endif //NO_STB_IMAGE_STATIC
#ifndef NO_STB_IMAGE_IMPLEMENTATION 	// Tweakable
#define STB_IMAGE_IMPLEMENTATION
#endif //NO_STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#ifndef NO_STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif //NO_STB_IMAGE_IMPLEMENTATION
#ifndef NO_STB_IMAGE_STATIC
#undef STB_IMAGE_STATIC
#endif //NO_STB_IMAGE_STATIC
#endif //STBI_INCLUDE_STB_IMAGE_H

#include <stdio.h> // FILE printf fprintf


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
GLuint OpenGLLoadTexture(const char* filename,int req_comp,bool useMipmapsIfPossible,bool wraps,bool wrapt)	{
    int w,h,n;
    unsigned char* pixels = stbi_load(filename,&w,&h,&n,req_comp);
    if (!pixels) {
        fprintf(stderr,"Error: can't load texture: \"%s\".\n",filename);
        return 0;
    }
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


// Simple class that uses STBI allocators/deallocators
class OpenGLMemoryBuffer {
	protected:
	char* data;
	public:
	OpenGLMemoryBuffer(const char* path=NULL) : data(NULL) {load(path);}
	OpenGLMemoryBuffer(size_t sizeToAllocate) : data(NULL) {allocateMemory(sizeToAllocate);}		
	const char* load(const char* path)	{
		freeMemory();
		if (!path || strlen(path)==0) return NULL;

	   const bool appendTrailingZero = true;
    	FILE* f;
    	if ((f = fopen(path, "r")) == NULL) return NULL;
    	if (fseek(f, 0, SEEK_END))  {
        	fclose(f);
        	return NULL;
	    }
    	const long f_size_signed = ftell(f);
    	if (f_size_signed == -1)    {
    	    fclose(f);
    	    return NULL;
    	}
	    size_t f_size = (size_t)f_size_signed;
    	if (fseek(f, 0, SEEK_SET))  {
    	    fclose(f);
    	    return NULL;
    	}
		// Allocate data
		allocateMemory(f_size+(appendTrailingZero?1:0));

    	const size_t f_size_read = fread(&data[0], 1, f_size, f);
    	fclose(f);
    	if (f_size_read == 0 || f_size_read!=f_size)    {freeMemory();return NULL;}
    	if (appendTrailingZero) data[f_size] = '\0';
		return data;
	}
	void allocateMemory(size_t size) {
		freeMemory();
		data = (char*) STBI_MALLOC(size);
	}
	void freeMemory() {
		if (data) {
			STBI_FREE(data);
			data = NULL;	
		}	
	}
	~OpenGLMemoryBuffer() {freeMemory();}
	
	inline const char* getData() const {return data;}
	inline char* getData() {return data;}
};


static GLuint OpenGLCreateShader(GLenum type,const GLchar** shaderSource, OpenGLCompileShaderStruct *pOptionalOptions, const char* shaderTypeName="Shader",const int prefixFileNumLines=0)    {

    const char* sourcePrefix = NULL;
    int numPrefixLines =  0;

    //Preprocess shader source
    const GLchar* pSource = *shaderSource;

    //Compile shader
    GLuint shader( glCreateShader( type ) );
    glShaderSource( shader, 1, &pSource, NULL );
    glCompileShader( shader );

    // check
    GLint bShaderCompiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &bShaderCompiled);

    if (!bShaderCompiled)        {
        int i32InfoLogLength, i32CharsWritten;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

        OpenGLMemoryBuffer buffer(i32InfoLogLength+2);char* pszInfoLog = buffer.getData();
		pszInfoLog[0]='\0';
        glGetShaderInfoLog(shader, i32InfoLogLength, &i32CharsWritten, &pszInfoLog[0]);
        if (numPrefixLines==0 && prefixFileNumLines==0) printf("********%s %s\n", shaderTypeName, &pszInfoLog[0]);
        else printf("********%s %s\n", shaderTypeName, &pszInfoLog[0]);
        fflush(stdout);
        glDeleteShader(shader);shader=0;
    }

    return shader;
}

GLuint OpenGLCompileShadersFromMemory(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource , OpenGLCompileShaderStruct *pOptionalOptions)	{
    GLuint vertexShader = 0, fragmentShader = 0;
    bool mustProcessVertexShaderSource = true, mustProcessFragmentShaderSource = true;
    if (pOptionalOptions)   {
        mustProcessVertexShaderSource = false;
        if (pOptionalOptions->vertexShaderOverride) vertexShader = pOptionalOptions->vertexShaderOverride;
        else if (!pOptionalOptions->programOverride) mustProcessVertexShaderSource = true;

        mustProcessFragmentShaderSource = false;
        if (pOptionalOptions->fragmentShaderOverride) fragmentShader = pOptionalOptions->fragmentShaderOverride;
        else if (!pOptionalOptions->programOverride) mustProcessFragmentShaderSource = true;
    }


    int vertexShaderSourcePrefixNumLines = 0;
    if (mustProcessVertexShaderSource)  {
        vertexShader = OpenGLCreateShader(GL_VERTEX_SHADER,vertexShaderSource,pOptionalOptions,"VertexShader");
    }

    int fragmentShaderSourcePrefixNumLines = 0;
    if (mustProcessFragmentShaderSource)    {
        fragmentShader = OpenGLCreateShader(GL_FRAGMENT_SHADER,fragmentShaderSource,pOptionalOptions,"FragmentShader");
    }

    //Link vertex and fragment shader together
    GLuint program = 0;
    if (pOptionalOptions && pOptionalOptions->programOverride!=0) program = pOptionalOptions->programOverride;
    else program = glCreateProgram();

    if (vertexShader)   glAttachShader( program, vertexShader );
    if (fragmentShader) glAttachShader( program, fragmentShader );

    if (pOptionalOptions && pOptionalOptions->dontLinkProgram)  {
        pOptionalOptions->programOverride = program;
        if (pOptionalOptions->dontDeleteAttachedShaders) {
            pOptionalOptions->vertexShaderOverride   = vertexShader;
            pOptionalOptions->fragmentShaderOverride = fragmentShader;
        }
        return program;
    }

    glLinkProgram( program );

    //check
    GLint bLinked;
    glGetProgramiv(program, GL_LINK_STATUS, &bLinked);
    if (!bLinked)        {
        int i32InfoLogLength, i32CharsWritten;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

        OpenGLMemoryBuffer buffer(i32InfoLogLength+2);char* pszInfoLog = buffer.getData();
		pszInfoLog[0]='\0';
        glGetProgramInfoLog(program, i32InfoLogLength, &i32CharsWritten, &pszInfoLog[0]);

        printf("********ShaderLinkerLog:\n%s\n", &pszInfoLog[0]);
        fflush(stdout);
        if (!pOptionalOptions || !pOptionalOptions->dontDeleteAttachedShaders) {
            glDetachShader(program,vertexShader);glDeleteShader(vertexShader);vertexShader=0;
            glDetachShader(program,fragmentShader);glDeleteShader(fragmentShader);fragmentShader=0;
        }
        glDeleteProgram(program);program=0;
    }

    //Delete shaders objects
    if (!pOptionalOptions || !pOptionalOptions->dontDeleteAttachedShaders)  {
        if (program)    {
            GLuint attachedShaders[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};GLsizei attachedShaderCount = 0;
            glGetAttachedShaders(program,sizeof(attachedShaders)/sizeof(attachedShaders[0]),&attachedShaderCount,attachedShaders);
            for (GLsizei i=0;i<attachedShaderCount;i++) glDeleteShader( attachedShaders[i] );
        }
    }

    return program;
}


GLuint OpenGLCompileShadersFromFile(const char* vertexShaderFilePath, const char* fragmentShaderFilePath, OpenGLCompileShaderStruct* pOptionalOptions) {
    OpenGLMemoryBuffer vertexShaderBuffer,fragmentShaderBuffer;
    bool vok = vertexShaderFilePath ? (vertexShaderBuffer.load(vertexShaderFilePath)!=NULL) : true;
    bool fok = fragmentShaderFilePath ? (fragmentShaderBuffer.load(fragmentShaderFilePath)!=NULL) : true;
    if (!vok || !fok)    {
        printf("Error in OpenGL_CompileShadersFromFile(\"%s\",\"%s\"). ",vertexShaderFilePath,fragmentShaderFilePath);
        if (!vok) printf("Vertex shader file not found. ");
        if (!fok) printf("Fragment shader file not found. ");
        fflush(stdout);
        return 0;
    }
    const char *pStartVertexShaderBuffer   = vertexShaderFilePath ? vertexShaderBuffer.getData() : NULL;
    const char *pStartFragmentShaderBuffer = fragmentShaderFilePath ? fragmentShaderBuffer.getData() : NULL;

    return OpenGLCompileShadersFromMemory(&pStartVertexShaderBuffer,&pStartFragmentShaderBuffer,pOptionalOptions);
}


glLightModelParams glLightModelParams::instance;

void OpenGLSkinningShaderData::destroy() {
	if (program) {
		glDeleteProgram(program);program=0;
	}
}
inline static GLint GetAttrLoc(GLuint p,const char* name)	{
	GLint rv = glGetAttribLocation(p,name);
    if (rv<0)
        fprintf(stderr,"Error: glGetAttribLocation(\"%s\")=%d;\n",name,rv);
	return rv;
}
inline static GLint GetUnifLoc(GLuint p,const char* name)	{
	GLint rv = glGetUniformLocation(p,name);
	if (rv<0) fprintf(stderr,"Error: glGetUniformLocation(\"%s\")=%d;\n",name,rv);
	return rv;
}
void OpenGLSkinningShaderData::init() {
	if (program) return;
    const GLchar* gVertexShaderSource[] = {
        "//-------------------- This part must be the same for both shaders ---------------------------\n"
        "// Generic skinning shader with vertex lighting (NO NORMAL MAPPING AND NO SHADOW!)\n"
        "// Full material and light properties. Configurable number of lights (but needs background C++ code)\n"
        "\n"
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "precision lowp int;\n"
        "#endif //GL_ES\n"
        "\n"
        "#define SPECULAR\n"
        "#define TEXTURE\n"
        "//#define USE_FOG\n"
        "#define NO_BRANCHING\n"
        "\n"
        "#ifdef TEXTURE\n"
        "varying vec2 v_texcoords;\n"
        "uniform sampler2D	u_textureUnit0;\n"
        "#endif //TEXTURE\n"
        "varying vec3 v_globalAmbientSum; 	 // global ambient only\n"
        "varying vec3 v_ambientSum; 	 	 // ambient only\n"
        "varying vec3 v_diffuseSum; 		 // u_material.diffuse.rgb is left outside if TEXTURE_USE_ALPHA_CHANNEL_AS_DIFFUSE_COLOR_MASK is defined\n"
        "#ifdef SPECULAR\n"
        "varying vec3 v_specularSum;\n"
        "#endif //SPECULAR\n"
        "#ifdef USE_FOG\n"
        "varying float v_fogFactor;\n"
        "#endif //USE_FOG\n"
        "\n"
        "struct MaterialParams\n"
        "{\n"
        "   vec4 emission;\n"
        "   vec4 ambient;\n"
        "   vec4 diffuse;\n"
        "   #ifdef SPECULAR\n"
        "   vec4 specular;\n"
        "   float shininess;\n"
        "   #endif\n"
        "};\n"
        "\n"
        "struct LightModelParams\n"
        "{\n"
        "	vec4 ambient;\n"
        "	#ifdef USE_FOG\n"
        "	vec4 fogColorAndDensity;\n"
        "	#endif //USE_FOG\n"
        "};\n"
        "\n"
        "uniform MaterialParams u_material;  // The material of the object to render\n"
        "uniform LightModelParams u_lightModel;	// A single global uniform\n"
        "\n"
        "//--------------------------------------------------------------------------------------------\n"
        "\n"
        "\n"
        "//--------------------------------\n"
        "#define NUM_DIRECTIONAL_LIGHTS 1\n"
        "#define NUM_POINT_LIGHTS 0\n"
        "#define NUM_SPOT_LIGHTS 0	    // Spot lights don't look good with vertex lighting...\n"
        "#define MAX_NUM_BONE_MATRICES 128\n"
        "//#define USE_INTEGER_BONE_INDICES\n"
        "//--------------------------------\n"
        "\n"
        "#define NUM_LIGHTS 	(NUM_DIRECTIONAL_LIGHTS+NUM_POINT_LIGHTS+NUM_SPOT_LIGHTS)\n"
        "#if NUM_LIGHTS>0\n"
        "	#define INV_NUM_LIGHTS	(1.0/(float(NUM_LIGHTS)))   // we divide the ambient light per num lights (it's an optional tweak)\n"
        "#endif //NUM_LIGHTS>0\n"
        "\n"
        "\n"
        "struct LightParams\n"
        "{\n"
        "    vec4 ambient;\n"
        "    vec4 diffuse;\n"
        "    #ifdef SPECULAR\n"
        "    vec4 specular;\n"
        "    #endif\n"
        "    vec4 vector; 	// normalized light direction in eye space for point and spot lights, otherwise normalized light direction in eye space\n"
        "			// For point and spot lights the last component is the attenuation factor (0 = no attenuation)\n"
        "\n"
        "	#if NUM_SPOT_LIGHTS>0\n"
        "		vec3 spotDirection;	// normalized spot direction in eye space\n"
        "		vec2 spotInnerAndOuterCosConeAngle;	//.x = spotCosCutoff; (0.6,0.8)	// 0.8 = 36 degrees\n"
        "		/*\n"
        "		vec3 D = normalize(u_spot_lights[i].spotDirection);\n"
        "		float cos_cur_angle = dot(-L, D);\n"
        "		float spot_falloff = clamp((cos_cur_angle - u_spot_lights[i].spotInnerAndOuterCosConeAngle.y) / (u_spot_lights[i].spotInnerAndOuterCosConeAngle.x - u_spot_lights[i].spotInnerAndOuterCosConeAngle.y), 0.0, 1.0);\n"
        "		// multiply everything (except ambient) per spot_falloff\n"
        "		*/\n"
        "	#endif //NUM_SPOT_LIGHTS\n"
        "};\n"
        "\n"
        "uniform mat4 u_pMatrix;       	// A constant representing the camera projection matrix.\n"
        "uniform mat4 u_mvMatrix;        // A constant representing the combined ORTHONORMAL model/view matrix (i.e. without any scaling applied).\n"
        "uniform vec3 u_mMatrixScaling;	// default (1,1,1). Note that the scale usually refers to the model matrix only, since the view matrix is usually orthonormal (gluLookAt(...) and similiar are OK)\n"
        "\n"
        "#if NUM_DIRECTIONAL_LIGHTS>0\n"
        "uniform LightParams u_directional_lights[NUM_DIRECTIONAL_LIGHTS];\n"
        "#endif //NUM_DIRECTIONAL_LIGHTS\n"
        "#if NUM_POINT_LIGHTS>0\n"
        "uniform LightParams u_point_lights[NUM_POINT_LIGHTS];\n"
        "#endif //NUM_POINT_LIGHTS\n"
        "#if NUM_SPOT_LIGHTS>0\n"
        "uniform LightParams u_spot_lights[NUM_SPOT_LIGHTS];\n"
        "#endif //NUM_SPOT_LIGHTS\n"
        "\n"
        "\n"
        "attribute vec4 a_vertex;       // vertex in object space\n"
        "attribute vec3 a_normal;       // normal in object space\n"
        "\n"
        "#ifdef TEXTURE\n"
        "attribute vec2 a_texcoords;       // normal in object space\n"
        "#endif //TEXTURE\n"
        "\n"
        "#ifndef USE_INTEGER_BONE_INDICES\n"
        "attribute vec4 a_boneInds;\n"
        "#else //USE_INTEGER_BONE_INDICES\n"
        "attribute ivec4 a_boneInds;\n"
        "#endif //USE_INTEGER_BONE_INDICES\n"
        "attribute vec4 a_boneWeights;\n"
        "uniform mat4 u_boneMatrices[MAX_NUM_BONE_MATRICES];\n"
        "\n"
        "\n"
        "// The entry point for our vertex shader.\n"
        "void main()\n"
        "{\n"
        "#ifdef TEXTURE\n"
        "v_texcoords = a_texcoords;\n"
        "#endif //TEXTURE\n"
        "\n"
        "#ifndef USE_INTEGER_BONE_INDICES\n"
        "mat4 boneMatrix = u_boneMatrices[int(a_boneInds.x)] * a_boneWeights.x +\n"
        "	u_boneMatrices[int(a_boneInds.y)] * a_boneWeights.y +\n"
        "	u_boneMatrices[int(a_boneInds.z)] * a_boneWeights.z +\n"
        "	u_boneMatrices[int(a_boneInds.w)] * a_boneWeights.w;\n"
        "#else // USE_INTEGER_BONE_INDICES\n"
        "mat4 boneMatrix = u_boneMatrices[a_boneInds.x] * a_boneWeights.x +\n"
        "	u_boneMatrices[a_boneInds.y] * a_boneWeights.y +\n"
        "	u_boneMatrices[a_boneInds.z] * a_boneWeights.z +\n"
        "	u_boneMatrices[a_boneInds.w] * a_boneWeights.w;\n"
        "#endif // USE_INTEGER_BONE_INDICES\n"
        "\n"
        "vec4 v_vertex = u_mvMatrix * ((boneMatrix * a_vertex) * vec4(u_mMatrixScaling,1));	// vertex in eye space\n"
        "\n"
        "gl_Position = u_pMatrix * v_vertex;\n"
        "\n"
        "//v_vertex.xyz = v_vertex.xyz/v_vertex.w; //needed? Before or after the previous line?\n"
        "//v_vertex.w = 1;\n"
        "\n"
        "vec3 v_normal = vec3(u_mvMatrix * (boneMatrix * vec4(a_normal, 0.0)));	//normal into eye space.\n"
        "v_normal = normalize(v_normal);\n"
        "\n"
        "#if NUM_LIGHTS>0\n"
        "	 //region Vertex lighting here:\n"
        "	vec3 normal = v_normal;\n"
        "	v_globalAmbientSum = u_material.emission.rgb + u_lightModel.ambient.rgb * u_material.ambient.rgb;\n"
        "\n"
        "	v_ambientSum = v_diffuseSum =\n"
        "	#ifdef SPECULAR\n"
        "	v_specularSum =\n"
        "	#endif //SPECULAR\n"
        "	vec3(0.0,0.0,0.0);\n"
        "\n"
        "	float fdot;\n"
        "	vec3 L;	// normalized light vector\n"
        "		#ifdef SPECULAR\n"
        "			vec3 E = normalize(-v_vertex).xyz; // we are in Eye Coordinates, so EyePos is (0,0,0)\n"
        "			vec3 halfVector;\n"
        "			float nxHalf;\n"
        "		#endif // SPECULAR\n"
        "\n"
        "		 //region Directional light here\n"
        "		#if NUM_DIRECTIONAL_LIGHTS>0\n"
        "		for (int i=0;i<NUM_DIRECTIONAL_LIGHTS;i++)\n"
        "		{\n"
        "			L = -u_directional_lights[i].vector.xyz;\n"
        "			fdot = max(dot(normal, L), 0.0);\n"
        "\n"
        "			v_ambientSum+=u_directional_lights[i].ambient.rgb;\n"
        "			v_diffuseSum+=u_directional_lights[i].diffuse.rgb*fdot;\n"
        "\n"
        "\n"
        "			#ifdef SPECULAR\n"
        "				#ifndef NO_BRANCHING\n"
        "					if (fdot>0.025 && u_material.diffuse.a>0.025)\n"
        "				#endif //NO_BRANCHING\n"
        "				{\n"
        "					halfVector = normalize(L + E);\n"
        "					nxHalf = max(0.0,dot(normal, halfVector));\n"
        "					v_specularSum+= u_directional_lights[i].specular.rgb * pow(nxHalf,u_material.shininess);\n"
        "				}\n"
        "			#endif //SPECULAR\n"
        "		}\n"
        "		#endif //NUM_DIRECTIONAL_LIGHTS>0\n"
        "		//endregion\n"
        "\n"
        "		 //region Point lights here\n"
        "		#if NUM_POINT_LIGHTS>0\n"
        "			{\n"
        "			float distance2;float attenuation;\n"
        "			for (int i = 0; i < NUM_POINT_LIGHTS;i++)\n"
        "			{\n"
        "			L = u_point_lights[i].vector.xyz - v_vertex;\n"
        "				distance2 = dot(L,L);\n"
        "				attenuation = (1.0 / (1.0 + (u_point_lights[i].vector.w * distance2)));\n"
        "			L = normalize(L);\n"
        "			fdot = max(dot(normal, L), 0.0)*attenuation;\n"
        "\n"
        "				v_ambientSum+=u_point_lights[i].ambient.rgb;\n"
        "				v_diffuseSum+=u_point_lights[i].diffuse.rgb*fdot;\n"
        "\n"
        "				#ifdef SPECULAR\n"
        "					#ifndef NO_BRANCHING\n"
        "						if (fdot>0.025 && u_material.diffuse.a>0.025)\n"
        "					#endif //NO_BRANCHING\n"
        "						{\n"
        "					halfVector = normalize(L + E);\n"
        "						nxHalf = max(0.0,dot(normal, halfVector));\n"
        "						v_specularSum+= u_point_lights[i].specular.rgb * pow(nxHalf,u_material.shininess);\n"
        "						}\n"
        "				#endif // SPECULAR\n"
        "			}\n"
        "			}\n"
        "		#endif //NUM_POINT_LIGHTS>0\n"
        "		//endregion\n"
        "\n"
        "		 //region Spot lights here\n"
        "		#if NUM_SPOT_LIGHTS>0\n"
        "			{\n"
        "			float distance2;float attenuation;\n"
        "			for (int i = 0; i < NUM_SPOT_LIGHTS;i++)\n"
        "			{\n"
        "			L = u_spot_lights[i].vector.xyz - v_vertex;\n"
        "				distance2 = dot(L,L);\n"
        "				attenuation = (1.0 / (1.0 + (u_spot_lights[i].vector.w * distance2)));\n"
        "			L = normalize(L);\n"
        "			//					dot(-L, u_spot_lights[i].spotDirection	???\n"
        "			attenuation*= clamp((dot(-L, u_spot_lights[i].spotDirection) - u_spot_lights[i].spotInnerAndOuterCosConeAngle.y) / (u_spot_lights[i].spotInnerAndOuterCosConeAngle.x - u_spot_lights[i].spotInnerAndOuterCosConeAngle.y), 0.0, 1.0);\n"
        "				fdot = max(dot(normal, L), 0.0)*attenuation;\n"
        "\n"
        "				v_ambientSum+=u_spot_lights[i].ambient.rgb;\n"
        "				v_diffuseSum+=u_spot_lights[i].diffuse.rgb*fdot;\n"
        "\n"
        "				#ifdef SPECULAR\n"
        "					#ifndef NO_BRANCHING\n"
        "						if (fdot>0.025 && u_material.diffuse.a>0.025)\n"
        "					#endif //NO_BRANCHING\n"
        "						{\n"
        "					halfVector = normalize(L + E);\n"
        "						nxHalf = max(0.0,dot(normal, halfVector));\n"
        "						v_specularSum+= u_spot_lights[i].specular.rgb * pow(nxHalf,u_material.shininess);\n"
        "						}\n"
        "				#endif // SPECULAR\n"
        "			}\n"
        "			}\n"
        "		#endif //NUM_SPOT_LIGHTS>0\n"
        "		//endregion\n"
        "\n"
        "\n"
        "					v_ambientSum*= u_material.ambient.rgb;\n"
        "					v_diffuseSum*=u_material.diffuse.rgb;\n"
        "			#ifdef SPECULAR\n"
        "				v_specularSum*=u_material.specular.rgb;\n"
        "			#endif //SPECULAR\n"
        "\n"
        "\n"
        "			v_ambientSum*=INV_NUM_LIGHTS;\n"
        "\n"
        "\n"
        "	#else //NUM_LIGHTS>0\n"
        "		v_ambientSum = vec3(0.0,0.0,0.0);\n"
        "	#endif //NUM_LIGHTS>0\n"
        "\n"
        "	 //region USE_FOG\n"
        "	#ifdef USE_FOG\n"
        "	const float LOG2 = 1.442695;\n"
        "	float fogFragCoord2 = dot(v_vertex,v_vertex);//length(v_vertex);\n"
        "											// the distance between the camera and the currently processed vertex in the vertex pipeline.\n"
        "											// Thanks to the fact that in the vertex shader, we are working in the camera space,\n"
        "											// the distance is equal to length(vVertex) where vVertex is the vertex position in the camera's space .\n"
        "	v_fogFactor = exp2( -u_lightModel.fogColorAndDensity.a *\n"
        "					   u_lightModel.fogColorAndDensity.a *\n"
        "					   fogFragCoord2 *\n"
        "					   /*\n"
        "					   fogFragCoord *\n"
        "					   fogFragCoord *\n"
        "					   */\n"
        "					   LOG2 );\n"
        "	v_fogFactor = clamp(v_fogFactor, 0.0, 1.0);\n"
        "\n"
        "	#endif //USE_FOG\n"
        "	//endregion\n"
        "\n"
        "\n"
        "\n"
        "}\n"
        "\n"
        "\n"
        "/*\n"
        "********VertexShader 0(129) : error C7503: OpenGL does not allow C-style casts\n"
        "0(130) : error C7503: OpenGL does not allow C-style casts\n"
        "0(131) : error C7503: OpenGL does not allow C-style casts\n"
        "0(132) : error C7503: OpenGL does not allow C-style casts\n"
        "*/\n"
        "\n"
        "\n"
    };





    const GLchar* gFragmentShaderSource[] = {
        "//-------------------- This part must be the same for both shaders ---------------------------\n"
        "// Generic skinning shader with vertex lighting (NO NORMAL MAPPING AND NO SHADOW!)\n"
        "// Full material and light properties. Configurable number of lights (but needs background C++ code)\n"
        "\n"
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "precision lowp int;\n"
        "#endif //GL_ES\n"
        "\n"
        "#define SPECULAR\n"
        "#define TEXTURE\n"
        "//#define USE_FOG\n"
        "#define NO_BRANCHING\n"
        "\n"
        "#ifdef TEXTURE\n"
        "varying vec2 v_texcoords;\n"
        "uniform sampler2D	u_textureUnit0;\n"
        "#endif //TEXTURE\n"
        "varying vec3 v_globalAmbientSum; 	 // global ambient only\n"
        "varying vec3 v_ambientSum; 	 		//ambient only\n"
        "varying vec3 v_diffuseSum; 			// u_material.diffuse.rgb is left outside if TEXTURE_USE_ALPHA_CHANNEL_AS_DIFFUSE_COLOR_MASK is defined\n"
        "#ifdef SPECULAR\n"
        "varying vec3 v_specularSum;\n"
        "#endif //SPECULAR\n"
        "#ifdef USE_FOG\n"
        "varying float v_fogFactor;\n"
        "#endif //USE_FOG\n"
        "\n"
        "struct MaterialParams\n"
        "{\n"
        "   vec4 emission;\n"
        "   vec4 ambient;\n"
        "   vec4 diffuse;\n"
        "   #ifdef SPECULAR\n"
        "   vec4 specular;\n"
        "   float shininess;\n"
        "   #endif\n"
        "};\n"
        "\n"
        "struct LightModelParams\n"
        "{\n"
        "	vec4 ambient;\n"
        "	#ifdef USE_FOG\n"
        "	vec4 fogColorAndDensity;\n"
        "	#endif //USE_FOG\n"
        "};\n"
        "\n"
        "uniform MaterialParams u_material;  // The material of the object to render (u_material.specular.a is used to tune the power of the specular highlight)\n"
        "uniform LightModelParams u_lightModel;	// A single global uniform\n"
        "\n"
        "//--------------------------------------------------------------------------------------------\n"
        "\n"
        "void main()\n"
        "{\n"
        "\n"
        "#ifdef TEXTURE\n"
        "\n"
        "vec4 tColor = texture2D(u_textureUnit0, v_texcoords);\n"
        "gl_FragColor.a = u_material.diffuse.a * tColor.a;\n"
        "#ifndef NO_BRANCHING\n"
        "if (gl_FragColor.a<0.075) {\n"
        "    discard;\n"
        "    return;\n"
        "}\n"
        "#endif //NO_BRANCHING\n"
        "#ifdef SPECULAR\n"
        "vec3 tColorSpecularMultiplier = (vec3(0.25,0.25,0.25)+tColor.rgb)*(tColor.a*u_material.specular.a); //This is a hack to make the specular highlight look like the texture a bit (cheap per-pixel operation to improve specular-vertex-lighting)\n"
        "gl_FragColor.rgb =  v_globalAmbientSum + (v_ambientSum + v_diffuseSum)*tColor.rgb + v_specularSum*tColorSpecularMultiplier;\n"
        "#else //SPECULAR\n"
        "gl_FragColor.rgb =  v_globalAmbientSum + (v_ambientSum + v_diffuseSum)*tColor.rgb;\n"
        "#endif //SPECULAR\n"
        "\n"
        "#else //TEXTURE\n"
        "\n"
        "gl_FragColor.a = u_material.diffuse.a;\n"
        "#ifndef NO_BRANCHING\n"
        "if (gl_FragColor.a<0.075) {\n"
        "    discard;\n"
        "    return;\n"
        "}\n"
        "#endif //NO_BRANCHING\n"
        "#ifdef SPECULAR\n"
        "gl_FragColor.rgb = v_globalAmbientSum+v_ambientSum+v_diffuseSum+v_specularSum*u_material.specular.a;\n"
        "#else //SPECULAR\n"
        "gl_FragColor.rgb = v_globalAmbientSum+v_ambientSum+v_diffuseSum;\n"
        "#endif //SPECULAR\n"
        "\n"
        "#endif //TEXTURE\n"
        "\n"
        "\n"
        "\n"
        "#ifdef USE_FOG\n"
        "gl_FragColor.rgb = mix(u_lightModel.fogColorAndDensity.rgb, gl_FragColor.rgb, v_fogFactor );\n"
        "#endif //USE_FOG\n"
        "\n"
        "}\n"
        "\n"
    };

	
    const bool bindAttrLoc = true;
    OpenGLCompileShaderStruct css;
    css.dontLinkProgram = bindAttrLoc;

    program = OpenGLCompileShadersFromMemory(gVertexShaderSource, gFragmentShaderSource, &css);
    if (program==0) {
        fprintf(stderr,"Error compiling shaders.\n");
        return;
    }
    if (css.dontLinkProgram) {
        css.dontLinkProgram = false;
        glBindAttribLocation(program,0,"a_vertex");             // on my system, default is 0
        glBindAttribLocation(program,2,"a_normal");             // on my system, default is 1
        glBindAttribLocation(program,8,"a_texcoords");          // on my system, default is 2
        glBindAttribLocation(program,4,"a_boneInds");           // on my system, default is 3
        glBindAttribLocation(program,5,"a_boneWeights");        // on my system, default is 4

        program = OpenGLCompileShadersFromMemory(NULL,NULL,&css);
        if (program==0) {
            fprintf(stderr,"Error linking shader program.\n");
            return;
        }
    }

	//Get attribute locations (we could just bind them before linking shaders...)
	vertexAttrLoc=GetAttrLoc(program,"a_vertex");
	normalAttrLoc=GetAttrLoc(program,"a_normal");
	texcoordsAttrLoc=GetAttrLoc(program,"a_texcoords");
	boneIndsAttrLoc=GetAttrLoc(program,"a_boneInds");
	boneWeightsAttrLoc=GetAttrLoc(program,"a_boneWeights");

	// Get uniform locations
    textureUnit0UnifLoc			= GetUnifLoc(program,"u_textureUnit0");
	
	material_emissionUnifLoc	= GetUnifLoc(program,"u_material.emission");
	material_ambientUnifLoc		= GetUnifLoc(program,"u_material.ambient");
	material_diffuseUnifLoc		= GetUnifLoc(program,"u_material.diffuse");
	material_specularUnifLoc	= GetUnifLoc(program,"u_material.specular");
	material_shininessUnifLoc	= GetUnifLoc(program,"u_material.shininess");
	
	lightModel_ambientUnifLoc	= GetUnifLoc(program,"u_lightModel.ambient");
	
	pMatrixUnifLoc				= GetUnifLoc(program,"u_pMatrix");
	mvMatrixUnifLoc				= GetUnifLoc(program,"u_mvMatrix");
	mMatrixScalingUnifLoc		= GetUnifLoc(program,"u_mMatrixScaling");

	directional_lights0_ambientUnifLoc	= GetUnifLoc(program,"u_directional_lights[0].ambient");
	directional_lights0_diffuseUnifLoc	= GetUnifLoc(program,"u_directional_lights[0].diffuse");
	directional_lights0_specularUnifLoc	= GetUnifLoc(program,"u_directional_lights[0].specular");
	directional_lights0_vectorUnifLoc	= GetUnifLoc(program,"u_directional_lights[0].vector");

	boneMatricesUnifLoc			= GetUnifLoc(program,"u_boneMatrices");

    glUseProgram(program);
    glUniform1i(textureUnit0UnifLoc,0);

    glUseProgram(0);
}
bool OpenGLSkinningShaderData::setLightSourceParams(const glLightSourceParams& lightSourceParams,bool mustBindAndUnbindProgram) {
    if (program)    {
        if (mustBindAndUnbindProgram) glUseProgram(program);

        //glLightModelParams::instance.setAmbientColor(glm::vec4(1,1,1,1));
        glUniform4fv(lightModel_ambientUnifLoc,1,glm::value_ptr(glLightModelParams::instance.getAmbientColor()));

        /*lightSourceParams.ambient = glm::vec4(0.25,0.25,0.25,1);
        lightSourceParams.diffuse = glm::vec4(0.8,0.8,0.8,1);
        lightSourceParams.specular = glm::vec4(1,1,1,1);*/
        glUniform4fv(directional_lights0_ambientUnifLoc,1,glm::value_ptr(lightSourceParams.ambient));
        glUniform4fv(directional_lights0_diffuseUnifLoc,1,glm::value_ptr(lightSourceParams.diffuse));
        glUniform4fv(directional_lights0_specularUnifLoc,1,glm::value_ptr(lightSourceParams.specular));

        if (mustBindAndUnbindProgram) glUseProgram(0);
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------------
inline static void AdjustVertexAttributeArrayIndex(GLuint program,const char* attributeString,GLint& attributeIndexToSet,bool verbose)
{
    if (attributeString && strlen(attributeString)>0)
    {
        const GLint attributeIndex = glGetAttribLocation(program,attributeString);
        if (attributeIndex<0) {
            if (verbose) printf("GLMiniXXXBatch::glSetVertexAttributeArrayIndices(...) Warning: attribute \"%s\" not found in shader program: using attribute index %d.\n",attributeString,attributeIndexToSet);
        }
        else attributeIndexToSet = attributeIndex;
    }
}


GLMiniTriangleBatch::GLMiniTriangleBatch()   {
	vao = vbo = vboIA = 0;    

	primitive=GL_TRIANGLES;
    drawType=GL_STATIC_DRAW;
    drawTypeIA=GL_STATIC_DRAW;

    gpuArrayVertsSize = 0;
    gpuElementArrayIndsSize = 0;
    gpuArraySizeInBytes = 0;
    gpuElementArraySizeInBytes = 0;
    indexTypeSizeInBytes = 0;

    colorsOffsetInBytes = 0;
    normsOffsetInBytes = 0;
    tangsOffsetInBytes = 0;
    texCoordsOffsetInBytes = 0;
    boneIndsOffsetInBytes = 0;
    boneWeightsOffsetInBytes = 0;

    // Default attribute indices:
    vertsAttributeIndex = 0;
    colorsAttributeIndex = 3;
    normsAttributeIndex = 2;
    tangsAttributeIndex = 6;
    texCoordsAttributeIndex = 8;
    boneIndsAttributeIndex = 4;
    boneWeightsAttributeIndex = 5;
    // (Optional) Better check if the default attribute indices are correct:
    /*
    const GLProgram::AttributeLocationsMapType& ALM = GLProgram::g_defaultAttributeLocationsMap;
    for (GLProgram::AttributeLocationsMapTypeConstIterator it = ALM.begin();it!=ALM.end();++it)
    {
        const char* name = it->second;
        if (name == "a_vertex") {vertsAttributeIndex = it->first;continue;}
        else if (name == "a_normal") {normsAttributeIndex = it->first;continue;}
        else if (name == "a_tangent") {tangsAttributeIndex = it->first;continue;}
        else if (name == "a_texcoords") {texCoordsAttributeIndex = it->first;continue;}
        else if (name == "a_boneInds") {boneIndsAttributeIndex = it->first;continue;}
        else if (name == "a_boneWeights") {boneWeightsAttributeIndex = it->first;continue;}
    }
    */
}
GLMiniTriangleBatch::~GLMiniTriangleBatch() {}
void GLMiniTriangleBatch::destroyGL()    {
    if (vao) {glDeleteVertexArrays(1, &vao);vao=0;}
    if (vboIA) {glDeleteBuffers(1, &vboIA);vboIA=0;}
    if (vbo) {glDeleteBuffers(1, &vbo);vbo=0;}

    gpuArrayVertsSize = 0;
    gpuElementArrayIndsSize = 0;
    gpuArraySizeInBytes = 0;
    gpuElementArraySizeInBytes = 0;
    indexTypeSizeInBytes = 0;

    colorsOffsetInBytes = 0;
    normsOffsetInBytes = 0;
    tangsOffsetInBytes = 0;
    texCoordsOffsetInBytes = 0;
    boneIndsOffsetInBytes = 0;
    boneWeightsOffsetInBytes = 0;
}
// Must be called BEFORE calling upload()
bool GLMiniTriangleBatch::glSetVertexAttributeArrayIndices(GLuint program,
                                    const char* vertexAttributeNameInShader,
                                    const char* colorAttributeNameInShader,
                                    const char* normalAttributeNameInShader,
                                    const char* tangentAttributeNameInShader,
                                    const char* texcoordsAttributeNameInShader,
                                    const char* boneIndsAttributeNameInShader,
                                    const char* boneWeightsAttributeNameInShader,
                                    bool verbose
                                    )
{
    if (!program) {
        if (verbose) printf("GLMiniTriangleBatch::glSetVertexAttributeArrayIndices(...) Error: invalid shader program.\n");
        return false;
    }
    AdjustVertexAttributeArrayIndex(program,vertexAttributeNameInShader,vertsAttributeIndex,verbose);
    AdjustVertexAttributeArrayIndex(program,colorAttributeNameInShader,colorsAttributeIndex,verbose);
    AdjustVertexAttributeArrayIndex(program,normalAttributeNameInShader,normsAttributeIndex,verbose);
    AdjustVertexAttributeArrayIndex(program,tangentAttributeNameInShader,tangsAttributeIndex,verbose);
    AdjustVertexAttributeArrayIndex(program,texcoordsAttributeNameInShader,texCoordsAttributeIndex,verbose);
    AdjustVertexAttributeArrayIndex(program,boneIndsAttributeNameInShader,boneIndsAttributeIndex,verbose);
    AdjustVertexAttributeArrayIndex(program,boneWeightsAttributeNameInShader,boneWeightsAttributeIndex,verbose);
    return true;
}


