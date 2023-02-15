#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

#extension GL_GOOGLE_include_directive: require

layout(row_major) uniform;
layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"
// include paths based on root of shader directory
#include "./Deferred/PBRCommon.glsl"

const int MAX_VOXEL_LEVELS = 15;

layout(set = 1, binding = 1) readonly buffer _VoxelInfo
{
	int activeLevels;
    uint pageSize;
    ivec3 dimensions;

    vec3 VoxelSize[MAX_VOXEL_LEVELS];
    vec3 HalfVoxel[MAX_VOXEL_LEVELS];
    vec3 step[MAX_VOXEL_LEVELS];
    vec3 tDelta[MAX_VOXEL_LEVELS];
} VoxelInfo;

layout(set = 1, binding = 2) readonly buffer _LevelInfo
{
    ivec3 localPageMask;
    ivec3 localPageVoxelDimensions;
    ivec3 localPageVoxelDimensionsP2;
    ivec3 localPageCounts;
} LevelInfos[MAX_VOXEL_LEVELS];

layout(set = 1, binding = 3) readonly buffer _LevelVoxels
{
	uint8_t voxels[];
} LevelVoxels[MAX_VOXEL_LEVELS];

//INPUT FROM VS
layout (location = 0) in vec4 inPixelPosition;
layout (location = 1) in vec2 inUV;

//OUTPUT TO MRTs
layout (location = 0) out vec4 outDiffuse;
// specular, metallic, roughness, emissive
layout (location = 1) out vec4 outSMRE;
// 
layout (location = 2) out vec4 outNormal;

struct RayInfo
{
	vec3 rayOrg;
	vec3 rayDir;
    vec3 rayDirSign;
    vec3 rayDirInvAbs;
    vec3 rayDirInv;
    int curMisses;
    
    vec3 lastStep;
    bool bHasStepped;
};

struct PageIdxAndMemOffset
{
    uint pageIDX;
    uint memoffset;
};

struct VoxelHitInfo
{
    vec3 location;
    vec3 normal;
    uint totalChecks;
};


void GetOffsets(in ivec3 InPosition, in uint InLevel, out PageIdxAndMemOffset oMem)
{           
    // find the local voxel
    ivec3 LocalVoxel = ( InPosition & LevelInfos[InLevel].localPageMask);
    uint localVoxelIdx = (LocalVoxel.x +
        LocalVoxel.y * LevelInfos[InLevel].localPageVoxelDimensions.x +
        LocalVoxel.z * LevelInfos[InLevel].localPageVoxelDimensions.x * LevelInfos[InLevel].localPageVoxelDimensions.y);

    // find which page we are on
    ivec3 PageCubePos = InPosition >> LevelInfos[InLevel].localPageVoxelDimensionsP2;
    uint pageIdx = (PageCubePos.x +
        PageCubePos.y * LevelInfos[InLevel].localPageCounts.x +
        PageCubePos.z * LevelInfos[InLevel].localPageCounts.x * LevelInfos[InLevel].localPageCounts.y);
                       
    uint memOffset = (pageIdx * VoxelInfo.pageSize) + localVoxelIdx; // always 1 * _dataTypeSize;

    oMem.pageIDX = pageIdx;
    oMem.memoffset = memOffset;
}

int GetUnScaledAtLevel(in ivec3 InPosition, uint InLevel)
{
    ivec3 levelPos = InPosition >> InLevel;
    PageIdxAndMemOffset pageAndMem;
    GetOffsets(InPosition, InLevel, pageAndMem);
    return LevelVoxels[InLevel].voxels[pageAndMem.memoffset];
}

bool ValidSample(in vec3 InPos) 
{
    if (all(greaterThanEqual(InPos, vec3(0.0f) )) &&
        all(lessThan(VoxelInfo.dimensions, InPos)) )
    {
        return true;
    }

    return false;
}

