// Linux:
// g++ -Os -no-pie testConsole.cpp -I"./" -I"../" -o testConsole -D"FBT_USE_GZ_FILE=1" -lz
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

    // Object List (probably the only relevant in most cases)
    long cnt=-1;
    fbtList& objects = fp.m_object;
    if (objects.first) printf("____________\nOBJECT LIST:\n____________\n");
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
                        printf("SubsurfModifier: subdivType:%d levels:%d renderLevels:%d  flags:%d \n",md->subdivType,md->levels,md->renderLevels,md->flags);
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
                        printf("Deform Group Name: %s\n",md->defgrp_name);

                    }
                    //else fprintf(stderr,"Detected unknown modifier: %d for object: %s\n",mdf->type,ob->id.name);
                }
                mdf = mdf->next;
            }
        }
        //---------------------
    }

    // However we can directly list other structs (most of them are better accessed through the Object List I suppose)
    /* From the declaration of struct fbtBlend:
    fbtList m_scene;
    fbtList m_library;
    fbtList m_object;
    fbtList m_mesh;
    fbtList m_curve;
    fbtList m_mball;
    fbtList m_mat;
    fbtList m_tex;
    fbtList m_image;
    fbtList m_latt;
    fbtList m_lamp;
    fbtList m_camera;
    fbtList m_ipo;
    fbtList m_key;
    fbtList m_world;
    fbtList m_screen;
    fbtList m_script;
    fbtList m_vfont;
    fbtList m_text;
    fbtList m_sound;
    fbtList m_group;
    fbtList m_armature;
    fbtList m_action;
    fbtList m_nodetree;
    fbtList m_brush;
    fbtList m_particle;
    fbtList m_wm;
    fbtList m_gpencil;*/

    // As an example: Image List
    cnt=-1;
    fbtList& images = fp.m_image;
    if (images.first) printf("\n___________\nIMAGE LIST:\n___________\n");
    for (Blender::Image* im = (Blender::Image*)images.first; im; im = (Blender::Image*)im->id.next)	{
        if (!im) break;
        ++cnt;
        printf("%ld)\tIMAGE: \"%s\"\ttype=%d\tpath=\"%s\"\tpacked:%s\n",cnt,im->id.name,im->type,im->name,im->packedfile && im->packedfile->size>0 ? "true" : "false");   // There's actually a ListBase im->packedfiles too (?)
        //if (im->preview) printf("\tPreview Image: Size[0](%d,%d) Size[1](%d,%d)\n",im->preview->w[0],im->preview->h[0],im->preview->w[1],im->preview->h[1]);   // No idea on what this is... (for sure it's not the image size).
    }

    // As an example: Scene List
    cnt=-1;
    fbtList& scenes = fp.m_scene;
    if (scenes.first) printf("\n___________\nSCENE LIST:\n___________\n");
    for (Blender::Scene* sc = (Blender::Scene*)scenes.first; sc; sc = (Blender::Scene*)sc->id.next)	{
        if (!sc) break;
        ++cnt;
        printf("%ld)\tSCENE: \"%s\"\n",cnt,sc->id.name);
        if (sc->preview)  printf("\tPreview Image: Size[0](%d,%d) Size[1](%d,%d)\n",sc->preview->w[0],sc->preview->h[0],sc->preview->w[1],sc->preview->h[1]);
        if (sc->world) printf("\tWorld: \"%s\"\n",sc->world->id.name);
        if (sc->camera) printf("\tCamera: \"%s\"\n",sc->camera->id.name);
    }

    return 0;
}
