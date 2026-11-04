# dlssg-to-xefg

Replaces DLSS Frame Generation with Intel XeSS Frame Generation (XeFG), up to 6x. Inspired by Nukem9's
[dlssg-to-fsr3](https://github.com/Nukem9/dlssg-to-fsr3).

`libxess_fg.dll` only exports XeFG's proxy swap chain. Internally that swap chain drives an unexported
`xefgD3D12CreateContext / Init / Execute` API, which this dll locates and calls directly with the DLSSG inputs. That's
what makes it work as an `nvngx_dlssg` replacement, and it lifts XeFG's 4x cap and its non-Intel 2x clamp without
patching any code.

Intel GPUs with XMX get XeFG's XMX network, decided by XeFG's own check. Every other GPU gets the default one.

XeFG has no Vulkan API out of the box. On Vulkan XeFG runs on a small D3D12 layer on top of the game's Vulkan device, 
which translates XeFG's compute shaders from DXIL to SPIR-V with
[dxil-spirv](https://github.com/HansKristian-Work/dxil-spirv) when they're created. Nothing modified is shipped.
The XMX network isn't available there.

## Usage

`dlssg_to_xefg_intel_is_better.dll` exports the same `NVSDK_NGX_D3D12_*` and `NVSDK_NGX_VULKAN_*` functions as
`nvngx_dlssg.dll`, for a loader that calls those directly. Put `libxess_fg.dll` (XeFG 1.3.1 or 1.3.4) next to it, unless
the game loads a compatible one itself.

## Which libxess_fg.dll is used

1. Every `libxess_fg.dll` already loaded in the process, the first compatible one wins
2. The `libxess_fg.dll` next to this dll
3. When this dll is in `<folder with dlls>\custom\<any name>\dlssg`, the `libxess_fg.dll` in `<folder with dlls>`

Compatible means a build with known function addresses (XeFG 1.3.1 from XeSS SDK 3.0.2, XeFG 1.3.4), or any XeFG
1.3.x where the functions can be found by scanning and the settings match one of the known layouts. The struct layouts
were only verified for 1.3.1 and 1.3.4.

`dlssg_to_xefg_intel_is_better.log` next to the dll says which one was picked.

## OptiScaler menu

Loaded by OptiScaler from `custom\<name>\dlssg`, the dll adds a **dlssg-to-xefg** tab under PLUGINS with XeFG's status
and its settings, kept in `[Plugin.<name>]` of `OptiScaler.ini`.

## Build

Visual Studio 2026 (v145 toolset) and CMake on PATH. Clone with `--recursive` (or run
`git submodule update --init --recursive`), open `dlssg-to-xefg.sln` and build Release x64. The output lands in
`bin\Release`.

## License

GPLv3, see [LICENSE](LICENSE).
