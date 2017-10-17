// Linux:
// g++ -Os testConsole.cpp -I"./" -I"../" -o testConsole -D"FBT_USE_GZ_FILE=1" -lz
// Windows:
// cl ./testConsole.cpp /I"./" /I"../" /D"FBT_USE_GZ_FILE=1" /link /out:testConsole.exe zlib.lib Shell32.lib comdlg32.lib user32.lib kernel32.lib

// Can be compiler without "FBT_USE_GZ_FILE=1" and  -lz (zlib.lib) too, but test.blend must be uncompressed

// Emscripten: (requires a file named test.blend to be preloaded)
// em++ -O2 -fno-rtti -fno-exceptions -o html/test_console.html testConsole.cpp -I"./" -I"../" --preload-file test.blend -D"FBT_USE_GZ_FILE=1" -s ALLOW_MEMORY_GROWTH=1 -s USE_ZLIB=1 --closure 1

#define FBTBLEND_IMPLEMENTATION
#include "../fbtBlend.h"

#include <stdio.h>

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


int main(int argc, const char* argv[]) {
    fbtBlend fp;
    const char* filePath = argc>1 ? argv[1] : "test.blend";
    if (fp.parse(filePath)!=fbtFile::FS_OK)   return 1;

    fbtList& objects = fp.m_object;long cnt=-1;
    for (Blender::Object* ob = (Blender::Object*)objects.first; ob; ob = (Blender::Object*)ob->id.next)	{
        if (!ob) break;
        ++cnt;
        if (!ob->data || ob->type != (short) BL_OBTYPE_MESH)	{
            printf("%ld)\tOBJECT: \"%s\"\ttype=%d\n",cnt,ob->id.name,ob->type);
            continue;
        }
        // It's a Mesh

        printf("%ld)\tOBJECT: \"%s\"\n",cnt,ob->id.name);  // it's actually objectName

        const Blender::Mesh* me = (const Blender::Mesh*)ob->data;
        printf("\tMESH: \"%s\"\n",me->id.name);  // it's actually meshName

        // Fill 'mirrorModifier' and 'bArmature' and skip mesh if not good or not skeletal animated
        {
            const Blender::ModifierData* mdf = (const Blender::ModifierData*) ob->modifiers.first;
            while (mdf)    {
                // mdf->type(1= SubSurf 5=mirror 8=armature)
                printf("Object '%s' had modifier: %s (type=%d mode=%d stackindex=%d)\n",ob->id.name,mdf->name,mdf->type,mdf->mode,mdf->stackindex);
                if (mdf->error) {
                    printf("Error: %s\n",mdf->error);
                }
                else {
                    if (mdf->type==1) {
                        // Subsurf modifier:
                        const Blender::SubsurfModifierData* md = (const Blender::SubsurfModifierData*) mdf;
                        printf("SubsurfModifier: subdivType:%d levels:%d renderLevels:%d  flags:%d  use_opensubdiv:%d\n",md->subdivType,md->levels,md->renderLevels,md->flags,md->use_opensubdiv);
                    }
                    if (mdf->type==5) {
                        //Mirror modifier:
                        const Blender::MirrorModifierData* md = (const Blender::MirrorModifierData*) mdf;
                        printf("MirrorModifier: axis:%d flag:%d tolerance:%1.4f\n",md->axis,md->flag,md->tolerance);
                        if (md->mirror_ob) printf("\nMirrorObj: %s\n",md->mirror_ob->id.name);
                    }
                    else if (mdf->type==8) {
                        //Armature Modifier:
                        const Blender::ArmatureModifierData* md = (const Blender::ArmatureModifierData*) mdf;

                        printf("ArmatureModifier: deformflag:%d multi:%d\n",md->deformflag,md->multi);
                        if (md->object) printf("Armature Object: %s\n",md->object->id.name);
                        if (md->defgrp_name) printf("Deform Group Name: %s\n",md->defgrp_name);

                    }
                    //else fprintf(stderr,"Detected unknown modifier: %d for object: %s\n",mdf->type,ob->id.name);
                }
                mdf = mdf->next;
            }
        }
        //---------------------
    }
    return 0;
}
