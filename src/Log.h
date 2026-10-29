#pragma once

#include <format>
#include <string_view>

// Appends to dlssg_to_xefg_intel_is_better.log next to the dll and to OutputDebugString
namespace Log
{
	void Write(std::string_view level, std::string_view message);

	template <class... Args> void Debug(std::format_string<Args...> format, Args&&... args)
	{
		Write("debug", std::format(format, std::forward<Args>(args)...));
	}

	template <class... Args> void Info(std::format_string<Args...> format, Args&&... args)
	{
		Write("info", std::format(format, std::forward<Args>(args)...));
	}

	template <class... Args> void Warn(std::format_string<Args...> format, Args&&... args)
	{
		Write("warn", std::format(format, std::forward<Args>(args)...));
	}

	template <class... Args> void Error(std::format_string<Args...> format, Args&&... args)
	{
		Write("error", std::format(format, std::forward<Args>(args)...));
	}
} // namespace Log
