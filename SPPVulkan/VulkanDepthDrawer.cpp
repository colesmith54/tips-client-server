// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanDevice.h"
#include "VulkanShaders.h"
#include "VulkanTexture.h"
#include "VulkanRenderableMesh.h"
#include "VulkanFrameBuffer.hpp"

#include "VulkanDepthDrawer.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

namespace SPP
{
	extern LogEntry LOG_VULKAN;

	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	const std::vector<VertexStream>& Depth_GetVertexStreams_SM()
	{
		static std::vector<VertexStream> vertexStreams;
		if (vertexStreams.empty())
		{
			MeshVertex dummy;
			vertexStreams.push_back(
				CreateVertexStream(dummy,
					dummy.position));
		}
		return vertexStreams;
	}

	class GlobalDepthDrawerResources : public GlobalGraphicsResource
	{
		GLOBAL_RESOURCE(GlobalDepthDrawerResources)

	private:
		GPUReferencer < VulkanShader > _depthVS;
		GPUReferencer < VulkanShader > _depthPyramidCreationCS, _depthCullingCS;
		GPUReferencer< SafeVkDescriptorSetLayout > _depthPyramidCreationLayout, _depthCullingCSLayout;

		GPUReferencer< VulkanPipelineState > _depthPyramidPSO, _depthCullingPSO;
		GPUReferencer< SafeVkSampler > _depthPyramidSampler;
		GPUReferencer< GPUInputLayout > _SMDepthlayout;
		
		GPUReferencer< VulkanPipelineState > _SMDepthPSO;
		GPUReferencer< VulkanPipelineState > _SMDepthPSOInvertedZ;

	public:
		// called on render thread
		GlobalDepthDrawerResources()
		{
			auto owningDevice = GGlobalVulkanGI;

			// DEPTH CULLING
			_depthCullingCS = Make_GPU(VulkanShader, EShaderType::Compute);
			_depthCullingCS->CompileShaderFromFile("shaders/Depth/DepthPyramidCull.glsl", "main");

			{
				auto& layoutSet = _depthCullingCS->GetLayoutSets();
				_depthCullingCSLayout = Make_GPU(SafeVkDescriptorSetLayout, layoutSet.front().bindings);
			}

			_depthCullingPSO = VulkanPipelineStateBuilder().Set(_depthCullingCS).Build();

			// DEPTH PYRAMID
			_depthPyramidCreationCS = Make_GPU(VulkanShader, EShaderType::Compute);
			_depthPyramidCreationCS->CompileShaderFromFile("shaders/Depth/DepthPyramidCompute.hlsl", "main_cs");
			_depthPyramidPSO = VulkanPipelineStateBuilder().Set(_depthPyramidCreationCS).Build();

			auto& depthPyrCreationCS = _depthPyramidCreationCS->GetLayoutSets();
			_depthPyramidCreationLayout = Make_GPU(SafeVkDescriptorSetLayout, depthPyrCreationCS.front().bindings);

			/// special min mod depth sampler
			VkSamplerCreateInfo createInfo = {};

			//fill the normal stuff
			createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			createInfo.magFilter = VK_FILTER_LINEAR;
			createInfo.minFilter = VK_FILTER_LINEAR;
			createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.minLod = 0;
			createInfo.maxLod = 16.f;
			//createInfo.unnormalizedCoordinates = true;

			//add a extension struct to enable Min mode
			VkSamplerReductionModeCreateInfoEXT createInfoReduction = {};

			createInfoReduction.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT;
			createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;
			createInfo.pNext = &createInfoReduction;

			_depthPyramidSampler = Make_GPU(SafeVkSampler, createInfo);

			//DEPTH VS
			_depthVS = Make_GPU(VulkanShader, EShaderType::Vertex);
			_depthVS->CompileShaderFromFile("shaders/Depth/DepthDrawVS.glsl");

			_SMDepthlayout = Make_GPU(VulkanInputLayout);
			_SMDepthlayout->InitializeLayout(Depth_GetVertexStreams_SM());

			_SMDepthPSOInvertedZ = VulkanPipelineStateBuilder()
				.Set(owningDevice->GetDepthOnlyFrameData())
				.Set(EBlendState::Disabled)
				.Set(ERasterizerState::BackFaceCull)
				.Set(EDepthState::Enabled)
				.Set(EDrawingTopology::TriangleList)
				.Set(EDepthOp::GreaterOrEqual)
				.Set(_SMDepthlayout)
				.Set(_depthVS)
				.Build();

			_SMDepthPSO = VulkanPipelineStateBuilder()
				.Set(owningDevice->GetDepthOnlyFrameData())
				.Set(EBlendState::Disabled)
				.Set(ERasterizerState::BackFaceCull)
				.Set(EDepthState::Enabled)
				.Set(EDrawingTopology::TriangleList)
				.Set(EDepthOp::LessOrEqual)
				.Set(_SMDepthlayout)
				.Set(_depthVS)
				.Build(); 
		}

