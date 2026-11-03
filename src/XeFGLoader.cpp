#include "XeFG.h"
#include "Log.h"
#include "Util.h"

#include <Psapi.h>

#include <atomic>
#include <vector>

namespace
{
	constexpr wchar_t XeFGDllName[] = L"libxess_fg.dll";

	// Every libxess_fg.dll in the process, the game and Streamline plugins can bring their own copies
	std::vector<HMODULE> LoadedXeFGModules()
	{
		std::vector<HMODULE> modules(1024);
		DWORD needed = 0;

		if (!EnumProcessModules(GetCurrentProcess(), modules.data(), (DWORD)(modules.size() * sizeof(HMODULE)), &needed))
			return {};

		modules.resize(std::min<size_t>(modules.size(), needed / sizeof(HMODULE)));
		std::erase_if(modules,
			[](HMODULE m) { return _wcsicmp(ModulePath(m).filename().c_str(), XeFGDllName) != 0; });

		return modules;
	}

	bool TryModule(HMODULE module)
	{
		Log::Info("Trying {}", ModulePath(module).string());

		if (XeFG::Resolve(module))
			return true;

		Log::Warn("Incompatible libxess_fg.dll");
		return false;
	}

	// Unloads it again when it's incompatible
	bool TryPath(const std::filesystem::path& path)
	{
		HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

		if (!module)
		{
			Log::Warn("Can't load {}", path.string());
			return false;
		}

		if (TryModule(module))
			return true;

		FreeLibrary(module);
		return false;
	}

	// The libxess_fg.dll in <folder with dlls> when this dll is in <folder with dlls>\custom\<any name>\dlssg, empty
	// otherwise
	std::filesystem::path CustomFolderXeFGPath()
	{
		auto dlssg = ThisModulePath().parent_path();
		auto custom = dlssg.parent_path().parent_path();

		if (_wcsicmp(dlssg.filename().c_str(), L"dlssg") != 0 || _wcsicmp(custom.filename().c_str(), L"custom") != 0)
			return {};

		return custom.parent_path() / XeFGDllName;
	}

	bool Search()
	{
		for (HMODULE module : LoadedXeFGModules())
		{
			if (!TryModule(module))
				continue;

			// Keeps it loaded even if its owner unloads it
			HMODULE pinned = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(module), &pinned);

			return true;
		}

		if (TryPath(ThisModulePath().replace_filename(XeFGDllName)))
			return true;

		if (auto path = CustomFolderXeFGPath(); !path.empty() && TryPath(path))
			return true;

		Log::Error("No compatible libxess_fg.dll found");
		return false;
	}
} // namespace

static std::atomic<XeFG::LoadState> state = XeFG::LoadState::NotTried;

bool XeFG::Load()
{
	static const bool loaded = []
		{
			const bool found = Search();
			state = found ? LoadState::Loaded : LoadState::Failed;
			return found;
		}();

	return loaded;
}

XeFG::LoadState XeFG::State() { return state; }
