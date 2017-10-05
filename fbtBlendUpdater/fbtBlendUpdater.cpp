
// g++ -Os fbtBlendUpdater.cpp -o fbtBlendUpdater
// or better:
// g++ -Os fbtBlendUpdater.cpp -D"BLENDPARSER_USE_ZLIB" -o fbtBlendUpdater -lz

// The first commandline can cause segmentation fault when loading a -blend compressed file.

//#define BLENDPARSER_USE_ZLIB

#include <string.h>

#ifndef BLENDPARSER_MALLOC
#include <stdlib.h> // malloc
#define BLENDPARSER_MALLOC(X) malloc(X)
#endif
#ifndef BLENDPARSER_REALLOC
//#include <stdlib.h> // realloc
#define BLENDPARSER_REALLOC(X,Y) realloc(X,Y)
#endif
#ifndef BLENDPARSER_FREE
//#include <stdlib.h> // free
#define BLENDPARSER_FREE(X) free(X)
#endif

class BlendFile {
public:

BlendFile();
~BlendFile() {clear();}
void clear();
// if (passBufOwnershipToMe==true), "buf" must be allocated with BLENDPARSER_MALLOC(...)
// However "buf" (NOT the input .blend file) will be slightly modified when the file is parsed (old memory function ptrs can be changed)
bool loadFromMemory(unsigned char* buf,unsigned long bufferSize,bool passBufOwnershipToMe=false);
bool loadFromFile(const char* filePath);

// After a .blend file has been successfully loaded, we can ALWAYS generate the header "Blender.h"
// It will have the same BLENDER_VERSION, as the input .blend file that you loaded.
// This method does not modify in any way the input "buf"
// AFAIK "Blender.h" depends only on the Blender version of the file (not on endianess and 32/64 bit)
bool generateBlenderHeader(const char* saveFileName="Blender.h",const char* guardName="BLENDER_H",const char* namespaceName="Blender") const;

// This is less useful: it just saves the SDNA block of a file (without header).
bool generateBlenderSDNACpp(const char* saveFileName="BlenderSDNA.cpp", const char* prefixText=NULL, const char *suffixText=NULL) const;

bool updatefbtBlendHeader(const char* oldfbtBlendPath,const char* newfbtBlendPath="fbtBlend.h");

int getVersion() const;     // returns the blender version of the file (e.g. 279)
bool is32bit() const;       // returns true if the file is 32 bit
bool isBigEndian() const;   // returns true if the file is big endian

unsigned long getNumBlocks() const {return numBlockHeaders;}

private:
BlendFile(const BlendFile&) {}
void operator=(const BlendFile&) {}

unsigned char* blendPtr;
unsigned long blendPtrSize;
bool blendPtrOwned;

char version[4];
unsigned int _uiPointerSize;   // 4 or 8
bool _bLittleEndian;

const unsigned char** blockHeaderPtrs;  // The DNA1 block is NOT included
unsigned long numBlockHeaders;

struct BlendDNA1Block* dna1Block;

const unsigned char* _blockDnaPtr;  // Used only by generateBlenderSDNACpp
unsigned long _blockDnaSize;        // Used only generateBlenderSDNACpp


bool parseDNA1Block(const unsigned char* pDNA1,unsigned int dna1BlockDataSize);

};


#include <stdio.h>
#include <string.h> // strlen
#include <inttypes.h> // uint64_t

#ifdef BLENDPARSER_USE_ZLIB
#include <zlib.h>
#endif

// We call C++ constructor on own allocated memory via the placement "new(ptr) Type()" syntax.
// Defining a custom placement new() with a dummy parameter allows us to bypass including <new> which on some platforms complains when user has disabled exceptions.
struct BlendParserPlacementNewDummy {};
inline void* operator new(size_t, BlendParserPlacementNewDummy, void* ptr) { return ptr; }
inline void operator delete(void*, BlendParserPlacementNewDummy, void*) {}
#define BLENDPARSER_PLACEMENT_NEW(_PTR)  new(BlendParserPlacementNewDummy(), _PTR)

