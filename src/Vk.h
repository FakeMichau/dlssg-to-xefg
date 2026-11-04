// The Vulkan functions we call, loaded through the application's vkGetInstanceProcAddr instead of linking
// vulkan-1.lib, the application might not use the system's loader
#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#define VK_INSTANCE_FUNCTIONS(X) \
	X(vkGetPhysicalDeviceProperties2) \
	X(vkGetPhysicalDeviceFeatures2) \
	X(vkGetPhysicalDeviceMemoryProperties)

#define VK_DEVICE_FUNCTIONS(X) \
	X(vkAllocateMemory) \
	X(vkFreeMemory) \
	X(vkMapMemory) \
	X(vkCreateBuffer) \
	X(vkDestroyBuffer) \
	X(vkCreateImage) \
	X(vkDestroyImage) \
	X(vkBindBufferMemory) \
	X(vkBindImageMemory) \
	X(vkGetBufferMemoryRequirements) \
	X(vkGetImageMemoryRequirements) \
	X(vkGetDeviceBufferMemoryRequirements) \
	X(vkGetDeviceImageMemoryRequirements) \
	X(vkCreateImageView) \
	X(vkDestroyImageView) \
	X(vkCreateBufferView) \
	X(vkDestroyBufferView) \
	X(vkCreateSampler) \
	X(vkDestroySampler) \
	X(vkCreateDescriptorSetLayout) \
	X(vkDestroyDescriptorSetLayout) \
	X(vkCreatePipelineLayout) \
	X(vkDestroyPipelineLayout) \
	X(vkCreateShaderModule) \
	X(vkDestroyShaderModule) \
	X(vkCreatePipelineCache) \
	X(vkDestroyPipelineCache) \
	X(vkCreateComputePipelines) \
	X(vkDestroyPipeline) \
	X(vkCreateDescriptorPool) \
	X(vkDestroyDescriptorPool) \
	X(vkResetDescriptorPool) \
	X(vkAllocateDescriptorSets) \
	X(vkUpdateDescriptorSets) \
	X(vkCmdBindPipeline) \
	X(vkCmdBindDescriptorSets) \
	X(vkCmdPushConstants) \
	X(vkCmdDispatch) \
	X(vkCmdPipelineBarrier) \
	X(vkCmdCopyBuffer) \
	X(vkCmdCopyImage) \
	X(vkCmdCopyBufferToImage) \
	X(vkCmdCopyImageToBuffer) \
	X(vkCmdClearColorImage) \
	X(vkCmdFillBuffer)

struct Vk
{
#define VK_MEMBER(name) PFN_##name name = nullptr;
	VK_INSTANCE_FUNCTIONS(VK_MEMBER)
	VK_DEVICE_FUNCTIONS(VK_MEMBER)
#undef VK_MEMBER

	void Load(PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkInstance instance, VkDevice device,
		PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr)
	{
#define VK_LOAD_INSTANCE(name) name = reinterpret_cast<PFN_##name>(getInstanceProcAddr(instance, #name));
		VK_INSTANCE_FUNCTIONS(VK_LOAD_INSTANCE)
#undef VK_LOAD_INSTANCE

		if (!getDeviceProcAddr)
			getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(getInstanceProcAddr(instance, "vkGetDeviceProcAddr"));

#define VK_LOAD_DEVICE(name) name = reinterpret_cast<PFN_##name>(getDeviceProcAddr(device, #name));
		VK_DEVICE_FUNCTIONS(VK_LOAD_DEVICE)
#undef VK_LOAD_DEVICE
	}
};
