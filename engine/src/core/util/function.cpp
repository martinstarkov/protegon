#include "core/util/function.h"

#include <string>
#include <string_view>

namespace ptgn::impl {

std::string TrimFunctionSignature(std::string_view signature) {
	const std::string begin{ "__cdecl" }; // trim return type
	const std::string end{ "(" };		  // trim function parameter list
	std::string new_signature{ signature };
	if (const auto position{ new_signature.find(begin) }; position != std::string::npos) {
		new_signature = new_signature.substr(position + begin.length() + 1);
	}
	if (const auto position{ new_signature.find(end) }; position != std::string::npos) {
		new_signature = new_signature.substr(0, position);
	}
	return new_signature;
}

} // namespace ptgn::impl