namespace {

#ifdef BLENDPARSER_USE_ZLIB
unsigned char* GzDecompressFromMemory(const unsigned char* memoryBuffer,int memoryBufferSize,unsigned long* pSizeOut=NULL)    {
    if (pSizeOut) *pSizeOut=0;
    unsigned char* ptr = NULL;unsigned long ptrSize=0;
    if (memoryBufferSize == 0  || !memoryBuffer) return ptr;
    const int memoryChunk = memoryBufferSize > (16*1024) ? (16*1024) : memoryBufferSize;
    ptrSize = memoryChunk;
    ptr = (unsigned char*) BLENDPARSER_MALLOC(ptrSize);  // we start using the memoryChunk length

    z_stream myZStream;
    myZStream.next_in = (Bytef *) memoryBuffer;
    myZStream.avail_in = memoryBufferSize;
    myZStream.total_out = 0;
    myZStream.zalloc = Z_NULL;
    myZStream.zfree = Z_NULL;

    bool done = false;
    if (inflateInit2(&myZStream, (16+MAX_WBITS)) == Z_OK) {
        int err = Z_OK;
        while (!done) {
            if (myZStream.total_out >= (uLong)(ptrSize)) {
                // not enough space: we add the memoryChunk each step
                ptrSize+= memoryChunk;
                ptr = (unsigned char*) BLENDPARSER_REALLOC(ptr,ptrSize);
            }

            myZStream.next_out = (Bytef *) (ptr + myZStream.total_out);
            myZStream.avail_out = ptrSize - 0 - myZStream.total_out;

            if ((err = inflate (&myZStream, Z_SYNC_FLUSH))==Z_STREAM_END) done = true;
            else if (err != Z_OK)  break;
        }
        if ((err=inflateEnd(&myZStream))!= Z_OK) done = false;
    }
    ptrSize = (done ? myZStream.total_out : 0);
    if (ptrSize>0) ptr = (unsigned char*) BLENDPARSER_REALLOC(ptr,ptrSize);
    else {BLENDPARSER_FREE(ptr);ptr=NULL;}
    if (pSizeOut) *pSizeOut=ptrSize;

    return ptr;
}
#endif //BLENDPARSER_USE_ZLIB

static FILE* UTF8FileOpen(const char* filename, const char* mode)
{
// TODO: Make it work on Windows [we need to convert UTF8 to wchar_t].
// The following code is taken by imgui:
//#if defined(_WIN32) && !defined(__CYGWIN__)
//    // We need a fopen() wrapper because MSVC/Windows fopen doesn't handle UTF-8 filenames. Converting both strings from UTF-8 to wchar format
//    const int filename_wsize = ImTextCountCharsFromUtf8(filename, NULL) + 1;
//    const int mode_wsize = ImTextCountCharsFromUtf8(mode, NULL) + 1;
//    WCharVector buf;
//    buf.resize(filename_wsize + mode_wsize);
//    ImTextStrFromUtf8(&buf[0], filename_wsize, filename, NULL);
//    ImTextStrFromUtf8(&buf[filename_wsize], mode_wsize, mode, NULL);
//    return _wfopen((wchar_t*)&buf[0], (wchar_t*)&buf[filename_wsize]);
//#else
    return fopen(filename, mode);
//#endif
}
static unsigned char* GetFileContent(const char *filePath,unsigned long* pSizeOut=NULL,const char modes[] = "rb")   {
    unsigned char* ptr = NULL;
    if (pSizeOut) *pSizeOut=0;
    if (!filePath) return ptr;
    FILE* f;
    if ((f = UTF8FileOpen(filePath, modes)) == NULL) return ptr;
    if (fseek(f, 0, SEEK_END))  {fclose(f);return ptr;}
    const long f_size_signed = ftell(f);
    if (f_size_signed == -1)    {fclose(f);return ptr;}
    size_t f_size = (size_t)f_size_signed;
    if (fseek(f, 0, SEEK_SET))  {fclose(f);return ptr;}
    ptr = (unsigned char*) BLENDPARSER_MALLOC(f_size);
    if (!ptr) return ptr;
    const size_t f_size_read = f_size>0 ? fread((unsigned char*)ptr, 1, f_size, f) : 0;
    fclose(f);
    if (f_size_read == 0 || f_size_read!=f_size)    {BLENDPARSER_FREE(ptr);ptr=NULL;}
    else if (pSizeOut) *pSizeOut=f_size;
    return ptr;
}
static bool FileExists(const char *filePath)   {
    if (!filePath || strlen(filePath)==0) return false;
    FILE* f = UTF8FileOpen(filePath, "rb");
    if (!f) return false;
    fclose(f);f=NULL;
    return true;
}

static uint16_t ParseUInt16(const unsigned char* p,bool littleEndian=true) {
    uint16_t rv = 0;
#   ifdef BLENDPARSER_MACHINE_IS_BIG_ENDIAN
    const bool machineIsLittleEndian = false;
#   else
    const bool machineIsLittleEndian = true;
#   endif
    if (littleEndian==machineIsLittleEndian) rv = *((uint16_t*) p);
    else {
        unsigned char tmp[2];
        for (int i=0;i<2;i++) tmp[i]=p[1-i];
        rv = *((uint16_t*) tmp);
    }
    return rv;
}
static uint32_t ParseUInt32(const unsigned char* p,bool littleEndian=true) {
    uint32_t rv = 0;
#   ifdef BLENDPARSER_MACHINE_IS_BIG_ENDIAN
    const bool machineIsLittleEndian = false;
#   else
    const bool machineIsLittleEndian = true;
#   endif
    if (littleEndian==machineIsLittleEndian) rv = *((uint32_t*) p);
    else {
        unsigned char tmp[4];
        for (int i=0;i<4;i++) tmp[i]=p[3-i];
        rv = *((uint32_t*) tmp);
    }
    return rv;
}
// _uiPointerSize must be 4 or 8
static uint64_t ParseUInt64(const unsigned char* p,unsigned int _uiPointerSize=8,bool littleEndian=true) {
    uint64_t rv = 0;
#   ifdef BLENDPARSER_MACHINE_IS_BIG_ENDIAN
    const bool machineIsLittleEndian = false;
#   else
    const bool machineIsLittleEndian = true;
#   endif
    if (littleEndian==machineIsLittleEndian) {
        if (_uiPointerSize==8) rv = *((uint64_t*) p);
        else rv = (uint64_t) (*((uint32_t*) p));
    }
    else {
        unsigned char tmp[8];
        for (unsigned int i=0;i<_uiPointerSize;i++) tmp[i]=p[_uiPointerSize-i];
        if (_uiPointerSize==8) rv = *((uint64_t*) tmp);
        else rv = (uint64_t) (*((uint32_t*) tmp));
    }
    return rv;
}

static void WriteMemPointer(const unsigned char* p,const uint64_t& value,unsigned int _uiPointerSize=8,bool littleEndian=true) {
    // Well, we should handle different endianess here...
    if (_uiPointerSize==4) *((uint32_t*)(p)) = (uint32_t) value;
    else *((uint64_t*)(p)) = value;
}


} // namespace

struct BlendDNA1StructField {
    uint16_t  typeIdx;
    uint16_t  nameIdx;

