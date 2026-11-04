#include "D3D12OnVk.h"
#include "D3D12Vtables.h"
#include "Log.h"

#include <dxil_spirv_c.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <span>
#include <vector>

namespace
{
	// Formats
	struct Format
	{
		DXGI_FORMAT dxgi;
		VkFormat vk;
		uint32_t bytes;
	};

	// Typed formats come first, so they win the Vulkan to DXGI lookup. Typeless ones map to a format of their class,
	// every image is created mutable
	constexpr Format Formats[] = {
		{ DXGI_FORMAT_R32G32B32A32_FLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, 16 },
		{ DXGI_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_UINT, 16 },
		{ DXGI_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SINT, 16 },
		{ DXGI_FORMAT_R16G16B16A16_FLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, 8 },
		{ DXGI_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_UNORM, 8 },
		{ DXGI_FORMAT_R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_UINT, 8 },
		{ DXGI_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R16G16B16A16_SNORM, 8 },
		{ DXGI_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SINT, 8 },
		{ DXGI_FORMAT_R32G32_FLOAT, VK_FORMAT_R32G32_SFLOAT, 8 },
		{ DXGI_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_UINT, 8 },
		{ DXGI_FORMAT_R32G32_SINT, VK_FORMAT_R32G32_SINT, 8 },
		{ DXGI_FORMAT_R10G10B10A2_UNORM, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4 },
		{ DXGI_FORMAT_R10G10B10A2_UINT, VK_FORMAT_A2B10G10R10_UINT_PACK32, 4 },
		{ DXGI_FORMAT_R11G11B10_FLOAT, VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4 },
		{ DXGI_FORMAT_R9G9B9E5_SHAREDEXP, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, 4 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, 4 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, VK_FORMAT_R8G8B8A8_SRGB, 4 },
		{ DXGI_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_UINT, 4 },
		{ DXGI_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_SNORM, 4 },
		{ DXGI_FORMAT_R8G8B8A8_SINT, VK_FORMAT_R8G8B8A8_SINT, 4 },
		{ DXGI_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM, 4 },
		{ DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, VK_FORMAT_B8G8R8A8_SRGB, 4 },
		{ DXGI_FORMAT_R16G16_FLOAT, VK_FORMAT_R16G16_SFLOAT, 4 },
		{ DXGI_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16_UNORM, 4 },
		{ DXGI_FORMAT_R16G16_UINT, VK_FORMAT_R16G16_UINT, 4 },
		{ DXGI_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16_SNORM, 4 },
		{ DXGI_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SINT, 4 },
		{ DXGI_FORMAT_R32_FLOAT, VK_FORMAT_R32_SFLOAT, 4 },
		{ DXGI_FORMAT_R32_UINT, VK_FORMAT_R32_UINT, 4 },
		{ DXGI_FORMAT_R32_SINT, VK_FORMAT_R32_SINT, 4 },
		{ DXGI_FORMAT_D32_FLOAT, VK_FORMAT_D32_SFLOAT, 4 },
		{ DXGI_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, 4 },
		{ DXGI_FORMAT_D32_FLOAT_S8X24_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, 8 },
		{ DXGI_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8_UNORM, 2 },
		{ DXGI_FORMAT_R8G8_UINT, VK_FORMAT_R8G8_UINT, 2 },
		{ DXGI_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8_SNORM, 2 },
		{ DXGI_FORMAT_R8G8_SINT, VK_FORMAT_R8G8_SINT, 2 },
		{ DXGI_FORMAT_R16_FLOAT, VK_FORMAT_R16_SFLOAT, 2 },
		{ DXGI_FORMAT_R16_UNORM, VK_FORMAT_R16_UNORM, 2 },
		{ DXGI_FORMAT_R16_UINT, VK_FORMAT_R16_UINT, 2 },
		{ DXGI_FORMAT_R16_SNORM, VK_FORMAT_R16_SNORM, 2 },
		{ DXGI_FORMAT_R16_SINT, VK_FORMAT_R16_SINT, 2 },
		{ DXGI_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM, 2 },
		{ DXGI_FORMAT_R8_UNORM, VK_FORMAT_R8_UNORM, 1 },
		{ DXGI_FORMAT_R8_UINT, VK_FORMAT_R8_UINT, 1 },
		{ DXGI_FORMAT_R8_SNORM, VK_FORMAT_R8_SNORM, 1 },
		{ DXGI_FORMAT_R8_SINT, VK_FORMAT_R8_SINT, 1 },
		{ DXGI_FORMAT_R32G32B32A32_TYPELESS, VK_FORMAT_R32G32B32A32_SFLOAT, 16 },
		{ DXGI_FORMAT_R16G16B16A16_TYPELESS, VK_FORMAT_R16G16B16A16_SFLOAT, 8 },
		{ DXGI_FORMAT_R32G32_TYPELESS, VK_FORMAT_R32G32_SFLOAT, 8 },
		{ DXGI_FORMAT_R10G10B10A2_TYPELESS, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4 },
		{ DXGI_FORMAT_R8G8B8A8_TYPELESS, VK_FORMAT_R8G8B8A8_UNORM, 4 },
		{ DXGI_FORMAT_B8G8R8A8_TYPELESS, VK_FORMAT_B8G8R8A8_UNORM, 4 },
		{ DXGI_FORMAT_R16G16_TYPELESS, VK_FORMAT_R16G16_SFLOAT, 4 },
		{ DXGI_FORMAT_R32_TYPELESS, VK_FORMAT_R32_SFLOAT, 4 },
		{ DXGI_FORMAT_R8G8_TYPELESS, VK_FORMAT_R8G8_UNORM, 2 },
		{ DXGI_FORMAT_R16_TYPELESS, VK_FORMAT_R16_SFLOAT, 2 },
		{ DXGI_FORMAT_R8_TYPELESS, VK_FORMAT_R8_UNORM, 1 },
	};

	const Format* FindFormat(DXGI_FORMAT format)
	{
		auto it = std::ranges::find(Formats, format, &Format::dxgi);
		if (it == std::end(Formats))
		{
			Log::Error("D3D12OnVk: unsupported DXGI format {}", (int)format);
			return nullptr;
		}

		return it;
	}

	const Format* FindFormat(VkFormat format)
	{
		auto it = std::ranges::find(Formats, format, &Format::vk);
		return it == std::end(Formats) ? nullptr : it;
	}

	bool IsDepth(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D24_UNORM_S8_UINT ||
			format == VK_FORMAT_D32_SFLOAT_S8_UINT;
	}

