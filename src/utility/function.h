#pragma once

#include <string>
#include <string_view>

// Function signature macro: PTGN_FULL_FUNCTION_SIGNATURE
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || \
	(defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define PTGN_FULL_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define PTGN_FULL_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#elif (defined(__FUNCSIG__) || (_MSC_VER))
#define PTGN_FULL_FUNCTION_SIGNATURE __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || \
	(defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define PTGN_FULL_FUNCTION_SIGNATURE __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define PTGN_FULL_FUNCTION_SIGNATURE __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define PTGN_FULL_FUNCTION_SIGNATURE __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define PTGN_FULL_FUNCTION_SIGNATURE __func__
#else
#define PTGN_FULL_FUNCTION_SIGNATURE "PTGN_FULL_FUNCTION_SIGNATURE unknown!"
#endif

namespace ptgn::impl {

// Returns the name of the function with the return type and function parameter list trimmed away.
[[nodiscard]] std::string TrimFunctionSignature(std::string_view signature);

} // namespace ptgn::impl

#define PTGN_FUNCTION_NAME() ptgn::impl::TrimFunctionSignature(PTGN_FULL_FUNCTION_SIGNATURE)