    mutable uint16_t  numElementsInName;    // [Helper] Usually 1, but e.g. "var1" -> 1, "*var2" -> 1, "*var3[4]" -> 4 "var3[4][3]" -> 12 and so on
    mutable uint16_t  offsetInBytes32bit;   // [Helper]
    mutable uint16_t  offsetInBytes64bit;   // [Helper]
    uint16_t getOffset(unsigned int uiPointerSize) const {return uiPointerSize==8 ? offsetInBytes64bit : offsetInBytes32bit;}
};
struct BlendDNA1Struct {
    uint16_t  typeIdx;
    uint16_t  numFields;
    uint32_t fieldOffset;  // index into BlendDNA1Block::pFieldsAllTogether
    BlendDNA1Struct() : typeIdx(0),numFields(0),fieldOffset(0),bForwardDeclared(false),bDeclared(false) {}

    // These are runtime extra stuff internally needed to generate "Blender.h". Do not use.
    mutable bool bForwardDeclared;
    mutable bool bDeclared;
    mutable uint16_t lenIn32bitMode;
    mutable uint16_t lenIn64bitMode;
    uint16_t getLen(unsigned int uiPointerSize) const {return uiPointerSize==8 ? lenIn64bitMode : lenIn32bitMode;}
};
struct BlendDNA1Block {
    const char** names;
    uint32_t numNames;

    const char** types;
    const unsigned char* pTLens;    // endian-dependent unsigned shorts
    BlendDNA1Struct** pTypeStructs; // Just an [helper] array reference inside pStructs. Size = numTypes. Elements can be NULL for basic types or zero length types.
    uint32_t numTypes;

    uint32_t numStructs;
    BlendDNA1Struct* pStructs;

    BlendDNA1StructField* pFieldsAllTogether;   // Just to limit the number of allocations (do not use)
    uint32_t numAllFields;

    void clear() {
        if (pFieldsAllTogether) {BLENDPARSER_FREE(pFieldsAllTogether);pFieldsAllTogether=NULL;}
        if (pStructs) {BLENDPARSER_FREE(pStructs);pStructs=NULL;}
        if (pTypeStructs) {BLENDPARSER_FREE(pTypeStructs);pTypeStructs=NULL;}
        if (names) {BLENDPARSER_FREE(names);names=NULL;}
        if (types) {BLENDPARSER_FREE(types);types=NULL;}
        pTLens=NULL;
        numNames=numTypes=numStructs=numAllFields=0;
    }
    BlendDNA1Block() : names(NULL),numNames(0),types(NULL),pTLens(NULL),pTypeStructs(NULL),numTypes(0),numStructs(0),pStructs(NULL),pFieldsAllTogether(NULL),numAllFields(0) {}
    ~BlendDNA1Block() {clear();}

    void printNames() const {
        if (names) {
            printf("NAME: %u\n",numNames);
            for (uint32_t i = 0;i<numNames;i++) {printf("%u)\t\"%s\"\n",i,(const char*) names[i]);}
            printf("\n");
        }
    }
    void printTypes(bool littleEndian) const {
        if (types) {
            printf("TYPE: %u\n",numTypes);
            for (uint32_t i = 0;i<numTypes;i++) {printf("%u)\t\"%s\"\t(%u bytes)\n",i,(const char*)types[i],ParseUInt16(pTLens+2*i,littleEndian));}
            printf("\n");
        }
    }

    uint16_t getTypeSize(uint32_t typeIdx,bool littleEndian) const {
        return ParseUInt16(pTLens+(typeIdx*2),littleEndian);
    }

    static uint16_t GetNumArrayElementsFromName(const char* name) {
        if (!name) return 1;
        const uint16_t len = strlen(name);
        if (len<4 || name[len-1]!=']') return 1;    // "a[2]"
        // We want to parse something like "name[20][10]" too
        uint16_t rv = 1;int e = len-1;
        for (int i=e-1;i>=0;--i) {
            const char c = name[i];
            if (c=='[') {
                uint32_t tmp = 1;
                char tmpc[16]="";
                strncpy(tmpc,&name[i+1],e-i-1);
                tmpc[e-i-1]='\0';
                //printf("\ttmpc=\"%s\"\n",tmpc);
                if (sscanf(tmpc,"%u",&tmp)<=0) break;
                rv*=tmp;
                if (i>0 && name[i-1]==']') {
                    e = i-1;--i;continue;
                }
                else break;
            }
            if (c<'0' || c>'9') break;
        }
        return rv;
    /*
    // Tests:
    const char* tests[6]={"ciao","(*ciao())","*bonasera","*bon[5]","*ptr[54]","d[20][10]"};
    for (int i=0;i<6;i++) {
        printf("BlendDNA1Block::GetNumArrayElemetsFromName(\"%s\") = %u\n",tests[i],BlendDNA1Block::GetNumArrayElemetsFromName(tests[i]));
    }*/
    }
    static bool GetIsPointerFromName(const char* name) {return (name && (name[0]=='*' || (name[0]=='(' && name[1]=='*')));}