		auto GetInvertedDepthDrawingPSO()
		{
			return _SMDepthPSOInvertedZ;
		}

		auto GetDepthDrawingPSO()
		{
			return _SMDepthPSO;
		}

		auto GetDepthCSPyramidLayout()
		{
			return _depthPyramidCreationLayout;
		}

		auto GetDepthPyramidPSO()
		{
			return _depthPyramidPSO;
		}

		auto GetDepthCullingCS()
		{
			return _depthCullingCS;
		}
		auto GetDepthCullingPSO()
		{
			return _depthCullingPSO;
		}
		auto GepthCullingCSLayout()
		{
			return _depthCullingCSLayout;
		}


		GPUReferencer< GPUInputLayout > GetSMLayout()
		{
			return _SMDepthlayout;
		}

		GPUReferencer < VulkanShader > GetDepthVS()
		{
			return _depthVS;
		}

		GPUReferencer< SafeVkSampler > GetDepthSampler()
		{
			return _depthPyramidSampler;
		}
	};

	REGISTER_GLOBAL_RESOURCE(GlobalDepthDrawerResources);

	struct alignas(16u) DepthReduceData
	{
		Vector2 srcSize;
		Vector2 dstSize;
	};

	DepthDrawer::DepthDrawer(VulkanRenderScene* InScene) : _owningScene(InScene)
	{
		auto _owningDevice = GGlobalVulkanGI;
		auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();
		
		///////////////////////////////////
		// SETUP Depth pyramid texture
		///////////////////////////////////

		_depthPyramidDescriptorSet = Make_GPU(SafeVkDescriptorSet, 
			_owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthCSPyramidLayout()->Get(), 
			globalSharedPool);

		auto DeviceExtents = _owningDevice->GetExtents();

		_depthPyramidExtents = Vector2i(DeviceExtents[0] >> 1, DeviceExtents[1] >> 1);
		_depthPyramidMips = std::max(powerOf2(_depthPyramidExtents[0]), powerOf2(_depthPyramidExtents[1]));

		_depthPyramidTexture = Make_GPU(VulkanTexture, _depthPyramidExtents[0], _depthPyramidExtents[1], _depthPyramidMips, 1, TextureFormat::R32F,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		_depthPyramidTexture->SetName("_depthPyramidTexture");

		_depthPyramidViews = _depthPyramidTexture->GetMipChainViews();

		auto ColorTarget = _owningDevice->GetColorTarget();
		auto& depthAttachment = ColorTarget->GetBackAttachment();
		auto depthTexture = depthAttachment.texture.get();
		_depthPyramidDescriptors.clear();

		// create those descriptors of the mip chain
		auto depthDownsizeSampler = _owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthSampler();
		for (int32_t Iter = 0; Iter < _depthPyramidViews.size(); Iter++)
		{
			VkDescriptorImageInfo sourceTarget;
			sourceTarget.sampler = depthDownsizeSampler->Get();

			VkDescriptorImageInfo destTarget;
			destTarget.sampler = depthDownsizeSampler->Get();
			destTarget.imageView = _depthPyramidViews[Iter]->Get();
			destTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			//for te first iteration, we grab it from the depth image
			if (Iter == 0)
			{
				sourceTarget.imageView = depthTexture->GetVkImageView();
				sourceTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			//afterwards, we copy from a depth mipmap into the next
			else
			{
				sourceTarget.imageView = _depthPyramidViews[Iter - 1]->Get();
				sourceTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			}

			auto descChainSet = Make_GPU(SafeVkDescriptorSet,
				_owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthCSPyramidLayout()->Get(),
				globalSharedPool);

			{
				std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(descChainSet->Get(),
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0, &sourceTarget),
				vks::initializers::writeDescriptorSet(descChainSet->Get(),
					VK_DESCRIPTOR_TYPE_SAMPLER, 1, &sourceTarget),
				vks::initializers::writeDescriptorSet(descChainSet->Get(),
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &destTarget),
				};

				vkUpdateDescriptorSets(_owningDevice->GetDevice(),
					static_cast<uint32_t>(writeDescriptorSets.size()),
					writeDescriptorSets.data(), 0, nullptr);
			}

			_depthPyramidDescriptors.push_back(descChainSet);
		}

		//DEPTH CULLING AGAINST PYRAMID
		_depthCullingDescriptorSet = Make_GPU(SafeVkDescriptorSet, 
			_owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GepthCullingCSLayout()->Get(),
			globalSharedPool);

		VkDescriptorBufferInfo drawSphereInfo = _owningScene->GetCullDataBuffer()->GetDescriptorInfo();
		VkDescriptorBufferInfo drawVisibilityInfo = _owningScene->GetVisibleGPUBuffer()->GetDescriptorInfo();


		auto cameraBuffer = _owningScene->GetCameraBuffer();
		VkDescriptorBufferInfo perFrameInfo;
		perFrameInfo.buffer = cameraBuffer->GetBuffer();
		perFrameInfo.offset = 0;
		perFrameInfo.range = cameraBuffer->GetPerElementSize();

		VkDescriptorImageInfo depthPyramidInfo;
		depthPyramidInfo.sampler = depthDownsizeSampler->Get();
		depthPyramidInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		depthPyramidInfo.imageView = _depthPyramidTexture->GetVkImageView();

		{
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
				vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, &drawSphereInfo),
				vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2, &drawVisibilityInfo),
				vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &depthPyramidInfo),
			};

			vkUpdateDescriptorSets(_owningDevice->GetDevice(),
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void DepthDrawer::ProcessDepthPyramid()
	{
		auto _owningDevice = GGlobalVulkanGI;
		auto DeviceExtents = _owningDevice->GetExtents();

		auto depthDownsizeSampler = _owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthSampler();
		auto depthPyrPSO = _owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthPyramidPSO();

		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

		auto ColorTarget = _owningDevice->GetColorTarget();
		auto& depthAttachment = ColorTarget->GetBackAttachment();
		auto depthTexture = depthAttachment.texture.get();
		
		vks::tools::setImageLayout(commandBuffer, depthTexture->GetVkImage(),
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			{ depthTexture->GetImageAspect(), 0, 1, 0, 1 });

		DepthReduceData reduceData =
		{
			Vector2(DeviceExtents[0], DeviceExtents[1]),
			Vector2(DeviceExtents[0], DeviceExtents[1])
		};

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthPyrPSO->GetVkPipeline());

		for (int32_t Iter = 0; Iter < _depthPyramidViews.size(); Iter++)
		{
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthPyrPSO->GetVkPipelineLayout(), 0, 1, &_depthPyramidDescriptors[Iter]->Get(), 0, nullptr);

			uint32_t levelWidth = std::max(1, _depthPyramidExtents[0] >> Iter);
			uint32_t levelHeight = std::max(1, _depthPyramidExtents[1] >> Iter);

			// put dst into src
			std::swap(reduceData.dstSize, reduceData.srcSize);
			// shift dst
			reduceData.dstSize = { Vector2(levelWidth, levelHeight) };

			//execute downsample compute shader
			vkCmdPushConstants(commandBuffer, depthPyrPSO->GetVkPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduceData), &reduceData);
			auto xCount = getGroupCount(levelWidth, 32);
			auto yCount = getGroupCount(levelHeight, 32);
			vkCmdDispatch(commandBuffer, xCount, yCount, 1);

			vks::tools::insertImageMemoryBarrier(commandBuffer,
				_depthPyramidTexture->GetVkImage(),

				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,

				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_GENERAL,

				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)Iter, 1, 0, 1 });
		}

		vks::tools::setImageLayout(commandBuffer, depthTexture->GetVkImage(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			{ depthTexture->GetImageAspect(), 0, 1, 0, 1 });
	}

	void DepthDrawer::RunDepthCullingAgainstPyramid()
	{
		auto _owningDevice = GGlobalVulkanGI;
		auto DeviceExtents = _owningDevice->GetExtents();
		auto depthDownsizeSampler = _owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthSampler();

		auto depthCullingPSO = _owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthCullingPSO();

		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();


		GPUDrawCullData cullData;
		reinterpret_cast<CameraCullInfo&>(cullData) = _owningScene->GetCameraCullData();

		cullData.pyramidWidth = _depthPyramidExtents[0];
		cullData.pyramidHeight = _depthPyramidExtents[1];

		cullData.drawCount = _owningScene->GetMaxRenderableIdx();

		cullData.cullingEnabled = 1;
		cullData.occlusionEnabled = 1;

		uint32_t uniform_offsets[] = {
				0,
				0,
				0
		};

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthCullingPSO->GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			depthCullingPSO->GetVkPipelineLayout(),
			0, 1,
			&_depthCullingDescriptorSet->Get(), ARRAY_SIZE(uniform_offsets), uniform_offsets);
		//execute downsample compute shader
		vkCmdPushConstants(commandBuffer,
			depthCullingPSO->GetVkPipelineLayout(),
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(cullData), &cullData);

		auto xCount = getGroupCount(cullData.drawCount, 64);
		vkCmdDispatch(commandBuffer, xCount, 1, 1);

		// copy over
		auto vksDevice = _owningDevice->GetVKSVulkanDevice();
		auto gQueue = _owningDevice->GetGraphicsQueue(); //should be transfer
		VkCommandBuffer copyCmd = vksDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		_owningScene->GetVisibleGPUBuffer()->CopyTo(copyCmd, *_owningScene->GetVisibleCPUBuffer().get());

		vksDevice->flushCommandBuffer(copyCmd, gQueue);

		auto& depthCull = _owningScene->GetDepthCullVisiblity();

		uint32_t* MappedCPUMem = (uint32_t*)_owningScene->GetVisibleCPUBuffer()->GetMappedMemory();
		for (uint32_t Iter = 0; Iter < cullData.drawCount; Iter++)
		{
			uint32_t byteIndex = (Iter >> 5);
			uint32_t bitIndex = 1 << (Iter - (byteIndex << 5));

			if (MappedCPUMem[byteIndex] & bitIndex)
			{
				depthCull.Set(Iter, true);
			}
			else
			{
				depthCull.Set(Iter, false);
			}
		}
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="InVulkanRenderableMesh"></param>
	void DepthDrawer::Render(RT_VulkanRenderableMesh& InVulkanRenderableMesh, bool InvertedZ)
	{
		auto _owningDevice = GGlobalVulkanGI;
		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

		auto vulkanMat = static_pointer_cast<RT_Vulkan_Material>(InVulkanRenderableMesh.GetMaterial());
		auto meshPSO = InvertedZ ? _owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetInvertedDepthDrawingPSO() :
			_owningDevice->GetGlobalResource< GlobalDepthDrawerResources >()->GetDepthDrawingPSO();
		auto meshCache = GetMeshCache(InVulkanRenderableMesh);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshCache->vertexBuffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, meshCache->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPSO->GetVkPipeline());

		// if static we have everything pre cached
		if (InVulkanRenderableMesh.IsStatic())
		{
			uint32_t uniform_offsets[] = {
				0,
				(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
			};

			VkDescriptorSet locaDrawSets[] = {
				_owningScene->GetCommondDescriptorSet(),
				_owningScene->GetStaticTransformsDescriptorSet()->Get()
			};

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				meshPSO->GetVkPipelineLayout(),
				0,
				ARRAY_SIZE(locaDrawSets), locaDrawSets,
				ARRAY_SIZE(uniform_offsets), uniform_offsets);
		}
		// if not static we need to write transforms
		else
		{
			//check this
			SE_ASSERT(false);
			//auto CurPool = _owningDevice->GetPerFrameResetDescriptorPool();
			//auto vsLayout = GetOpaqueVSLayout();

			//VkDescriptorSet dynamicTransformSet;
			//VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, &vsLayout->Get(), 1);
			//VK_CHECK_RESULT(vkAllocateDescriptorSets(_owningDevice->GetDevice(), &allocInfo, &dynamicTransformSet));

			//auto cameraBuffer = _owningScene->GetCameraBuffer();

			//VkDescriptorBufferInfo perFrameInfo;
			//perFrameInfo.buffer = cameraBuffer->GetBuffer();
			//perFrameInfo.offset = 0;
			//perFrameInfo.range = cameraBuffer->GetPerElementSize();

			//std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			//vks::initializers::writeDescriptorSet(dynamicTransformSet,
			//	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
			//vks::initializers::writeDescriptorSet(dynamicTransformSet,
			//	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &meshCache->transformBufferInfo),
			//};

			//vkUpdateDescriptorSets(_owningDevice->GetDevice(),
			//	static_cast<uint32_t>(writeDescriptorSets.size()),
			//	writeDescriptorSets.data(), 0, nullptr);

			//uint32_t uniform_offsets[] = {
			//	(sizeof(GPUViewConstants)) * currentFrame,
			//	(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
			//};

			//VkDescriptorSet locaDrawSets[] = {
			//	_camStaticBufferDescriptorSet->Get(),
			//	dynamicTransformSet
			//};

			//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			//	meshPSO->GetVkPipelineLayout(),
			//	0,
			//	ARRAY_SIZE(locaDrawSets), locaDrawSets,
			//	ARRAY_SIZE(uniform_offsets), uniform_offsets);
		}

		vkCmdDrawIndexed(commandBuffer, meshCache->indexedCount, 1, 0, 0, 0);
	}
}