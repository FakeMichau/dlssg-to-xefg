// The nvngx_dlssg exports. XeFG has no Vulkan API, on Vulkan it runs on D3D12OnVk
#include "D3D12OnVk.h"
#include "FrameGenerator.h"
#include "Log.h"
#include "Util.h"

#include <dxgi.h>

#include <atomic>
#include <vector>

#define NGX_EXPORT extern "C" __declspec(dllexport)

// The handle given out is ours, the caller hands it back untouched
struct FeatureHandle : NGXHandle
{
	std::shared_ptr<FrameGenerator> generator;
};

static NGXResult Init()
{
	static const bool logged = []
		{
			Log::Info("dlssg-to-xefg, XeFG replacing DLSS Frame Generation. Free software under GPLv3, if you paid for "
				"it you've been scammed");
			return true;
		}();

	return XeFG::Load() ? NGX_SUCCESS : NGX_FAIL_FEATURE_NOT_SUPPORTED;
}

static NGXResult CreateFrameGenerator(ID3D12Device* device, NGXHandle** handle)
{
	static std::atomic_uint32_t nextId = 1;

	// The XeFG context is created on the first evaluation, once the resources are known
	*handle = new FeatureHandle{ { nextId++ }, std::make_shared<FrameGenerator>(device) };

	return NGX_SUCCESS;
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_Init(unsigned long long, const wchar_t*, ID3D12Device*, const void*, uint32_t)
{
	return Init();
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_Init_Ext(unsigned long long, const wchar_t*, ID3D12Device*, uint32_t,
	const void*)
{
	return Init();
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_Shutdown()
{
	DeferredDestroy::Collect(true);
	return NGX_SUCCESS;
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_Shutdown1(ID3D12Device*) { return NVSDK_NGX_D3D12_Shutdown(); }

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_GetScratchBufferSize(uint32_t, const NGXParameter*, size_t* size)
{
	if (!size)
		return NGX_FAIL_INVALID_PARAMETER;

	*size = 0;
	return NGX_SUCCESS;
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_GetFeatureRequirements(IDXGIAdapter*, const void*,
	NGXFeatureRequirement* requirement)
{
	if (!requirement)
		return NGX_FAIL_INVALID_PARAMETER;

	requirement->FeatureSupported = 0;
	requirement->MinHWArchitecture = 0;
	strcpy_s(requirement->MinOSVersion, "10.0.0");

	return NGX_SUCCESS;
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_CreateFeature(ID3D12GraphicsCommandList* cmdList, uint32_t feature,
	NGXParameter* params, NGXHandle** handle)
{
	if (!cmdList || !params || !handle)
		return NGX_FAIL_INVALID_PARAMETER;

	if (feature != NGX_FEATURE_FRAME_GENERATION || Init() != NGX_SUCCESS)
		return NGX_FAIL_FEATURE_NOT_SUPPORTED;

	ComPtr<ID3D12Device> device;
	if (FAILED(cmdList->GetDevice(IID_PPV_ARGS(&device))))
		return NGX_FAIL_PLATFORM_ERROR;

	return CreateFrameGenerator(device.Get(), handle);
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_ReleaseFeature(NGXHandle* handle)
{
	if (!handle)
		return NGX_FAIL_INVALID_PARAMETER;

	auto feature = static_cast<FeatureHandle*>(handle);
	DeferredDestroy::Add(std::move(feature->generator));
	delete feature;

	return NGX_SUCCESS;
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList* cmdList, const NGXHandle* handle,
	NGXParameter* params, void*)
{
	if (!cmdList || !handle || !params)
		return NGX_FAIL_INVALID_PARAMETER;

	DeferredDestroy::Collect();

	return static_cast<const FeatureHandle*>(handle)->generator->Evaluate(cmdList, *params);
}

static NGXResult GetCurrentSettingsCallback(NGXHandle* handle, NGXParameter* params)
{
	if (!handle || !params)
		return NGX_FAIL_INVALID_PARAMETER;

	params->Set("DLSSG.MustCallEval", 1);
	params->Set("DLSSG.BurstCaptureRunning", 0);

	return NGX_SUCCESS;
}

static NGXResult EstimateVRAMCallback(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, size_t* estimatedSize)
{
	// A guess, same as dlssg-to-fsr3
	if (estimatedSize)
		*estimatedSize = 300 * 1024 * 1024;

	return NGX_SUCCESS;
}

NGX_EXPORT NGXResult NVSDK_NGX_D3D12_PopulateParameters_Impl(NGXParameter* params)
{
	if (!params)
		return NGX_FAIL_INVALID_PARAMETER;

	params->Set("DLSSG.GetCurrentSettingsCallback", reinterpret_cast<void*>(&GetCurrentSettingsCallback));
	params->Set("DLSSG.EstimateVRAMCallback", reinterpret_cast<void*>(&EstimateVRAMCallback));
	params->Set("DLSSG.MultiFrameCountMax", (int)FrameGenerator::MaxFrames);

	return NGX_SUCCESS;
}

// Vulkan, the same exports as OptiScaler calls them
static D3D12OnVk::DeviceInfo vulkanInfo;
static ComPtr<ID3D12Device> vulkanDevice; // created by the first CreateFeature

// FrameGenerator's view of the Vulkan parameters: images become D3D12OnVk resources, alive until the evaluation returns
class VulkanParameters : public NGXParameter
{
public:
	VulkanParameters(NGXParameter& params, ID3D12Device* device) : params(params), device(device) {}

	void Set(const char* name, unsigned long long value) override { params.Set(name, value); }
	void Set(const char* name, float value) override { params.Set(name, value); }
	void Set(const char* name, double value) override { params.Set(name, value); }
	void Set(const char* name, unsigned int value) override { params.Set(name, value); }
	void Set(const char* name, int value) override { params.Set(name, value); }
	void Set(const char* name, ID3D11Resource* value) override { params.Set(name, value); }
	void Set(const char* name, ID3D12Resource* value) override { params.Set(name, value); }
	void Set(const char* name, void* value) override { params.Set(name, value); }

	NGXResult Get(const char* name, unsigned long long* value) const override { return params.Get(name, value); }
	NGXResult Get(const char* name, float* value) const override { return params.Get(name, value); }
	NGXResult Get(const char* name, double* value) const override { return params.Get(name, value); }
	NGXResult Get(const char* name, unsigned int* value) const override { return params.Get(name, value); }
	NGXResult Get(const char* name, int* value) const override { return params.Get(name, value); }
	NGXResult Get(const char* name, ID3D11Resource** value) const override { return params.Get(name, value); }
	NGXResult Get(const char* name, void** value) const override { return params.Get(name, value); }

	NGXResult Get(const char* name, ID3D12Resource** value) const override
	{
		void* pointer = nullptr;
		if (params.Get(name, &pointer) != NGX_SUCCESS || !pointer)
			return NGX_FAIL;

		auto resource = static_cast<const NGXResourceVK*>(pointer);
		if (resource->Type != NGX_RESOURCE_VK_TYPE_IMAGE_VIEW)
			return NGX_FAIL;

		auto& info = resource->Resource.ImageViewInfo;
		ComPtr<ID3D12Resource> wrapped;
		wrapped.Attach(D3D12OnVk::WrapImage(device, info.Image, info.Format, info.Width, info.Height, resource->ReadWrite));
		if (!wrapped)
			return NGX_FAIL;

		*value = wrapped.Get();
		resources.push_back(std::move(wrapped));
		return NGX_SUCCESS;
	}

	void Reset() override { params.Reset(); }

private:
	NGXParameter& params;
	ID3D12Device* device;
	mutable std::vector<ComPtr<ID3D12Resource>> resources;
};

static NGXResult VulkanInit(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
	PFN_vkGetInstanceProcAddr getInstanceProcAddr, PFN_vkGetDeviceProcAddr getDeviceProcAddr)
{
	if (!getInstanceProcAddr)
	{
		getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
			GetProcAddress(LoadLibraryW(L"vulkan-1.dll"), "vkGetInstanceProcAddr"));
	}

	vulkanInfo = { getInstanceProcAddr, getDeviceProcAddr, instance, physicalDevice, device };
	return Init();
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_Init(unsigned long long, const wchar_t*, VkInstance instance,
	VkPhysicalDevice physicalDevice, VkDevice device, PFN_vkGetInstanceProcAddr getInstanceProcAddr,
	PFN_vkGetDeviceProcAddr getDeviceProcAddr, const void*, uint32_t)
{
	return VulkanInit(instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_Init_Ext(unsigned long long, const wchar_t*, VkInstance instance,
	VkPhysicalDevice physicalDevice, VkDevice device, uint32_t, const void*)
{
	return VulkanInit(instance, physicalDevice, device, nullptr, nullptr);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_Init_Ext2(unsigned long long, const wchar_t*, VkInstance instance,
	VkPhysicalDevice physicalDevice, VkDevice device, PFN_vkGetInstanceProcAddr getInstanceProcAddr,
	PFN_vkGetDeviceProcAddr getDeviceProcAddr, uint32_t, const void*)
{
	return VulkanInit(instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_Shutdown()
{
	DeferredDestroy::Collect(true);

	if (vulkanDevice)
		D3D12OnVk::ReleaseHeld(vulkanDevice.Get());

	vulkanDevice.Reset();
	return NGX_SUCCESS;
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_Shutdown1(VkDevice) { return NVSDK_NGX_VULKAN_Shutdown(); }

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_GetScratchBufferSize(uint32_t feature, const NGXParameter* params, size_t* size)
{
	return NVSDK_NGX_D3D12_GetScratchBufferSize(feature, params, size);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_GetFeatureRequirements(VkInstance, VkPhysicalDevice, const void* info,
	NGXFeatureRequirement* requirement)
{
	return NVSDK_NGX_D3D12_GetFeatureRequirements(nullptr, info, requirement);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_CreateFeature1(VkDevice, VkCommandBuffer cmdBuffer, uint32_t feature,
	NGXParameter* params, NGXHandle** handle)
{
	if (!cmdBuffer || !params || !handle)
		return NGX_FAIL_INVALID_PARAMETER;

	if (feature != NGX_FEATURE_FRAME_GENERATION || Init() != NGX_SUCCESS || !vulkanInfo.device)
		return NGX_FAIL_FEATURE_NOT_SUPPORTED;

	if (!vulkanDevice)
		vulkanDevice.Attach(D3D12OnVk::CreateDevice(vulkanInfo));

	if (!vulkanDevice)
		return NGX_FAIL_PLATFORM_ERROR;

	return CreateFrameGenerator(vulkanDevice.Get(), handle);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_CreateFeature(VkCommandBuffer cmdBuffer, uint32_t feature, NGXParameter* params,
	NGXHandle** handle)
{
	return NVSDK_NGX_VULKAN_CreateFeature1(vulkanInfo.device, cmdBuffer, feature, params, handle);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_ReleaseFeature(NGXHandle* handle) { return NVSDK_NGX_D3D12_ReleaseFeature(handle); }

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_EvaluateFeature(VkCommandBuffer cmdBuffer, const NGXHandle* handle,
	NGXParameter* params, void*)
{
	if (!cmdBuffer || !handle || !params || !vulkanDevice)
		return NGX_FAIL_INVALID_PARAMETER;

	DeferredDestroy::Collect();

	ComPtr<ID3D12GraphicsCommandList> cmdList;
	cmdList.Attach(D3D12OnVk::WrapCommandBuffer(vulkanDevice.Get(), cmdBuffer));

	VulkanParameters vulkanParams(*params, vulkanDevice.Get());
	return static_cast<const FeatureHandle*>(handle)->generator->Evaluate(cmdList.Get(), vulkanParams);
}

NGX_EXPORT NGXResult NVSDK_NGX_VULKAN_PopulateParameters_Impl(NGXParameter* params)
{
	return NVSDK_NGX_D3D12_PopulateParameters_Impl(params);
}
