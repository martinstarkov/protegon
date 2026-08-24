#include "app/project.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app/application.h"
#include "app/application_context.h"
#include "core/assert.h"
#include "core/util/file.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_registry.h"
#include "serialization/json/json.h"
#include "serialization/json/json_file.h"

namespace ptgn {

namespace {

bool ScenePathsEqual(const path& a, const path& b) {
	return a.lexically_normal().generic_string() == b.lexically_normal().generic_string();
}

std::string TypeNameWithoutNamespaces(std::string_view type) {
	if (type == impl::kBaseSceneType) {
		return "Scene";
	}

	const auto separator{ type.rfind("::") };
	return separator == std::string_view::npos ? std::string{ type }
												 : std::string{ type.substr(separator + 2) };
}

void ValidateProject(const Project& project) {
	PTGN_ASSERT(!project.asset_directory.empty(), "Project asset directory cannot be empty");
	PTGN_ASSERT(
		!project.asset_directory.is_absolute() &&
			!project.asset_directory.lexically_normal().generic_string().starts_with(".."),
		"Project asset directory must remain inside the project root: ",
		project.asset_directory.string()
	);
	PTGN_ASSERT(!project.startup_scene_key.empty(), "Project startup scene key cannot be empty");
	PTGN_ASSERT(!project.scenes.empty(), "Project must contain at least one scene");

	for (std::size_t i{ 0 }; i < project.scenes.size(); ++i) {
		const auto& scene{ project.scenes[i] };
		PTGN_ASSERT(!scene.key.empty(), "Project scene key cannot be empty");
		PTGN_ASSERT(!scene.display_name.empty(), "Project scene display name cannot be empty");
		PTGN_ASSERT(!scene.scene_path.empty(), "Project scene path cannot be empty");

		for (std::size_t j{ i + 1 }; j < project.scenes.size(); ++j) {
			const auto& other{ project.scenes[j] };
			PTGN_ASSERT(scene.key != other.key, "Duplicate project scene key: ", scene.key);
			PTGN_ASSERT(
				!ScenePathsEqual(scene.scene_path, other.scene_path),
				"Duplicate project scene path: ",
				scene.scene_path.string()
			);
		}
	}

	PTGN_ASSERT(
		FindProjectScene(project, project.startup_scene_key),
		"Project startup scene key is not present in the scene list: ",
		project.startup_scene_key
	);
}

std::string SanitizeAssetKeyBase(const path& source_path) {
	std::string value{ source_path.stem().string() };
	for (char& c : value) {
		const auto byte{ static_cast<unsigned char>(c) };
		if (std::isalnum(byte) == 0 && c != '_' && c != '-') {
			c = '_';
		} else {
			c = static_cast<char>(std::tolower(byte));
		}
	}
	return value.empty() ? std::string{ "asset" } : value;
}

std::string AssetIdentity(AssetKind kind, std::string_view key) {
	return std::to_string(std::to_underlying(kind)) + ":" + std::string{ key };
}

bool ReplaceAssetKeyStrings(
	json& value,
	const std::unordered_map<std::string, std::string>& replacements
) {
	bool changed{ false };

	if (value.is_string()) {
		const auto current{ value.template get<std::string>() };
		if (const auto it{ replacements.find(current) }; it != replacements.end()) {
			value = it->second;
			return true;
		}
		return false;
	}

	if (value.is_array()) {
		for (auto& item : value) {
			changed |= ReplaceAssetKeyStrings(item, replacements);
		}
		return changed;
	}

	if (value.is_object()) {
		for (auto it{ value.begin() }; it != value.end(); ++it) {
			changed |= ReplaceAssetKeyStrings(it.value(), replacements);
		}
	}

	return changed;
}

void RewriteAssetKeyReferencesInJsonFile(
	const path& file_path,
	const std::unordered_map<std::string, std::string>& replacements
) {
	if (!FileExists(file_path)) {
		return;
	}

	try {
		json value{ LoadJson(file_path) };
		if (ReplaceAssetKeyStrings(value, replacements)) {
			SaveJson(value, file_path);
		}
	} catch (...) {
	}
}

bool NormalizeLegacyProjectAssetKeys(Project& project) {
	std::unordered_set<std::string> used;
	std::vector<std::size_t> normalize_indices;

	for (std::size_t i{ 0 }; i < project.assets.size(); ++i) {
		const auto& asset{ project.assets[i] };
		const bool path_key{
			asset.key.value.find('/') != std::string::npos ||
			asset.key.value.find('\\') != std::string::npos
		};
		if (path_key) {
			normalize_indices.emplace_back(i);
		} else {
			used.emplace(AssetIdentity(asset.kind, asset.key.value));
		}
	}

	std::ranges::sort(normalize_indices, [&](std::size_t lhs, std::size_t rhs) {
		return project.assets[lhs].source_path.generic_string() <
			project.assets[rhs].source_path.generic_string();
	});

	std::unordered_map<std::string, std::string> replacements;
	for (const auto index : normalize_indices) {
		auto& asset{ project.assets[index] };
		const std::string old_key{ asset.key.value };
		const std::string base{ SanitizeAssetKeyBase(asset.source_path) };
		std::string key{ base };

		for (std::size_t suffix{ 2 }; used.contains(AssetIdentity(asset.kind, key)); ++suffix) {
			key = base + "_" + std::to_string(suffix);
		}

		used.emplace(AssetIdentity(asset.kind, key));
		asset.key = AssetKey{ key };
		if (old_key != key) {
			replacements.insert_or_assign(old_key, key);
		}
	}

	if (replacements.empty()) {
		return false;
	}

	for (auto& key : project.preload_assets) {
		if (const auto it{ replacements.find(key.value) }; it != replacements.end()) {
			key = AssetKey{ it->second };
		}
	}

	json screen_effects = project.screen_effects;
	if (ReplaceAssetKeyStrings(screen_effects, replacements)) {
		screen_effects.get_to(project.screen_effects);
	}

	const path project_root{ project.file_path.parent_path() };
	std::unordered_set<std::string> rewritten_files;
	for (const auto& scene : project.scenes) {
		const path scene_path{ (project_root / scene.scene_path).lexically_normal() };
		rewritten_files.emplace(scene_path.generic_string());
		RewriteAssetKeyReferencesInJsonFile(scene_path, replacements);
	}

	const path assets_root{ (project_root / project.asset_directory).lexically_normal() };
	std::error_code error;
	for (std::filesystem::recursive_directory_iterator it{ assets_root, error }, end;
		 !error && it != end; it.increment(error)) {
		std::error_code entry_error;
		if (!it->is_regular_file(entry_error) || entry_error) {
			continue;
		}

		const path file_path{ it->path().lexically_normal() };
		if (rewritten_files.contains(file_path.generic_string())) {
			continue;
		}

		const auto extension{ file_path.extension().string() };
		if (extension != ".json" && extension != ".ptgnprefab" && extension != ".ptgnscene") {
			continue;
		}

		RewriteAssetKeyReferencesInJsonFile(file_path, replacements);
	}

	return true;
}

} // namespace

namespace impl {

bool SaveProjectScenes(Application& app, std::span<const Scene* const> scenes) {
	auto& app_context{ ApplicationAccessor::ctx(app) };
	if (!app_context.project.has_value()) {
		return false;
	}

	auto& project{ app_context.project.value() };

	struct PendingSceneWrite {
		path file_path{};
		SerializedScene scene{};
	};

	std::vector<PendingSceneWrite> writes;
	writes.reserve(scenes.size());

	for (const Scene* scene : scenes) {
		if (!scene || scene->IsRuntime()) {
			return false;
		}

		const auto* project_scene{ FindProjectScene(project, scene->GetTag()) };
		PTGN_ASSERT(project_scene, "Scene is not registered in project: ", scene->GetTag());

		writes.emplace_back(PendingSceneWrite{
			.file_path = GetProjectScenePath(project, *project_scene),
			.scene = CaptureScene(*scene),
		});
	}

	project.assets = app_context.assets.GetCatalog();
	project.preload_assets = app_context.assets.GetProjectAssetDependencies();
	project.settings = GetProjectSettings(app);
	SaveProject(project);

	for (const auto& write : writes) {
		SaveSceneFile(write.file_path, write.scene);
	}
	return true;
}

bool SaveBootstrapProjectScene(Application& app, const Scene& scene, bool save) {
	if (!save || scene.IsRuntime()) {
		return false;
	}

	const Scene* scenes[]{ &scene };
	if (!SaveProjectScenes(app, std::span<const Scene* const>{ scenes })) {
		return false;
	}

	ApplicationAccessor::ctx(app).project_bootstrap_save_pending = false;
	return true;
}

} // namespace impl

Project LoadProject(const path& file_path, const ProjectSettings& default_settings) {
	Project project;
	project.settings = default_settings;

	LoadJson(file_path).get_to(project);
	project.file_path = file_path;

	ValidateProject(project);
	EnsureProjectAssetDirectories(project);
#if !defined(__EMSCRIPTEN__)
	if (NormalizeLegacyProjectAssetKeys(project)) {
		SaveProject(project);
	}
#endif
	return project;
}

Project CreateProject(
	const path& file_path,
	const impl::SceneRegistryEntry& default_scene,
	ProjectSettings settings
) {
	Project project{
		.name = file_path.stem().string(),
		.file_path = file_path,
		.asset_directory = "assets",
		.startup_scene_key = "main",
		.scenes = {
			ProjectSceneEntry{
				.key = "main",
				.display_name = TypeNameWithoutNamespaces(default_scene.type),
				.scene_path = path{ "assets" } / "scenes" / "main.ptgnscene",
			},
		},
		.settings = std::move(settings),
	};

	EnsureProjectAssetDirectories(project);

	SaveSceneFile(
		GetStartupScenePath(project),
		SerializedScene{
			.type = default_scene.type,
			.parameters = default_scene.default_parameters(),
			.assets = {},
			.preload_assets = {},
			.content = std::nullopt,
		}
	);

	SaveProject(project);
	return project;
}

void SaveProject(const Project& project) {
	ValidateProject(project);
	EnsureProjectAssetDirectories(project);
	EnsureDirectory(project.file_path.parent_path());
	json value = project;
	SaveJson(value, project.file_path);
}

ProjectSceneEntry* FindProjectScene(Project& project, std::string_view scene_key) {
	auto it{ std::ranges::find_if(project.scenes, [scene_key](const ProjectSceneEntry& scene) {
		return scene.key == scene_key;
	}) };
	return it == project.scenes.end() ? nullptr : &*it;
}

const ProjectSceneEntry* FindProjectScene(const Project& project, std::string_view scene_key) {
	auto it{ std::ranges::find_if(project.scenes, [scene_key](const ProjectSceneEntry& scene) {
		return scene.key == scene_key;
	}) };
	return it == project.scenes.end() ? nullptr : &*it;
}

const ProjectSceneEntry& GetStartupProjectScene(const Project& project) {
	const auto* scene{ FindProjectScene(project, project.startup_scene_key) };
	PTGN_ASSERT(scene, "Project startup scene key is missing: ", project.startup_scene_key);
	return *scene;
}

path GetProjectScenePath(const Project& project, const ProjectSceneEntry& scene) {
	return project.file_path.parent_path() / scene.scene_path;
}

path GetStartupScenePath(const Project& project) {
	return GetProjectScenePath(project, GetStartupProjectScene(project));
}

path GetProjectAssetDirectory(const Project& project) {
	return project.file_path.parent_path() / project.asset_directory;
}

void EnsureProjectAssetDirectories(const Project& project) {
	const auto root{ GetProjectAssetDirectory(project) };
	EnsureDirectory(root);

	constexpr std::array<std::string_view, 7> directories{
		"audio",
		"data",
		"fonts",
		"prefabs",
		"scenes",
		"shaders",
		"textures",
	};

	for (const auto directory : directories) {
		EnsureDirectory(root / directory);
	}
}

} // namespace ptgn
