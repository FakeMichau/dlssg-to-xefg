# dlssg-to-xefg

Replaces DLSS Frame Generation with Intel XeSS Frame Generation (XeFG), up to 6x. Inspired by Nukem9's
[dlssg-to-fsr3](https://github.com/Nukem9/dlssg-to-fsr3).

`libxess_fg.dll` only exports XeFG's proxy swap chain. Internally that swap chain drives an unexported
`xefgD3D12CreateContext / Init / Execute` API, which this dll locates and calls directly with the DLSSG inputs. That's
what makes it work as an `nvngx_dlssg` replacement, and it lifts XeFG's 4x cap and its non-Intel 2x clamp without
patching any code.

DX12 only, XeFG has no Vulkan API.

## Usage

`dlssg_to_xefg_intel_is_better.dll` exports the same `NVSDK_NGX_D3D12_*` functions as `nvngx_dlssg.dll`, for a loader
that calls those directly. Put `libxess_fg.dll` (XeFG 1.3.1 or 1.3.4) next to it, unless the game loads a compatible one
itself.

## Which libxess_fg.dll is used

1. Every `libxess_fg.dll` already loaded in the process, the first compatible one wins
2. The `libxess_fg.dll` next to this dll

Compatible means a build with known function addresses (XeFG 1.3.1 from XeSS SDK 3.0.2, XeFG 1.3.4), or any XeFG
1.3.x where the functions can be found by scanning and the settings match one of the known layouts. The struct layouts
were only verified for 1.3.1 and 1.3.4.

`dlssg_to_xefg_intel_is_better.log` next to the dll says which one was picked.

## Build

Visual Studio 2026 (v145 toolset), open `dlssg-to-xefg.sln` and build Release x64. The output lands in `bin\Release`.

## License

GPLv3, see [LICENSE](LICENSE).
