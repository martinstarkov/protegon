#pragma once

#include <string_view>

// Function signature macro: PTGN_FUNCTION_FULL_SIGNATURE
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || \
	(defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define PTGN_FUNCTION_FULL_SIGNATURE __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define PTGN_FUNCTION_FULL_SIGNATURE __PRETTY_FUNCTION__
#elif (defined(__FUNCSIG__) || (_MSC_VER))
#define PTGN_FUNCTION_FULL_SIGNATURE __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || \
	(defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define PTGN_FUNCTION_FULL_SIGNATURE __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define PTGN_FUNCTION_FULL_SIGNATURE __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define PTGN_FUNCTION_FULL_SIGNATURE __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define PTGN_FUNCTION_FULL_SIGNATURE __func__
#else
#define PTGN_FUNCTION_FULL_SIGNATURE "PTGN_FUNCTION_FULL_SIGNATURE unknown!"
#endif

namespace ptgn::impl {

// Returns the name of the function with the return type and function parameter list trimmed away.
[[nodiscard]] constexpr std::string_view TrimFunctionSignature(std::string_view signature) {
	const std::string_view begin{ "__cdecl" }; // trim return type
	const std::string_view end{ "(" };		   // trim function parameter list

	// Trim the return type
	if (const auto position = signature.find(begin); position != std::string_view::npos) {
		signature = signature.substr(position + begin.length());
	}

	// Trim the function parameters
	if (const auto position = signature.find(end); position != std::string_view::npos) {
		signature = signature.substr(0, position);
	}

	return signature;
}

} // namespace ptgn::impl

#define PTGN_FUNCTION_NAME() ::ptgn::impl::TrimFunctionSignature(PTGN_FUNCTION_FULL_SIGNATURE)