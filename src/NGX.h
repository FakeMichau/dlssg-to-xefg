// The parts of the NGX ABI an nvngx_dlssg replacement has to speak. Declared from scratch (like dlssg-to-fsr3 does)
// because NVIDIA's headers aren't GPL compatible.
#pragma once

#include <cstdint>

struct ID3D11Resource;
struct ID3D12Resource;

using NGXResult = uint32_t;

constexpr NGXResult NGX_SUCCESS = 0x1;
constexpr NGXResult NGX_FAIL = 0xBAD00000;
constexpr NGXResult NGX_FAIL_FEATURE_NOT_SUPPORTED = NGX_FAIL | 1;
constexpr NGXResult NGX_FAIL_PLATFORM_ERROR = NGX_FAIL | 2;
constexpr NGXResult NGX_FAIL_FEATURE_NOT_FOUND = NGX_FAIL | 4;
constexpr NGXResult NGX_FAIL_INVALID_PARAMETER = NGX_FAIL | 5;

constexpr uint32_t NGX_FEATURE_FRAME_GENERATION = 11;

struct NGXHandle
{
	uint32_t Id;
};

struct NGXFeatureRequirement
{
	uint32_t FeatureSupported; // 0 = supported
	uint32_t MinHWArchitecture;
	char MinOSVersion[255];
};

// NVSDK_NGX_Parameter. MSVC orders the vtable of overloaded virtuals by their declaration order, so this has to be
// declared exactly like NVIDIA's for the slots to line up.
struct NGXParameter
{
	virtual void Set(const char* name, unsigned long long value) = 0;
	virtual void Set(const char* name, float value) = 0;
	virtual void Set(const char* name, double value) = 0;
	virtual void Set(const char* name, unsigned int value) = 0;
	virtual void Set(const char* name, int value) = 0;
	virtual void Set(const char* name, ID3D11Resource* value) = 0;
	virtual void Set(const char* name, ID3D12Resource* value) = 0;
	virtual void Set(const char* name, void* value) = 0;

	virtual NGXResult Get(const char* name, unsigned long long* value) const = 0;
	virtual NGXResult Get(const char* name, float* value) const = 0;
	virtual NGXResult Get(const char* name, double* value) const = 0;
	virtual NGXResult Get(const char* name, unsigned int* value) const = 0;
	virtual NGXResult Get(const char* name, int* value) const = 0;
	virtual NGXResult Get(const char* name, ID3D11Resource** value) const = 0;
	virtual NGXResult Get(const char* name, ID3D12Resource** value) const = 0;
	virtual NGXResult Get(const char* name, void** value) const = 0;

	virtual void Reset() = 0;

	// value, or fallback when the parameter isn't set
	template <class T> T GetOr(const char* name, T fallback) const
	{
		T value = fallback;
		return Get(name, &value) == NGX_SUCCESS ? value : fallback;
	}
};