    void _fprintSingleStructInternal(FILE* f,bool littleEndian,const BlendDNA1Struct& bstr) const    {
        // Look for dependent fields:
        for (uint32_t fd=0;fd<bstr.numFields;fd++) {
            const BlendDNA1StructField& field = pFieldsAllTogether[bstr.fieldOffset+fd];
            if (field.typeIdx>11 && field.typeIdx!=bstr.typeIdx && ParseUInt16(pTLens+(field.typeIdx*2),littleEndian)!=0)   {
                // See if field.typeIdx is already declared:
                // NEW CODE (FASTER) -- (untested)
                const BlendDNA1Struct* pbstrj = pTypeStructs[field.typeIdx];
                if (pbstrj) {
                    if (!pbstrj->bDeclared) {
                        const char* fieldName = names[field.nameIdx];
                        if (fieldName && strlen(fieldName)>0 && fieldName[0]=='*')  {
                            if (!pbstrj->bForwardDeclared && bstr.typeIdx!=pbstrj->typeIdx) {
                                fprintf(f,"struct %s;\n",types[pbstrj->typeIdx]);
                                pbstrj->bForwardDeclared = true;
                            }
                        }
                        else {
                            // We must declare bstrj before bstr
                            _fprintSingleStructInternal(f,littleEndian,*pbstrj);
                            pbstrj->bDeclared = true;
                        }
                    }
                }
            }
        }
        // Now we can safely define this struct:
        fprintf(f,"struct %s\t{\n",types[bstr.typeIdx]);
        uint16_t fieldTypeSize = 0;
        const char *fieldName = NULL,*fieldType = NULL;
        for (uint32_t fd=0;fd<bstr.numFields;fd++) {
            const BlendDNA1StructField& field = pFieldsAllTogether[bstr.fieldOffset+fd];
            fieldType       = types[field.typeIdx];
            fieldTypeSize   = ParseUInt16(pTLens+(field.typeIdx*2),littleEndian);
            fieldName       = names[field.nameIdx];
            if (strlen(fieldName)>2 && fieldName[0]=='(' && fieldName[1]=='*') fprintf(f,"\t%s\t%s;\n",fieldType,fieldName);            // Function pointers
            else if (fieldTypeSize>0 || fieldName[0]=='*') fprintf(f,"\t%s\t%s;\n",fieldTypeSize==0 ? "void" : fieldType,fieldName);    // Other variables
            else fprintf(f,"#\terror %s\t%s;\t // Type %s has zero size and is not defined anywhere\n",fieldType,fieldName,fieldType);  // Errors
        }
        fprintf(f,"};\n\n");
        bstr.bDeclared = true;
    }
    void fprintStructs(FILE* f,bool littleEndian,const unsigned char* pWholeBlendFilePtr=NULL,const char* guardName="BLENDER_H",const char* namespaceName="Blender") const  {
        if (pStructs && types && names)   {
            if (guardName && strlen(guardName)>0) fprintf(f,"#ifndef %s\n#define %s\n",guardName,guardName);
            if (pWholeBlendFilePtr) {
                fprintf(f,"\n// Generated using %.12s\n",pWholeBlendFilePtr);
                fprintf(f,"#define BLENDER_VERSION %.3s\n\n",pWholeBlendFilePtr+9);
            }


            fprintf(f,"#include <inttypes.h>\t // int64_t uint64_t\n\n");

			// This has some effect on _MSC_VER only AFAIK
	        fprintf(f,"// This has some effect on _MSC_VER only AFAIK\n#ifdef near\n#undef near\n#endif\n#ifdef far\n#undef far\n#endif\n#ifdef rad2\n#undef rad2\n#endif\n\n");

            fprintf(f,"#ifdef __cplusplus\n");
            if (namespaceName && strlen(namespaceName)>0)fprintf(f,"namespace %s\t{\n",namespaceName);
            fprintf(f,"extern \"C\"\t{\n");
            fprintf(f,"#endif // __cplusplus\n\n\n");


            // Reset struct flags
            for (uint32_t i=0;i<numStructs;i++) {
                const BlendDNA1Struct& bstr = pStructs[i];
                bstr.bForwardDeclared=bstr.bDeclared=false;
            }

            // Write all structs
            for (uint32_t i=0;i<numStructs;i++) {
                const BlendDNA1Struct& bstr = pStructs[i];
                if (bstr.bDeclared) continue;
                _fprintSingleStructInternal(f,littleEndian,bstr);
            }

            fprintf(f,"\n");

            fprintf(f,"\n#ifdef __cplusplus\n");
            fprintf(f,"}\t// extern \"C\"\n");
            if (namespaceName && strlen(namespaceName)>0)fprintf(f,"}\t// namespace %s\n",namespaceName);
            fprintf(f,"#endif // __cplusplus\n\n");

            if (guardName && strlen(guardName)>0) fprintf(f,"#endif //%s\n\n",guardName);
        }
    }
    void printStructs(bool littleEndian,const unsigned char* pWholeBlendFilePtr=NULL,const char* guardName="BLENDER_H",const char* namespaceName="Blender") const  {
        fprintStructs(stdout,littleEndian,pWholeBlendFilePtr,guardName,namespaceName);
    }


    void _calculateSingleStructLengths(const BlendDNA1Struct& bstr,bool _bLittleEndian,unsigned int _uiPointerSize) const {
        // Look for dependent fields:
        for (uint32_t fd=0;fd<bstr.numFields;fd++) {
            const BlendDNA1StructField& field = pFieldsAllTogether[bstr.fieldOffset+fd];
            if (field.typeIdx>11 && field.typeIdx!=bstr.typeIdx && !BlendDNA1Block::GetIsPointerFromName(names[field.nameIdx]) && ParseUInt16(pTLens+(field.typeIdx*2),_bLittleEndian)!=0)   {
                // See if field.typeIdx is already calculated:
                const BlendDNA1Struct* pbstrj = pTypeStructs[field.typeIdx];
                if (pbstrj) {
                    if (!pbstrj->bDeclared) {
                        // We must declare bstrj before bstr
                        _calculateSingleStructLengths(*pbstrj,_bLittleEndian,_uiPointerSize);
                        pbstrj->bDeclared = true;
                    }
                }
            }
        }
        // Now we can safely calculate the length of this struct:
        bstr.lenIn32bitMode=bstr.lenIn64bitMode=0;
        for (uint32_t fd=0;fd<bstr.numFields;fd++) {
            const BlendDNA1StructField& field = pFieldsAllTogether[bstr.fieldOffset+fd];
            field.offsetInBytes32bit = bstr.lenIn32bitMode;
            field.offsetInBytes64bit = bstr.lenIn64bitMode;
            const bool fieldIsPointer = BlendDNA1Block::GetIsPointerFromName(names[field.nameIdx]);
            if (fieldIsPointer) {
                bstr.lenIn32bitMode+=4*field.numElementsInName;
                bstr.lenIn64bitMode+=8*field.numElementsInName;
                continue;
            }
            const BlendDNA1Struct* pFieldStruct = pTypeStructs[field.typeIdx];
            if (pFieldStruct) {
                bstr.lenIn32bitMode+=pFieldStruct->lenIn32bitMode*field.numElementsInName;
                bstr.lenIn64bitMode+=pFieldStruct->lenIn64bitMode*field.numElementsInName;
                continue;
            }
            // We assume that the "legacy" size is the same in 32 bit and 64 bit modes
            const uint16_t fieldTypeSize   = ParseUInt16(pTLens+(field.typeIdx*2),_bLittleEndian);
            bstr.lenIn32bitMode+=fieldTypeSize*field.numElementsInName;
            bstr.lenIn64bitMode+=fieldTypeSize*field.numElementsInName;
        }
        bstr.bDeclared = true;
    }

};

