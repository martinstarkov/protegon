#include "platform/executable.h"

#include <filesystem>
#include <string>
#include <vector>

#include "core/assert.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <unistd.h>
#endif

namespace ptgn::impl {

path GetExecutableDirectory() {
#if defined(__EMSCRIPTEN__)
	return path{ "/" };
#elif defined(_WIN32)
	std::vector<wchar_t> buffer(1024);

	for (;;) {
		const DWORD length{ GetModuleFileNameW(
			nullptr,
			buffer.data(),
			static_cast<DWORD>(buffer.size())
		) };

		PTGN_ASSERT(
			length != 0,
			"Failed to determine executable path"
		);

		if (length < buffer.size()) {
			return path{
				std::wstring{ buffer.data(), length }
			}.parent_path();
		}

		buffer.resize(buffer.size() * 2);
	}
#elif defined(__APPLE__)
	std::uint32_t size{ 0 };
	_NSGetExecutablePath(nullptr, &size);

	PTGN_ASSERT(
		size > 0,
		"Failed to determine executable path size"
	);

	std::vector<char> buffer(size);
	const int result{
		_NSGetExecutablePath(buffer.data(), &size)
	};

	PTGN_ASSERT(
		result == 0,
		"Failed to determine executable path"
	);

	return std::filesystem::absolute(
		path{ buffer.data() }
	).lexically_normal().parent_path();
#elif defined(__linux__)
	std::vector<char> buffer(1024);

	for (;;) {
		const auto length{
			::readlink(
				"/proc/self/exe",
				buffer.data(),
				buffer.size()
			)
		};

		PTGN_ASSERT(
			length >= 0,
			"Failed to determine executable path"
		);

		if (static_cast<std::size_t>(length) < buffer.size()) {
			return path{
				std::string{
					buffer.data(),
					static_cast<std::size_t>(length)
				}
			}.parent_path();
		}

		buffer.resize(buffer.size() * 2);
	}
#else
#error Unsupported platform for GetExecutableDirectory()
#endif
}

} // namespace ptgn::impl
