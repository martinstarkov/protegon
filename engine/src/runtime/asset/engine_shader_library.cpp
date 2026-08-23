#include "runtime/asset/engine_shader_library.h"

#include <cmrc/cmrc.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/string.h"
#include "serialization/json/json.h"

CMRC_DECLARE(shaders);

namespace ptgn::impl {

namespace {

struct EngineShaderLibrary {
	std::vector<EngineShaderFile> files{};
	json manifest = json::object();
};

const EngineShaderLibrary& Library() {
	static const EngineShaderLibrary library = [] {
		EngineShaderLibrary result;
		auto filesystem{ cmrc::shaders::get_filesystem() };

		for (const auto& resource : filesystem.iterate_directory("")) {
			if (!resource.is_file()) {
				continue;
			}

			const path filename{ resource.filename() };
			if (ToLower(filename.extension().string()) != ".glsl") {
				continue;
			}

			auto file{ filesystem.open(filename.string()) };
			result.files.push_back(EngineShaderFile{
				.filename = filename,
				.source = std::string{ file.begin(), file.end() },
			});
		}

		std::ranges::sort(result.files, {}, &EngineShaderFile::filename);

		constexpr std::string_view kManifestName{ "manifest.json" };
		PTGN_ASSERT(
			filesystem.exists(std::string{ kManifestName }),
			"Could not find engine shader manifest: ",
			kManifestName
		);
		auto manifest_file{ filesystem.open(std::string{ kManifestName }) };
		const std::string manifest_text{ manifest_file.begin(), manifest_file.end() };
		result.manifest = json::parse(manifest_text);
		return result;
	}();

	return library;
}

} // namespace

std::span<const EngineShaderFile> GetEngineShaderFiles() {
	return Library().files;
}

const json& GetEngineShaderManifest() {
	return Library().manifest;
}

} // namespace ptgn::impl
