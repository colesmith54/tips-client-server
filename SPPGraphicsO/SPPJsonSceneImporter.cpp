// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPJsonSceneImporter.h"
#include "SPPJsonUtils.h"
#include "SPPGraphics.h"

#include "ThreadPool.h"

namespace SPP
{
	ORenderableScene* LoadJsonScene(const char* FilePath)
	{
		Json::Value JsonScene;
		if (!FileToJson(FilePath, JsonScene))
		{
			return nullptr;
		}

		return nullptr;

		//TODO fix this?
#if 0
		std::string ParentPath = stdfs::path(FilePath).parent_path().generic_string();
		std::string SimpleSceneName = stdfs::path(FilePath).stem().generic_string();
		auto FileScene = AllocateObject<ORenderableScene>(SimpleSceneName, nullptr);

		Json::Value materials = JsonScene.get("materials", Json::Value::nullSingleton());
		Json::Value meshes = JsonScene.get("meshes", Json::Value::nullSingleton());

		std::map<std::string, OMaterial*> MaterialMap;
		std::map<std::string, OTexture*> TextureMap;
		std::map<std::string, OMesh*> MeshMap;

		auto vsShader = AllocateObject<OShader>("MeshVS", FileScene);
		auto psShader = AllocateObject<OShader>("MeshPS", FileScene);

		vsShader->Initialize(EShaderType::Vertex, "shaders/SimpleTextureMesh.hlsl", "main_vs");
		psShader->Initialize(EShaderType::Pixel, "shaders/SimpleTextureMesh.hlsl", "main_ps");

		if (!materials.isNull() && materials.isArray())
		{
			for (int32_t Iter = 0; Iter < materials.size(); Iter++)
			{
				auto currentMaterial = materials[Iter];

				Json::Value jName = currentMaterial.get("name", Json::Value::nullSingleton());
				Json::Value textures = currentMaterial.get("textures", Json::Value::nullSingleton());

				std::string MatName = jName.asCString();
				auto meshMat = AllocateObject<OMaterial>(MatName + ".mat", FileScene);

				auto &matshaders = meshMat->GetShaders();
				matshaders.push_back(vsShader);
				matshaders.push_back(psShader);

				MaterialMap[jName.asCString()] = meshMat;

				if (!textures.isNull())
				{
					Json::Value diffuse = textures.get("diffuse", Json::Value::nullSingleton());

					if (!diffuse.isNull() && diffuse.isArray())
					{
						for (int32_t Iter = 0; Iter < diffuse.size(); Iter++)
						{
							std::string CurTextureName = diffuse[Iter].asCString();
							auto foundTexture = TextureMap.find(CurTextureName);
							if (foundTexture == TextureMap.end())
							{
								auto curTexture = AllocateObject<OTexture>(CurTextureName, FileScene);

								curTexture->LoadFromDisk( ((ParentPath + "/") + CurTextureName).c_str() );

								TextureMap[CurTextureName] = curTexture;
								meshMat->SetTexture(curTexture, 0);
							}
							else
							{
								meshMat->SetTexture(foundTexture->second, 0);
							}
							// just stop at 1 for now
							break;
						}
					}
				}
			}
		}

		if (!meshes.isNull() && meshes.isArray())
		{
			for (int32_t Iter = 0; Iter < meshes.size(); Iter++)
			{
				auto currentMesh = meshes[Iter];

				Json::Value jName = currentMesh.get("name", Json::Value::nullSingleton());
				Json::Value jMaterials = currentMesh.get("materials", Json::Value::nullSingleton());
				Json::Value jMesh = currentMesh.get("mesh", Json::Value::nullSingleton());
				Json::Value jTransform = currentMesh.get("transform", Json::Value::nullSingleton());
				
				if (!jName.isNull() &&
					!jMaterials.isNull() &&
					!jMesh.isNull() &&
					!jTransform.isNull())
				{
					auto meshElement = AllocateObject<OMeshElement>(jName.asCString(), FileScene);

					Json::Value jLocation = jTransform.get("location", Json::Value::nullSingleton());
					Json::Value jRot = jTransform.get("rotation", Json::Value::nullSingleton());
					Json::Value jScale = jTransform.get("scale", Json::Value::nullSingleton());
					
					auto& curRot = meshElement->GetRotation();
					auto& curPos = meshElement->GetPosition();
					auto& curScale = meshElement->GetScale();

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{						
						curPos[Iter] = jLocation[Iter].asDouble();
					}

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{						
						curRot[Iter] = jRot[Iter].asFloat();
					}
										
					std::swap(curRot[0], curRot[1]);
					curRot[1] = -curRot[1];
					std::swap(curRot[0], curRot[2]);
					curRot[0] = -curRot[0];

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{						
						curScale[Iter] = jScale[Iter].asFloat();
					}

					std::string MeshName = jMesh.asCString();
					auto foundMesh = MeshMap.find(MeshName);
					if (foundMesh == MeshMap.end())
					{
						auto meshtest = std::make_shared< Mesh>();

						auto MeshPath = (ParentPath + "/") + MeshName + ".bin";

						meshtest->LoadSimpleBinaryMesh(MeshPath.c_str());

						auto loadedMesh = AllocateObject<OMesh>(MeshName+".SM", FileScene);
						loadedMesh->SetMesh(meshtest);

						MeshMap[MeshName] = loadedMesh;
						meshElement->SetMesh(loadedMesh);
					}
					else
					{
						meshElement->SetMesh(foundMesh->second);
					}

					for (int32_t Iter = 0; Iter < jMaterials.size(); Iter++)
					{
						auto curMat = jMaterials[Iter];
						std::string MatName = curMat.asCString();

						auto foundMat = MaterialMap.find(MatName);

						meshElement->SetMaterial(foundMat->second);
						//hmm 
						break;
					}

					FileScene->AddChild(meshElement);
				}
			}
		}

		return FileScene;
#endif
	}
}
