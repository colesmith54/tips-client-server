// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once


#include "vulkan/vulkan.h"

#include "SPPVulkan.h"
#include "SPPSceneRendering.h"

namespace SPP
{
	class VulkanRenderScene : public RenderScene
	{
	protected:
		//D3D12PartialResourceMemory _currentFrameMem;
		////std::vector< D3D12RenderableMesh* > _renderMeshes;
		//bool _bMeshInstancesDirty = false;

		GPUReferencer< GPUShader > _debugVS;
		GPUReferencer< GPUShader > _debugPS;

		GPUReferencer< PipelineState > _debugPSO;
		GPUReferencer< GPUInputLayout > _debugLayout;

		std::shared_ptr < ArrayResource >  _debugResource;
		GPUReferencer< GPUBuffer > _debugBuffer;
		std::vector< DebugVertex > _lines;

		GPUReferencer< GPUShader > _fullscreenRayVS;
		GPUReferencer< GPUShader > _fullscreenRaySDFPS, _fullscreenRaySkyBoxPS;

		GPUReferencer< PipelineState > _fullscreenRaySDFPSO, _fullscreenSkyBoxPSO;
		GPUReferencer< GPUInputLayout > _fullscreenRayVSLayout;

		
		std::shared_ptr< ArrayResource > _cameraData;
		GPUReferencer< class VulkanBuffer > _cameraBuffer;

		std::shared_ptr< ArrayResource > _drawConstants;
		GPUReferencer< class VulkanBuffer > _drawConstantsBuffer;

		std::shared_ptr< ArrayResource > _drawParams;
		GPUReferencer< class VulkanBuffer > _drawParamsBuffer;

		std::shared_ptr< ArrayResource > _shapes;
		GPUReferencer< class VulkanBuffer > _shapesBuffer;

		VkDescriptorSetLayout _perFrameSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _perDrawSetLayout = VK_NULL_HANDLE;

		VkDescriptorSet _perFrameDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet _perDrawDescriptorSet = VK_NULL_HANDLE;

		// Descriptor set pool
		VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;


	public:
		VulkanRenderScene();

		void DrawSkyBox();

		GPUReferencer< GPUShader > GetSDFVS()
		{
			return _fullscreenRayVS;
		}
		GPUReferencer< GPUShader > GetSDFPS()
		{
			return _fullscreenRaySDFPS;
		}
		GPUReferencer< PipelineState > GetSDFPSO()
		{
			return _fullscreenRaySDFPSO;
		}
		GPUReferencer< GPUInputLayout > GetRayVSLayout()
		{
			return _fullscreenRayVSLayout;
		}

		virtual void AddedToGraphicsDevice() override;

		//void DrawDebug();

		//virtual void Build() {};

		//D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddrOfViewConstants()
		//{
		//	return _currentFrameMem.gpuAddr;
		//}

		virtual void AddToScene(Renderable* InRenderable) override;
		virtual void RemoveFromScene(Renderable* InRenderable) override;
		
		virtual void BeginFrame() override {} 
		virtual void Draw() override;		
		virtual void EndFrame() override {}
	};
}