bool CastRay(in vec3 rayOrg, in vec3 rayDir, out VoxelHitInfo oInfo)
{
    RayInfo rayInfo;

    rayInfo.rayOrg = rayOrg;
    rayInfo.rayDir = rayDir;
    rayInfo.rayDirSign = sign(rayInfo.rayDir);

    float epsilon = 0.001f;
    vec3 ZeroEpsilon = vec3(equal(rayInfo.rayDir, vec3(0, 0, 0))) * epsilon;

    rayInfo.rayDirInv = 1.0f / (rayDir + ZeroEpsilon);
    rayInfo.rayDirInvAbs = abs(rayInfo.rayDirInv);
    
    uint CurrentLevel = VoxelInfo.activeLevels - 1;
    uint LastLevel = 0;

	// get in correct voxel spacing
	vec3 voxel = floor(rayInfo.rayOrg / VoxelInfo.VoxelSize[CurrentLevel]) * VoxelInfo.VoxelSize[CurrentLevel];
	vec3 tMax = (voxel - rayInfo.rayOrg + VoxelInfo.HalfVoxel[CurrentLevel] + VoxelInfo.step[CurrentLevel] * vec3(0.5f, 0.5f, 0.5f)) * (rayInfo.rayDirInv);

	vec3 dim = vec3(0, 0, 0);
	vec3 samplePos = voxel;

    oInfo.totalChecks = 0;

    for (int Iter = 0; Iter < 128; Iter++)
    {
        LastLevel = CurrentLevel;

        if (!ValidSample(samplePos))
        {
            return false;
        }

        oInfo.totalChecks++;

        bool bLevelChangeRecalc = false;

        // we hit something?
        if (GetUnScaledAtLevel(ivec3(samplePos), CurrentLevel) != 0)
        {
            rayInfo.curMisses = 0;

            if (CurrentLevel == 0)
            {
                vec3 normal = -sign(rayInfo.lastStep);
                oInfo.normal = normal;
                return true;
            }

            CurrentLevel--;
            bLevelChangeRecalc = true;
        }
        else
        {
            vec3 tMaxMins = min(tMax.yzx, tMax.zxy);
            dim = step(tMax, tMaxMins);
            tMax += dim * VoxelInfo.tDelta[CurrentLevel];
            rayInfo.lastStep = dim * VoxelInfo.step[CurrentLevel];
            samplePos += rayInfo.lastStep;

            if (!ValidSample(samplePos))
            {
                return false;
            }

            rayInfo.curMisses++;
            rayInfo.bHasStepped = true;

            if (CurrentLevel < VoxelInfo.activeLevels - 1 &&
                rayInfo.curMisses > 2 &&
                GetUnScaledAtLevel(ivec3(samplePos), CurrentLevel + 1) == 0)
            {
                bLevelChangeRecalc = true;
                CurrentLevel++;
                //hmmm think more about this 
                //InRayInfo.bHasStepped = false;
            }
        }

        if (bLevelChangeRecalc)
        {
            if (rayInfo.bHasStepped)
            {
                // did it step already
                vec3 normal = -sign(rayInfo.lastStep);

                vec3 VoxelCenter = samplePos + VoxelInfo.HalfVoxel[LastLevel];
                vec3 VoxelPlaneEdge = VoxelCenter + VoxelInfo.HalfVoxel[LastLevel] * normal;

                float denom = dot(normal, rayInfo.rayDir);
                if (denom == 0)
                    denom = 0.0000001f;

                vec3 p0l0 = (VoxelPlaneEdge - rayInfo.rayOrg);
                float t = dot(p0l0, normal) / denom;

                float epsilon = 0.001f;
                rayInfo.rayOrg = rayInfo.rayOrg + rayInfo.rayDir * (t + epsilon);
            }

            voxel = floor(rayInfo.rayOrg / VoxelInfo.VoxelSize[CurrentLevel]) * VoxelInfo.VoxelSize[CurrentLevel];
	        tMax = (voxel - rayInfo.rayOrg + VoxelInfo.HalfVoxel[CurrentLevel] + VoxelInfo.step[CurrentLevel] * vec3(0.5f, 0.5f, 0.5f)) * (rayInfo.rayDirInv);

	        dim = vec3(0, 0, 0);
            samplePos = voxel;
        }
    }

    return false;
}

void main()
{
	vec3 cameraRay = normalize(Multiply(vec4(inPixelPosition.xy, 1, 1.0), ViewConstants.InvViewProjectionMatrix).xyz);		
  
    outDiffuse = vec4( 0,0,0, 1 );
	outSMRE = vec4( 0,0,0, 0 );
	outNormal = vec4( 0,0,0, 0 );

    VoxelHitInfo info;
    if(CastRay(cameraRay, normalize(cameraRay), info))
    {
        outDiffuse.xyz = vec3(0.5f);
        outNormal.xyz = info.normal.xyz;
        gl_FragDepth = 0.2f;
    }

    gl_FragDepth = 1.0f;
}