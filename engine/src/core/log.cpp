#include "core/log.h"

#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>

namespace ptgn::impl {

namespace {

std::string Basename(std::string_view path) {
	try {
		return std::filesystem::path(path).filename().string();
	} catch (...) {
		return std::string(path);
	}
}

} // namespace

void DebugPrint(std::string_view prefix, std::string_view message, std::source_location where) {
	const auto file{ impl::Basename(where.file_name()) };
	if (!message.empty()) {
		PrintLine(prefix, file, ':', where.line(), " in ", where.function_name(), ": ", message);
	} else {
		PrintLine(prefix, file, ':', where.line(), " in ", where.function_name());
	}
}

} // namespace ptgn::impl