struct BlendBlock {
    static void PrintfType(const unsigned char* blockHeaderPtr) {printf("%.4s",(const char*)blockHeaderPtr);}
    static bool MatchType(const unsigned char* blockHeaderPtr,const char* type4) {return strncmp(type4,(const char*)blockHeaderPtr,4)==0;}
    static uint32_t GetDataSize(const unsigned char* blockHeaderPtr,bool litteEndian) {return ParseUInt32(blockHeaderPtr+4,litteEndian);}
    static uint64_t GetOldMemoryAddress(const unsigned char* blockHeaderPtr,bool litteEndian,unsigned int pointerSize) {return ParseUInt64(blockHeaderPtr+8,pointerSize,litteEndian);}
    static uint32_t GetSDNAIndex(const unsigned char* blockHeaderPtr,bool litteEndian,unsigned int pointerSize) {return ParseUInt32(blockHeaderPtr+8+pointerSize,litteEndian);}
    static uint32_t GetNumElements(const unsigned char* blockHeaderPtr,bool litteEndian,unsigned int pointerSize) {return ParseUInt32(blockHeaderPtr+12+pointerSize,litteEndian);}
    static const unsigned char* GetBlockDataPtr(const unsigned char* blockHeaderPtr,unsigned int pointerSize)    {return  blockHeaderPtr+16+pointerSize;}
    static const unsigned char* GetNextBlockPtr(const unsigned char* blockHeaderPtr,bool litteEndian,unsigned int pointerSize)    {return  blockHeaderPtr+16+pointerSize+GetDataSize(blockHeaderPtr,litteEndian);}

    static void Printf(unsigned long blockIndex,const unsigned char* blockHeaderPtr,bool litteEndian,unsigned int pointerSize,const BlendDNA1Block* dna1Block=NULL) {
        const uint32_t sdnaIndex = GetSDNAIndex(blockHeaderPtr,litteEndian,pointerSize);
        printf("%lu)\t\"%.4s\"\tobjDataSize: %u\tobjSDNAIndex: %u [%s]\tobjNumElements: %u\t objDataOldMemoryAddress=%lu\n",
        blockIndex,
        (const char*)blockHeaderPtr,
        GetDataSize(blockHeaderPtr,litteEndian),
        sdnaIndex,
        dna1Block ? dna1Block->types[dna1Block->pStructs[sdnaIndex].typeIdx] : "NULL",
        GetNumElements(blockHeaderPtr,litteEndian,pointerSize),
        (unsigned long)GetOldMemoryAddress(blockHeaderPtr,litteEndian,pointerSize)
        );
    }
};

bool BlendFile::loadFromFile(const char *filePath)  {
    unsigned long size=0;
    unsigned char* ptr = GetFileContent(filePath,&size);
    bool rv = false;
    if (ptr) {rv = loadFromMemory(ptr,size,true);}
    return rv;
}
BlendFile::BlendFile() : blendPtr(NULL),blendPtrSize(0),blendPtrOwned(false) {
strcpy(version,"000");
_uiPointerSize=8;
_bLittleEndian=true;
blockHeaderPtrs = NULL;
numBlockHeaders = 0;
dna1Block = NULL;
_blockDnaPtr=NULL;
_blockDnaSize=0;
}
void BlendFile::clear() {
    if (blockHeaderPtrs) {BLENDPARSER_FREE(blockHeaderPtrs);blockHeaderPtrs=NULL;}
    numBlockHeaders = 0;
    if (blendPtr) {
        if (blendPtrOwned)  BLENDPARSER_FREE(blendPtr);
        blendPtr=NULL;
    }
    blendPtrSize=0;
    blendPtrOwned=false;
    if (dna1Block) {dna1Block->~BlendDNA1Block();BLENDPARSER_FREE(dna1Block);dna1Block=NULL;}
    _blockDnaPtr=NULL;
    _blockDnaSize=0;
}

