// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphicsO.h"
#include "SPPAssetCache.h"
#include "ThreadPool.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	uint32_t GetGraphicsSceneVersion()
	{
		return 1;
	}

	const std::vector<VertexStream>& GetVertexStreams(const MeshVertex& InPlaceholder)
	{
		static std::vector<VertexStream> vertexStreams;
		if (vertexStreams.empty())
		{
			vertexStreams.push_back(
				CreateVertexStream(InPlaceholder, 
					InPlaceholder.position,
					InPlaceholder.normal, 
					InPlaceholder.texcoord[0], 
					InPlaceholder.texcoord[1], 
					InPlaceholder.color));
		}
		return vertexStreams;
	}

	void OMesh::InitializeGraphicsDeviceResources()
	{
		if (!_renderMesh)
		{
			auto graphicsDevice = GGI()->GetGraphicsDevice();
			auto firstMesh = GetMesh()->GetMeshElements().front();

			_renderMesh = graphicsDevice->CreateStaticMesh();

			_renderMesh->SetMeshArgs({
				.vertexStreams = GetVertexStreams(MeshVertex{}),
				.vertexResource = firstMesh->VertexResource,
				.indexResource = firstMesh->IndexResource
				});

			RunOnRT([this]()
				{
					_renderMesh->Initialize();
				});
		}
	}

	void OMesh::UinitializeGraphicsDeviceResources()
	{
		SE_ASSERT(IsOnCPUThread());
		RunOnRT([_renderMesh=_renderMesh]()
			{
				//
			});
		_renderMesh.reset();
	}

	ORenderableScene::ORenderableScene(const std::string& InName, SPPDirectory* InParent) : OScene(InName, InParent)
	{
		_renderScene = GGI()->GetGraphicsDevice()->CreateRenderScene();
		GGI()->GetGraphicsDevice()->AddScene(_renderScene.get());
	}

	ORenderableScene::~ORenderableScene()
	{
		_children.clear();

		if (_renderScene)
		{
			RunOnRT([_renderScene = this->_renderScene]()
			{
				GGI()->GetGraphicsDevice()->RemoveScene(_renderScene.get());
			});
			_renderScene.reset();
		}
	}

	void ORenderableElement::UpdateSelection(bool IsSelected)
	{
		_selected = IsSelected;
	}

	void OMeshElement::UpdateSelection(bool IsSelected)
	{
		//if (_renderableMesh)
		//{
		//	_renderableMesh->SetSelected(IsSelected);
		//}
	}

	void OMeshElement::AddedToScene(class OScene* InScene)
	{
		ORenderableElement::AddedToScene(InScene);

		if (!InScene) return; 

		auto thisRenderableScene = dynamic_cast<ORenderableScene*>(InScene);
		SE_ASSERT(thisRenderableScene);

		auto SceneType = InScene->get_type();
		if (_meshObj &&
			_meshObj->GetMesh() &&
			!_meshObj->GetMesh()->GetMeshElements().empty())
		{
			SE_ASSERT(GGI() && GGI()->GetGraphicsDevice());
					
			auto graphicsDevice = GGI()->GetGraphicsDevice();
			auto curRenderScene = thisRenderableScene->GetRenderScene();
			auto firstMesh = _meshObj->GetMesh()->GetMeshElements().front();
						
			auto localToWorld = GenerateLocalToWorld(true);	

			Vector3d TopPosition = GetTopBeforeScene()->GetPosition();
			Vector3d FinalPosition = localToWorld.block<1, 3>(3, 0).cast<double>() + TopPosition;
			_bounds = Sphere();

			if (_meshObj)
			{
				auto curMesh = _meshObj->GetMesh();
				if (curMesh)
				{
					_bounds = curMesh->GetBounds();

					Vector3d newBoundsCenter = (ToVector4(_bounds.GetCenter().cast<float>()) * localToWorld).cast<double>().head<3>() + TopPosition;					
					float maxScale = std::fabs(std::max(std::max(localToWorld.block<1, 3>(0, 0).norm(), localToWorld.block<1, 3>(1, 0).norm()), localToWorld.block<1, 3>(2, 0).norm()));
										
					_bounds = Sphere(newBoundsCenter, _bounds.GetRadius() * maxScale);
				}
			}

			if (!_bounds)
			{
				_bounds = Sphere(FinalPosition, 1);
			}

			SE_ASSERT(_materialObj);

			_meshObj->InitializeGraphicsDeviceResources();
			_materialObj->InitializeGraphicsDeviceResources();

			_renderableMesh = graphicsDevice->CreateRenderableMesh();

			_renderableMesh->SetRenderableMeshArgs({
				.mesh = _meshObj->GetDeviceMesh(),
				.material = _materialObj->GetMaterial()
				});

			_renderableMesh->SetArgs({
				.position = FinalPosition,
				.eulerRotationYPR = _rotation,
				.scale = _scale,
				.bIsStatic = _bIsStatic,
				.bounds = _bounds
				});

			_renderableMesh->AddToRenderScene(thisRenderableScene->GetRenderScene());
		}
	}
	void OMeshElement::RemovedFromScene()
	{
		ORenderableElement::RemovedFromScene();

		if (_renderableMesh)
		{
			RunOnRT([_renderableMesh = this->_renderableMesh]()
			{
				_renderableMesh->RemoveFromRenderScene();
			});
			_renderableMesh.reset();
		}
	}

	void OMeshElement::SetMesh(OMesh* InMesh)
	{
		if (_meshObj == InMesh) return;
		_meshObj = InMesh;
		// update bounds...
	}

	bool OMeshElement::Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const
	{
		auto localToWorld = GenerateLocalToWorld();
		//if (_bounds)
		{
			if (_meshObj &&
				_meshObj->GetMesh() &&
				!_meshObj->GetMesh()->GetMeshElements().empty())
			{
				auto meshElements = _meshObj->GetMesh()->GetMeshElements();

				for (auto& curMesh : meshElements)
				{
					auto curBounds = curMesh->Bounds.Transform(localToWorld);

					if (curBounds)
					{
						if (Intersection::Intersect_RaySphere(InRay, curBounds, oInfo.location))
						{
							oInfo.hitName = curMesh->Name;
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	std::shared_ptr<IMaterialParameter> MatParameterToIMat(BaseMaterialParameter* InParam)
	{
		static auto paramFloat = rttr::type::get<ConstantParamter_Float>();
		static auto paramFloat2 = rttr::type::get<ConstantParamter_Float2>();
		static auto paramFloat3 = rttr::type::get<ConstantParamter_Float3>();
		static auto paramFloat4 = rttr::type::get<ConstantParamter_Float4>();
		static auto paramTexture = rttr::type::get<TextureParamater>();

		auto curType = InParam->get_type();

		if (curType == paramFloat)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float*>(InParam);
			return std::make_shared< FloatParamter >( castedValue->GetValue() );
		}
		else if (curType == paramFloat2)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float2*>(InParam);
			return std::make_shared< Float2Paramter >(castedValue->GetValue());
		}
		else if (curType == paramFloat3)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float3*>(InParam);
			return std::make_shared< Float3Paramter >(castedValue->GetValue());
		}
		else if (curType == paramFloat4)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float4*>(InParam);
			return std::make_shared< Float4Paramter >(castedValue->GetValue());
		}
		else if (curType == paramTexture)
		{
			auto castedValue = rttr::rttr_cast<TextureParamater*>(InParam);
			auto thisTexture = castedValue->GetValue();
			if (thisTexture)
			{
				thisTexture->InitializeGraphicsDeviceResources();
			}
			return thisTexture->GetDeviceTexture();
		}

		return nullptr;
	}

	void OMaterial::InitializeGraphicsDeviceResources()
	{
		if (!_material)
		{
			std::map< std::string, std::shared_ptr<IMaterialParameter> > parameterMap;

			for (auto& [key, value] : _parameters)
			{
				auto imapParam = MatParameterToIMat(value.GetParam());
				
				if (imapParam)
				{					
					parameterMap[key] = imapParam;
				}
			}

			auto graphicsDevice = GGI()->GetGraphicsDevice();
			_material = graphicsDevice->CreateMaterial();
			_material->SetMaterialArgs(
				{
					.parameterMap = parameterMap
				}
			);
		}
	}
	void OMaterial::UinitializeGraphicsDeviceResources()
	{
		SE_ASSERT(IsOnCPUThread());
		RunOnRT([_material=_material]()
			{
				//dies automagically 
			});
		_material.reset();
	}

	bool OTexture::LoadFromDisk(const char* FileName)
	{		
		TextureAsset testTexture;
		
		AssetPath curTexture(FileName);
		auto uExt = str_to_upper(curTexture.GetExtension());

		if (uExt != ".KTX2")
		{
			AssetPath cachedTexture;
			bool bHasCache = GetCachedFile(curTexture, cachedTexture, ".KTX2" );

			if (!bHasCache)
			{			
				stdfs::path TmpKTX2File = cachedTexture.GetAbsolutePath().replace_filename("TMPTEXT.KTX2");

				auto parentPath = TmpKTX2File.parent_path();
				stdfs::create_directories(parentPath);

				const bool bHasAlpha = false;
				if (GenerateMipMapCompressedTexture(FileName, TmpKTX2File.generic_string().c_str(), bHasAlpha))
				{
					stdfs::remove(cachedTexture.GetAbsolutePath());
					stdfs::rename(TmpKTX2File, cachedTexture.GetAbsolutePath());
					curTexture = cachedTexture;
				}
			}
			else
			{
				curTexture = cachedTexture;
			}
		}

		if (testTexture.LoadFromDisk(*curTexture))
		{
			_width = testTexture.width;
			_height = testTexture.height;
			_faceData = testTexture.faceData;
			_format = testTexture.format;
		}			

		return false;
	}

	void OTexture::InitializeGraphicsDeviceResources()
	{
		if (!_texture)
		{
			auto graphicsDevice = GGI()->GetGraphicsDevice();
			_texture = graphicsDevice->CreateTexture();

			RunOnRT([this]()
				{
					_texture->Initialize(
						TextureAsset{
							.width = _width,
							.height = _height,
							.format = _format,							
							.faceData = _faceData,
						});
				});
		}
	}

	void OTexture::UinitializeGraphicsDeviceResources()
	{
		SE_ASSERT(IsOnCPUThread());
		RunOnRT([_texture = _texture]()
			{
				//dies automagically 
			});
		_texture.reset();
	}

	void OShader::InitializeGraphicsDeviceResources()
	{
		if (!_shader)
		{
			auto graphicsDevice = GGI()->GetGraphicsDevice();
			_shader = graphicsDevice->CreateShader();

			RunOnRT([shader =_shader,
				shaderType = _shaderType, 
				filePath = _filePath,
				entryPoint = _entryPoint]()
				{
					shader->Initialize(shaderType);
					shader->CompileShaderFromFile(filePath.c_str(), entryPoint.c_str());
				});
		}
	}

	void OShader::UinitializeGraphicsDeviceResources()
	{
		_shader.reset();
	}

	//LIGHTS
	void OSun::AddedToScene(class OScene* InScene)
	{
		OLight::AddedToScene(InScene);

		if (!InScene) return;

		auto thisRenderableScene = dynamic_cast<ORenderableScene*>(InScene);
		SE_ASSERT(thisRenderableScene);

		_bounds = Sphere(Vector3d(0,0,0), 20000000);

		auto sceneGD = GGI()->GetGraphicsDevice();
		auto renderScene = thisRenderableScene->GetRenderScene();
		if (sceneGD && renderScene)
		{
			_light = sceneGD->CreateSunLight();

			auto localToWorld = GenerateLocalToWorld(true);
			Vector3d TopPosition = GetTopBeforeScene()->GetPosition();
			Vector3d FinalPosition = localToWorld.block<1, 3>(3, 0).cast<double>() + TopPosition;

			_light->SetArgs({
				.position = FinalPosition,
				.eulerRotationYPR = _rotation,
				.scale = _scale,
				.bIsStatic = _bIsStatic,
				.bounds = _bounds
				});

			_light->SetLightArgs(
				{
					.irradiance = _irradiance
				}
			);

			_light->AddToRenderScene(renderScene);
		}
	}
	void OSun::RemovedFromScene()
	{
		OLight::RemovedFromScene();

		if (_light)
		{
			RunOnRT([_light = this->_light]()
			{
				_light->RemoveFromRenderScene();
			});
			_light.reset();
		}
	}
}

using namespace SPP;

RTTR_REGISTRATION
{	
	rttr::registration::class_<ORenderableScene>("ORenderableScene")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OMesh>("OMesh")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OTexture>("OTexture")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;

	rttr::registration::class_<ORenderableElement>("ORenderableElement")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OMeshElement>("OMeshElement")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_meshObj", &OMeshElement::_meshObj)(rttr::policy::prop::as_reference_wrapper)
		.property("_materialObj", &OMeshElement::_materialObj)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<OMaterial>("OMaterial")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)		
		.property("_parameters", &OMaterial::_parameters)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<OShader>("OShader")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<ODebugScene>("ODebugScene")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	// paramters
	rttr::registration::class_<BaseMaterialParameter>("BaseMaterialParameter");

	rttr::registration::class_<ConstantParamter_Float>("ConstantParamter_Float")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &ConstantParamter_Float::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<ConstantParamter_Float2>("ConstantParamter_Float2")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		.property("_value", &ConstantParamter_Float2::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<ConstantParamter_Float3>("ConstantParamter_Float3")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &ConstantParamter_Float3::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<ConstantParamter_Float4>("ConstantParamter_Float4")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &ConstantParamter_Float4::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<TextureParamater>("TextureParamater")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &TextureParamater::_value)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<MaterialParameterContainer>("MaterialParameterContainer")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		.property("_param", &MaterialParameterContainer::_param)(rttr::policy::prop::as_reference_wrapper)
		;


	//lights
	rttr::registration::class_<OLight>("OLight")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_irradiance", &OLight::_irradiance)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<OSun>("OSun")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
	rttr::registration::class_<OPointLight>("OPointLight")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;
	rttr::registration::class_<OSpotLight>("OSpotLight")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;
}