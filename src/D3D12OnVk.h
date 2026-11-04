// A D3D12 device on top of a Vulkan device, with just enough of D3D12 for XeFG and FrameGenerator: compute work,
// copies and the resources they use. Compute shaders are translated from DXIL to SPIR-V with dxil-spirv, linked in
// statically.
//
// Our own images always stay in VK_IMAGE_LAYOUT_GENERAL and every barrier is a full memory barrier. Wrapped images
// take the layouts matching the D3D12 states their barriers name.
//
// Nothing is submitted to the game's queues: every command list records into the wrapped command buffer of the
// evaluation in progress, queues and fences complete right away.
#pragma once

#include "Vk.h"

#include <d3d12.h>

namespace D3D12OnVk
{
	struct DeviceInfo
	{
		PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
		PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr; // optional
		VkInstance instance = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
	};

	// nullptr on failure, the log says why
	ID3D12Device* CreateDevice(const DeviceInfo& info);

	// A command list recording into commandBuffer, which must be in the recording state. It can't be closed or reset.
	// Command lists the device creates record into it too while it's alive
	ID3D12GraphicsCommandList* WrapCommandBuffer(ID3D12Device* device, VkCommandBuffer commandBuffer);

	bool IsDevice(ID3D12Device* device);

	// Releases what the device kept alive for command buffers the GPU may still run, once it's idle
	void ReleaseHeld(ID3D12Device* device);

	// A 2D image the caller owns. Its layout is whatever the D3D12 state of the first barrier on it says
	ID3D12Resource* WrapImage(ID3D12Device* device, VkImage image, VkFormat format, uint32_t width, uint32_t height,
		bool unorderedAccess);
} // namespace D3D12OnVk
