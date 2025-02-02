// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPSceneRendering.h"

#include "SPPPooledChunkBuffer.h"
#include "ThreadPool.h"
#include "SPPGraphics.h"

namespace SPP
{	
	GlobalRenderableID::GlobalRenderableID(std::shared_ptr<class RT_RenderScene> currentScene) : 
		_scene(currentScene)
	{
		_globalID = currentScene->GetGlobalRenderableID();
	}	

	GlobalRenderableID::~GlobalRenderableID()
	{
		if (auto lckScene = _scene.lock())
		{
			lckScene->ReturnToGlobalRenderableID(_globalID);
		}
	}

	void Renderable::_AddToRenderScene(class RT_RenderScene* InScene)
	{
		SE_ASSERT(InScene);
		SE_ASSERT(IsOnGPUThread());

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();

		_parentScene = InScene;
		_parentScene->AddRenderable(this);
	};

	void Renderable::_RemoveFromRenderScene()
	{
		SE_ASSERT(_parentScene);
		SE_ASSERT(IsOnGPUThread());
		_parentScene->RemoveRenderable(this);
		_parentScene = nullptr;
	};

	RT_StaticMesh::RT_StaticMesh()
	{
		_vertexBuffer = GGI()->GetGraphicsDevice()->CreateBuffer(GPUBufferType::Vertex);
		_indexBuffer = GGI()->GetGraphicsDevice()->CreateBuffer(GPUBufferType::Index);
	}

	void RT_StaticMesh::Initialize()
	{
		SE_ASSERT(IsOnGPUThread());
		SE_ASSERT(_indexResource->GetPerElementSize() == 4);
		_vertexBuffer->Initialize(_vertexResource);
		_indexBuffer->Initialize(_indexResource);
	}


	RT_RenderScene::RT_RenderScene()
	{
		_viewCPU.Initialize(Vector3d(0, 0, 0), Vector3(0, 0, 0), 65.0f, 1.77f);
		_octree.Initialize(Vector3d(0, 0, 0), 50000, 3);

		_renderables.resize(1024);
	}
	RT_RenderScene::~RT_RenderScene() {}

	void RT_RenderScene::AddedToGraphicsDevice() {};
	void RT_RenderScene::RemovedFromGraphicsDevice() {};	

	void RT_RenderScene::SetRenderToBackBuffer(bool bInRenderToBackBuffer)
	{
		_bRenderToBackBuffer = bInRenderToBackBuffer;
	}
	void RT_RenderScene::SetUseBackBufferDepthWithCustomColor(bool bInUseBackBufferDepths)
	{
		_bUseBBWithCustomColor = bInUseBackBufferDepths;
	}
	void RT_RenderScene::SetSkyBox(GPUReferencer< GPUTexture > InSkyBox)
	{
		_skyBox = InSkyBox;
	}

	PooledChunkBuffer GPooledBuffer;

	struct ColoBGRA
	{
		uint8_t b;
		uint8_t g;		
		uint8_t r;
		uint8_t a;
	};

	void RT_RenderScene::PushUIUpdate(const Vector2i& FullSize, const Vector2i& Start, const Vector2i& Extents, const void* Memory, uint32_t MemorySize)
	{
		uint32_t UpdateSize = Extents[0] * Extents[1] * sizeof(ColoBGRA);
		auto CurrentChunk = GPooledBuffer.GetChunk(UpdateSize);

		auto startX = Start[0];
		auto startY = Start[1];
		auto copySize = sizeof(ColoBGRA) * Extents[0];

		for (int32_t RowIter = 0; RowIter < Extents[1]; RowIter++)
		{
			int32_t SrcIdx = ((startY + RowIter) * FullSize[0]) + startX;
			int32_t DstIdx = RowIter * Extents[0];

			memcpy((ColoBGRA*)CurrentChunk->data + DstIdx, (ColoBGRA*)Memory + SrcIdx, copySize);
		}

		auto gpuCommand = RunOnRT([CurrentChunk = CurrentChunk, Start, Extents, UpdateSize, this]()
			{
				if (_offscreenUI)
				{
					_offscreenUI->PushAsyncUpdate(Start, Extents, CurrentChunk->data, UpdateSize);
				}
			});

	}

	void RT_RenderScene::SetDepthTarget(GPUReferencer< GPURenderTarget > InActiveDepth)
	{
		_activeDepth = InActiveDepth;
	}

	void RT_RenderScene::UnsetAllRTs()
	{
		for (int32_t Iter = 0; Iter < ARRAY_SIZE(_activeRTs); Iter++)
		{
			_activeRTs[Iter].Reset();
		}
		_activeDepth.Reset();
	}

	void RT_RenderScene::AddRenderable(Renderable* InRenderable)
	{
		SE_ASSERT(IsOnGPUThread());

		InRenderable->_globalID = Make_GPU(GlobalRenderableID, downcasted_shared_from_this< RT_RenderScene>());
		auto thisID = InRenderable->_globalID->GetID();

		while (thisID >= _renderables.size())
		{
			_renderables.resize(_renderables.size() * 2);
		}

		_renderables[thisID] = InRenderable;

		_maxRenderableIdx = std::max(_maxRenderableIdx, thisID);

		_octree.AddElement(InRenderable);
	}

	void RT_RenderScene::RemoveRenderable(Renderable* InRenderable)
	{
		SE_ASSERT(IsOnGPUThread());

		auto thisID = InRenderable->_globalID->GetID();
		SE_ASSERT(_renderables[thisID] == InRenderable);
		_renderables[thisID] = nullptr;

		if (_maxRenderableIdx == thisID)
		{			
			for (int32_t Iter = _maxRenderableIdx - 1; Iter >= 0; Iter--)
			{
				_maxRenderableIdx = Iter;
				if (_renderables[Iter])
				{
					break;
				}
			}
		}
		
		_octree.RemoveElement(InRenderable);
	}

	void RT_RenderScene::PrepareScenesToDraw()
	{
		_viewGPU = _viewCPU;
	};

	void RT_RenderScene::BeginFrame() 
	{ 
		auto dwidth = GGI()->GetGraphicsDevice()->GetDeviceWidth();
		auto dheight = GGI()->GetGraphicsDevice()->GetDeviceHeight();

		if (!_offscreenUI || 
			_offscreenUI->GetWidth() != dwidth ||
			_offscreenUI->GetHeight() != dheight )
		{
			_offscreenUI = GGI()->GetGraphicsDevice()->_gxCreateTexture(dwidth, dheight, TextureFormat::BGRA_8888);
		}
	};
	void RT_RenderScene::Draw() { };
	void RT_RenderScene::EndFrame() { };
}