	VkImageAspectFlags Aspect(VkFormat format)
	{
		if (format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT)
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

		return IsDepth(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	}

	// The image format for a resource, typeless depth resources become depth formats
	const Format* ResourceFormat(const D3D12_RESOURCE_DESC& desc)
	{
		DXGI_FORMAT format = desc.Format;
		if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		{
			if (format == DXGI_FORMAT_R32_TYPELESS)
				format = DXGI_FORMAT_D32_FLOAT;
			else if (format == DXGI_FORMAT_R16_TYPELESS)
				format = DXGI_FORMAT_D16_UNORM;
		}

		return FindFormat(format);
	}

	// COM objects. Their vtables are built by hand: every slot starts out as a stub logging the method it stands for,
	// then the implemented methods replace theirs
	struct Stub
	{
		const char* family;
		const char* method;
		std::atomic_bool logged;
	};

	// Indexed by the stubs' code
	std::deque<Stub> stubs;

	HRESULT UnimplementedCall(uint32_t index)
	{
		auto& stub = stubs[index];
		if (!stub.logged.exchange(true))
			Log::Warn("D3D12OnVk: {}::{} isn't implemented", stub.family, stub.method);

		return E_NOTIMPL;
	}

	using Methods = std::initializer_list<std::pair<const char*, void*>>;

	// common holds the IUnknown/ID3D12Object/ID3D12DeviceChild methods, the ones a family lacks are skipped
	void** BuildVTable(const char* family, std::span<const char* const> names, Methods common, Methods methods)
	{
		constexpr size_t StubSize = 32;

		auto vtable = new void*[names.size()];
		auto code = static_cast<uint8_t*>(VirtualAlloc(nullptr, names.size() * StubSize, MEM_COMMIT | MEM_RESERVE,
			PAGE_EXECUTE_READWRITE));

		for (size_t i = 0; i < names.size(); i++)
		{
			auto index = (uint32_t)stubs.size();
			stubs.emplace_back(family, names[i]);

			// mov ecx, index ; mov rax, UnimplementedCall ; jmp rax
			uint8_t* p = code + i * StubSize;
			uint64_t target = reinterpret_cast<uint64_t>(&UnimplementedCall);
			vtable[i] = p;
			*p++ = 0xB9, memcpy(p, &index, 4), p += 4;
			*p++ = 0x48, *p++ = 0xB8, memcpy(p, &target, 8), p += 8;
			*p++ = 0xFF, *p++ = 0xE0;
		}

		FlushInstructionCache(GetCurrentProcess(), code, names.size() * StubSize);

		auto set = [&](const char* name, void* function, bool required)
			{
				auto it = std::ranges::find_if(names, [name](const char* n) { return strcmp(n, name) == 0; });
				if (it != names.end())
					vtable[it - names.begin()] = function;
				else if (required)
					Log::Error("D3D12OnVk: {} has no method {}", family, name);
			};

		for (auto& [name, function] : common)
			set(name, function, false);

		for (auto& [name, function] : methods)
			set(name, function, true);

		return vtable;
	}

#define METHOD(Type, Name) { #Name, reinterpret_cast<void*>(&Type::Name) }

	struct Device;

	struct Object
	{
		void** vtable = nullptr;
		std::atomic<ULONG> refs = 1;
		Device* device = nullptr; // itself for the device
		std::span<const IID> iids;
		void (*destroy)(Object*) = nullptr;

		static HRESULT STDMETHODCALLTYPE QueryInterface(Object* self, REFIID riid, void** object)
		{
			if (!object)
				return E_POINTER;

			if (std::ranges::find(self->iids, riid) == self->iids.end())
			{
				*object = nullptr;
				return E_NOINTERFACE;
			}

			AddRef(self);
			*object = self;
			return S_OK;
		}

		static ULONG STDMETHODCALLTYPE AddRef(Object* self) { return ++self->refs; }

		static ULONG STDMETHODCALLTYPE Release(Object* self)
		{
			ULONG refs = --self->refs;
			if (refs == 0)
				self->destroy(self);

			return refs;
		}

		static HRESULT STDMETHODCALLTYPE GetPrivateData(Object*, REFGUID, UINT* size, void*)
		{
			if (size)
				*size = 0;

			return DXGI_ERROR_NOT_FOUND;
		}

		static HRESULT STDMETHODCALLTYPE SetPrivateData(Object*, REFGUID, UINT, const void*) { return S_OK; }
		static HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(Object*, REFGUID, const IUnknown*) { return S_OK; }
		static HRESULT STDMETHODCALLTYPE SetName(Object*, LPCWSTR) { return S_OK; }

		static HRESULT STDMETHODCALLTYPE GetDevice(Object* self, REFIID riid, void** device)
		{
			return QueryInterface(reinterpret_cast<Object*>(self->device), riid, device);
		}
	};

	const Methods CommonMethods = {
		METHOD(Object, QueryInterface),
		METHOD(Object, AddRef),
		METHOD(Object, Release),
		METHOD(Object, GetPrivateData),
		METHOD(Object, SetPrivateData),
		METHOD(Object, SetPrivateDataInterface),
		METHOD(Object, SetName),
		METHOD(Object, GetDevice),
	};

	template <class T> T* New(Device* device)
	{
		auto object = new T();
		object->vtable = T::VTable();
		object->iids = T::Iids;
		object->device = device ? device : reinterpret_cast<Device*>(object);
		object->destroy = [](Object* o)
			{
				auto device = reinterpret_cast<Object*>(o->device);
				delete static_cast<T*>(o);
				if (device != o)
					Object::Release(device);
			};

		if (device)
			Object::AddRef(reinterpret_cast<Object*>(device));

		return object;
	}

	// Hands a newly created object out as riid, the creation reference is dropped
	HRESULT Return(Object* object, REFIID riid, void** out)
	{
		HRESULT hr = out ? Object::QueryInterface(object, riid, out) : S_FALSE;
		Object::Release(object);
		return hr;
	}

	template <class T> T* As(void* object) { return static_cast<T*>(reinterpret_cast<Object*>(object)); }

	struct Resource;
	struct Heap;

	struct Device : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12Device),
			__uuidof(ID3D12Device1) };
		static void** VTable();

		Vk vk;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice handle = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties properties{};
		VkPhysicalDeviceMemoryProperties memoryProperties{};
		LUID luid{};
		bool integerDotProduct = false;
		VkPipelineCache pipelineCache = VK_NULL_HANDLE;

		// The game's command buffer of the evaluation in progress. XeFG's own command lists record into it instead of
		// being submitted, a game's queues are only safe to use from the game
		VkCommandBuffer current = VK_NULL_HANDLE;

		std::mutex mutex;

		// Fake GPU addresses of the buffers, for constant buffer views
		std::map<uint64_t, Resource*> addresses;
		uint64_t nextAddress = 1ull << 40;

		// Images that still need their transition out of VK_IMAGE_LAYOUT_UNDEFINED, done by the next recorded command
		std::vector<std::pair<VkImage, VkImageAspectFlags>> uninitialized;

		// Descriptor sets come from a ring of pool groups, a group is reset when the ring comes back to it.
		// ponytail: assumes 64 command list recordings are enough for the GPU to be done with a group, like
		// DeferredDestroy's evaluation count
		static constexpr uint32_t PoolRing = 64;
		std::vector<VkDescriptorPool> pools[PoolRing];
		uint32_t poolSlot = 0;
		size_t poolNext = 0;

		// What XeFG's own command lists used, XeFG releases it once their fence says done but the GPU only runs them
		// with the game's command buffer. Released with the pool group
		std::vector<Object*> held[PoolRing];

		~Device();

		uint32_t MemoryType(D3D12_HEAP_TYPE type, uint32_t allowed) const;
		VkDeviceMemory Allocate(VkDeviceSize size, D3D12_HEAP_TYPE type, uint32_t allowed, uint8_t** mapped);
		void NextPoolSlot();
		VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout);
		Resource* FindAddress(uint64_t address, uint64_t& offset);
	};

	uint32_t Device::MemoryType(D3D12_HEAP_TYPE type, uint32_t allowed) const
	{
		constexpr VkMemoryPropertyFlags HostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		// Readback prefers cached memory, but takes any host visible memory
		const VkMemoryPropertyFlags wanted[] = {
			type == D3D12_HEAP_TYPE_DEFAULT ? (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			: type == D3D12_HEAP_TYPE_UPLOAD ? HostVisible : HostVisible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
			type == D3D12_HEAP_TYPE_DEFAULT ? (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : HostVisible,
		};

		for (VkMemoryPropertyFlags flags : wanted)
		{
			for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
			{
				if ((allowed & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
					return i;
			}
		}

		return UINT32_MAX;
	}

	VkDeviceMemory Device::Allocate(VkDeviceSize size, D3D12_HEAP_TYPE type, uint32_t allowed, uint8_t** mapped)
	{
		VkMemoryAllocateInfo info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, size, MemoryType(type, allowed) };
		VkDeviceMemory memory = VK_NULL_HANDLE;

		if (info.memoryTypeIndex == UINT32_MAX || vk.vkAllocateMemory(handle, &info, nullptr, &memory) != VK_SUCCESS)
		{
			Log::Error("D3D12OnVk: can't allocate {} bytes of heap type {}", size, (int)type);
			return VK_NULL_HANDLE;
		}

		*mapped = nullptr;
		if (type != D3D12_HEAP_TYPE_DEFAULT)
			vk.vkMapMemory(handle, memory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(mapped));

		return memory;
	}

	void Device::NextPoolSlot()
	{
		std::vector<Object*> release;
		{
			std::scoped_lock lock(mutex);
			poolSlot = (poolSlot + 1) % PoolRing;
			poolNext = 0;

			for (VkDescriptorPool pool : pools[poolSlot])
				vk.vkResetDescriptorPool(handle, pool, 0);

			release.swap(held[poolSlot]);
		}

		// Outside the lock, destructors take it
		for (Object* object : release)
			Object::Release(object);
	}

	VkDescriptorSet Device::AllocateSet(VkDescriptorSetLayout layout)
	{
		std::scoped_lock lock(mutex);
		auto& group = pools[poolSlot];

		for (;; poolNext++)
		{
			if (poolNext == group.size())
			{
				const VkDescriptorPoolSize sizes[] = {
					{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
					{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },
					{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4096 },
					{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2048 },
					{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 2048 },
					{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048 },
					{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096 },
				};

				VkDescriptorPoolCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
				info.maxSets = 1024;
				info.poolSizeCount = (uint32_t)std::size(sizes);
				info.pPoolSizes = sizes;

				VkDescriptorPool pool = VK_NULL_HANDLE;
				if (vk.vkCreateDescriptorPool(handle, &info, nullptr, &pool) != VK_SUCCESS)
					return VK_NULL_HANDLE;

				group.push_back(pool);
			}

			VkDescriptorSetAllocateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, group[poolNext], 1,
				&layout };
			VkDescriptorSet set = VK_NULL_HANDLE;
			if (vk.vkAllocateDescriptorSets(handle, &info, &set) == VK_SUCCESS)
				return set;
		}
	}

	Device::~Device()
	{
		for (auto& group : pools)
		{
			for (VkDescriptorPool pool : group)
				vk.vkDestroyDescriptorPool(handle, pool, nullptr);
		}

		vk.vkDestroyPipelineCache(handle, pipelineCache, nullptr);
	}

	// Heaps
	struct Heap : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12Pageable), __uuidof(ID3D12Heap) };
		static void** VTable();

		D3D12_HEAP_DESC desc{};
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint8_t* mapped = nullptr;

		~Heap() { device->vk.vkFreeMemory(device->handle, memory, nullptr); }
	};

	void** Heap::VTable()
	{
		static void** vtable = BuildVTable("ID3D12Heap", D3D12Vtables::HeapMethods, CommonMethods, {});
		return vtable;
	}

	D3D12_HEAP_TYPE HeapType(const D3D12_HEAP_PROPERTIES& properties)
	{
		if (properties.Type != D3D12_HEAP_TYPE_CUSTOM)
			return properties.Type;

		switch (properties.CPUPageProperty)
		{
		case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:
			return D3D12_HEAP_TYPE_READBACK;
		case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE:
			return D3D12_HEAP_TYPE_UPLOAD;
		default:
			return D3D12_HEAP_TYPE_DEFAULT;
		}
	}

	// Resources
	struct ViewKey
	{
		VkImageViewType type;
		VkFormat format;
		uint32_t baseMip, mips, baseLayer, layers;
		VkComponentMapping components;

		auto operator<=>(const ViewKey& o) const { return memcmp(this, &o, sizeof(*this)) <=> 0; }
		bool operator==(const ViewKey& o) const { return memcmp(this, &o, sizeof(*this)) == 0; }
	};

	struct Resource : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12Pageable), __uuidof(ID3D12Resource) };
		static void** VTable();

		D3D12_RESOURCE_DESC desc{};

		VkBuffer buffer = VK_NULL_HANDLE;
		VkImage image = VK_NULL_HANDLE;
		const Format* format = nullptr;
		uint32_t mips = 1, layers = 1;

		VkDeviceMemory memory = VK_NULL_HANDLE; // committed resources
		Heap* heap = nullptr;                   // placed resources
		uint8_t* mapped = nullptr;
		uint64_t address = 0;

		// Images the caller owns, their layout follows the barriers
		bool external = false;
		VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;

		std::map<ViewKey, VkImageView> imageViews;
		std::map<std::tuple<VkFormat, VkDeviceSize, VkDeviceSize>, VkBufferView> bufferViews;

		// Depth to color copies go through this buffer
		VkBuffer bounce = VK_NULL_HANDLE;
		VkDeviceMemory bounceMemory = VK_NULL_HANDLE;

		~Resource();

		VkImageView ImageView(const ViewKey& key);
		VkBufferView BufferView(VkFormat format, VkDeviceSize offset, VkDeviceSize range);
		VkImageSubresourceLayers Subresource(uint32_t index) const;

		static HRESULT STDMETHODCALLTYPE Map(Resource* self, UINT, const D3D12_RANGE*, void** data)
		{
			if (!self->mapped)
				return E_INVALIDARG;

			if (data)
				*data = self->mapped;

			return S_OK;
		}

		static void STDMETHODCALLTYPE Unmap(Resource*, UINT, const D3D12_RANGE*) {}

		static D3D12_RESOURCE_DESC* STDMETHODCALLTYPE GetDesc(Resource* self, D3D12_RESOURCE_DESC* desc)
		{
			*desc = self->desc;
			return desc;
		}

		static D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress(Resource* self) { return self->address; }
	};

	void** Resource::VTable()
	{
		static void** vtable = BuildVTable("ID3D12Resource", D3D12Vtables::ResourceMethods, CommonMethods,
			{ METHOD(Resource, Map), METHOD(Resource, Unmap), METHOD(Resource, GetDesc),
				METHOD(Resource, GetGPUVirtualAddress) });
		return vtable;
	}

	Resource::~Resource()
	{
		auto& vk = device->vk;
		VkDevice handle = device->handle;

		for (auto& [key, view] : imageViews)
			vk.vkDestroyImageView(handle, view, nullptr);

		for (auto& [key, view] : bufferViews)
			vk.vkDestroyBufferView(handle, view, nullptr);

		{
			std::scoped_lock lock(device->mutex);
			if (address)
				device->addresses.erase(address);

			std::erase_if(device->uninitialized, [this](auto& pending) { return pending.first == image; });
		}

		vk.vkDestroyBuffer(handle, bounce, nullptr);
		vk.vkFreeMemory(handle, bounceMemory, nullptr);

		if (!external)
		{
			vk.vkDestroyBuffer(handle, buffer, nullptr);
			vk.vkDestroyImage(handle, image, nullptr);
		}

		vk.vkFreeMemory(handle, memory, nullptr);

		if (heap)
			Object::Release(heap);
	}

	VkImageView Resource::ImageView(const ViewKey& key)
	{
		std::scoped_lock lock(device->mutex);
		if (auto it = imageViews.find(key); it != imageViews.end())
			return it->second;

		VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		info.image = image;
		info.viewType = key.type;
		info.format = key.format;
		info.components = key.components;
		VkImageAspectFlags aspect = IsDepth(key.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange = { aspect, key.baseMip, key.mips, key.baseLayer, key.layers };

		VkImageView view = VK_NULL_HANDLE;
		if (device->vk.vkCreateImageView(device->handle, &info, nullptr, &view) != VK_SUCCESS)
			Log::Error("D3D12OnVk: can't create an image view of format {}", (int)key.format);

		return imageViews[key] = view;
	}

	VkBufferView Resource::BufferView(VkFormat viewFormat, VkDeviceSize offset, VkDeviceSize range)
	{
		std::scoped_lock lock(device->mutex);
		auto key = std::make_tuple(viewFormat, offset, range);
		if (auto it = bufferViews.find(key); it != bufferViews.end())
			return it->second;

		VkBufferViewCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, nullptr, 0, buffer, viewFormat, offset, range };
		VkBufferView view = VK_NULL_HANDLE;
		if (device->vk.vkCreateBufferView(device->handle, &info, nullptr, &view) != VK_SUCCESS)
			Log::Error("D3D12OnVk: can't create a buffer view of format {}", (int)viewFormat);

		return bufferViews[key] = view;
	}

	VkImageSubresourceLayers Resource::Subresource(uint32_t index) const
	{
		return { Aspect(format->vk), index % mips, (index / mips) % layers, 1 };
	}

	Resource* Device::FindAddress(uint64_t gpuAddress, uint64_t& offset)
	{
		std::scoped_lock lock(mutex);
		auto it = addresses.upper_bound(gpuAddress);
		if (it == addresses.begin())
			return nullptr;

		--it;
		offset = gpuAddress - it->first;
		return offset < it->second->desc.Width ? it->second : nullptr;
	}

	VkBufferCreateInfo BufferCreateInfo(const D3D12_RESOURCE_DESC& desc)
	{
		VkBufferCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		info.size = desc.Width;
		info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		return info;
	}

	uint32_t MipCount(const D3D12_RESOURCE_DESC& desc)
	{
		if (desc.MipLevels)
			return desc.MipLevels;

		uint64_t size = std::max<uint64_t>({ desc.Width, desc.Height,
			desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1u });
		uint32_t mips = 1;
		while (size >>= 1)
			mips++;

		return mips;
	}

	VkImageCreateInfo ImageCreateInfo(const D3D12_RESOURCE_DESC& desc, const Format* format)
	{
		const bool is3d = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;

		VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		info.imageType = is3d ? VK_IMAGE_TYPE_3D
			: desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ? VK_IMAGE_TYPE_1D : VK_IMAGE_TYPE_2D;
		info.format = format->vk;
		info.extent = { (uint32_t)desc.Width, desc.Height, is3d ? desc.DepthOrArraySize : 1u };
		info.mipLevels = MipCount(desc);
		info.arrayLayers = is3d ? 1u : desc.DepthOrArraySize;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		{
			info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
			info.flags |= VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
		}

		if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
			info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
			info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		return info;
	}

	// Creates the buffer or image of desc, without memory
	Resource* CreateResource(Device* device, const D3D12_RESOURCE_DESC& desc, VkMemoryRequirements& reqs)
	{
		auto& vk = device->vk;
		auto resource = New<Resource>(device);
		resource->desc = desc;

		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			auto info = BufferCreateInfo(desc);
			if (vk.vkCreateBuffer(device->handle, &info, nullptr, &resource->buffer) != VK_SUCCESS)
			{
				Object::Release(resource);
				return nullptr;
			}

			vk.vkGetBufferMemoryRequirements(device->handle, resource->buffer, &reqs);

			std::scoped_lock lock(device->mutex);
			resource->address = device->nextAddress;
			device->nextAddress += (desc.Width + 0xFFFF) & ~0xFFFFull;
			device->addresses[resource->address] = resource;
			return resource;
		}

		resource->format = ResourceFormat(desc);
		if (!resource->format)
		{
			Object::Release(resource);
			return nullptr;
		}

		auto info = ImageCreateInfo(desc, resource->format);
		resource->mips = info.mipLevels;
		resource->layers = info.arrayLayers;
		resource->desc.MipLevels = (UINT16)info.mipLevels;

		if (vk.vkCreateImage(device->handle, &info, nullptr, &resource->image) != VK_SUCCESS)
		{
			Log::Error("D3D12OnVk: can't create a {}x{} image of format {}", desc.Width, desc.Height, (int)desc.Format);
			Object::Release(resource);
			return nullptr;
		}

		vk.vkGetImageMemoryRequirements(device->handle, resource->image, &reqs);

		std::scoped_lock lock(device->mutex);
		device->uninitialized.emplace_back(resource->image, Aspect(resource->format->vk));
		return resource;
	}

	bool BindMemory(Resource* resource, VkDeviceMemory memory, VkDeviceSize offset)
	{
		auto& vk = resource->device->vk;
		VkDevice handle = resource->device->handle;
		return (resource->buffer ? vk.vkBindBufferMemory(handle, resource->buffer, memory, offset)
			: vk.vkBindImageMemory(handle, resource->image, memory, offset)) == VK_SUCCESS;
	}

	// Descriptors, the CPU and GPU handles both point at them
	struct Descriptor
	{
		enum Kind : uint32_t
		{
			Empty,
			Image,
			TexelBuffer,
			Buffer,
		} kind = Empty;

		VkImageView imageView = VK_NULL_HANDLE;
		ViewKey key{};
		VkBufferView bufferView = VK_NULL_HANDLE;
		VkDescriptorBufferInfo buffer{};
		Resource* resource = nullptr;
	};

	struct DescriptorHeap : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12Pageable), __uuidof(ID3D12DescriptorHeap) };
		static void** VTable();

		std::vector<Descriptor> descriptors;

		static D3D12_CPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart(DescriptorHeap* self,
			D3D12_CPU_DESCRIPTOR_HANDLE* handle)
		{
			handle->ptr = reinterpret_cast<SIZE_T>(self->descriptors.data());
			return handle;
		}

		static D3D12_GPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart(DescriptorHeap* self,
			D3D12_GPU_DESCRIPTOR_HANDLE* handle)
		{
			handle->ptr = reinterpret_cast<UINT64>(self->descriptors.data());
			return handle;
		}
	};

	void** DescriptorHeap::VTable()
	{
		static void** vtable = BuildVTable("ID3D12DescriptorHeap", D3D12Vtables::DescriptorHeapMethods, CommonMethods,
			{ METHOD(DescriptorHeap, GetCPUDescriptorHandleForHeapStart),
				METHOD(DescriptorHeap, GetGPUDescriptorHandleForHeapStart) });
		return vtable;
	}

	Descriptor& DescriptorAt(SIZE_T handle) { return *reinterpret_cast<Descriptor*>(handle); }

	VkComponentSwizzle Swizzle(UINT mapping, int component)
	{
		switch (D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(component, mapping))
		{
		case D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0:
			return VK_COMPONENT_SWIZZLE_R;
		case D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1:
			return VK_COMPONENT_SWIZZLE_G;
		case D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2:
			return VK_COMPONENT_SWIZZLE_B;
		case D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3:
			return VK_COMPONENT_SWIZZLE_A;
		case D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0:
			return VK_COMPONENT_SWIZZLE_ZERO;
		default:
			return VK_COMPONENT_SWIZZLE_ONE;
		}
	}

	// Raw and structured buffers are R32_UINT texel buffers, dxil-spirv indexes them in words
	void BufferDescriptor(Descriptor& d, Resource* r, DXGI_FORMAT viewFormat, UINT64 first, UINT count, UINT stride, bool raw)
	{
		const Format* format = viewFormat == DXGI_FORMAT_UNKNOWN || raw ? FindFormat(DXGI_FORMAT_R32_UINT) : FindFormat(viewFormat);
		if (!format)
			return;

		uint64_t elementSize = stride ? stride : raw ? 4 : format->bytes;
		d.kind = Descriptor::TexelBuffer;
		d.bufferView = r->BufferView(format->vk, first * elementSize, count * elementSize);
	}

	// Root signatures
	struct RootParameter
	{
		struct Range
		{
			D3D12_DESCRIPTOR_RANGE_TYPE type;
			uint32_t count, base, space, offset;
		};

		D3D12_ROOT_PARAMETER_TYPE type;
		std::vector<Range> ranges;
		uint32_t space = 0, reg = 0, words = 0, wordOffset = 0; // root constants
	};

	struct StaticSampler
	{
		uint32_t space, reg;
		VkSampler sampler;
	};

	struct RootSignature : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12RootSignature) };
		static void** VTable();

		std::vector<RootParameter> parameters;
		std::vector<StaticSampler> samplers;
		uint32_t rootWords = 0;

		~RootSignature()
		{
			for (auto& sampler : samplers)
				device->vk.vkDestroySampler(device->handle, sampler.sampler, nullptr);
		}

		// The root parameter and table offset holding register reg of space
		bool Locate(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t space, uint32_t reg, uint32_t& parameter,
			uint32_t& offset) const
		{
			for (uint32_t p = 0; p < parameters.size(); p++)
			{
				for (auto& range : parameters[p].ranges)
				{
					if (range.type == type && range.space == space && reg >= range.base &&
						(range.count == UINT_MAX || reg - range.base < range.count))
					{
						parameter = p;
						offset = range.offset + (reg - range.base);
						return true;
					}
				}
			}

			return false;
		}
	};

	void** RootSignature::VTable()
	{
		static void** vtable = BuildVTable("ID3D12RootSignature", D3D12Vtables::RootSignatureMethods, CommonMethods, {});
		return vtable;
	}

	VkSampler CreateSampler(Device* device, const D3D12_STATIC_SAMPLER_DESC& desc)
	{
		auto filter = [](D3D12_FILTER_TYPE type) { return type == D3D12_FILTER_TYPE_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST; };
		auto address = [](D3D12_TEXTURE_ADDRESS_MODE mode)
			{
				switch (mode)
				{
				case D3D12_TEXTURE_ADDRESS_MODE_WRAP:
					return VK_SAMPLER_ADDRESS_MODE_REPEAT;
				case D3D12_TEXTURE_ADDRESS_MODE_MIRROR:
					return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
				case D3D12_TEXTURE_ADDRESS_MODE_BORDER:
					return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
				case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE:
					return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
				default:
					return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				}
			};

		if (D3D12_DECODE_FILTER_REDUCTION(desc.Filter) >= D3D12_FILTER_REDUCTION_TYPE_MINIMUM)
			Log::Warn("D3D12OnVk: min/max sampler reduction isn't supported");

		VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		info.magFilter = filter(D3D12_DECODE_MAG_FILTER(desc.Filter));
		info.minFilter = filter(D3D12_DECODE_MIN_FILTER(desc.Filter));
		info.mipmapMode = D3D12_DECODE_MIP_FILTER(desc.Filter) == D3D12_FILTER_TYPE_LINEAR
			? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		info.addressModeU = address(desc.AddressU);
		info.addressModeV = address(desc.AddressV);
		info.addressModeW = address(desc.AddressW);
		info.mipLodBias = desc.MipLODBias;
		info.compareEnable = D3D12_DECODE_IS_COMPARISON_FILTER(desc.Filter);
		info.compareOp = info.compareEnable ? (VkCompareOp)(desc.ComparisonFunc - 1) : VK_COMPARE_OP_NEVER;
		info.minLod = desc.MinLOD;
		info.maxLod = desc.MaxLOD >= D3D12_FLOAT32_MAX ? VK_LOD_CLAMP_NONE : desc.MaxLOD;
		info.borderColor = desc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
			: desc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK ? VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK
			: VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		// ponytail: anisotropy needs a device feature the game may not have enabled, linear filtering instead
		VkSampler sampler = VK_NULL_HANDLE;
		device->vk.vkCreateSampler(device->handle, &info, nullptr, &sampler);
		return sampler;
	}

	// Pipelines
	struct Binding
	{
		uint32_t binding;
		VkDescriptorType type;
		uint32_t count;
		uint32_t parameter, offset; // where in the root signature's tables the descriptors are
		VkSampler sampler;          // static samplers
		VkImageViewType viewType;   // what the shader declared, D3D12 lets a 2D view stand in for a 2D array
	};

	struct PipelineState : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12Pageable), __uuidof(ID3D12PipelineState) };
		static void** VTable();

		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		std::vector<Binding> bindings;
		uint32_t rootWords = 0;

		~PipelineState()
		{
			device->vk.vkDestroyPipeline(device->handle, pipeline, nullptr);
			device->vk.vkDestroyPipelineLayout(device->handle, layout, nullptr);
			device->vk.vkDestroyDescriptorSetLayout(device->handle, setLayout, nullptr);
		}
	};

	void** PipelineState::VTable()
	{
		static void** vtable = BuildVTable("ID3D12PipelineState", D3D12Vtables::PipelineStateMethods, CommonMethods, {});
		return vtable;
	}

	// State of one DXIL to SPIR-V translation, the remappers give every resource the next binding of set 0
	struct Translation
	{
		const RootSignature* rootSignature;
		std::vector<Binding> bindings;

		uint32_t Add(VkDescriptorType type, uint32_t count, uint32_t parameter, uint32_t offset,
			VkSampler sampler = VK_NULL_HANDLE, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D)
		{
			bindings.push_back({ (uint32_t)bindings.size(), type, count, parameter, offset, sampler, viewType });
			return bindings.back().binding;
		}

		bool Locate(D3D12_DESCRIPTOR_RANGE_TYPE type, const dxil_spv_d3d_binding& d3d, uint32_t& parameter,
			uint32_t& offset) const
		{
			// Declined, dxil-spirv then asks again for a single descriptor
			if (d3d.range_size == UINT_MAX)
				return false;

			if (!rootSignature->Locate(type, d3d.register_space, d3d.register_index, parameter, offset))
			{
				Log::Error("D3D12OnVk: register {} of space {} isn't in the root signature", d3d.register_index,
					d3d.register_space);
				return false;
			}

			return true;
		}
	};

	bool IsBuffer(dxil_spv_resource_kind kind)
	{
		return kind == DXIL_SPV_RESOURCE_KIND_RAW_BUFFER || kind == DXIL_SPV_RESOURCE_KIND_STRUCTURED_BUFFER;
	}

	VkImageViewType ViewType(dxil_spv_resource_kind kind)
	{
		switch (kind)
		{
		case DXIL_SPV_RESOURCE_KIND_TEXTURE_1D:
			return VK_IMAGE_VIEW_TYPE_1D;
		case DXIL_SPV_RESOURCE_KIND_TEXTURE_1D_ARRAY:
			return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		case DXIL_SPV_RESOURCE_KIND_TEXTURE_2D_ARRAY:
			return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case DXIL_SPV_RESOURCE_KIND_TEXTURE_3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		case DXIL_SPV_RESOURCE_KIND_TEXTURE_CUBE:
			return VK_IMAGE_VIEW_TYPE_CUBE;
		case DXIL_SPV_RESOURCE_KIND_TEXTURE_CUBE_ARRAY:
			return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		default:
			return VK_IMAGE_VIEW_TYPE_2D;
		}
	}

	dxil_spv_bool RemapSrv(void* userdata, const dxil_spv_d3d_binding* d3d, dxil_spv_srv_vulkan_binding* vulkan)
	{
		auto& t = *static_cast<Translation*>(userdata);
		uint32_t parameter, offset;
		if (!t.Locate(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, *d3d, parameter, offset))
			return false;

		VkDescriptorType type = IsBuffer(d3d->kind) || d3d->kind == DXIL_SPV_RESOURCE_KIND_TYPED_BUFFER
			? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

		*vulkan = {};
		vulkan->buffer_binding.binding = t.Add(type, d3d->range_size, parameter, offset, VK_NULL_HANDLE, ViewType(d3d->kind));
		vulkan->buffer_binding.descriptor_type = IsBuffer(d3d->kind) ? DXIL_SPV_VULKAN_DESCRIPTOR_TYPE_TEXEL_BUFFER
			: DXIL_SPV_VULKAN_DESCRIPTOR_TYPE_IDENTITY;
		return true;
	}

	dxil_spv_bool RemapUav(void* userdata, const dxil_spv_uav_d3d_binding* d3d, dxil_spv_uav_vulkan_binding* vulkan)
	{
		auto& t = *static_cast<Translation*>(userdata);
		uint32_t parameter, offset;
		if (!t.Locate(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, d3d->d3d_binding, parameter, offset))
			return false;

		if (d3d->has_counter)
		{
			Log::Error("D3D12OnVk: UAV counters aren't supported");
			return false;
		}

		auto kind = d3d->d3d_binding.kind;
		VkDescriptorType type = IsBuffer(kind) || kind == DXIL_SPV_RESOURCE_KIND_TYPED_BUFFER
			? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		*vulkan = {};
		vulkan->buffer_binding.binding = t.Add(type, d3d->d3d_binding.range_size, parameter, offset, VK_NULL_HANDLE,
			ViewType(kind));
		vulkan->buffer_binding.descriptor_type = IsBuffer(kind) ? DXIL_SPV_VULKAN_DESCRIPTOR_TYPE_TEXEL_BUFFER
			: DXIL_SPV_VULKAN_DESCRIPTOR_TYPE_IDENTITY;
		return true;
	}

	dxil_spv_bool RemapCbv(void* userdata, const dxil_spv_d3d_binding* d3d, dxil_spv_cbv_vulkan_binding* vulkan)
	{
		auto& t = *static_cast<Translation*>(userdata);
		*vulkan = {};

		// Root constants become push constants
		for (auto& parameter : t.rootSignature->parameters)
		{
			if (parameter.type == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS && parameter.space == d3d->register_space &&
				parameter.reg == d3d->register_index)
			{
				vulkan->push_constant = true;
				vulkan->vulkan.push_constant.offset_in_words = parameter.wordOffset;
				return true;
			}
		}

		uint32_t parameter, offset;
		if (!t.Locate(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, *d3d, parameter, offset))
			return false;

		vulkan->vulkan.uniform_binding.binding = t.Add(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, d3d->range_size, parameter, offset);
		return true;
	}

	dxil_spv_bool RemapSampler(void* userdata, const dxil_spv_d3d_binding* d3d, dxil_spv_vulkan_binding* vulkan)
	{
		auto& t = *static_cast<Translation*>(userdata);
		*vulkan = {};

		for (auto& sampler : t.rootSignature->samplers)
		{
			if (sampler.space == d3d->register_space && sampler.reg == d3d->register_index)
			{
				vulkan->binding = t.Add(VK_DESCRIPTOR_TYPE_SAMPLER, 1, 0, 0, sampler.sampler);
				return true;
			}
		}

		Log::Error("D3D12OnVk: only static samplers are supported, s{} of space {} isn't one", d3d->register_index,
			d3d->register_space);
		return false;
	}

	void DxilSpirvLog(void*, dxil_spv_log_level level, const char* message)
	{
		if (level >= DXIL_SPV_LOG_LEVEL_WARN)
			Log::Warn("dxil-spirv: {}", message);
	}

	bool Translate(Device* device, const D3D12_SHADER_BYTECODE& bytecode, Translation& translation,
		std::vector<uint32_t>& spirv)
	{
		dxil_spv_set_thread_log_callback(DxilSpirvLog, nullptr);

		dxil_spv_parsed_blob blob = nullptr;
		if (dxil_spv_parse_dxil_blob(bytecode.pShaderBytecode, bytecode.BytecodeLength, &blob) != DXIL_SPV_SUCCESS)
		{
			Log::Error("D3D12OnVk: dxil-spirv can't parse a {} byte shader", bytecode.BytecodeLength);
			return false;
		}

		dxil_spv_converter converter = nullptr;
		dxil_spv_create_converter(blob, &converter);
		dxil_spv_converter_set_srv_remapper(converter, RemapSrv, &translation);
		dxil_spv_converter_set_uav_remapper(converter, RemapUav, &translation);
		dxil_spv_converter_set_cbv_remapper(converter, RemapCbv, &translation);
		dxil_spv_converter_set_sampler_remapper(converter, RemapSampler, &translation);
		dxil_spv_converter_set_root_constant_word_count(converter, translation.rootSignature->rootWords);

		// Without it dot4add is emulated with plain integer math
		dxil_spv_option_shader_i8_dot i8Dot{ { DXIL_SPV_OPTION_SHADER_I8_DOT }, device->integerDotProduct };
		dxil_spv_converter_add_option(converter, &i8Dot.base);

		bool ok = dxil_spv_converter_run(converter) == DXIL_SPV_SUCCESS;
		if (ok)
		{
			dxil_spv_compiled_spirv compiled{};
			dxil_spv_converter_get_compiled_spirv(converter, &compiled);

			spirv.assign(static_cast<const uint32_t*>(compiled.data),
				static_cast<const uint32_t*>(compiled.data) + compiled.size / 4);
		}
		else
		{
			Log::Error("D3D12OnVk: dxil-spirv can't translate a {} byte shader", bytecode.BytecodeLength);
		}

		dxil_spv_converter_free(converter);
		dxil_spv_parsed_blob_free(blob);
		return ok;
	}

	// Fences. Nothing is ever submitted, so every signaled value is complete right away
	struct Fence : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12Pageable), __uuidof(ID3D12Fence) };
		static void** VTable();

		std::atomic_uint64_t completed = 0;

		static UINT64 STDMETHODCALLTYPE GetCompletedValue(Fence* self) { return self->completed; }

		static HRESULT STDMETHODCALLTYPE SetEventOnCompletion(Fence*, UINT64, HANDLE event)
		{
			if (event)
				SetEvent(event);

			return S_OK;
		}

		static HRESULT STDMETHODCALLTYPE Signal(Fence* self, UINT64 value)
		{
			self->completed = value;
			return S_OK;
		}
	};

	void** Fence::VTable()
	{
		static void** vtable = BuildVTable("ID3D12Fence", D3D12Vtables::FenceMethods, CommonMethods,
			{ METHOD(Fence, GetCompletedValue), METHOD(Fence, SetEventOnCompletion), METHOD(Fence, Signal) });
		return vtable;
	}

	// Command allocators and lists. XeFG's own lists record into the game's command buffer, allocators hold nothing
	struct CommandAllocator : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12Pageable), __uuidof(ID3D12CommandAllocator) };
		static void** VTable();

		static HRESULT STDMETHODCALLTYPE Reset(CommandAllocator*) { return S_OK; }
	};

	void** CommandAllocator::VTable()
	{
		static void** vtable = BuildVTable("ID3D12CommandAllocator", D3D12Vtables::CommandAllocatorMethods, CommonMethods,
			{ METHOD(CommandAllocator, Reset) });
		return vtable;
	}

	void FullBarrier(Device* device, VkCommandBuffer cmd, std::span<const VkImageMemoryBarrier> images = {})
	{
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT };
		device->vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			1, &barrier, 0, nullptr, (uint32_t)images.size(), images.data());
	}

	VkImageMemoryBarrier ImageBarrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to)
	{
		VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.oldLayout = from;
		barrier.newLayout = to;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		return barrier;
	}

	// The layout a wrapped image is in, in a D3D12 state. Matches the states OptiScaler gives DLSSG's Vulkan resources
	VkImageLayout StateLayout(D3D12_RESOURCE_STATES state, VkFormat format)
	{
		if (state == D3D12_RESOURCE_STATE_COPY_DEST)
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		if (state == D3D12_RESOURCE_STATE_COPY_SOURCE)
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		if (state == D3D12_RESOURCE_STATE_RENDER_TARGET)
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (state == D3D12_RESOURCE_STATE_DEPTH_WRITE)
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (state && !(state & ~(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ)))
			return IsDepth(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		return VK_IMAGE_LAYOUT_GENERAL;
	}

	VkImageLayout Layout(const Resource* resource) { return resource->external ? resource->layout : VK_IMAGE_LAYOUT_GENERAL; }

	struct CommandList : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12CommandList), __uuidof(ID3D12GraphicsCommandList) };
		static void** VTable();

		VkCommandBuffer cmd = VK_NULL_HANDLE;
		CommandAllocator* allocator = nullptr; // null for wrapped command buffers
		bool started = false;

		PipelineState* pipeline = nullptr;
		RootSignature* rootSignature = nullptr;
		std::vector<UINT64> tables;
		std::vector<uint32_t> rootWords;

		~CommandList()
		{
			if (allocator)
				Object::Release(allocator);
			else if (device->current == cmd)
				device->current = VK_NULL_HANDLE;
		}

		// XeFG's own lists only record while an evaluation is in progress, during its Init
		void Begin()
		{
			cmd = device->current;
			if (!cmd)
				Log::Error("D3D12OnVk: a command list began outside an evaluation");

			started = false;
			pipeline = nullptr;
			rootSignature = nullptr;
		}

		// Keeps object alive until the GPU is done with this evaluation's work, for XeFG's own lists
		void Hold(void* object)
		{
			if (!allocator)
				return;

			auto o = reinterpret_cast<Object*>(object);
			Object::AddRef(o);
			std::scoped_lock lock(device->mutex);
			device->held[device->poolSlot].push_back(o);
		}

		// Every recording method starts here. The first command waits for everything submitted before it and new
		// images leave VK_IMAGE_LAYOUT_UNDEFINED
		VkCommandBuffer Record()
		{
			std::vector<VkImageMemoryBarrier> barriers;
			{
				std::scoped_lock lock(device->mutex);
				for (auto& [image, aspect] : device->uninitialized)
					barriers.push_back(ImageBarrier(image, aspect, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL));

				device->uninitialized.clear();
			}

			if (!started || !barriers.empty())
				FullBarrier(device, cmd, barriers);

			started = true;
			return cmd;
		}

		static HRESULT STDMETHODCALLTYPE Close(CommandList* self) { return self->allocator ? S_OK : E_FAIL; }

		static HRESULT STDMETHODCALLTYPE Reset(CommandList* self, ID3D12CommandAllocator* allocator, ID3D12PipelineState* state)
		{
			if (!self->allocator)
				return E_FAIL;

			Object::AddRef(As<CommandAllocator>(allocator));
			Object::Release(self->allocator);
			self->allocator = As<CommandAllocator>(allocator);

			self->Begin();
			self->pipeline = As<PipelineState>(state);
			return S_OK;
		}

		static void STDMETHODCALLTYPE SetPipelineState(CommandList* self, ID3D12PipelineState* state)
		{
			self->pipeline = As<PipelineState>(state);
		}

		static void STDMETHODCALLTYPE SetDescriptorHeaps(CommandList*, UINT, ID3D12DescriptorHeap* const*) {}

		static void STDMETHODCALLTYPE SetComputeRootSignature(CommandList* self, ID3D12RootSignature* rootSignature)
		{
			self->rootSignature = As<RootSignature>(rootSignature);
			self->tables.assign(self->rootSignature->parameters.size(), 0);
			self->rootWords.assign(self->rootSignature->rootWords, 0);
		}

		static void STDMETHODCALLTYPE SetComputeRootDescriptorTable(CommandList* self, UINT index,
			D3D12_GPU_DESCRIPTOR_HANDLE base)
		{
			self->tables[index] = base.ptr;
		}

		static void STDMETHODCALLTYPE SetComputeRoot32BitConstants(CommandList* self, UINT index, UINT count,
			const void* data, UINT offset)
		{
			memcpy(self->rootWords.data() + self->rootSignature->parameters[index].wordOffset + offset, data, count * 4);
		}

		static void STDMETHODCALLTYPE Dispatch(CommandList* self, UINT x, UINT y, UINT z);

		static void STDMETHODCALLTYPE ResourceBarrier(CommandList* self, UINT count, const D3D12_RESOURCE_BARRIER* barriers)
		{
			auto cmd = self->Record();
			std::vector<VkImageMemoryBarrier> images;

			for (UINT i = 0; i < count; i++)
			{
				if (barriers[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
					continue;

				auto resource = As<Resource>(barriers[i].Transition.pResource);
				if (!resource->external)
					continue;

				VkImageLayout from = StateLayout(barriers[i].Transition.StateBefore, resource->format->vk);
				VkImageLayout to = StateLayout(barriers[i].Transition.StateAfter, resource->format->vk);
				resource->layout = to;

				if (from != to)
					images.push_back(ImageBarrier(resource->image, Aspect(resource->format->vk), from, to));
			}

			FullBarrier(self->device, cmd, images);
		}

		static void STDMETHODCALLTYPE CopyBufferRegion(CommandList* self, ID3D12Resource* destination, UINT64 destinationOffset,
			ID3D12Resource* source, UINT64 sourceOffset, UINT64 bytes)
		{
			self->Hold(destination);
			self->Hold(source);
			VkBufferCopy region{ sourceOffset, destinationOffset, bytes };
			self->device->vk.vkCmdCopyBuffer(self->Record(), As<Resource>(source)->buffer, As<Resource>(destination)->buffer,
				1, &region);
		}

		static void STDMETHODCALLTYPE CopyTextureRegion(CommandList* self, const D3D12_TEXTURE_COPY_LOCATION* destination,
			UINT x, UINT y, UINT z, const D3D12_TEXTURE_COPY_LOCATION* source, const D3D12_BOX* box);

		static void STDMETHODCALLTYPE CopyResource(CommandList* self, ID3D12Resource* destination, ID3D12Resource* source);

		static void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(CommandList* self, D3D12_GPU_DESCRIPTOR_HANDLE,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, const UINT values[4], UINT rectCount,
			const D3D12_RECT*)
		{
			auto cmd = self->Record();
			auto r = As<Resource>(resource);
			const Descriptor& descriptor = DescriptorAt(cpuHandle.ptr);

			if (rectCount)
				Log::Warn("D3D12OnVk: ClearUnorderedAccessViewUint rects are ignored");

			if (r->image)
			{
				VkClearColorValue color;
				memcpy(color.uint32, values, sizeof(color.uint32));
				VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0,
					VK_REMAINING_ARRAY_LAYERS };
				self->device->vk.vkCmdClearColorImage(cmd, r->image, Layout(r), &color, 1, &range);
				return;
			}

			// ponytail: fills with the first value only, enough for the zero clears seen so far
			VkDeviceSize offset = descriptor.kind == Descriptor::Buffer ? descriptor.buffer.offset : 0;
			VkDeviceSize size = descriptor.kind == Descriptor::Buffer ? descriptor.buffer.range & ~3ull : VK_WHOLE_SIZE;
			self->device->vk.vkCmdFillBuffer(cmd, r->buffer, offset, size, values[0]);
		}
	};

	void CommandList::Dispatch(CommandList* self, UINT x, UINT y, UINT z)
	{
		auto cmd = self->Record();
		auto& vk = self->device->vk;
		auto pso = self->pipeline;

		vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pso->pipeline);

		if (!pso->bindings.empty())
		{
			size_t total = 0;
			for (auto& b : pso->bindings)
				total += b.count;

			std::vector<VkWriteDescriptorSet> writes;
			std::vector<VkDescriptorImageInfo> images;
			std::vector<VkDescriptorBufferInfo> buffers;
			std::vector<VkBufferView> views;
			images.reserve(total);
			buffers.reserve(total);
			views.reserve(total);

			VkDescriptorSet set = self->device->AllocateSet(pso->setLayout);

			for (auto& b : pso->bindings)
			{
				if (b.sampler)
					continue;

				VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, b.binding, 0, b.count, b.type };
				write.pImageInfo = images.data() + images.size();
				write.pBufferInfo = buffers.data() + buffers.size();
				write.pTexelBufferView = views.data() + views.size();

				UINT64 table = self->tables[b.parameter];
				for (uint32_t i = 0; i < b.count; i++)
				{
					static const Descriptor empty;
					const Descriptor& d = table ? DescriptorAt(table + (b.offset + i) * sizeof(Descriptor)) : empty;

					// Never seen from XeFG, null descriptors would need stand-ins
					if (d.kind == Descriptor::Empty)
						Log::Error("D3D12OnVk: binding {} of a dispatch has no descriptor", b.binding);

					switch (b.type)
					{
					case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					{
						VkImageView view = d.imageView;
						if (d.kind == Descriptor::Image && d.key.type != b.viewType)
						{
							ViewKey key = d.key;
							key.type = b.viewType;
							if (key.type == VK_IMAGE_VIEW_TYPE_2D)
								key.layers = 1;

							view = d.resource->ImageView(key);
						}

						images.push_back({ VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL });
						break;
					}
					case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
						views.push_back(d.bufferView);
						break;
					default:
						buffers.push_back(d.buffer);
						break;
					}
				}

				writes.push_back(write);
			}

			vk.vkUpdateDescriptorSets(self->device->handle, (uint32_t)writes.size(), writes.data(), 0, nullptr);
			vk.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pso->layout, 0, 1, &set, 0, nullptr);
		}

		if (pso->rootWords)
		{
			vk.vkCmdPushConstants(cmd, pso->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pso->rootWords * 4,
				self->rootWords.data());
		}

		vk.vkCmdDispatch(cmd, x, y, z);
	}

	void CommandList::CopyTextureRegion(CommandList* self, const D3D12_TEXTURE_COPY_LOCATION* destination, UINT x, UINT y,
		UINT z, const D3D12_TEXTURE_COPY_LOCATION* source, const D3D12_BOX* box)
	{
		auto cmd = self->Record();
		auto& vk = self->device->vk;
		auto dst = As<Resource>(destination->pResource);
		auto src = As<Resource>(source->pResource);
		self->Hold(dst);
		self->Hold(src);

		VkOffset3D srcOffset = box ? VkOffset3D{ (int32_t)box->left, (int32_t)box->top, (int32_t)box->front } : VkOffset3D{};
		VkOffset3D dstOffset{ (int32_t)x, (int32_t)y, (int32_t)z };

		auto extentOf = [box](const Resource* r, uint32_t subresource)
			{
				if (box)
					return VkExtent3D{ box->right - box->left, box->bottom - box->top, box->back - box->front };

				uint32_t mip = subresource % r->mips;
				return VkExtent3D{ std::max(1u, (uint32_t)r->desc.Width >> mip), std::max(1u, r->desc.Height >> mip),
					r->desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? std::max(1u, (uint32_t)r->desc.DepthOrArraySize >> mip) : 1u };
			};

		if (source->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)
		{
			auto& footprint = source->PlacedFootprint;
			uint32_t bytes = dst->format->bytes;

			VkBufferImageCopy region{};
			region.bufferOffset = footprint.Offset + srcOffset.z * footprint.Footprint.RowPitch * footprint.Footprint.Height +
				srcOffset.y * footprint.Footprint.RowPitch + srcOffset.x * bytes;
			region.bufferRowLength = footprint.Footprint.RowPitch / bytes;
			region.bufferImageHeight = footprint.Footprint.Height;
			region.imageSubresource = dst->Subresource(destination->SubresourceIndex);
			region.imageOffset = dstOffset;
			region.imageExtent = box ? extentOf(dst, 0)
				: VkExtent3D{ footprint.Footprint.Width, footprint.Footprint.Height, footprint.Footprint.Depth };
			vk.vkCmdCopyBufferToImage(cmd, src->buffer, dst->image, Layout(dst), 1, &region);
		}
		else if (destination->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)
		{
			auto& footprint = destination->PlacedFootprint;
			uint32_t bytes = src->format->bytes;

			VkBufferImageCopy region{};
			region.bufferOffset = footprint.Offset + z * footprint.Footprint.RowPitch * footprint.Footprint.Height +
				y * footprint.Footprint.RowPitch + x * bytes;
			region.bufferRowLength = footprint.Footprint.RowPitch / bytes;
			region.bufferImageHeight = footprint.Footprint.Height;
			region.imageSubresource = src->Subresource(source->SubresourceIndex);
			region.imageOffset = srcOffset;
			region.imageExtent = extentOf(src, source->SubresourceIndex);
			vk.vkCmdCopyImageToBuffer(cmd, src->image, Layout(src), dst->buffer, 1, &region);
		}
		else
		{
			VkImageCopy region{};
			region.srcSubresource = src->Subresource(source->SubresourceIndex);
			region.srcOffset = srcOffset;
			region.dstSubresource = dst->Subresource(destination->SubresourceIndex);
			region.dstOffset = dstOffset;
			region.extent = extentOf(src, source->SubresourceIndex);
			vk.vkCmdCopyImage(cmd, src->image, Layout(src), dst->image, Layout(dst), 1, &region);
		}
	}

	void CommandList::CopyResource(CommandList* self, ID3D12Resource* destination, ID3D12Resource* source)
	{
		auto cmd = self->Record();
		auto& vk = self->device->vk;
		auto dst = As<Resource>(destination);
		auto src = As<Resource>(source);
		self->Hold(dst);
		self->Hold(src);

		if (src->buffer)
		{
			VkBufferCopy region{ 0, 0, std::min(src->desc.Width, dst->desc.Width) };
			vk.vkCmdCopyBuffer(cmd, src->buffer, dst->buffer, 1, &region);
			return;
		}

		// Depth can't be copied to color directly, it goes through a buffer
		if (Aspect(src->format->vk) != Aspect(dst->format->vk))
		{
			auto device = self->device;
			VkDeviceSize size = dst->desc.Width * dst->desc.Height * dst->format->bytes;

			if (!dst->bounce)
			{
				VkBufferCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				info.size = size;
				info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				vk.vkCreateBuffer(device->handle, &info, nullptr, &dst->bounce);

				VkMemoryRequirements reqs;
				vk.vkGetBufferMemoryRequirements(device->handle, dst->bounce, &reqs);
				uint8_t* mapped;
				dst->bounceMemory = device->Allocate(reqs.size, D3D12_HEAP_TYPE_DEFAULT, reqs.memoryTypeBits, &mapped);
				vk.vkBindBufferMemory(device->handle, dst->bounce, dst->bounceMemory, 0);
			}

			VkBufferImageCopy region{};
			region.imageSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
			region.imageExtent = { (uint32_t)dst->desc.Width, dst->desc.Height, 1 };
			vk.vkCmdCopyImageToBuffer(cmd, src->image, Layout(src), dst->bounce, 1, &region);
			FullBarrier(device, cmd);
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vk.vkCmdCopyBufferToImage(cmd, dst->bounce, dst->image, Layout(dst), 1, &region);
			return;
		}

		std::vector<VkImageCopy> regions;
		for (uint32_t mip = 0; mip < std::min(src->mips, dst->mips); mip++)
		{
			VkImageCopy region{};
			region.srcSubresource = { Aspect(src->format->vk), mip, 0, std::min(src->layers, dst->layers) };
			region.dstSubresource = { Aspect(dst->format->vk), mip, 0, region.srcSubresource.layerCount };
			region.extent = { std::max(1u, (uint32_t)src->desc.Width >> mip), std::max(1u, src->desc.Height >> mip),
				src->desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? std::max(1u, (uint32_t)src->desc.DepthOrArraySize >> mip) : 1u };
			regions.push_back(region);
		}

		vk.vkCmdCopyImage(cmd, src->image, Layout(src), dst->image, Layout(dst), (uint32_t)regions.size(), regions.data());
	}

	void** CommandList::VTable()
	{
		static void** vtable = BuildVTable("ID3D12GraphicsCommandList", D3D12Vtables::GraphicsCommandListMethods,
			CommonMethods,
			{ METHOD(CommandList, Close), METHOD(CommandList, Reset), METHOD(CommandList, SetPipelineState),
				METHOD(CommandList, SetDescriptorHeaps), METHOD(CommandList, SetComputeRootSignature),
				METHOD(CommandList, SetComputeRootDescriptorTable), METHOD(CommandList, SetComputeRoot32BitConstants),
				METHOD(CommandList, Dispatch), METHOD(CommandList, ResourceBarrier), METHOD(CommandList, CopyBufferRegion),
				METHOD(CommandList, CopyTextureRegion), METHOD(CommandList, CopyResource),
				METHOD(CommandList, ClearUnorderedAccessViewUint) });
		return vtable;
	}

	// Queues
	struct CommandQueue : Object
	{
		static inline const IID Iids[] = { __uuidof(IUnknown), __uuidof(ID3D12Object), __uuidof(ID3D12DeviceChild),
			__uuidof(ID3D12Pageable), __uuidof(ID3D12CommandQueue) };
		static void** VTable();

		// The lists already recorded into the game's command buffer, which runs after anything recorded before it
		static void STDMETHODCALLTYPE ExecuteCommandLists(CommandQueue*, UINT, ID3D12CommandList* const*) {}

		static HRESULT STDMETHODCALLTYPE Signal(CommandQueue*, ID3D12Fence* fence, UINT64 value)
		{
			return Fence::Signal(As<Fence>(fence), value);
		}
	};

	void** CommandQueue::VTable()
	{
		static void** vtable = BuildVTable("ID3D12CommandQueue", D3D12Vtables::CommandQueueMethods, CommonMethods,
			{ METHOD(CommandQueue, ExecuteCommandLists), METHOD(CommandQueue, Signal) });
		return vtable;
	}

	// The device's methods
	struct DeviceMethods
	{
		static HRESULT STDMETHODCALLTYPE CreateCommandQueue(Device* self, const D3D12_COMMAND_QUEUE_DESC*, REFIID riid,
			void** out)
		{
			return Return(New<CommandQueue>(self), riid, out);
		}

		static HRESULT STDMETHODCALLTYPE CreateCommandAllocator(Device* self, D3D12_COMMAND_LIST_TYPE, REFIID riid, void** out)
		{
			return Return(New<CommandAllocator>(self), riid, out);
		}

		static HRESULT STDMETHODCALLTYPE CreateCommandList(Device* self, UINT, D3D12_COMMAND_LIST_TYPE,
			ID3D12CommandAllocator* allocator, ID3D12PipelineState* state, REFIID riid, void** out)
		{
			auto list = New<CommandList>(self);
			list->allocator = As<CommandAllocator>(allocator);
			Object::AddRef(list->allocator);
			list->Begin();
			list->pipeline = As<PipelineState>(state);
			return Return(list, riid, out);
		}

		static HRESULT STDMETHODCALLTYPE CheckFeatureSupport(Device*, D3D12_FEATURE feature, void* data, UINT size)
		{
			switch (feature)
			{
			case D3D12_FEATURE_D3D12_OPTIONS:
			{
				if (size != sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS))
					return E_INVALIDARG;

				auto options = static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS*>(data);
				*options = {};
				options->TypedUAVLoadAdditionalFormats = TRUE;
				options->ResourceBindingTier = D3D12_RESOURCE_BINDING_TIER_3;
				options->ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_2;
				return S_OK;
			}
			case D3D12_FEATURE_SHADER_MODEL:
			{
				if (size != sizeof(D3D12_FEATURE_DATA_SHADER_MODEL))
					return E_INVALIDARG;

				auto model = static_cast<D3D12_FEATURE_DATA_SHADER_MODEL*>(data);
				model->HighestShaderModel = std::min(model->HighestShaderModel, D3D_SHADER_MODEL_6_6);
				return S_OK;
			}
			default:
				Log::Debug("D3D12OnVk: CheckFeatureSupport({}) isn't supported", (int)feature);
				return E_INVALIDARG;
			}
		}

		static HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(Device* self, const D3D12_DESCRIPTOR_HEAP_DESC* desc,
			REFIID riid, void** out)
		{
			auto heap = New<DescriptorHeap>(self);
			heap->descriptors.resize(desc->NumDescriptors);
			return Return(heap, riid, out);
		}

		static UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(Device*, D3D12_DESCRIPTOR_HEAP_TYPE)
		{
			return sizeof(Descriptor);
		}

		static HRESULT STDMETHODCALLTYPE CreateRootSignature(Device* self, UINT, const void* blob, SIZE_T size, REFIID riid,
			void** out)
		{
			Microsoft::WRL::ComPtr<ID3D12VersionedRootSignatureDeserializer> deserializer;
			const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* versioned = nullptr;

			if (FAILED(D3D12CreateVersionedRootSignatureDeserializer(blob, size, IID_PPV_ARGS(&deserializer))) ||
				FAILED(deserializer->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &versioned)))
			{
				Log::Error("D3D12OnVk: can't read a root signature");
				return E_INVALIDARG;
			}

			auto& desc = versioned->Desc_1_1;
			auto rs = New<RootSignature>(self);

			for (UINT p = 0; p < desc.NumParameters; p++)
			{
				auto& in = desc.pParameters[p];
				RootParameter parameter{ in.ParameterType };

				if (in.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
				{
					uint32_t offset = 0;
					for (UINT r = 0; r < in.DescriptorTable.NumDescriptorRanges; r++)
					{
						auto& range = in.DescriptorTable.pDescriptorRanges[r];
						if (range.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
							offset = range.OffsetInDescriptorsFromTableStart;

						parameter.ranges.push_back({ range.RangeType, range.NumDescriptors, range.BaseShaderRegister,
							range.RegisterSpace, offset });
						offset += range.NumDescriptors;
					}
				}
				else if (in.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
				{
					parameter.space = in.Constants.RegisterSpace;
					parameter.reg = in.Constants.ShaderRegister;
					parameter.words = in.Constants.Num32BitValues;
					parameter.wordOffset = rs->rootWords;
					rs->rootWords += parameter.words;
				}
				else
				{
					Log::Error("D3D12OnVk: root descriptors aren't supported");
				}

				rs->parameters.push_back(std::move(parameter));
			}

			for (UINT s = 0; s < desc.NumStaticSamplers; s++)
			{
				auto& sampler = desc.pStaticSamplers[s];
				rs->samplers.push_back({ sampler.RegisterSpace, sampler.ShaderRegister, CreateSampler(self, sampler) });
			}

			return Return(rs, riid, out);
		}

		static HRESULT STDMETHODCALLTYPE CreateComputePipelineState(Device* self,
			const D3D12_COMPUTE_PIPELINE_STATE_DESC* desc, REFIID riid, void** out)
		{
			auto& vk = self->vk;
			Translation translation{ As<RootSignature>(desc->pRootSignature) };
			std::vector<uint32_t> spirv;

			if (!Translate(self, desc->CS, translation, spirv))
				return E_INVALIDARG;

			auto pso = New<PipelineState>(self);
			pso->bindings = std::move(translation.bindings);
			pso->rootWords = translation.rootSignature->rootWords;

			std::vector<VkDescriptorSetLayoutBinding> bindings;
			for (auto& b : pso->bindings)
				bindings.push_back({ b.binding, b.type, b.count, VK_SHADER_STAGE_COMPUTE_BIT, b.sampler ? &b.sampler : nullptr });

			VkDescriptorSetLayoutCreateInfo setInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			setInfo.bindingCount = (uint32_t)bindings.size();
			setInfo.pBindings = bindings.data();
			vk.vkCreateDescriptorSetLayout(self->handle, &setInfo, nullptr, &pso->setLayout);

			VkPushConstantRange pushConstants{ VK_SHADER_STAGE_COMPUTE_BIT, 0, pso->rootWords * 4 };
			VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
			layoutInfo.setLayoutCount = 1;
			layoutInfo.pSetLayouts = &pso->setLayout;
			layoutInfo.pushConstantRangeCount = pso->rootWords ? 1 : 0;
			layoutInfo.pPushConstantRanges = &pushConstants;
			vk.vkCreatePipelineLayout(self->handle, &layoutInfo, nullptr, &pso->layout);

			VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			moduleInfo.codeSize = spirv.size() * 4;
			moduleInfo.pCode = spirv.data();
			VkShaderModule module = VK_NULL_HANDLE;
			vk.vkCreateShaderModule(self->handle, &moduleInfo, nullptr, &module);

			VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
			pipelineInfo.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
				VK_SHADER_STAGE_COMPUTE_BIT, module, "main" }; // dxil-spirv's entry point, whatever the DXIL called it
			pipelineInfo.layout = pso->layout;
			VkResult result = vk.vkCreateComputePipelines(self->handle, self->pipelineCache, 1, &pipelineInfo, nullptr,
				&pso->pipeline);
			vk.vkDestroyShaderModule(self->handle, module, nullptr);

			if (result != VK_SUCCESS)
			{
				Log::Error("D3D12OnVk: vkCreateComputePipelines error {}", (int)result);
				Object::Release(pso);
				return E_FAIL;
			}

			return Return(pso, riid, out);
		}

		static void STDMETHODCALLTYPE CreateConstantBufferView(Device* self, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,
			D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			Descriptor& d = DescriptorAt(handle.ptr);
			d = {};

			uint64_t offset = 0;
			auto resource = desc ? self->FindAddress(desc->BufferLocation, offset) : nullptr;
			if (!resource)
				return;

			d.kind = Descriptor::Buffer;
			d.resource = resource;
			d.buffer = { resource->buffer, offset, desc->SizeInBytes };
		}

		static void STDMETHODCALLTYPE CreateShaderResourceView(Device*, ID3D12Resource* resource,
			const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			Descriptor& d = DescriptorAt(handle.ptr);
			d = {};

			auto r = As<Resource>(resource);
			if (!r)
				return;

			d.resource = r;

			if (r->buffer)
			{
				auto& buffer = desc->Buffer;
				BufferDescriptor(d, r, desc->Format, buffer.FirstElement, buffer.NumElements, buffer.StructureByteStride,
					buffer.Flags & D3D12_BUFFER_SRV_FLAG_RAW);
				return;
			}

			ViewKey key{ VK_IMAGE_VIEW_TYPE_2D, r->format->vk, 0, r->mips, 0, 1 };
			UINT mapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			if (desc)
			{
				mapping = desc->Shader4ComponentMapping;
				if (desc->Format != DXGI_FORMAT_UNKNOWN)
				{
					auto format = FindFormat(desc->Format);
					if (!format)
						return;

					key.format = format->vk;
				}

				switch (desc->ViewDimension)
				{
				case D3D12_SRV_DIMENSION_TEXTURE2D:
					key.baseMip = desc->Texture2D.MostDetailedMip;
					key.mips = desc->Texture2D.MipLevels;
					break;
				case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
					key.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
					key.baseMip = desc->Texture2DArray.MostDetailedMip;
					key.mips = desc->Texture2DArray.MipLevels;
					key.baseLayer = desc->Texture2DArray.FirstArraySlice;
					key.layers = desc->Texture2DArray.ArraySize;
					break;
				case D3D12_SRV_DIMENSION_TEXTURE3D:
					key.type = VK_IMAGE_VIEW_TYPE_3D;
					key.baseMip = desc->Texture3D.MostDetailedMip;
					key.mips = desc->Texture3D.MipLevels;
					break;
				default:
					Log::Error("D3D12OnVk: SRV dimension {} isn't supported", (int)desc->ViewDimension);
					return;
				}
			}
			else if (r->desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
			{
				key.type = VK_IMAGE_VIEW_TYPE_3D;
			}

			if (key.mips == UINT_MAX)
				key.mips = r->mips - key.baseMip;

			key.components = { Swizzle(mapping, 0), Swizzle(mapping, 1), Swizzle(mapping, 2), Swizzle(mapping, 3) };
			d.kind = Descriptor::Image;
			d.key = key;
			d.imageView = r->ImageView(key);
		}

		static void STDMETHODCALLTYPE CreateUnorderedAccessView(Device*, ID3D12Resource* resource, ID3D12Resource*,
			const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			Descriptor& d = DescriptorAt(handle.ptr);
			d = {};

			auto r = As<Resource>(resource);
			if (!r)
				return;

			d.resource = r;

			if (r->buffer)
			{
				auto& buffer = desc->Buffer;
				BufferDescriptor(d, r, desc->Format, buffer.FirstElement, buffer.NumElements, buffer.StructureByteStride,
					buffer.Flags & D3D12_BUFFER_UAV_FLAG_RAW);
				return;
			}

			ViewKey key{ VK_IMAGE_VIEW_TYPE_2D, r->format->vk, 0, 1, 0, 1 };

			if (desc)
			{
				if (desc->Format != DXGI_FORMAT_UNKNOWN)
				{
					auto format = FindFormat(desc->Format);
					if (!format)
						return;

					key.format = format->vk;
				}

				switch (desc->ViewDimension)
				{
				case D3D12_UAV_DIMENSION_TEXTURE2D:
					key.baseMip = desc->Texture2D.MipSlice;
					break;
				case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
					key.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
					key.baseMip = desc->Texture2DArray.MipSlice;
					key.baseLayer = desc->Texture2DArray.FirstArraySlice;
					key.layers = desc->Texture2DArray.ArraySize;
					break;
				case D3D12_UAV_DIMENSION_TEXTURE3D:
					key.type = VK_IMAGE_VIEW_TYPE_3D;
					key.baseMip = desc->Texture3D.MipSlice;
					break;
				default:
					Log::Error("D3D12OnVk: UAV dimension {} isn't supported", (int)desc->ViewDimension);
					return;
				}
			}

			key.components = {};
			d.kind = Descriptor::Image;
			d.key = key;
			d.imageView = r->ImageView(key);
		}

		static D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE GetResourceAllocationInfo(Device* self,
			D3D12_RESOURCE_ALLOCATION_INFO* info, UINT, UINT count, const D3D12_RESOURCE_DESC* descs)
		{
			*info = { 0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT };

			for (UINT i = 0; i < count; i++)
			{
				VkMemoryRequirements2 reqs{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
				auto& vk = self->vk;
				VkResult result = VK_SUCCESS;

				// Devices created below Vulkan 1.3 can't query without an object, so one is created and destroyed
				if (descs[i].Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
				{
					auto create = BufferCreateInfo(descs[i]);
					VkDeviceBufferMemoryRequirements query{ VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS, nullptr, &create };
					VkBuffer buffer;
					if (vk.vkGetDeviceBufferMemoryRequirements)
						vk.vkGetDeviceBufferMemoryRequirements(self->handle, &query, &reqs);
					else if ((result = vk.vkCreateBuffer(self->handle, &create, nullptr, &buffer)) == VK_SUCCESS)
					{
						vk.vkGetBufferMemoryRequirements(self->handle, buffer, &reqs.memoryRequirements);
						vk.vkDestroyBuffer(self->handle, buffer, nullptr);
					}
				}
				else
				{
					auto format = ResourceFormat(descs[i]);
					if (!format)
					{
						*info = { UINT64_MAX, 0 };
						return info;
					}

					auto create = ImageCreateInfo(descs[i], format);
					VkDeviceImageMemoryRequirements query{ VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS, nullptr, &create };
					VkImage image;
					if (vk.vkGetDeviceImageMemoryRequirements)
						vk.vkGetDeviceImageMemoryRequirements(self->handle, &query, &reqs);
					else if ((result = vk.vkCreateImage(self->handle, &create, nullptr, &image)) == VK_SUCCESS)
					{
						vk.vkGetImageMemoryRequirements(self->handle, image, &reqs.memoryRequirements);
						vk.vkDestroyImage(self->handle, image, nullptr);
					}
				}

				if (result != VK_SUCCESS)
				{
					*info = { UINT64_MAX, 0 };
					return info;
				}

				UINT64 alignment = std::max<UINT64>(reqs.memoryRequirements.alignment, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
				info->Alignment = std::max(info->Alignment, alignment);
				info->SizeInBytes = (info->SizeInBytes + alignment - 1) / alignment * alignment +
					(reqs.memoryRequirements.size + alignment - 1) / alignment * alignment;
			}

			return info;
		}

		static HRESULT STDMETHODCALLTYPE CreateCommittedResource(Device* self, const D3D12_HEAP_PROPERTIES* properties,
			D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID riid,
			void** out)
		{
			VkMemoryRequirements reqs;
			auto type = HeapType(*properties);
			auto resource = CreateResource(self, *desc, reqs);
			if (!resource)
				return E_INVALIDARG;

			resource->memory = self->Allocate(reqs.size, type, reqs.memoryTypeBits, &resource->mapped);
			if (!resource->memory || !BindMemory(resource, resource->memory, 0))
			{
				Object::Release(resource);
				return E_OUTOFMEMORY;
			}

			return Return(resource, riid, out);
		}

		static HRESULT STDMETHODCALLTYPE CreateHeap(Device* self, const D3D12_HEAP_DESC* desc, REFIID riid, void** out)
		{
			auto heap = New<Heap>(self);
			heap->desc = *desc;
			heap->memory = self->Allocate(desc->SizeInBytes, HeapType(desc->Properties), ~0u, &heap->mapped);
			if (!heap->memory)
			{
				Object::Release(heap);
				return E_OUTOFMEMORY;
			}

			return Return(heap, riid, out);
		}

		static HRESULT STDMETHODCALLTYPE CreatePlacedResource(Device* self, ID3D12Heap* heap, UINT64 offset,
			const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID riid, void** out)
		{
			auto h = As<Heap>(heap);
			VkMemoryRequirements reqs;
			auto resource = CreateResource(self, *desc, reqs);
			if (!resource)
				return E_INVALIDARG;

			Object::AddRef(h);
			resource->heap = h;
			resource->mapped = h->mapped ? h->mapped + offset : nullptr;

			uint32_t type = self->MemoryType(HeapType(h->desc.Properties), ~0u);
			if (!(reqs.memoryTypeBits & (1u << type)) || offset % reqs.alignment || !BindMemory(resource, h->memory, offset))
			{
				Log::Error("D3D12OnVk: can't place a resource at {} of a heap", offset);
				Object::Release(resource);
				return E_INVALIDARG;
			}

			return Return(resource, riid, out);
		}

		static HRESULT STDMETHODCALLTYPE CreateFence(Device* self, UINT64 value, D3D12_FENCE_FLAGS, REFIID riid, void** out)
		{
			auto fence = New<Fence>(self);
			fence->completed = value;
			return Return(fence, riid, out);
		}

		static void STDMETHODCALLTYPE GetCopyableFootprints(Device*, const D3D12_RESOURCE_DESC* desc, UINT first, UINT count,
			UINT64 baseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts, UINT* rowCounts, UINT64* rowSizes, UINT64* total)
		{
			const Format* format = desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? nullptr : ResourceFormat(*desc);
			uint32_t mips = desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? 1 : MipCount(*desc);
			UINT64 offset = baseOffset;

			for (UINT i = 0; i < count; i++)
			{
				uint32_t mip = (first + i) % mips;
				uint32_t width = std::max(1u, (uint32_t)desc->Width >> mip);
				uint32_t height = std::max(1u, desc->Height >> mip);
				uint32_t depth = desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? std::max(1u, (uint32_t)desc->DepthOrArraySize >> mip) : 1u;
				UINT64 rowSize = format ? (UINT64)width * format->bytes : desc->Width;
				UINT rowPitch = (UINT)((rowSize + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(UINT64)(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1));

				offset = (offset + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) & ~(UINT64)(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
				if (layouts)
					layouts[i] = { offset, { format ? desc->Format : DXGI_FORMAT_UNKNOWN, width, height, depth, rowPitch } };
				if (rowCounts)
					rowCounts[i] = height;
				if (rowSizes)
					rowSizes[i] = rowSize;

				offset += (UINT64)rowPitch * (height * depth - 1) + rowSize;
			}

			if (total)
				*total = offset - baseOffset;
		}

		static LUID* STDMETHODCALLTYPE GetAdapterLuid(Device* self, LUID* luid)
		{
			*luid = self->luid;
			return luid;
		}

		static HRESULT STDMETHODCALLTYPE SetResidencyPriority(Device*, UINT, ID3D12Pageable* const*,
			const D3D12_RESIDENCY_PRIORITY*) { return S_OK; }
	};

	void** Device::VTable()
	{
		static void** vtable = BuildVTable("ID3D12Device", D3D12Vtables::DeviceMethods, CommonMethods,
			{ METHOD(DeviceMethods, CreateCommandQueue), METHOD(DeviceMethods, CreateCommandAllocator),
				METHOD(DeviceMethods, CreateCommandList), METHOD(DeviceMethods, CheckFeatureSupport),
				METHOD(DeviceMethods, CreateDescriptorHeap), METHOD(DeviceMethods, GetDescriptorHandleIncrementSize),
				METHOD(DeviceMethods, CreateRootSignature), METHOD(DeviceMethods, CreateComputePipelineState),
				METHOD(DeviceMethods, CreateConstantBufferView), METHOD(DeviceMethods, CreateShaderResourceView),
				METHOD(DeviceMethods, CreateUnorderedAccessView), METHOD(DeviceMethods, GetResourceAllocationInfo),
				METHOD(DeviceMethods, CreateCommittedResource), METHOD(DeviceMethods, CreateHeap),
				METHOD(DeviceMethods, CreatePlacedResource), METHOD(DeviceMethods, CreateFence),
				METHOD(DeviceMethods, GetCopyableFootprints), METHOD(DeviceMethods, GetAdapterLuid),
				METHOD(DeviceMethods, SetResidencyPriority) });
		return vtable;
	}
} // namespace

ID3D12Device* D3D12OnVk::CreateDevice(const DeviceInfo& info)
{
	auto device = New<Device>(nullptr);
	auto& vk = device->vk;
	vk.Load(info.getInstanceProcAddr, info.instance, info.device, info.getDeviceProcAddr);
	device->physicalDevice = info.physicalDevice;
	device->handle = info.device;

	if (!vk.vkCreateComputePipelines)
	{
		Log::Error("D3D12OnVk: can't load the device's Vulkan functions");
		Object::Release(device);
		return nullptr;
	}

	vk.vkGetPhysicalDeviceMemoryProperties(info.physicalDevice, &device->memoryProperties);

	VkPhysicalDeviceIDProperties id{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
	VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &id };
	vk.vkGetPhysicalDeviceProperties2(info.physicalDevice, &properties);
	device->properties = properties.properties;
	if (id.deviceLUIDValid)
		memcpy(&device->luid, id.deviceLUID, sizeof(LUID));

	// vkGetDeviceProcAddr returns no 1.3 core functions for a device created below 1.3, which then can't use the
	// feature unless it enabled VK_KHR_shader_integer_dot_product
	// ponytail: assumes a 1.3 device enabled the feature when the GPU has it, and ignores the extension on older ones
	VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &features13 };
	vk.vkGetPhysicalDeviceFeatures2(info.physicalDevice, &features);
	device->integerDotProduct = features13.shaderIntegerDotProduct && vk.vkGetDeviceImageMemoryRequirements;

	VkPipelineCacheCreateInfo cacheInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	vk.vkCreatePipelineCache(info.device, &cacheInfo, nullptr, &device->pipelineCache);

	Log::Info("D3D12OnVk: device on {}, integer dot product: {}", device->properties.deviceName, device->integerDotProduct);
	return reinterpret_cast<ID3D12Device*>(device);
}

ID3D12GraphicsCommandList* D3D12OnVk::WrapCommandBuffer(ID3D12Device* device, VkCommandBuffer commandBuffer)
{
	auto list = New<CommandList>(As<Device>(device));
	list->cmd = commandBuffer;
	list->device->NextPoolSlot();
	list->device->current = commandBuffer;
	return reinterpret_cast<ID3D12GraphicsCommandList*>(list);
}

bool D3D12OnVk::IsDevice(ID3D12Device* device) { return *reinterpret_cast<void***>(device) == Device::VTable(); }

void D3D12OnVk::ReleaseHeld(ID3D12Device* device)
{
	for (uint32_t i = 0; i < Device::PoolRing; i++)
		As<Device>(device)->NextPoolSlot();
}

ID3D12Resource* D3D12OnVk::WrapImage(ID3D12Device* device, VkImage image, VkFormat format, uint32_t width,
	uint32_t height, bool unorderedAccess)
{
	auto f = FindFormat(format);
	if (!f)
	{
		Log::Error("D3D12OnVk: unsupported Vulkan format {}", (int)format);
		return nullptr;
	}

	auto resource = New<Resource>(As<Device>(device));
	resource->external = true;
	resource->image = image;
	resource->format = f;
	resource->desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, width, height, 1, 1, f->dxgi, { 1, 0 },
		D3D12_TEXTURE_LAYOUT_UNKNOWN };

	// Only the depth of a depth/stencil image is ever copied, so it passes for a plain depth image
	if (format == VK_FORMAT_D32_SFLOAT_S8_UINT)
		resource->desc.Format = DXGI_FORMAT_D32_FLOAT;

	if (unorderedAccess)
		resource->desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if (IsDepth(format))
		resource->desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	return reinterpret_cast<ID3D12Resource*>(resource);
}
