// The method names of the D3D12 vtables, generated from the Windows SDK 10.0.26100 d3d12.h. Each family lists its
// newest interface's methods in slot order.
#pragma once

namespace D3D12Vtables
{
	constexpr const char* DeviceMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetNodeCount", "CreateCommandQueue", "CreateCommandAllocator", "CreateGraphicsPipelineState",
		"CreateComputePipelineState", "CreateCommandList", "CheckFeatureSupport", "CreateDescriptorHeap",
		"GetDescriptorHandleIncrementSize", "CreateRootSignature", "CreateConstantBufferView", "CreateShaderResourceView",
		"CreateUnorderedAccessView", "CreateRenderTargetView", "CreateDepthStencilView", "CreateSampler",
		"CopyDescriptors", "CopyDescriptorsSimple", "GetResourceAllocationInfo", "GetCustomHeapProperties",
		"CreateCommittedResource", "CreateHeap", "CreatePlacedResource", "CreateReservedResource", "CreateSharedHandle",
		"OpenSharedHandle", "OpenSharedHandleByName", "MakeResident", "Evict", "CreateFence", "GetDeviceRemovedReason",
		"GetCopyableFootprints", "CreateQueryHeap", "SetStablePowerState", "CreateCommandSignature", "GetResourceTiling",
		"GetAdapterLuid", "CreatePipelineLibrary", "SetEventOnMultipleFenceCompletion", "SetResidencyPriority",
		"CreatePipelineState", "OpenExistingHeapFromAddress", "OpenExistingHeapFromFileMapping", "EnqueueMakeResident",
		"CreateCommandList1", "CreateProtectedResourceSession", "CreateCommittedResource1", "CreateHeap1",
		"CreateReservedResource1", "GetResourceAllocationInfo1", "CreateLifetimeTracker", "RemoveDevice",
		"EnumerateMetaCommands", "EnumerateMetaCommandParameters", "CreateMetaCommand", "CreateStateObject",
		"GetRaytracingAccelerationStructurePrebuildInfo", "CheckDriverMatchingIdentifier", "SetBackgroundProcessingMode",
		"AddToStateObject", "CreateProtectedResourceSession1", "GetResourceAllocationInfo2", "CreateCommittedResource2",
		"CreatePlacedResource1", "CreateSamplerFeedbackUnorderedAccessView", "GetCopyableFootprints1",
		"CreateShaderCacheSession", "ShaderCacheControl", "CreateCommandQueue1", "CreateCommittedResource3",
		"CreatePlacedResource2", "CreateReservedResource2", "CreateSampler2", "GetResourceAllocationInfo3",
		"OpenExistingHeapFromAddress1", "CreateRootSignatureFromSubobjectInLibrary"
	};

	constexpr const char* GraphicsCommandListMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "GetType", "Close", "Reset", "ClearState", "DrawInstanced", "DrawIndexedInstanced", "Dispatch",
		"CopyBufferRegion", "CopyTextureRegion", "CopyResource", "CopyTiles", "ResolveSubresource",
		"IASetPrimitiveTopology", "RSSetViewports", "RSSetScissorRects", "OMSetBlendFactor", "OMSetStencilRef",
		"SetPipelineState", "ResourceBarrier", "ExecuteBundle", "SetDescriptorHeaps", "SetComputeRootSignature",
		"SetGraphicsRootSignature", "SetComputeRootDescriptorTable", "SetGraphicsRootDescriptorTable",
		"SetComputeRoot32BitConstant", "SetGraphicsRoot32BitConstant", "SetComputeRoot32BitConstants",
		"SetGraphicsRoot32BitConstants", "SetComputeRootConstantBufferView", "SetGraphicsRootConstantBufferView",
		"SetComputeRootShaderResourceView", "SetGraphicsRootShaderResourceView", "SetComputeRootUnorderedAccessView",
		"SetGraphicsRootUnorderedAccessView", "IASetIndexBuffer", "IASetVertexBuffers", "SOSetTargets",
		"OMSetRenderTargets", "ClearDepthStencilView", "ClearRenderTargetView", "ClearUnorderedAccessViewUint",
		"ClearUnorderedAccessViewFloat", "DiscardResource", "BeginQuery", "EndQuery", "ResolveQueryData",
		"SetPredication", "SetMarker", "BeginEvent", "EndEvent", "ExecuteIndirect", "AtomicCopyBufferUINT",
		"AtomicCopyBufferUINT64", "OMSetDepthBounds", "SetSamplePositions", "ResolveSubresourceRegion",
		"SetViewInstanceMask", "WriteBufferImmediate", "SetProtectedResourceSession", "BeginRenderPass", "EndRenderPass",
		"InitializeMetaCommand", "ExecuteMetaCommand", "BuildRaytracingAccelerationStructure",
		"EmitRaytracingAccelerationStructurePostbuildInfo", "CopyRaytracingAccelerationStructure", "SetPipelineState1",
		"DispatchRays", "RSSetShadingRate", "RSSetShadingRateImage", "DispatchMesh", "Barrier",
		"OMSetFrontAndBackStencilRef", "RSSetDepthBias", "IASetIndexBufferStripCutValue", "SetProgram", "DispatchGraph"
	};

	constexpr const char* CommandQueueMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "UpdateTileMappings", "CopyTileMappings", "ExecuteCommandLists", "SetMarker", "BeginEvent",
		"EndEvent", "Signal", "Wait", "GetTimestampFrequency", "GetClockCalibration", "GetDesc"
	};

	constexpr const char* CommandAllocatorMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "Reset"
	};

	constexpr const char* ResourceMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "Map", "Unmap", "GetDesc", "GetGPUVirtualAddress", "WriteToSubresource", "ReadFromSubresource",
		"GetHeapProperties", "GetProtectedResourceSession", "GetDesc1"
	};

	constexpr const char* HeapMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "GetDesc", "GetProtectedResourceSession"
	};

	constexpr const char* DescriptorHeapMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "GetDesc", "GetCPUDescriptorHandleForHeapStart", "GetGPUDescriptorHandleForHeapStart"
	};

	constexpr const char* FenceMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "GetCompletedValue", "SetEventOnCompletion", "Signal", "GetCreationFlags"
	};

	constexpr const char* RootSignatureMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice"
	};

	constexpr const char* PipelineStateMethods[] = {
		"QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName",
		"GetDevice", "GetCachedBlob"
	};
} // namespace D3D12Vtables