bool BlendFile::parseDNA1Block(const unsigned char* pDNA1,unsigned int dna1BlockDataSize) {
    const unsigned char* pDNA = pDNA1;if (!pDNA) return false;
    uint32_t parsedSize = 0;
    if (strncmp((const char*)pDNA,"SDNANAME",8)!=0) return false;
    pDNA+=8;parsedSize+=8;
    if (!dna1Block) {
        dna1Block = (BlendDNA1Block*)BLENDPARSER_MALLOC(sizeof(BlendDNA1Block));
        BLENDPARSER_PLACEMENT_NEW(dna1Block) BlendDNA1Block();
    }
    else dna1Block->clear();
    uint32_t tmp = 0;

    // Parse "NAME":
    dna1Block->numNames = ParseUInt32(pDNA,_bLittleEndian);pDNA+=4;parsedSize+=4;
    dna1Block->names = (const char**)BLENDPARSER_MALLOC(dna1Block->numNames*sizeof(const char*));
    for (uint32_t i=0;i<dna1Block->numNames;i++) {
        dna1Block->names[i] = (const char*) pDNA;
        //printf("%u)\t\"%s\"\n",i,dna1Block->names[i]);
        tmp = strlen(dna1Block->names[i])+1;
        parsedSize+=tmp;
        if (parsedSize>=dna1BlockDataSize) return false;
        pDNA+=tmp;
        // A few notes, Blender DNA structures are 4 byte aligned;
        // variable names that begin with ‘*’ are pointers for example ‘*next’ is a pointer named next;
        // variable names that end with brackets like ‘vector[3]’ is an array of three elements named vector;
        // and variable names that begin with ‘(*’ and end with ‘())’ are function pointers within Blender (don’t be to concern of these).
    }

    // Parse "TYPE":
    if (parsedSize+9>dna1BlockDataSize) return false;
    for (uint32_t i=0;i<4;i++) {
        // Blender DNA structures are 4 byte aligned:
        if (*((char*)(pDNA))!='T') {++pDNA;++parsedSize;}
    }
    if (strncmp((const char*)pDNA,"TYPE",4)!=0) return false;
    pDNA+=4;parsedSize+=4;
    dna1Block->numTypes = ParseUInt32(pDNA,_bLittleEndian);pDNA+=4;parsedSize+=4;
    dna1Block->types = (const char**)BLENDPARSER_MALLOC(dna1Block->numTypes*sizeof(const char*));
    for (uint32_t i=0;i<dna1Block->numTypes;i++) {
        dna1Block->types[i] = (const char*) pDNA;
        //printf("%u)\t\"%s\"\n",i,dna1Block->types[i]);
        tmp = strlen(dna1Block->types[i])+1;
        parsedSize+=tmp;
        if (parsedSize>=dna1BlockDataSize) break;
        pDNA+=tmp;
    }

    // Parse "TLEN: (they are endian-dependent unsigned short (2 bytes). Their size is dna1Block->numTypes. We just store their stream ptr here.)
    if (parsedSize+9>dna1BlockDataSize) return false;
    for (uint32_t i=0;i<4;i++) {
        // Blender DNA structures are 4 byte aligned:
        if (*((char*)(pDNA))!='T') {++pDNA;++parsedSize;}
    }
    if (strncmp((const char*)pDNA,"TLEN",4)!=0) return false;
    pDNA+=4;parsedSize+=4;
    if (parsedSize+2*dna1Block->numTypes>dna1BlockDataSize) return false;
    dna1Block->pTLens = pDNA;
    pDNA+=2*dna1Block->numTypes;parsedSize+=2*dna1Block->numTypes;
    // Debug-only loop
    /*for (uint32_t i=0;i<dna1Block->numTypes;i++) {
        uint16_t tlen = ParseUInt16(dna1Block->pTLens+(i*2),_bLittleEndian);
        printf("%u)\t\"%u\"\n",i,tlen);
    }*/

    // Parse "STRC":
    if (parsedSize+9>dna1BlockDataSize) return false;
    for (uint32_t i=0;i<4;i++) {
        // Blender DNA structures are 4 byte aligned:
        if (*((char*)(pDNA))!='S') {++pDNA;++parsedSize;}
    }
    if (strncmp((const char*)pDNA,"STRC",4)!=0) return false;
    pDNA+=4;parsedSize+=4;
    dna1Block->numStructs = ParseUInt32(pDNA,_bLittleEndian);pDNA+=4;parsedSize+=4;
    dna1Block->pStructs = (BlendDNA1Struct*)BLENDPARSER_MALLOC(dna1Block->numStructs*sizeof(BlendDNA1Struct));
    const unsigned long fieldIncrement = 100;
    dna1Block->numAllFields = fieldIncrement;
    dna1Block->pFieldsAllTogether = (BlendDNA1StructField*)BLENDPARSER_MALLOC(dna1Block->numAllFields*sizeof(BlendDNA1StructField));
    unsigned long numFields = 0;    // effective

    //printf("%s\n","STRC");
    for (uint32_t i=0;i<dna1Block->numStructs;i++) {
        BlendDNA1Struct& bstr = dna1Block->pStructs[i];
        bstr.fieldOffset = numFields;
        bstr.typeIdx = ParseUInt16(pDNA,_bLittleEndian);pDNA+=2;parsedSize+=2;
        if (parsedSize>=dna1BlockDataSize) return false;
        bstr.numFields = ParseUInt16(pDNA,_bLittleEndian);pDNA+=2;parsedSize+=2;

        // Debug
        //if (strcmp(dna1Block->types[bstr.typeIdx],"ID")==0)
        //printf("%u)\tIndex in types:\"%u\"\tFields:\"%u\"\n",i,bstr.typeIdx,bstr.numFields);

        if (parsedSize>=dna1BlockDataSize) return false;

        if (numFields+bstr.numFields>=dna1Block->numAllFields) {
            dna1Block->numAllFields+= fieldIncrement<bstr.numFields ? bstr.numFields : fieldIncrement;
            dna1Block->pFieldsAllTogether = (BlendDNA1StructField*)BLENDPARSER_REALLOC(dna1Block->pFieldsAllTogether,dna1Block->numAllFields*sizeof(BlendDNA1StructField));
        }
        for (uint16_t fd=0;fd<bstr.numFields;fd++) {
            BlendDNA1StructField& field = dna1Block->pFieldsAllTogether[numFields+fd];
            field.numElementsInName = 1;
            field.typeIdx = ParseUInt16(pDNA,_bLittleEndian);pDNA+=2;parsedSize+=2;
            if (parsedSize>=dna1BlockDataSize) return false;
            field.nameIdx = ParseUInt16(pDNA,_bLittleEndian);pDNA+=2;parsedSize+=2;
            field.numElementsInName = BlendDNA1Block::GetNumArrayElementsFromName(dna1Block->names[field.nameIdx]);  // This can be deferred to BlenFile::adjustAllTheOldMemoryPts() to save time
            //printf("\t%u)\tIdxInTypes:\"%u\"\tIdxInNames:\"%u\"\n",fd,field.typeIdx,field.nameIdx);
            if (parsedSize>dna1BlockDataSize) return false;
        }
        numFields+=bstr.numFields;
    }

    // save memory
    dna1Block->pFieldsAllTogether = (BlendDNA1StructField*)BLENDPARSER_REALLOC(dna1Block->pFieldsAllTogether,numFields*sizeof(BlendDNA1StructField));
    dna1Block->numAllFields = numFields;

    // Now we create the helper references types -> structs
    dna1Block->pTypeStructs = (BlendDNA1Struct**)BLENDPARSER_MALLOC(dna1Block->numTypes*sizeof(BlendDNA1Struct*));
    for (uint32_t tIdx=0;tIdx<dna1Block->numTypes;tIdx++)   {
        BlendDNA1Struct*& pbstr = dna1Block->pTypeStructs[tIdx];
        pbstr = NULL;
        for (uint32_t sIdx=0;sIdx<dna1Block->numStructs;sIdx++)   {
            if (dna1Block->pStructs[sIdx].typeIdx==tIdx) {
                pbstr = &dna1Block->pStructs[sIdx];
                break;
            }
        }
    }

    return true;
}


