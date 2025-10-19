
#include "hmnpch.h"
#include "AssimpBoneUtils.h"
#include "Hominem/Core/Hominem.h"
#include <assimp/postprocess.h>


std::vector<VertexBoneData> vertexToBones;
std::vector<uint32_t> meshBaseVertex;
std::map<std::string, uint32_t> boneNameToIndexMap;

//skeletal anim is impl in the vertex shader, only place we you can change vertex pos
//i.e. you need a reversed mapping from vertex to the bones that influence it, to give to the vertex shader to calc the transformation of each vertex based on it's bones!
void ParseSingleBone(uint32_t meshIdx, const aiBone* pBone)
{
    if (!pBone)
    {
        HMN_CORE_INFO("Bone '{}': is null", meshIdx);
        return;
    }

    HMN_CORE_INFO("Bone '{}': {} num vertices affected by this bone: {}", meshIdx, pBone->mName.C_Str(), pBone->mNumWeights);


    uint32_t boneID = GetBoneId(pBone);
    HMN_CORE_INFO("Bone Id: {}\n", boneID);

    for (uint32_t i = 0; i < pBone->mNumWeights; ++i)
    {
        if (i == 0) HMN_CORE_INFO("\n");
        const aiVertexWeight& vw = pBone->mWeights[i]; //extracting the vertex weights from the bones
        
        //this creates the mapping from the vertices to bones for the vs
        //batches unique indices together in a stack to send to the vs, starting at zero and stacking up wherever the final elem is in the last batch
        uint32_t globalVertexId = meshBaseVertex[meshIdx] + vw.mVertexId;
        HMN_CORE_INFO("{}: vertex id {}  weight {}", i, vw.mVertexId, vw.mWeight);

        HMN_CORE_ASSERT(
            globalVertexId < vertexToBones.size(),
            "Out-of-range vertex-to-bone access in ParseSingleBone(): "
            "globalVertexId={}  vertexToBones.size={}  meshIdx={}  localVertex={}  bone={}",
            globalVertexId, vertexToBones.size(), meshIdx, vw.mVertexId, pBone->mName.C_Str()
        );

        vertexToBones[globalVertexId].AddBoneData(boneID, vw.mWeight);
    }

    HMN_CORE_INFO("\n");
}

void ParseMeshBones(uint32_t meshIndex, const aiMesh* pMesh)
{
    for (uint32_t i = 0; i < pMesh->mNumBones; ++i)
    {
        ParseSingleBone(meshIndex, pMesh->mBones[i]);
    }
}

void ParseMeshes(const aiScene* pScene)
{
    HMN_CORE_INFO("Parsing {} meshes \n\n", pScene->mNumMeshes);

    uint32_t totalVertices = 0;
    uint32_t totalIndices = 0;
    uint32_t totalBones = 0;

    meshBaseVertex.resize(pScene->mNumMeshes); //for each mesh we have a base idx, but we have to calc them

    for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
    {
        //aiMesh contains all the bones in the mesh
        const aiMesh* pMesh = pScene->mMeshes[i]; // this is the second highest level below the aiScene, it is an array

        uint32_t numVertices = pMesh->mNumVertices;
        uint32_t numIndices = pMesh->mNumFaces * 3;
        uint32_t numBones = pMesh->mNumBones;
        meshBaseVertex[i] = totalVertices;

        HMN_CORE_INFO("Mesh {} {}: vertices {} indices {} bones {}\n\n", i, pMesh->mName.C_Str(), numVertices, numIndices, numBones);

        totalVertices += numVertices;
        totalIndices += numIndices;
        totalBones += numBones;

        //below will resize again and again for each new mesh
        vertexToBones.resize(totalVertices); //this additional space will be needed when mesh is parsed

        if (pMesh->HasBones())
        {
            ParseMeshBones(i, pMesh);
        }

        HMN_CORE_INFO("\n");
    }

    HMN_CORE_INFO("\nTotal vertices {} total indices {} total bones {}\n", totalVertices, totalIndices, totalBones);

}

void ParseScene(const aiScene* pScene)
{
    ParseMeshes(pScene);
}

uint32_t GetBoneId(const aiBone* pBone)
{
    uint32_t boneID = 0;
    std::string boneName{ pBone->mName.C_Str() };

    if (boneNameToIndexMap.find(boneName) == boneNameToIndexMap.end())
    {
        //allocate an index for a new bone here
        boneID = boneNameToIndexMap.size();
        boneNameToIndexMap[boneName] = boneID;
    }
    else
    {
        boneID = boneNameToIndexMap[boneName];
    }

    return boneID;
}
