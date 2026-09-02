#pragma once

#include <stdint.h>
#include "model.h"
#include "model_kvx.h"
#include "intvec.h"

/*
	PC branch: 2048, where stock Raze has 1024.

	The community voxel packs have outgrown the stock ceiling. Voxel Duke 3D
	alone defines 884 with its episode-4 set left off, and enabling that set
	takes it past 1024 together with this port's own weapon voxels - the engine
	then reports "Maximum number of voxels already defined" and silently drops
	everything after it, which reads as a broken pack rather than a limit.

	Nothing here is a format constraint. The number sizes four static arrays -
	voxlumps, voxscale, voxmodels and the voxrotate bit array - at sixteen bytes
	a slot, so this costs sixteen kilobytes. A voxel index is never saved or
	sent over the wire. The real ceiling is texinfo's tiletovox, an int16_t, so
	anything up to 32767 is safe; 2048 is simply the next sensible step.
*/
constexpr int MAXVOXELS = 2048;

struct voxmodel_t // : public mdmodel_t
{
    FVoxelModel* model = nullptr;
    float scale, bscale, zadd, yoffset;
    vec3_t siz;
    FVector3 piv;
    int32_t is8bit;
};



extern float voxscale[];
extern voxmodel_t* voxmodels[MAXVOXELS];
extern FixedBitArray<MAXVOXELS> voxrotate;

void voxInit();
void voxClear();
int voxDefine(int voxindex, const char* filename);