bool BlendFile::loadFromMemory(unsigned char* buf,unsigned long bufferSize,bool passBufOwnershipToMe) {
    clear();
    char signature[8] = "BLENDER";

    if (!buf || bufferSize<15)  {
        blendPtr = (unsigned char*) buf;
        blendPtrSize = bufferSize;
        blendPtrOwned = passBufOwnershipToMe;
        return false;
    }

    // Check signature
    {
        for (int i=0;i<7;i++) {
            if ((char)buf[i]!=signature[i]) {
#               ifdef BLENDPARSER_USE_ZLIB
                unsigned long ptrSize = 0;
                unsigned char* ptr = GzDecompressFromMemory(buf,bufferSize,&ptrSize);
                if (ptr)    {
                    bool ok = loadFromMemory(ptr,ptrSize,true);
                    if (passBufOwnershipToMe) BLENDPARSER_FREE((unsigned char*)buf);
                    return ok;
                }
                else {
                    if (passBufOwnershipToMe) BLENDPARSER_FREE((unsigned char*)buf);
                    return false;
                }
#               else // BLENDPARSER_USE_ZLIB
                strncpy(signature,(const char*)blendPtr,7);
                signature[7]='\0';
                printf("ERROR: Wrong signature: \"%s\"\n",signature);
                blendPtr = (unsigned char*) buf;
                blendPtrSize = bufferSize;
                blendPtrOwned = passBufOwnershipToMe;
                return false;
#               endif //BLENDPARSER_USE_ZLIB
            }
        }
    }

    blendPtr = (unsigned char*) buf;
    blendPtrSize = bufferSize;
    blendPtrOwned = passBufOwnershipToMe;

    _uiPointerSize = (char)blendPtr[7]=='-' ? 8 : 4;
    _bLittleEndian = (char)blendPtr[8]=='v' ? true : false;
    for (int i=0;i<3;i++) version[i]=(char)blendPtr[9+i];

    //printf("DBG: bufferSize: %lu\n",bufferSize);
    //printf("DBG: Version: \"%s\"\t_uiPointerSize: %u\t _bLittleEndian=%s\n",version,_uiPointerSize,_bLittleEndian?"true":"false");

    const unsigned long blockIncrement = 100;
    const unsigned long sizeOfPtr = sizeof(const unsigned char*);
    numBlockHeaders = blockIncrement;
    blockHeaderPtrs = (const unsigned char**) BLENDPARSER_MALLOC(numBlockHeaders*sizeOfPtr);
    const unsigned long objHeaderSize = 16 + _uiPointerSize;
    unsigned long objOffset = 12;unsigned long cnt=0;
    const unsigned char* pObj = &blendPtr[objOffset];
    uint32_t objDataSize = 0;
    while (pObj && (objOffset+objHeaderSize<=blendPtrSize)) {
        if (strncmp((const char*)pObj,"ENDB",4)==0) break;
        objDataSize = BlendBlock::GetDataSize(pObj,_bLittleEndian);

        if (strncmp((const char*)pObj,"DNA1",4)==0) {
            // DNA
            if (objDataSize>12) {
                const unsigned char* pDNA = pObj+objHeaderSize;
                if (!parseDNA1Block(pDNA,objDataSize)) {
                    if (dna1Block) {dna1Block->~BlendDNA1Block();BLENDPARSER_FREE(dna1Block);dna1Block=NULL;}
                }
                else {
                    _blockDnaPtr=pDNA;
                    _blockDnaSize=objDataSize;
                }
            }
        }
        else {
            if (cnt>=numBlockHeaders) {
                numBlockHeaders+=blockIncrement;
                blockHeaderPtrs = (const unsigned char**) BLENDPARSER_REALLOC(blockHeaderPtrs,numBlockHeaders*sizeOfPtr);
            }
            blockHeaderPtrs[cnt] = pObj;
            ++cnt;
        }

        objOffset+=objHeaderSize+objDataSize;
        pObj = &blendPtr[objOffset];
    }

    // Free unused memory
    numBlockHeaders=cnt;
    blockHeaderPtrs = (const unsigned char**) BLENDPARSER_REALLOC(blockHeaderPtrs,numBlockHeaders*sizeOfPtr);

    return (blendPtr!=NULL);
}

bool BlendFile::generateBlenderHeader(const char* saveFileName,const char* guardName,const char* namespaceName) const   {
    if (!dna1Block || !saveFileName || strlen(saveFileName)==0) return false;
    FILE* f = UTF8FileOpen(saveFileName,"wt");
    if (f) {
        dna1Block->fprintStructs(f,_bLittleEndian,blendPtr,guardName,namespaceName);
        fclose(f);
        return true;
    }
    return false;
}

