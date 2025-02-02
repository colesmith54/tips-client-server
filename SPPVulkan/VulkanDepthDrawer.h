// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"

namespace SPP
{
	class DepthDrawer
	{
	protected:
		GPUReferencer< SafeVkDescriptorSet > _depthPyramidDescriptorSet, _depthCullingDescriptorSet;

		VulkanRenderScene* _owningScene = nullptr;

		GPUReferencer< VulkanTexture > _depthPyramidTexture;
		uint8_t _depthPyramidMips = 1;
		Vector2i _depthPyramidExtents;

		std::vector< GPUReferencer< SafeVkImageView > > _depthPyramidViews;
		std::vector< GPUReferencer< SafeVkDescriptorSet > > _depthPyramidDescriptors;

	public:
		DepthDrawer(VulkanRenderScene* InScene);
				
		void ProcessDepthPyramid();
		void RunDepthCullingAgainstPyramid();
		void Render(RT_VulkanRenderableMesh& InVulkanRenderableMesh, bool InvertedZ = true);
	};
}