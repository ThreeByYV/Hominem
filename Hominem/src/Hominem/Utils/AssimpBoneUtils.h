#pragma once

#include <assimp/scene.h>

#define MAX_NUM_BONES_PER_VERTEX 4

struct VertexBoneData
{ 
	uint32_t BoneIDs[MAX_NUM_BONES_PER_VERTEX] = { 0 }; //all the bones that affect the current vertex
	float Weights[MAX_NUM_BONES_PER_VERTEX] = { 0 }; //for each bone the equivalent weight

	VertexBoneData() {};

	void AddBoneData(uint32_t boneID, float weight)
	{
		for (uint32_t i = 0; ARRAY_SIZE_IN_ELEMENTS(BoneIDs); i++)
		{
			if (Weights[i] == 0.0)
			{
				BoneIDs[i] = boneID;
				Weights[i] = weight;
				HMN_CORE_INFO("Adding data for Bone {} weight {} index {}\n", boneID, weight, i);
				return;
			}
		}
	}
};

uint32_t GetBoneId(const aiBone* pBone);

void ParseSingleBone(uint32_t boneIdx, const aiBone* bone);

void ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh);

void ParseMeshes(const aiScene* pScene);

void ParseScene(const aiScene* pScene);