bool BlendFile::generateBlenderSDNACpp(const char *saveFileName, const char *prefixText, const char* suffixText) const  {
    if (!_blockDnaPtr || !saveFileName || strlen(saveFileName)==0) return false;
    FILE* f = UTF8FileOpen(saveFileName,"wt");
    if (f) {
        if (blendPtr) fprintf(f,"\n// Generated using %.12s\n",blendPtr);
        if (prefixText && strlen(prefixText)>0) fprintf(f,"%s",prefixText);
        fprintf(f," = {");
        for (size_t i=0;i<_blockDnaSize;i++)    {
            if (i%120==0) fprintf(f,"\n");
            fprintf(f,"%u",(unsigned int) *((unsigned char*) (_blockDnaPtr+i)));
            if (i!=_blockDnaSize-1) fprintf(f,",");
        }
        fprintf(f,"};\n\n");
        if (suffixText && strlen(suffixText)>0) fprintf(f,"%s\n\n",suffixText);
        fclose(f);
        return true;
    }
    return false;
}

bool BlendFile::updatefbtBlendHeader(const char* oldfbtBlendPath,const char* newfbtBlendPath)	{
	if (!_blockDnaPtr || !oldfbtBlendPath  || strlen(oldfbtBlendPath)==0 || !newfbtBlendPath || strlen(newfbtBlendPath)==0) return false;
	unsigned long contentSize = 0;
	char* content = (char*) GetFileContent(oldfbtBlendPath,&contentSize,"rt");
	if (!content) return false;
	if (contentSize==0) {BLENDPARSER_FREE(content);content=NULL;return false;}
	const char* startReplacement = strstr(content, "// bfBlender.cpp: ");
	if (startReplacement) startReplacement = strchr(startReplacement,'\n');	// end line
	if (!startReplacement) {
		printf("Error: Can't find substring: \"// bfBlender.cpp: \" inside old fbtBlend.h file.\n");
		BLENDPARSER_FREE(content);content=NULL;return false;
	}
	const char* endReplacement = strstr(startReplacement, "//=============================================================");
	if (!endReplacement) {
		printf("Error: Can't find substring: \"//=============================================================\" inside old fbtBlend.h file.\n");
		BLENDPARSER_FREE(content);content=NULL;return false;
	}
	// Write newfbtBlendPath
	{
    FILE* f = UTF8FileOpen(newfbtBlendPath,"wt");
    if (f) {
		fwrite(content,startReplacement-content,1,f);
        if (blendPtr) fprintf(f,"\n\n// Generated using %.12s\n",blendPtr);
        fprintf(f,"unsigned char bfBlenderFBT[]");
        fprintf(f," = {");
        for (size_t i=0;i<_blockDnaSize;i++)    {
            if (i%120==0) fprintf(f,"\n");
            fprintf(f,"%u",(unsigned int) *((unsigned char*) (_blockDnaPtr+i)));
            if (i!=_blockDnaSize-1) fprintf(f,",");
        }
        fprintf(f,"};\n\n");
        fprintf(f,"int bfBlenderLen=sizeof(bfBlenderFBT);\n");
		fwrite(endReplacement,contentSize-(endReplacement-content),1,f);        
        fclose(f);
    }	
	}
	BLENDPARSER_FREE(content);content=NULL;

	return true;

}


int BlendFile::getVersion() const   {return (blendPtr && blockHeaderPtrs && dna1Block) ? 100*(uint16_t)(blendPtr[9]-'0')+10*(uint16_t)(blendPtr[10]-'0')+(uint16_t)(blendPtr[11]-'0') : 0;}
bool BlendFile::is32bit() const {return _uiPointerSize==4;}
bool BlendFile::isBigEndian() const {return !_bLittleEndian;}



int main(int argc, const char* argv[]) {
	const char* blendFilePath = argc>1 ? argv[1] : "test.blend";
	const char* fbtBlendPath = "../fbtBlend.h";
	bool ok = blendFilePath && FileExists(blendFilePath) && fbtBlendPath && FileExists(fbtBlendPath);
	if (ok) {
		BlendFile blendfile;
		ok = blendfile.loadFromFile(blendFilePath);	
		if (ok) {
			printf("Input Blend File: \"%s\" (v.%d %s %s)\n",blendFilePath,blendfile.getVersion(),blendfile.is32bit() ? "32-bit" : "64-bit",blendfile.isBigEndian() ? "big-endian" : "little-endian");
			printf("Generating \"Blender.h\"... ");		
			ok = blendfile.generateBlenderHeader();
			if (ok) {
				printf("Done.\nGenerating \"fbtBlend.h\"... ");
				ok = blendfile.updatefbtBlendHeader(fbtBlendPath,"fbtBlend.h");
				if (ok) printf("Done.\n");
				else printf("Error.\n");		
			}
			else printf("Error.\n");
		}
		else {
		printf("Error: could not read file \"%s\".\n",blendFilePath);
#		ifndef BLENDPARSER_USE_ZLIB
		printf("\t(is it compressed? If so you can build fbtBlendUpdater with BLENDPARSER_USE_ZLIB to read it, linking to zlib: -lz)\n");
#		endif
		}
	}
	else {
		printf("USAGE:\n-> An old (amalgamated) version of \"fbtBlend.h\" must be present in \"../\"\n");
		printf("-> A file named \"test.blend\" must be present in the same folder as th fbtBlendUpdater exacutable. It must have been saved using the desired version of Blender.\n");
		printf("Now you can fire fbtBlendUpdater from its own folder, and it should output the two new files \"fbtBlend.h\" and \"Blender.h\" in its own folder two (leaving the old copies in \"../\" intact.\n\n");	
	}
	return ok ? 0 : 1;
}

