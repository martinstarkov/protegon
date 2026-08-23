#include "runtime/asset/asset_manager.h"

#include <ecs/ecs.h>
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <extras/decoders/libvorbis/miniaudio_libvorbis.h>
#include <miniaudio.h>
#include <stb_image.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <charconv>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "app/project.h"
#include "core/assert.h"
#include "core/build_info.h"
#include "core/graphics/surface.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "core/util/string.h"
#include "renderer/renderer.h"
#include "renderer/shader_compiler.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/text/font_atlas.h"
#include "renderer/text/font_cache.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/engine_shader_library.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/font_system.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json_file.h"

namespace ptgn {

namespace {

using namespace std::chrono_literals;

inline constexpr std::string_view kMissingTextureAssetKey{ "$ptgn/missing_texture" };
inline constexpr std::array<std::uint8_t, 4> kMissingTexturePixel{ 176, 48, 224, 255 };

[[nodiscard]] impl::AssetStorageKey MakeAssetStorageKey(
	const AssetKey& key,
	AssetKind kind
) {
	return impl::AssetStorageKey{ Hash(key), kind };
}

AssetKind GetAssetKindFromEntity(ecs::Entity asset, const path& source_path) {
	using enum AssetKind;

	if (asset.Has<impl::TextureObject>()) {
		return Texture;
	}
	if (asset.Has<impl::AudioObject>()) {
		return Audio;
	}
	if (asset.Has<impl::FontAtlas>()) {
		return Font;
	}
	if (asset.Has<impl::ShaderObject>()) {
		return Shader;
	}
	if (!source_path.empty()) {
		return impl::GetAssetKind(source_path);
	}
	return Unknown;
}

bool IsOggFile(const path& file_path) {
	return HasExtension(file_path, ".ogg");
}

std::string SanitizeKeySegment(std::string value) {
	for (char& c : value) {
		const auto character{ static_cast<unsigned char>(c) };
		if (std::isalnum(character) == 0 && c != '_' && c != '-') {
			c = '_';
		}
	}
	return ToLower(std::move(value));
}

std::string StripGeneratedAssetMetadataSuffix(std::string value) {
	auto strip_suffix = [&value](std::string_view marker) {
		const auto marker_position{ value.rfind(marker) };
		if (marker_position == std::string::npos || marker_position + marker.size() >= value.size()) {
			return false;
		}

		const std::string_view suffix{ value.data() + marker_position + marker.size(), value.size() - marker_position - marker.size() };
		if (!std::ranges::all_of(suffix, [](char c) {
			return std::isdigit(static_cast<unsigned char>(c)) != 0;
		})) {
			return false;
		}

		value.erase(marker_position);
		return true;
	};

	while (strip_suffix("_frames") || strip_suffix("_slices")) {
	}

	return value;
}

std::uintmax_t SafeFileSize(const path& file_path) {
	std::error_code error;
	auto size{ std::filesystem::file_size(file_path, error) };
	return error ? 0 : size;
}

bool FilesHaveSameContents(const path& lhs, const path& rhs) {
	std::error_code lhs_error;
	std::error_code rhs_error;
	auto lhs_size{ std::filesystem::file_size(lhs, lhs_error) };
	auto rhs_size{ std::filesystem::file_size(rhs, rhs_error) };

	if (lhs_error || rhs_error || lhs_size != rhs_size) {
		return false;
	}

	std::ifstream lhs_stream{ lhs, std::ios::binary };
	std::ifstream rhs_stream{ rhs, std::ios::binary };

	if (!lhs_stream || !rhs_stream) {
		return false;
	}

	std::array<char, 64 * 1024> lhs_buffer{};
	std::array<char, 64 * 1024> rhs_buffer{};

	while (lhs_stream && rhs_stream) {
		lhs_stream.read(lhs_buffer.data(), static_cast<std::streamsize>(lhs_buffer.size()));
		rhs_stream.read(rhs_buffer.data(), static_cast<std::streamsize>(rhs_buffer.size()));

		auto lhs_count{ lhs_stream.gcount() };
		auto rhs_count{ rhs_stream.gcount() };

		if (lhs_count != rhs_count ||
			!std::equal(
				lhs_buffer.begin(),
				lhs_buffer.begin() + lhs_count,
				rhs_buffer.begin()
			)) {
			return false;
		}
	}

	return true;
}

std::optional<path> FindExistingImportedAssetCopy(
	const path& import_directory,
	const path& source_path
) {
	std::error_code error;
	if (!std::filesystem::is_directory(import_directory, error) || error) {
		return std::nullopt;
	}

	std::vector<path> candidates;

	for (std::filesystem::recursive_directory_iterator it{
			 import_directory,
			 std::filesystem::directory_options::skip_permission_denied,
			 error
		 };
		 !error && it != std::filesystem::recursive_directory_iterator{};
		 it.increment(error)) {
		if (!it->is_regular_file(error) || error) {
			error.clear();
			continue;
		}

		if (it->path().filename() == source_path.filename()) {
			candidates.push_back(it->path().lexically_normal());
		}
	}

	std::ranges::sort(candidates, [](const path& lhs, const path& rhs) {
		return lhs.generic_string() < rhs.generic_string();
	});

	for (const auto& candidate : candidates) {
		if (FilesHaveSameContents(source_path, candidate)) {
			return candidate;
		}
	}

	return std::nullopt;
}

bool IsWithinDirectory(const path& candidate, const path& directory) {
	auto normalized_candidate{ candidate.lexically_normal() };
	auto normalized_directory{ directory.lexically_normal() };
	if (normalized_candidate == normalized_directory) {
		return true;
	}

	auto relative{ normalized_candidate.lexically_relative(normalized_directory) };
	if (relative.empty() || relative.is_absolute()) {
		return false;
	}

	auto first{ relative.begin() };
	return first != relative.end() && *first != "..";
}

[[nodiscard]] std::string_view ProjectAssetFolderName(AssetKind kind) {
	switch (kind) {
		using enum AssetKind;
		case Texture: return "Textures";
		case Audio: return "Audio";
		case Font: return "Fonts";
		case Shader: return "Shaders";
		case Json: return "Data";
		case Prefab: return "Prefabs";
		case Scene: return "Scenes";
		case Unknown: break;
	}

	return "Other";
}

[[nodiscard]] std::optional<AssetKind> ProjectAssetKindFromFolderName(std::string_view name) {
	for (AssetKind kind : {
		AssetKind::Texture,
		AssetKind::Audio,
		AssetKind::Font,
		AssetKind::Shader,
		AssetKind::Json,
		AssetKind::Prefab,
		AssetKind::Scene,
	}) {
		if (ProjectAssetFolderName(kind) == name) {
			return kind;
		}
	}
	return std::nullopt;
}

[[nodiscard]] bool IsSafeAssetRelativePath(const path& value) {
	if (value.empty() || value.is_absolute()) {
		return false;
	}
	auto normalized{ value.lexically_normal() };
	if (normalized.empty() || normalized == ".") {
		return false;
	}
	for (const auto& component : normalized) {
		if (component == "..") {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::optional<AssetKind> AssetDirectoryKind(const path& relative_directory) {
	if (!IsSafeAssetRelativePath(relative_directory)) {
		return std::nullopt;
	}
	auto normalized{ relative_directory.lexically_normal() };
	auto first{ normalized.begin() };
	if (first == normalized.end()) {
		return std::nullopt;
	}
	return ProjectAssetKindFromFolderName(first->string());
}

void EnsureProjectAssetTypeDirectories(const path& assets_root) {
	EnsureDirectory(assets_root);
	for (AssetKind kind : {
		AssetKind::Texture,
		AssetKind::Audio,
		AssetKind::Font,
		AssetKind::Shader,
		AssetKind::Json,
		AssetKind::Prefab,
		AssetKind::Scene,
	}) {
		EnsureDirectory(assets_root / ProjectAssetFolderName(kind));
	}
}

[[nodiscard]] bool IsPathInsideOrEqual(const path& candidate, const path& directory) {
	return IsWithinDirectory(candidate.lexically_normal(), directory.lexically_normal());
}

void ReplaceShaderPathReference(
	std::optional<std::string>& reference,
	const path& old_relative,
	const path& new_relative
) {
	if (!reference.has_value() || reference->empty() ||
		reference.value() == kShaderSourceToken || reference->starts_with(kBuiltinShaderPrefix)) {
		return;
	}
	if (path{ reference.value() }.lexically_normal() == old_relative.lexically_normal()) {
		reference = new_relative.generic_string();
	}
}

void ReplaceShaderDirectoryReference(
	std::optional<std::string>& reference,
	const path& old_directory,
	const path& new_directory
) {
	if (!reference.has_value() || reference->empty() ||
		reference.value() == kShaderSourceToken || reference->starts_with(kBuiltinShaderPrefix)) {
		return;
	}
	path current{ path{ reference.value() }.lexically_normal() };
	if (!IsPathInsideOrEqual(current, old_directory)) {
		return;
	}
	auto tail{ current.lexically_relative(old_directory) };
	reference = (new_directory / tail).lexically_normal().generic_string();
}

AssetKind DetectProjectAssetKind(const path& file_path) {
	auto kind{ impl::GetAssetKind(file_path) };
	if (kind == AssetKind::Texture && impl::IsFontAtlasPng(file_path)) {
		kind = AssetKind::Font;
	}
	return kind;
}

bool SerializedAssetsEquivalent(const SerializedAsset& lhs, const SerializedAsset& rhs) {
	if (lhs.key != rhs.key || lhs.kind != rhs.kind ||
		lhs.source_path.lexically_normal() != rhs.source_path.lexically_normal() ||
		lhs.shader.has_value() != rhs.shader.has_value()) {
		return false;
	}
	if (!lhs.shader.has_value()) {
		return true;
	}
	return lhs.shader->vertex == rhs.shader->vertex &&
		lhs.shader->fragment == rhs.shader->fragment;
}

bool CatalogDiffersFrom(
	std::span<const SerializedAsset> serialized,
	std::span<const SerializedAsset> current
) {
	if (serialized.size() != current.size()) {
		return true;
	}
	for (const auto& asset : serialized) {
		const auto it{ std::ranges::find_if(current, [&](const SerializedAsset& candidate) {
			return candidate.key == asset.key && candidate.kind == asset.kind;
		}) };
		if (it == current.end() || !SerializedAssetsEquivalent(asset, *it)) {
			return true;
		}
	}
	return false;
}

path MakeUniqueDestinationPath(const path& directory, const path& source_file) {
	path destination{ (directory / source_file.filename()).lexically_normal() };
	for (std::size_t suffix{ 2 }; FileExists(destination); ++suffix) {
		destination = (
			directory /
			(source_file.stem().string() + "_" + std::to_string(suffix) + source_file.extension().string())
		).lexically_normal();
	}
	return destination;
}

std::optional<V2_int> ProbeImageDimensions(const path& file_path) {
	int width{ 0 };
	int height{ 0 };
	int channels{ 0 };

	auto absolute_path{ GetAbsolutePath(file_path).string() };
	if (stbi_info(absolute_path.c_str(), &width, &height, &channels) == 0 || width <= 0 ||
		height <= 0) {
		return std::nullopt;
	}

	return V2_int{ width, height };
}

std::optional<double> ProbeAudioDuration(const path& file_path) {
	auto absolute_path{ GetAbsolutePath(file_path).string() };

	if (IsOggFile(file_path)) {
		ma_libvorbis vorbis{};
		if (ma_libvorbis_init_file(absolute_path.c_str(), nullptr, nullptr, &vorbis) != MA_SUCCESS) {
			return std::nullopt;
		}

		ma_uint64 frame_count{ 0 };
		ma_uint32 sample_rate{ 0 };
		ma_format format{};
		ma_uint32 channels{ 0 };

		const auto length_result{ ma_data_source_get_length_in_pcm_frames(
			static_cast<ma_data_source*>(&vorbis), &frame_count
		) };
		const auto format_result{ ma_data_source_get_data_format(
			static_cast<ma_data_source*>(&vorbis), &format, &channels, &sample_rate, nullptr, 0
		) };

		ma_libvorbis_uninit(&vorbis, nullptr);

		if (length_result != MA_SUCCESS || format_result != MA_SUCCESS || sample_rate == 0) {
			return std::nullopt;
		}

		return static_cast<double>(frame_count) / static_cast<double>(sample_rate);
	}

	ma_decoder decoder{};
	if (ma_decoder_init_file(absolute_path.c_str(), nullptr, &decoder) != MA_SUCCESS) {
		return std::nullopt;
	}

	ma_uint64 frame_count{ 0 };
	const auto result{ ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count) };
	auto sample_rate{ decoder.outputSampleRate };
	ma_decoder_uninit(&decoder);

	if (result != MA_SUCCESS || sample_rate == 0) {
		return std::nullopt;
	}

	return static_cast<double>(frame_count) / static_cast<double>(sample_rate);
}

void AddUnique(std::vector<AssetKey>& values, const AssetKey& key) {
	if (!key.value.empty() && !std::ranges::contains(values, key)) {
		values.emplace_back(key);
	}
}

std::string BuiltinShaderName(std::string_view value) {
	if (!value.starts_with(kBuiltinShaderPrefix)) {
		return std::string{ value };
	}
	value.remove_prefix(kBuiltinShaderPrefix.size());
	return std::string{ value };
}

std::optional<std::string> ResolveShaderPairStageForValidation(
	const std::variant<ShaderCode, ShaderPathOrName>& value
) {
	if (const auto* code{ std::get_if<ShaderCode>(&value) }) {
		return code->content;
	}

	const auto& path_or_name{ std::get<ShaderPathOrName>(value) };

	if (IsFilePath(path_or_name)) {
		path file_path{
			GetAbsolutePath(path{ path_or_name })
		};

		if (!FileExists(file_path)) {
			return std::nullopt;
		}

		return FileToString(file_path);
	}

	const auto engine_shaders{ impl::GetEngineShaderFiles() };
	const auto it{ std::ranges::find_if(engine_shaders, [&](const auto& shader_file) {
		return shader_file.filename.stem().string() == path_or_name;
	}) };
	if (it == engine_shaders.end()) {
		return std::nullopt;
	}
	return it->source;
}

ShaderCompileResult ValidateShaderPairForLoad(
	const ShaderPair& pair,
	std::size_t max_texture_slots
) {
	auto vertex{ ResolveShaderPairStageForValidation(pair.vertex) };
	auto fragment{ ResolveShaderPairStageForValidation(pair.fragment) };
	if (!vertex.has_value() || !fragment.has_value()) {
		return { false, "Could not resolve one or more shader-pair sources." };
	}
	return impl::ValidateShaderProgram(
		vertex.value(), fragment.value(), max_texture_slots
	);
}

ShaderCompileResult ValidateShaderProgramSourceForLoad(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::size_t max_texture_slots
) {
	if (const auto* pair{ std::get_if<ShaderPair>(&source) }) {
		return ValidateShaderPairForLoad(*pair, max_texture_slots);
	}

	std::string program_source;
	if (const auto* code{ std::get_if<ShaderCode>(&source) }) {
		program_source = code->content;
	} else {
		const auto& shader_path{ std::get<ShaderPath>(source).path };
		if (!FileExists(shader_path)) {
			return { false, "Shader source path does not exist: " + shader_path.string() };
		}
		program_source = FileToString(shader_path);
	}

	if (DetectShaderStages(program_source) != ShaderStageMask::VertexFragment) {
		return { false, "A shader program source requires both vertex and fragment stages." };
	}
	return impl::ValidateShaderSource(program_source, max_texture_slots);
}

std::variant<ShaderCode, ShaderPathOrName> ResolveSerializedShaderStage(
	std::string_view reference,
	const path& source_path,
	const path& project_root,
	ShaderStageMask stage
) {
	if (reference.starts_with(kBuiltinShaderPrefix)) {
		return BuiltinShaderName(reference);
	}

	path stage_path{
		reference == kShaderSourceToken
			? source_path
			: (project_root / path{ reference }).lexically_normal()
	};
	std::string source{ FileToString(stage_path) };
	return ShaderCode{ impl::ExtractShaderStageSource(source, stage) };
}

} // namespace

namespace impl {

struct AssetLoadBatchState {
	mutable std::mutex mutex{};
	AssetLoadProgress progress{};
};

void AddAssetKey(ecs::Entity asset, AssetKey key, const std::optional<path>& path) {
	asset.Add<AssetKey>(std::move(key));
	if (path.has_value()) {
		asset.Add<impl::AssetPath>(path.value());
	}
}

AssetKind GetAssetKind(const path& asset_path) {
	auto extension{ ToLower(GetExtension(asset_path)) };

	if (MatchesExtension<Texture>(extension)) {
		return AssetKind::Texture;
	}
	if (MatchesExtension<Audio>(extension)) {
		return AssetKind::Audio;
	}
	if (MatchesExtension<Font>(extension)) {
		return AssetKind::Font;
	}
	if (MatchesExtension<json>(extension)) {
		return AssetKind::Json;
	}
	if (MatchesExtension<Shader>(extension)) {
		return AssetKind::Shader;
	}
	if (MatchesExtension<Prefab>(extension)) {
		return AssetKind::Prefab;
	}
	if (extension == ".ptgnscene") {
		return AssetKind::Scene;
	}

	return AssetKind::Unknown;
}

AssetAccessor::AssetAccessor(AssetManager& asset_manager) : assets{ asset_manager } {}

std::vector<AssetRecord> AssetAccessor::GetAssets() const {
	return assets.GetAssets();
}

bool AssetAccessor::Unload(const AssetKey& key, AssetKind kind) {
	return assets.Unload(key, kind);
}

AssetLoadTicket::AssetLoadTicket(
	AssetManager& assets,
	std::shared_ptr<AssetLoadBatchState> state,
	std::vector<AssetKey> dependencies
) :
	assets_{ &assets },
	state_{ std::move(state) },
	dependencies_{ std::move(dependencies) },
	owns_references_{ true } {}

AssetLoadTicket::~AssetLoadTicket() noexcept {
	Reset();
}

AssetLoadTicket::AssetLoadTicket(AssetLoadTicket&& other) noexcept :
	assets_{ std::exchange(other.assets_, nullptr) },
	state_{ std::move(other.state_) },
	dependencies_{ std::move(other.dependencies_) },
	owns_references_{ std::exchange(other.owns_references_, false) } {}

AssetLoadTicket& AssetLoadTicket::operator=(AssetLoadTicket&& other) noexcept {
	if (this != &other) {
		Reset();
		assets_ = std::exchange(other.assets_, nullptr);
		state_ = std::move(other.state_);
		dependencies_ = std::move(other.dependencies_);
		owns_references_ = std::exchange(other.owns_references_, false);
	}
	return *this;
}

AssetLoadTicket::operator bool() const {
	return state_ != nullptr;
}

bool AssetLoadTicket::IsComplete() const {
	return GetProgress().IsComplete();
}

AssetLoadProgress AssetLoadTicket::GetProgress() const {
	if (!state_) {
		return {};
	}

	std::scoped_lock lock{ state_->mutex };
	return state_->progress;
}

const std::vector<AssetKey>& AssetLoadTicket::GetDependencies() const {
	return dependencies_;
}

std::vector<AssetKey> AssetLoadTicket::ReleaseOwnership() {
	owns_references_ = false;
	assets_ = nullptr;
	return std::move(dependencies_);
}

void AssetLoadTicket::Reset() noexcept {
	if (owns_references_ && assets_) {
		assets_->ReleaseDependencies(dependencies_);
	}

	assets_ = nullptr;
	state_.reset();
	dependencies_.clear();
	owns_references_ = false;
}

} // namespace impl

class AssetManager::AsyncLoader {
public:
	struct PreparedTexture {
		std::unique_ptr<impl::Surface> surface{};
	};

	struct PreparedAudio {
		path file_path{};
	};

	struct PreparedFont {
		path file_path{};
		impl::FontAtlasData data;
	};

	struct PreparedJson {
		json value = json::object();
	};

	struct PreparedPrefab {
		Prefab value{};
	};

	struct PreparedShader {
		std::variant<ShaderCode, ShaderPath, ShaderPair> source{};
	};

	struct PreparedScene {};

	using Payload = std::variant<
		PreparedTexture,
		PreparedAudio,
		PreparedFont,
		PreparedJson,
		PreparedPrefab,
		PreparedShader,
		PreparedScene>;

	struct Job {
		SerializedAsset asset{};
		path absolute_path{};
		path project_root{};
	};

	struct Result {
		SerializedAsset asset{};
		std::optional<Payload> payload{};
		std::string error{};
	};

	AsyncLoader() {
#ifndef __EMSCRIPTEN__
		auto hardware_threads{ std::max(1u, std::thread::hardware_concurrency()) };
		auto worker_count{ std::clamp(hardware_threads > 1 ? hardware_threads - 1 : 1u, 1u, 4u) };

		workers_.reserve(worker_count);
		for (auto i{ 0u }; i < worker_count; ++i) {
			workers_.emplace_back([this](std::stop_token stop_token) { WorkerLoop(stop_token); });
		}
#endif
	}

	~AsyncLoader() noexcept {
		for (auto& worker : workers_) {
			worker.request_stop();
		}
		condition_.notify_all();
	}

	void Queue(Job job) {
		{
			std::scoped_lock lock{ mutex_ };
			jobs_.emplace_back(std::move(job));
		}
		condition_.notify_one();
	}

	void PumpFallback() {
#ifdef __EMSCRIPTEN__
		std::optional<Job> job;
		{
			std::scoped_lock lock{ mutex_ };
			if (!jobs_.empty()) {
				job = std::move(jobs_.front());
				jobs_.pop_front();
			}
		}

		if (job.has_value()) {
			PushResult(Prepare(std::move(job.value())));
		}
#endif
	}

	std::optional<Result> PopResult() {
		std::scoped_lock lock{ mutex_ };
		if (results_.empty()) {
			return std::nullopt;
		}

		auto result{ std::move(results_.front()) };
		results_.pop_front();
		return result;
	}

private:
	static std::variant<ShaderCode, ShaderPathOrName> PrepareShaderStage(
		std::string_view reference,
		const path& source_path,
		const path& project_root,
		ShaderStageMask stage
	) {
		if (reference.starts_with(kBuiltinShaderPrefix)) {
			return BuiltinShaderName(reference);
		}

		path stage_path{
			reference == kShaderSourceToken
				? source_path
				: (project_root / path{ reference }).lexically_normal()
		};
		std::string source{ FileToString(stage_path) };
		return ShaderCode{ impl::ExtractShaderStageSource(source, stage) };
	}

	static Result Prepare(Job job) {
		Result result{ .asset = std::move(job.asset) };

		try {
			switch (result.asset.kind) {
				using enum AssetKind;
				case Texture: {
					if (impl::IsFontAtlasPng(job.absolute_path)) {
						result.payload = PreparedFont{
							.file_path = job.absolute_path,
							.data = AssetManager::PrepareFontAsset(job.absolute_path),
						};
					} else {
						result.payload = PreparedTexture{
							.surface = std::make_unique<impl::Surface>(job.absolute_path),
						};
					}
					break;
				}
				case Audio: result.payload = PreparedAudio{ job.absolute_path }; break;
				case Font:
					result.payload = PreparedFont{
						.file_path = job.absolute_path,
						.data = AssetManager::PrepareFontAsset(job.absolute_path),
					};
					break;
				case Json: {
					json value = json::parse(FileToString(job.absolute_path));
					PreparedJson prepared;
					prepared.value = std::move(value);
					result.payload = std::move(prepared);
					break;
				}
				case Prefab: {
					json value = json::parse(FileToString(job.absolute_path));
					ptgn::Prefab prefab;
					value.get_to(prefab);
					result.payload = PreparedPrefab{
						.value = std::move(prefab),
					};
					break;
				}
				case Shader: {
					if (!result.asset.shader.has_value()) {
						auto source{ FileToString(job.absolute_path) };
						if (!HasVertexAndFragmentShader(source)) {
							result.error =
								"Single-stage shader is missing a configured engine shader pair";
							break;
						}
						result.payload = PreparedShader{ ShaderCode{ source } };
						break;
					}

					const auto& program{ result.asset.shader.value() };
					if (program.vertex == std::optional<std::string>{ kShaderSourceToken } &&
						program.fragment == std::optional<std::string>{ kShaderSourceToken } &&
						HasVertexAndFragmentShader(FileToString(job.absolute_path))) {
						result.payload = PreparedShader{ ShaderCode{ FileToString(job.absolute_path) } };
						break;
					}

					if (!program.vertex.has_value() || !program.fragment.has_value()) {
						result.error = "Shader program requires both vertex and fragment stages";
						break;
					}

					result.payload = PreparedShader{
						ShaderPair{
							.vertex = PrepareShaderStage(
								program.vertex.value(), job.absolute_path, job.project_root, ShaderStageMask::Vertex
							),
							.fragment = PrepareShaderStage(
								program.fragment.value(), job.absolute_path, job.project_root, ShaderStageMask::Fragment
							),
						},
					};
					break;
				}
				case Scene: result.payload = PreparedScene{}; break;
				case Unknown: result.error = "Unsupported asset kind"; break;
			}
		} catch (const std::exception& error) {
			result.error = error.what();
		}

		return result;
	}

	void WorkerLoop(std::stop_token stop_token) {
		while (!stop_token.stop_requested()) {
			std::optional<Job> job;
			{
				std::unique_lock lock{ mutex_ };
				condition_.wait(lock, stop_token, [this] { return !jobs_.empty(); });

				if (stop_token.stop_requested()) {
					return;
				}

				job = std::move(jobs_.front());
				jobs_.pop_front();
			}

			PushResult(Prepare(std::move(job.value())));
		}
	}

	void PushResult(Result result) {
		std::scoped_lock lock{ mutex_ };
		results_.emplace_back(std::move(result));
	}

	std::mutex mutex_;
	std::condition_variable_any condition_;
	std::deque<Job> jobs_;
	std::deque<Result> results_;
	std::vector<std::jthread> workers_;
};

AssetManager::AssetManager(Renderer& renderer) :
	renderer_{ renderer }, async_loader_{ std::make_unique<AsyncLoader>() } {
	Texture texture{ CreateAsset(), true };
	texture.GetEntity().Add<impl::TextureObject>(CreateTexture(
		kMissingTexturePixel.data(),
		TextureDesc{
			.size = { 1, 1 },
			.format = TextureFormat::RGBA8,
		}
	));
	impl::AddAssetKey(
		texture.GetEntity(),
		TextureKey{ std::string{ kMissingTextureAssetKey } },
		std::nullopt
	);

	InitializeEngineShaderCatalog();
}


void AssetManager::InitializeEngineShaderCatalog() {
	engine_shader_sources_.clear();
	engine_vertex_shader_names_.clear();
	engine_fragment_shader_names_.clear();

	for (const auto& shader_file : impl::GetEngineShaderFiles()) {
		const path& filename{ shader_file.filename };
		std::string name{ filename.stem().string() };
		auto stages{ DetectShaderStages(shader_file.source) };
		engine_shader_sources_.push_back(impl::EngineShaderSource{
			.key = AssetKey{ "$" + name },
			.name = name,
			.virtual_path = path{ "Shaders" } / filename,
			.stages = stages,
			.source = shader_file.source,
		});
		if (HasShaderStage(stages, ShaderStageMask::Vertex)) {
			engine_vertex_shader_names_.push_back(name);
		}
		if (HasShaderStage(stages, ShaderStageMask::Fragment)) {
			engine_fragment_shader_names_.push_back(name);
		}
	}
	std::ranges::sort(engine_shader_sources_, {}, &impl::EngineShaderSource::name);
	std::ranges::sort(engine_vertex_shader_names_);
	std::ranges::sort(engine_fragment_shader_names_);
}

std::vector<impl::AssetRecord> AssetManager::GetEngineShaderAssets() const {
	std::vector<impl::AssetRecord> records;
	records.reserve(engine_shader_sources_.size());
	for (const auto& shader : engine_shader_sources_) {
		records.push_back(impl::AssetRecord{
			.key = shader.key,
			.source_path = shader.virtual_path,
			.kind = AssetKind::Shader,
			.load_state = AssetLoadState::Loaded,
			.cataloged = false,
			.engine_asset = true,
			.read_only = true,
			.metadata = impl::AssetMetadata{ .shader_stages = shader.stages },
		});
	}
	return records;
}

std::optional<std::string> AssetManager::GetEngineShaderSource(const AssetKey& key) const {
	if (const auto it{
			std::ranges::find(engine_shader_sources_, key, &impl::EngineShaderSource::key)
		};
		it != engine_shader_sources_.end()) {
		return it->source;
	}

	constexpr std::string_view legacy_prefix{ "$engine/shaders/" };
	if (!key.value.starts_with(legacy_prefix)) {
		return std::nullopt;
	}

	std::string name{
		path{ key.value.substr(legacy_prefix.size()) }.stem().string()
	};
	const auto it{
		std::ranges::find(engine_shader_sources_, name, &impl::EngineShaderSource::name)
	};
	return it == engine_shader_sources_.end()
		? std::nullopt
		: std::optional<std::string>{ it->source };
}

std::span<const std::string> AssetManager::GetEngineVertexShaderNames() const {
	return engine_vertex_shader_names_;
}

std::span<const std::string> AssetManager::GetEngineFragmentShaderNames() const {
	return engine_fragment_shader_names_;
}

std::optional<SerializedShaderProgram> AssetManager::SuggestShaderProgram(
	std::string_view source
) const {
	auto stages{ DetectShaderStages(source) };
	if (stages == ShaderStageMask::None) {
		return std::nullopt;
	}

	SerializedShaderProgram program;
	if (HasShaderStage(stages, ShaderStageMask::Vertex)) {
		program.vertex = std::string{ kShaderSourceToken };
	}
	if (HasShaderStage(stages, ShaderStageMask::Fragment)) {
		program.fragment = std::string{ kShaderSourceToken };
	}

	auto vertex_priority = [](std::string_view name) {
		std::string lower{ ToLower(std::string{ name }) };
		if (lower.contains("texture")) {
			return 0;
		}
		if (lower.contains("color")) {
			return 1;
		}
		if (lower.contains("passthrough")) {
			return 2;
		}
		if (lower.contains("shape")) {
			return 3;
		}
		return 4;
	};

	auto choose_builtin = [&](ShaderStageMask missing_stage) -> std::optional<std::string> {
		const impl::EngineShaderSource* best{ nullptr };
		int best_score{ -1 };
		int best_priority{ 5 };

		for (const auto& candidate : engine_shader_sources_) {
			if (!HasShaderStage(candidate.stages, missing_stage)) {
				continue;
			}

			int score{
				missing_stage == ShaderStageMask::Vertex
					? impl::ShaderStageCompatibilityScore(candidate.source, source)
					: impl::ShaderStageCompatibilityScore(source, candidate.source)
			};
			if (score < 0) {
				continue;
			}

			int priority{
				missing_stage == ShaderStageMask::Vertex
					? vertex_priority(candidate.name)
					: 0
			};
			if (!best || priority < best_priority ||
				(priority == best_priority && score > best_score)) {
				best = &candidate;
				best_score = score;
				best_priority = priority;
			}
		}

		if (!best) {
			return std::nullopt;
		}
		return std::string{ kBuiltinShaderPrefix } + best->name;
	};

	if (!program.vertex.has_value()) {
		program.vertex = choose_builtin(ShaderStageMask::Vertex);
	}
	if (!program.fragment.has_value()) {
		program.fragment = choose_builtin(ShaderStageMask::Fragment);
	}
	return program;
}

void AssetManager::NormalizeShaderProgramConfiguration(
	SerializedAsset& asset,
	std::string_view source
) const {
	if (asset.kind != AssetKind::Shader) {
		return;
	}

	auto suggested{ SuggestShaderProgram(source) };
	if (!suggested.has_value()) {
		return;
	}

	auto stages{ DetectShaderStages(source) };
	SerializedShaderProgram program{ suggested.value() };
	if (asset.shader.has_value()) {
		if (!HasShaderStage(stages, ShaderStageMask::Vertex) &&
			asset.shader->vertex.has_value() && !asset.shader->vertex->empty()) {
			program.vertex = asset.shader->vertex;
		}
		if (!HasShaderStage(stages, ShaderStageMask::Fragment) &&
			asset.shader->fragment.has_value() && !asset.shader->fragment->empty()) {
			program.fragment = asset.shader->fragment;
		}
	}
	asset.shader = std::move(program);
}

std::optional<std::string> AssetManager::GetShaderSource(const ShaderKey& key) const {
	if (auto engine{ GetEngineShaderSource(key) }) {
		return engine;
	}
	const auto it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Shader)) };
	if (it == catalog_.end() || it->second.kind != AssetKind::Shader) {
		return std::nullopt;
	}
	auto file_path{ ResolveAssetPath(it->second) };
	if (!FileExists(file_path)) {
		return std::nullopt;
	}
	return FileToString(file_path);
}

std::optional<std::string> AssetManager::ResolveShaderStageSourceText(
	std::string_view reference,
	const SerializedAsset& owner,
	std::optional<std::string_view> source_override
) const {
	if (reference == kShaderSourceToken) {
		if (source_override.has_value()) {
			return std::string{ source_override.value() };
		}
		auto file_path{ ResolveAssetPath(owner) };
		return FileExists(file_path) ? std::optional<std::string>{ FileToString(file_path) } : std::nullopt;
	}
	if (reference.starts_with(kBuiltinShaderPrefix)) {
		std::string name{ reference.substr(kBuiltinShaderPrefix.size()) };
		const auto it{ std::ranges::find(engine_shader_sources_, name, &impl::EngineShaderSource::name) };
		if (it == engine_shader_sources_.end()) {
			return std::nullopt;
		}
		return it->source;
	}
	auto root{ project_root_.value_or(GetWorkingDirectory()) };
	path stage_path{ (root / path{ reference }).lexically_normal() };
	if (!FileExists(stage_path)) {
		return std::nullopt;
	}
	return FileToString(stage_path);
}

std::optional<std::string> AssetManager::ResolveShaderStageSource(
	std::string_view owner_source,
	std::string_view reference,
	ShaderStageMask stage
) const {
	std::string source;

	if (reference == kShaderSourceToken) {
		source = std::string{ owner_source };
	} else if (reference.starts_with(kBuiltinShaderPrefix)) {
		std::string name{ reference.substr(kBuiltinShaderPrefix.size()) };
		const auto it{
			std::ranges::find(engine_shader_sources_, name, &impl::EngineShaderSource::name)
		};
		if (it == engine_shader_sources_.end()) {
			return std::nullopt;
		}
		source = it->source;
	} else {
		path root{ project_root_.value_or(GetWorkingDirectory()) };
		path stage_path{ (root / path{ reference }).lexically_normal() };
		if (!FileExists(stage_path)) {
			return std::nullopt;
		}
		source = FileToString(stage_path);
	}

	if (!HasShaderStage(DetectShaderStages(source), stage)) {
		return std::nullopt;
	}
	return impl::ExtractShaderStageSource(source, stage);
}

ShaderCompileResult AssetManager::ValidateShaderSource(
	const ShaderKey& key,
	std::string_view source
) const {
	const auto catalog_it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Shader)) };
	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	if (catalog_it == catalog_.end()) {
		return impl::ValidateShaderSource(source, max_texture_slots);
	}

	if (catalog_it->second.shader.has_value()) {
		return ValidateShaderSource(key, source, catalog_it->second.shader.value());
	}
	if (auto suggested{ SuggestShaderProgram(source) }) {
		return ValidateShaderSource(key, source, suggested.value());
	}
	return impl::ValidateShaderSource(source, max_texture_slots);
}

ShaderCompileResult AssetManager::ValidateShaderSource(
	const ShaderKey& key,
	std::string_view source,
	const SerializedShaderProgram& program
) const {
	const auto catalog_it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Shader)) };
	if (catalog_it == catalog_.end()) {
		return { false, "Shader is not in the project catalog." };
	}
	if (!program.vertex.has_value() || !program.fragment.has_value() ||
		program.vertex->empty() || program.fragment->empty()) {
		return { false, "Shader program configuration must provide both vertex and fragment stages." };
	}

	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	const auto& asset{ catalog_it->second };
	auto vertex{ ResolveShaderStageSourceText(program.vertex.value(), asset, source) };
	auto fragment{ ResolveShaderStageSourceText(program.fragment.value(), asset, source) };
	if (!vertex.has_value() || !fragment.has_value()) {
		return { false, "One or more configured shader stage sources could not be resolved." };
	}
	return impl::ValidateShaderProgram(vertex.value(), fragment.value(), max_texture_slots);
}

bool AssetManager::SaveShaderSource(
	const ShaderKey& key,
	std::string_view source,
	const ShaderCompileResult& validation
) {
	auto catalog_it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Shader)) };
	if (catalog_it == catalog_.end() || catalog_it->second.kind != AssetKind::Shader) {
		return false;
	}
	auto file_path{ ResolveAssetPath(catalog_it->second) };
	std::ofstream output{ file_path, std::ios::binary | std::ios::trunc };
	if (!output) {
		return false;
	}
	output.write(source.data(), static_cast<std::streamsize>(source.size()));
	if (!output) {
		return false;
	}
	auto& state{ runtime_states_[MakeAssetStorageKey(key, AssetKind::Shader)] };
	state.metadata = ProbeMetadata(catalog_it->second);
	state.compile_error = !validation.success;
	state.compile_log = validation.log;
	if (!validation.success) {
		PTGN_WARN("Saved shader with compile errors: ", key, "\n", validation.log);
	}
	return true;
}

std::optional<std::variant<ShaderCode, ShaderPath, ShaderPair>> AssetManager::BuildShaderProgramSource(
	const SerializedAsset& asset,
	std::optional<std::string_view> source_override,
	const std::optional<SerializedShaderProgram>& program_override
) const {
	auto source_path{ ResolveAssetPath(asset) };
	std::string source;
	if (source_override.has_value()) {
		source = std::string{ source_override.value() };
	} else if (FileExists(source_path)) {
		source = FileToString(source_path);
	} else {
		return std::nullopt;
	}
	auto program_value{ program_override.has_value() ? program_override : asset.shader };
	if (!program_value.has_value()) {
		return std::variant<ShaderCode, ShaderPath, ShaderPair>{ ShaderCode{ source } };
	}
	const auto& program{ program_value.value() };
	if (program.vertex == std::optional<std::string>{ kShaderSourceToken } &&
		program.fragment == std::optional<std::string>{ kShaderSourceToken }) {
		return std::variant<ShaderCode, ShaderPath, ShaderPair>{ ShaderCode{ source } };
	}
	if (!program.vertex.has_value() || !program.fragment.has_value()) {
		return std::nullopt;
	}
	auto root{ project_root_.value_or(GetWorkingDirectory()) };
	auto resolve_stage = [&](
		std::string_view reference, ShaderStageMask stage
	) -> std::variant<ShaderCode, ShaderPathOrName> {
		if (reference.starts_with(kBuiltinShaderPrefix)) {
			return std::string{ reference.substr(kBuiltinShaderPrefix.size()) };
		}

		std::string stage_source;
		if (reference == kShaderSourceToken) {
			stage_source = source;
		} else {
			stage_source = FileToString((root / path{ reference }).lexically_normal());
		}
		return ShaderCode{ impl::ExtractShaderStageSource(stage_source, stage) };
	};
	return std::variant<ShaderCode, ShaderPath, ShaderPair>{ ShaderPair{
		.vertex = resolve_stage(program.vertex.value(), ShaderStageMask::Vertex),
		.fragment = resolve_stage(program.fragment.value(), ShaderStageMask::Fragment),
	} };
}

ShaderCompileResult AssetManager::RecompileShaderSource(
	const ShaderKey& key,
	std::string_view source
) {
	const auto catalog_it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Shader)) };
	if (catalog_it == catalog_.end()) {
		return { false, "Shader is not in the project catalog." };
	}

	if (catalog_it->second.shader.has_value()) {
		return RecompileShaderSource(key, source, catalog_it->second.shader.value());
	}
	if (auto suggested{ SuggestShaderProgram(source) }) {
		return RecompileShaderSource(key, source, suggested.value());
	}
	return ValidateShaderSource(key, source);
}

ShaderCompileResult AssetManager::RecompileShaderSource(
	const ShaderKey& key,
	std::string_view source,
	const SerializedShaderProgram& program
) {
	auto validation{ ValidateShaderSource(key, source, program) };
	if (!validation.success) {
		return validation;
	}
	auto catalog_it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Shader)) };
	if (catalog_it == catalog_.end()) {
		return { false, "Shader is not in the project catalog." };
	}
	auto& state{ runtime_states_[MakeAssetStorageKey(key, AssetKind::Shader)] };
	if (state.load_state != AssetLoadState::Loaded || !Has<ptgn::Shader>(key)) {
		validation.log += "\nShader is not resident; source was validated but no runtime program was replaced.";
		return validation;
	}
	auto prepared{ BuildShaderProgramSource(catalog_it->second, source, program) };
	if (!prepared.has_value()) {
		return { false, "Could not resolve shader program sources for reload." };
	}

	RuntimeAssetState retained_state{ state };
	ForceUnload(key, AssetKind::Shader);
	LoadShader(ShaderKey{ key }, prepared.value(), key.value);
	auto& refreshed_state{ runtime_states_[MakeAssetStorageKey(key, AssetKind::Shader)] };
	refreshed_state.reference_count = retained_state.reference_count;
	refreshed_state.globally_pinned = retained_state.globally_pinned;
	refreshed_state.manually_pinned = retained_state.manually_pinned;
	refreshed_state.metadata = retained_state.metadata;
	refreshed_state.load_state = AssetLoadState::Loaded;
	refreshed_state.error.clear();
	refreshed_state.compile_error = false;
	refreshed_state.compile_log = validation.log;
	validation.log += "\nResident shader program recompiled successfully.";
	return validation;
}

AssetManager::~AssetManager() noexcept = default;

void AssetManager::Update() {
	async_loader_->PumpFallback();

	constexpr std::size_t kMaxFinalizationsPerFrame{ 4 };
	for (std::size_t i{ 0 }; i < kMaxFinalizationsPerFrame; ++i) {
		auto result{ async_loader_->PopResult() };
		if (!result.has_value()) {
			break;
		}

		auto storage_key{
			MakeAssetStorageKey(result->asset.key, result->asset.kind)
		};
		auto state_it{ runtime_states_.find(storage_key) };
		if (state_it == runtime_states_.end()) {
			continue;
		}

		state_it->second.load_state = AssetLoadState::Finalizing;

		if (!result->error.empty() || !result->payload.has_value()) {
			CompleteAssetLoad(storage_key, false, std::move(result->error));
			continue;
		}

		bool success{ true };
		std::string error;

		try {
			std::visit(
				[this, &result, &success, &error]<typename T>(T&& prepared) {
					using Value = std::remove_cvref_t<T>;
					const auto& asset{ result->asset };
					auto absolute_path{ ResolveAssetPath(asset) };

					if constexpr (std::same_as<Value, AsyncLoader::PreparedTexture>) {
						Texture texture{ CreateAsset(), true };
						texture.GetEntity().Add<impl::TextureObject>(CreateTexture(
							*prepared.surface, kDefaultTextureStorageFormat
						));
						impl::AddAssetKey(texture.GetEntity(), asset.key, absolute_path);
					} else if constexpr (std::same_as<Value, AsyncLoader::PreparedAudio>) {
						auto audio{ CreateAudio(true, prepared.file_path) };
						impl::AddAssetKey(audio.GetEntity(), asset.key, prepared.file_path);
					} else if constexpr (std::same_as<Value, AsyncLoader::PreparedFont>) {
						PTGN_ASSERT(font_, "FontSystem must be connected before loading fonts");
						auto font{ CreateFont(true, std::move(prepared.data)) };
						impl::AddAssetKey(font.GetEntity(), asset.key, prepared.file_path);
					} else if constexpr (std::same_as<Value, AsyncLoader::PreparedJson>) {
						jsons_.insert_or_assign(
							Hash(asset.key),
							impl::JsonAssetData{
								.key = asset.key,
								.source_path = asset.source_path,
								.value = std::move(prepared.value),
							}
						);
					} else if constexpr (std::same_as<Value, AsyncLoader::PreparedPrefab>) {
						prefabs_.insert_or_assign(
							Hash(asset.key),
							impl::PrefabAssetData{
								.key = PrefabKey{ asset.key },
								.file_path = absolute_path,
								.source_path = asset.source_path,
								.value = std::move(prepared.value),
							}
						);
					} else if constexpr (std::same_as<Value, AsyncLoader::PreparedShader>) {
						auto disk_source{ FileToString(absolute_path) };
						auto validation{ ValidateShaderSource(ShaderKey{ asset.key }, disk_source) };
						auto& shader_state{ runtime_states_[MakeAssetStorageKey(asset.key, AssetKind::Shader)] };
						shader_state.compile_error = !validation.success;
						shader_state.compile_log = validation.log;
						if (!validation.success) {
							success = false;
							error = validation.log;
							PTGN_WARN("Shader failed to compile and was left unavailable: ", asset.key);
							return;
						}
						auto shader{ CreateShader(true, prepared.source, asset.key.value) };
						impl::AddAssetKey(shader.GetEntity(), asset.key, absolute_path);
					} else if constexpr (std::same_as<Value, AsyncLoader::PreparedScene>) {
						// Scene files are catalog assets but not GPU/runtime resources.
					}
				},
				std::move(result->payload.value())
			);
		} catch (const std::exception& exception) {
			success = false;
			error = exception.what();
		}

		CompleteAssetLoad(storage_key, success, std::move(error));
	}

	std::erase_if(active_batches_, [](const auto& weak_batch) {
		auto batch{ weak_batch.lock() };
		if (!batch) {
			return true;
		}
		std::scoped_lock lock{ batch->mutex };
		return batch->progress.IsComplete();
	});

	std::erase_if(project_load_tickets_, [](const auto& ticket) {
		return ticket.IsComplete();
	});
	std::erase_if(manual_load_tickets_, [](const auto& ticket) {
		return ticket.IsComplete();
	});
	std::erase_if(manual_load_batches_, [](const auto& batch) {
		if (!batch) {
			return true;
		}
		std::scoped_lock lock{ batch->mutex };
		return batch->progress.IsComplete();
	});
}

void AssetManager::CompleteAssetLoad(
	impl::AssetStorageKey storage_key,
	bool success,
	std::string error
) {
	auto state_it{ runtime_states_.find(storage_key) };
	if (state_it == runtime_states_.end()) {
		return;
	}

	auto& state{ state_it->second };
	state.load_state = success ? AssetLoadState::Loaded : AssetLoadState::Failed;
	state.error = std::move(error);

	const auto catalog_it{ catalog_.find(storage_key) };
	auto file_size{
		catalog_it == catalog_.end() ? 0 : state.metadata.file_size
	};
	auto active_asset{
		catalog_it == catalog_.end() ? std::string{} : catalog_it->second.key.value
	};

	for (auto& weak_batch : state.waiters) {
		auto batch{ weak_batch.lock() };
		if (!batch) {
			continue;
		}

		std::scoped_lock lock{ batch->mutex };
		++batch->progress.completed_assets;
		batch->progress.completed_bytes += file_size;
		batch->progress.active_asset = active_asset;
		if (!success) {
			++batch->progress.failed_assets;
		}
	}

	state.waiters.clear();

	if (success && state.reference_count == 0 && !state.globally_pinned &&
		!state.manually_pinned && catalog_it != catalog_.end()) {
		ForceUnload(catalog_it->second.key, catalog_it->second.kind);
	}
}

void AssetManager::QueueAssetLoad(
	const SerializedAsset& asset,
	const std::shared_ptr<impl::AssetLoadBatchState>& batch
) {
	auto storage_key{ MakeAssetStorageKey(asset.key, asset.kind) };
	auto& state{ runtime_states_[storage_key] };
	state.waiters.emplace_back(batch);

	if (state.load_state == AssetLoadState::Loaded) {
		CompleteAssetLoad(storage_key, true);
		return;
	}
	if (state.load_state == AssetLoadState::Queued ||
		state.load_state == AssetLoadState::Loading ||
		state.load_state == AssetLoadState::Finalizing) {
		return;
	}

	state.load_state = AssetLoadState::Queued;
	state.error.clear();

	async_loader_->Queue(AsyncLoader::Job{
		.asset = asset,
		.absolute_path = ResolveAssetPath(asset),
		.project_root = project_root_.value_or(GetWorkingDirectory()),
	});
	state.load_state = AssetLoadState::Loading;
}

impl::AssetLoadTicket AssetManager::AcquireDependenciesAsync(
	std::span<const AssetKey> dependencies
) {
	auto expanded_dependencies{ ExpandDependencies(dependencies) };
	std::vector<SerializedAsset> available_assets;
	std::vector<AssetKey> available_dependencies;

	for (const auto& key : expanded_dependencies) {
		bool found{ false };
		for (const auto& [_, asset] : catalog_) {
			if (asset.key != key) {
				continue;
			}
			bool already_added{
				std::ranges::any_of(available_assets, [&](const SerializedAsset& candidate) {
					return candidate.key == asset.key && candidate.kind == asset.kind;
				})
			};
			if (!already_added) {
				available_assets.emplace_back(asset);
			}
			found = true;
		}
		if (!found) {
			PTGN_WARN(
				"Asset dependency is missing from the project catalog: ",
				key,
				". Texture requests use the purple fallback; other asset requests remain unavailable."
			);
			continue;
		}
		AddUnique(available_dependencies, key);
	}

	auto batch{ std::make_shared<impl::AssetLoadBatchState>() };
	{
		std::scoped_lock lock{ batch->mutex };
		batch->progress.total_assets = available_assets.size();
	}

	for (const auto& asset : available_assets) {
		auto& state{ runtime_states_[MakeAssetStorageKey(asset.key, asset.kind)] };
		++state.reference_count;
		{
			std::scoped_lock lock{ batch->mutex };
			batch->progress.total_bytes += state.metadata.file_size;
		}
		QueueAssetLoad(asset, batch);
	}

	active_batches_.emplace_back(batch);
	return impl::AssetLoadTicket{ *this, std::move(batch), std::move(available_dependencies) };
}

impl::AssetLoadProgress AssetManager::GetActiveLoadProgress() const {
	impl::AssetLoadProgress aggregate;

	for (const auto& weak_batch : active_batches_) {
		auto batch{ weak_batch.lock() };
		if (!batch) {
			continue;
		}

		std::scoped_lock lock{ batch->mutex };
		aggregate.total_assets += batch->progress.total_assets;
		aggregate.completed_assets += batch->progress.completed_assets;
		aggregate.failed_assets += batch->progress.failed_assets;
		aggregate.total_bytes += batch->progress.total_bytes;
		aggregate.completed_bytes += batch->progress.completed_bytes;
		if (!batch->progress.active_asset.empty()) {
			aggregate.active_asset = batch->progress.active_asset;
		}
	}

	return aggregate;
}

bool AssetManager::IsLoading() const {
	auto progress{ GetActiveLoadProgress() };
	return progress.total_assets > 0 && !progress.IsComplete();
}

void AssetManager::ReleaseDependencies(std::span<const AssetKey> dependencies) noexcept {
	for (const auto& key : dependencies) {
		for (const auto& [storage_key, asset] : catalog_) {
			if (asset.key != key) {
				continue;
			}

			auto state_it{ runtime_states_.find(storage_key) };
			if (state_it == runtime_states_.end()) {
				continue;
			}

			auto& state{ state_it->second };
			if (state.reference_count > 0) {
				--state.reference_count;
			}

			if (state.reference_count == 0 && !state.globally_pinned &&
				!state.manually_pinned && state.load_state == AssetLoadState::Loaded) {
				ForceUnload(asset.key, asset.kind);
			}
		}
	}
}

void AssetManager::BeginAssetCapture(std::vector<AssetKey>& dependencies) {
	PTGN_ASSERT(captured_asset_dependencies_ == nullptr, "Asset dependency capture cannot be nested");
	captured_asset_dependencies_ = &dependencies;
}

void AssetManager::EndAssetCapture(std::vector<AssetKey>& dependencies) {
	PTGN_ASSERT(
		captured_asset_dependencies_ == &dependencies,
		"Attempting to end an asset dependency capture that is not active"
	);
	captured_asset_dependencies_ = nullptr;
}

void AssetManager::TrackAssetDependency(const AssetKey& key) {
	if (key.value.empty()) {
		return;
	}

	if (captured_asset_dependencies_) {
		AddUnique(*captured_asset_dependencies_, key);
	}
}

void AssetManager::TrackAssetLoad(
	const AssetKey& key,
	AssetKind kind,
	const path& source_path
) {
	TrackAssetDependency(key);

	if (key.value.empty() || kind == AssetKind::Unknown || source_path.empty()) {
		return;
	}

	path serialized_path{ source_path.lexically_normal() };

	if (source_path.is_absolute() && project_root_.has_value()) {
		std::error_code error;
		auto relative{
			std::filesystem::relative(source_path, project_root_.value(), error)
		};

		if (!error && !relative.empty() &&
			!relative.generic_string().starts_with("..")) {
			serialized_path = relative.lexically_normal();
		}
	}

	SerializedAsset asset{
		.key = key,
		.kind = kind,
		.source_path = std::move(serialized_path),
	};
	auto storage_key{ MakeAssetStorageKey(key, kind) };

	if (auto existing{ catalog_.find(storage_key) }; existing != catalog_.end()) {
		asset.shader = existing->second.shader;
	}

	path resolved_asset_path{ source_path.lexically_normal() };
	for (auto it{ catalog_.begin() }; it != catalog_.end();) {
		if (it->first == storage_key || it->second.kind != kind) {
			++it;
			continue;
		}

		path existing_path{ ResolveAssetPath(it->second) };
		std::error_code equivalent_error;
		bool same_file{
			FileExists(existing_path) && FileExists(resolved_asset_path) &&
			std::filesystem::equivalent(existing_path, resolved_asset_path, equivalent_error) &&
			!equivalent_error
		};

		if (!same_file) {
			++it;
			continue;
		}

		AssetKey duplicate_key{ it->second.key };
		if (!asset.shader && it->second.shader) {
			asset.shader = it->second.shader;
		}

		ForceUnload(duplicate_key, kind);
		runtime_states_.erase(it->first);
		it = catalog_.erase(it);

		for (auto& dependency : project_asset_dependencies_) {
			if (dependency == duplicate_key) {
				dependency = key;
			}
		}
		std::ranges::sort(
			project_asset_dependencies_,
			[](const AssetKey& lhs, const AssetKey& rhs) {
				return lhs.value < rhs.value;
			}
		);
		auto duplicate_dependencies{
			std::ranges::unique(project_asset_dependencies_)
		};
		project_asset_dependencies_.erase(
			duplicate_dependencies.begin(),
			duplicate_dependencies.end()
		);
	}

	catalog_.insert_or_assign(storage_key, asset);
	runtime_states_[storage_key].metadata = ProbeMetadata(asset);
}

bool AssetManager::RegisterCatalog(
	std::span<const SerializedAsset> assets,
	const Project& project
) {
	const std::vector<SerializedAsset> serialized_catalog{ assets.begin(), assets.end() };
	project_load_tickets_.clear();
	manual_load_tickets_.clear();
	manual_load_batches_.clear();
	active_batches_.clear();
	async_loader_.reset();
	for (const auto& [_, asset] : catalog_) {
		ForceUnload(asset.key, asset.kind);
	}

	path project_file{
		project.file_path.is_absolute()
			? project.file_path.lexically_normal()
			: GetAbsolutePath(project.file_path).lexically_normal()
	};
	project_root_ = project_file.parent_path();
	asset_directory_ = (
		project.asset_directory.is_absolute()
			? project.asset_directory
			: project_root_.value() / project.asset_directory
	).lexically_normal();

	catalog_.clear();
	runtime_states_.clear();
	project_asset_dependencies_.clear();
	async_loader_ = std::make_unique<AsyncLoader>();

	EnsureProjectAssetTypeDirectories(asset_directory_.value());

	for (const auto& source_asset : assets) {
		PTGN_ASSERT(!source_asset.key.value.empty(), "Serialized asset key cannot be empty");
		PTGN_ASSERT(
			source_asset.kind != AssetKind::Unknown,
			"Serialized asset kind cannot be Unknown for key: ",
			source_asset.key
		);
		PTGN_ASSERT(
			!source_asset.source_path.empty(),
			"Serialized asset path cannot be empty for key: ",
			source_asset.key
		);

		SerializedAsset asset{ source_asset };

		if (asset.kind != AssetKind::Scene) {
			if (auto localized{
					LocalizeProjectAsset(asset.key, asset.kind, asset.source_path)
				}) {
				std::error_code error;
				path relative{
					std::filesystem::relative(localized.value(), project_root_.value(), error)
				};
				if (!error && !relative.empty() &&
					!relative.generic_string().starts_with("..")) {
					asset.source_path = relative.lexically_normal();
				}
			}
		}

		if (asset.kind == AssetKind::Shader) {
			auto shader_path{ ResolveAssetPath(asset) };
			if (FileExists(shader_path)) {
				NormalizeShaderProgramConfiguration(asset, FileToString(shader_path));
			}
		}

		auto storage_key{ MakeAssetStorageKey(asset.key, asset.kind) };
		catalog_.insert_or_assign(storage_key, asset);
		runtime_states_[storage_key].metadata = ProbeMetadata(asset);
	}

	RefreshCatalogFromDisk();
	SetProjectAssetDependencies(project.preload_assets);
	auto current_catalog{ GetCatalog() };
	return CatalogDiffersFrom(serialized_catalog, current_catalog);
}

void AssetManager::RefreshCatalogFromDisk() {
	if (!asset_directory_.has_value() || !project_root_.has_value()) {
		return;
	}

	EnsureProjectAssetTypeDirectories(asset_directory_.value());

	auto collect_files = [&]() {
		std::vector<path> files;
		std::error_code error;
		for (std::filesystem::recursive_directory_iterator it{ asset_directory_.value(), error }, end;
			 !error && it != end; it.increment(error)) {
			std::error_code entry_error;
			if (it->is_regular_file(entry_error) && !entry_error) {
				files.emplace_back(it->path());
			}
		}
		return files;
	};

	for (const auto& file : collect_files()) {
		auto kind{ DetectProjectAssetKind(file) };
		if (kind == AssetKind::Unknown) {
			continue;
		}

		auto normalized{ NormalizeProjectAssetFile(kind, file) };
		if (!normalized.has_value() || normalized.value() == file.lexically_normal()) {
			continue;
		}

		for (auto& [_, asset] : catalog_) {
			if (ResolveAssetPath(asset).lexically_normal() != file.lexically_normal()) {
				continue;
			}

			std::error_code relative_error;
			auto relative{
				std::filesystem::relative(normalized.value(), project_root_.value(), relative_error)
			};
			if (!relative_error) {
				asset.source_path = relative.lexically_normal();
			}
			break;
		}
	}

	for (auto it{ catalog_.begin() }; it != catalog_.end();) {
		auto& asset{ it->second };
		auto& state{ runtime_states_[it->first] };
		auto file_path{ ResolveAssetPath(asset) };
		if (FileExists(file_path)) {
			if (asset.kind == AssetKind::Shader) {
				NormalizeShaderProgramConfiguration(asset, FileToString(file_path));
			}
			state.metadata = ProbeMetadata(asset);
			if (state.load_state != AssetLoadState::Failed || state.error == "Source file is missing") {
				state.error.clear();
				if (state.load_state == AssetLoadState::Failed) {
					state.load_state = AssetLoadState::Unloaded;
				}
			}
			++it;
			continue;
		}

		bool in_use{
			state.reference_count > 0 || state.globally_pinned ||
			state.load_state == AssetLoadState::Queued ||
			state.load_state == AssetLoadState::Loading ||
			state.load_state == AssetLoadState::Finalizing
		};
		if (in_use) {
			state.error = "Source file is missing";
			++it;
			continue;
		}

		ForceUnload(asset.key, asset.kind);
		bool has_other_kind{
			std::ranges::any_of(catalog_, [&](const auto& entry) {
				return entry.first != it->first && entry.second.key == asset.key;
			})
		};
		if (!has_other_kind) {
			std::erase(project_asset_dependencies_, asset.key);
		}
		runtime_states_.erase(it->first);
		it = catalog_.erase(it);
	}

	std::unordered_set<std::string> catalog_paths;
	for (const auto& [_, asset] : catalog_) {
		catalog_paths.insert(asset.source_path.lexically_normal().generic_string());
	}

	for (const auto& file : collect_files()) {
		auto kind{ DetectProjectAssetKind(file) };
		if (kind == AssetKind::Unknown) {
			continue;
		}

		std::error_code error;
		auto relative_to_project{ std::filesystem::relative(file, project_root_.value(), error) };
		if (error) {
			continue;
		}
		auto normalized_path{ relative_to_project.lexically_normal().generic_string() };
		if (catalog_paths.contains(normalized_path)) {
			continue;
		}

		auto relative_to_assets{ std::filesystem::relative(file, asset_directory_.value(), error) };
		if (error) {
			continue;
		}

		auto key{ MakeUniqueAssetKey(kind, relative_to_assets) };
		SerializedAsset asset{
			.key = key,
			.kind = kind,
			.source_path = relative_to_project.lexically_normal(),
		};
		if (kind == AssetKind::Shader) {
			NormalizeShaderProgramConfiguration(asset, FileToString(file));
		}

		auto storage_key{ MakeAssetStorageKey(key, kind) };
		catalog_.emplace(storage_key, asset);
		runtime_states_[storage_key].metadata = ProbeMetadata(asset);
		catalog_paths.insert(normalized_path);
	}
}

std::vector<SerializedAsset> AssetManager::GetCatalog() const {
	std::vector<SerializedAsset> assets;
	assets.reserve(catalog_.size());

	for (const auto& [_, asset] : catalog_) {
		assets.emplace_back(asset);
	}

	std::ranges::sort(assets, [](const SerializedAsset& lhs, const SerializedAsset& rhs) {
		if (lhs.source_path != rhs.source_path) {
			return lhs.source_path.generic_string() < rhs.source_path.generic_string();
		}
		if (lhs.kind != rhs.kind) {
			return lhs.kind < rhs.kind;
		}
		return lhs.key < rhs.key;
	});

	return assets;
}

std::optional<SerializedAsset> AssetManager::GetCatalogAsset(const AssetKey& key) const {
	std::optional<SerializedAsset> result;
	for (const auto& [_, asset] : catalog_) {
		if (asset.key != key) {
			continue;
		}
		if (result.has_value()) {
			return std::nullopt;
		}
		result = asset;
	}
	return result;
}

std::optional<SerializedAsset> AssetManager::GetCatalogAsset(
	const AssetKey& key,
	AssetKind kind
) const {
	const auto it{ catalog_.find(MakeAssetStorageKey(key, kind)) };
	return it == catalog_.end() ? std::nullopt : std::optional<SerializedAsset>{ it->second };
}

void AssetManager::PinProjectDependency(const AssetKey& key) {
	for (const auto& [storage_key, asset] : catalog_) {
		if (asset.key == key) {
			runtime_states_[storage_key].globally_pinned = true;
		}
	}
}

void AssetManager::AddProjectAssetDependency(AssetKey key) {
	if (key.value.empty() || std::ranges::contains(project_asset_dependencies_, key)) {
		return;
	}

	if (!HasCatalogAsset(key)) {
		PTGN_WARN(
			"Cannot add an asset to the project preload list because it is missing from the catalog: ",
			key
		);
		return;
	}

	PinProjectDependency(key);
	project_asset_dependencies_.emplace_back(std::move(key));
}

void AssetManager::PreloadProjectAsset(AssetKey key) {
	AddProjectAssetDependency(key);
	if (!HasCatalogAsset(key)) {
		return;
	}

	const AssetKey dependency{ key };
	project_load_tickets_.emplace_back(AcquireDependenciesAsync(
		std::span<const AssetKey>{ &dependency, 1 }
	));
}

void AssetManager::RemoveProjectAssetDependency(const AssetKey& key) {
	if (std::erase(project_asset_dependencies_, key) == 0) {
		return;
	}

	for (const auto& [storage_key, asset] : catalog_) {
		if (asset.key != key) {
			continue;
		}
		auto state_it{ runtime_states_.find(storage_key) };
		if (state_it == runtime_states_.end()) {
			continue;
		}
		auto& state{ state_it->second };
		state.globally_pinned = false;
		if (state.reference_count == 0 && !state.manually_pinned &&
			state.load_state == AssetLoadState::Loaded) {
			ForceUnload(asset.key, asset.kind);
		}
	}
}

void AssetManager::AddProjectAssetDependencies(std::span<const AssetKey> dependencies) {
	for (const auto& key : dependencies) {
		AddProjectAssetDependency(key);
	}
}

void AssetManager::SetProjectAssetDependencies(std::span<const AssetKey> dependencies) {
	const auto old_dependencies{ project_asset_dependencies_ };
	for (const auto& key : old_dependencies) {
		RemoveProjectAssetDependency(key);
	}
	AddProjectAssetDependencies(dependencies);
}

const std::vector<AssetKey>& AssetManager::GetProjectAssetDependencies() const {
	return project_asset_dependencies_;
}

bool AssetManager::HasCatalogAsset(const AssetKey& key) const {
	return std::ranges::any_of(catalog_, [&](const auto& entry) {
		return entry.second.key == key;
	});
}

bool AssetManager::HasCatalogAsset(const AssetKey& key, AssetKind kind) const {
	return catalog_.contains(MakeAssetStorageKey(key, kind));
}

path AssetManager::ResolvePathBackedAssetSource(
	const path& source_path
) const {
	if (source_path.empty()) {
		return {};
	}

	if (source_path.is_absolute()) {
		return source_path.lexically_normal();
	}

	if (project_root_) {
		path project_candidate{
			(project_root_.value() /
			 source_path)
				.lexically_normal()
		};

		if (FileExists(project_candidate)) {
			return project_candidate;
		}
	}

	if (FileExists(source_path)) {
		return GetAbsolutePath(
			source_path
		).lexically_normal();
	}

	const auto& build_info{
		impl::GetBuildInfo()
	};
	path runtime_candidate{
		(build_info.runtime_root /
		 source_path)
			.lexically_normal()
	};

	if (FileExists(runtime_candidate)) {
		return runtime_candidate;
	}

	return source_path.lexically_normal();
}

std::optional<path> AssetManager::NormalizeProjectAssetFile(
	AssetKind kind,
	const path& source_path
) {
	if (!asset_directory_.has_value() || kind == AssetKind::Unknown || !FileExists(source_path)) {
		return std::nullopt;
	}

	path normalized_source{ source_path.lexically_normal() };
	if (!IsWithinDirectory(normalized_source, asset_directory_.value())) {
		return std::nullopt;
	}

	path canonical_directory{
		(asset_directory_.value() / ProjectAssetFolderName(kind)).lexically_normal()
	};
	EnsureDirectory(canonical_directory);

	if (IsWithinDirectory(normalized_source, canonical_directory)) {
		return normalized_source;
	}

	std::error_code relative_error;
	path relative_to_assets{
		std::filesystem::relative(normalized_source, asset_directory_.value(), relative_error)
	};

	path destination_directory{ canonical_directory };
	if (!relative_error && !relative_to_assets.empty()) {
		auto component{ relative_to_assets.begin() };
		if (component != relative_to_assets.end() &&
			ProjectAssetKindFromFolderName(component->string()).has_value()) {
			++component;
			path preserved_subdirectory;
			for (; component != relative_to_assets.end(); ++component) {
				auto next{ component };
				++next;
				if (next == relative_to_assets.end()) {
					break;
				}
				preserved_subdirectory /= *component;
			}
			if (!preserved_subdirectory.empty()) {
				destination_directory /= preserved_subdirectory;
			}
		}
	}

	EnsureDirectory(destination_directory);
	path destination{ MakeUniqueDestinationPath(destination_directory, normalized_source) };
	std::error_code error;
	std::filesystem::rename(normalized_source, destination, error);
	if (error) {
		PTGN_WARN(
			"Failed to move asset into its canonical project folder: ",
			normalized_source.string(),
			" -> ",
			destination.string(),
			" | ",
			error.message()
		);
		return std::nullopt;
	}

	return destination;
}

std::optional<path> AssetManager::LocalizeProjectAsset(
	const AssetKey& key,
	AssetKind kind,
	const path& source_path
) {
	path resolved_source;
	if (project_root_ && asset_directory_) {
		if (const auto existing{ catalog_.find(MakeAssetStorageKey(key, kind)) }; existing != catalog_.end()) {
			auto existing_path{ ResolveAssetPath(existing->second) };
			if (FileExists(existing_path)) {
				resolved_source = existing_path;
			}
		}
	}

	if (resolved_source.empty()) {
		resolved_source = ResolvePathBackedAssetSource(source_path);
	}
	if (!FileExists(resolved_source)) {
		return std::nullopt;
	}
	if (!project_root_ || !asset_directory_) {
		return resolved_source;
	}

	if (IsWithinDirectory(resolved_source, asset_directory_.value())) {
		return NormalizeProjectAssetFile(kind, resolved_source);
	}

#if defined(__EMSCRIPTEN__)
	return resolved_source;
#else
	path import_directory{
		(asset_directory_.value() / ProjectAssetFolderName(kind)).lexically_normal()
	};
	EnsureDirectory(import_directory);

	if (auto existing_copy{
			FindExistingImportedAssetCopy(import_directory, resolved_source)
		}) {
		return existing_copy;
	}

	path destination{ MakeUniqueDestinationPath(import_directory, resolved_source) };

	std::error_code error;
	std::filesystem::copy_file(
		resolved_source,
		destination,
		std::filesystem::copy_options::none,
		error
	);
	if (error) {
		PTGN_WARN(
			"Failed to copy external asset into project: ",
			resolved_source.string(),
			" -> ",
			destination.string(),
			" | ",
			error.message()
		);
		return std::nullopt;
	}
	return destination;
#endif
}

path AssetManager::ResolveAssetPath(const SerializedAsset& asset) const {
	if (asset.source_path.is_absolute()) {
		return asset.source_path.lexically_normal();
	}

	if (project_root_.has_value()) {
		auto project_path{
			(project_root_.value() / asset.source_path).lexically_normal()
		};
		if (FileExists(project_path)) {
			return project_path;
		}
	}

	auto external_path{ asset.source_path.lexically_normal() };
	if (FileExists(external_path) || !project_root_.has_value()) {
		return external_path;
	}

	return (project_root_.value() / asset.source_path).lexically_normal();
}

impl::AssetMetadata AssetManager::ProbeMetadata(const SerializedAsset& asset) const {
	impl::AssetMetadata metadata;
	auto file_path{ ResolveAssetPath(asset) };
	if (!FileExists(file_path)) {
		return metadata;
	}

	metadata.file_size = SafeFileSize(file_path);

	switch (asset.kind) {
		using enum AssetKind;
		case Texture: metadata.dimensions = ProbeImageDimensions(file_path); break;
		case Audio: metadata.duration_seconds = ProbeAudioDuration(file_path); break;
		case Shader: metadata.shader_stages = DetectShaderStages(FileToString(file_path)); break;
		case Font:
			if (HasExtension(file_path, ".png")) {
				metadata.dimensions = ProbeImageDimensions(file_path);
			}
			break;
		case Json: [[fallthrough]];
		case Prefab: [[fallthrough]];
		case Scene: [[fallthrough]];
		case Unknown: break;
	}

	return metadata;
}

AssetKey AssetManager::MakeUniqueAssetKey(AssetKind kind, const path& source_path) const {
	std::string sanitized{ SanitizeKeySegment(source_path.stem().string()) };
	std::string base{ StripGeneratedAssetMetadataSuffix(sanitized) };
	AssetKey key{ base.empty() ? std::string{ "asset" } : base };

	for (std::size_t suffix{ 2 }; HasCatalogAsset(key, kind); ++suffix) {
		key = (base.empty() ? std::string{ "asset" } : base) + "_" + std::to_string(suffix);
	}

	return key;
}

std::optional<AssetKey> AssetManager::ImportAsset(const path& source_file) {
	return ImportAsset(source_file, {});
}

std::optional<AssetKey> AssetManager::ImportAsset(
	const path& source_file,
	const path& destination_directory
) {
	if (!asset_directory_.has_value() || !project_root_.has_value()) {
		return std::nullopt;
	}

	auto kind{ DetectProjectAssetKind(source_file) };
	if (kind == AssetKind::Unknown) {
		return std::nullopt;
	}

	EnsureProjectAssetTypeDirectories(asset_directory_.value());

	path destination;
	path resolved_source{
		source_file.is_absolute()
			? source_file.lexically_normal()
			: ResolvePathBackedAssetSource(source_file).lexically_normal()
	};
	if (!FileExists(resolved_source)) {
		return std::nullopt;
	}

	if (IsWithinDirectory(resolved_source, asset_directory_.value())) {
		auto normalized{ NormalizeProjectAssetFile(kind, resolved_source) };
		if (!normalized.has_value()) {
			return std::nullopt;
		}
		destination = normalized.value();

		for (const auto& [_, asset] : catalog_) {
			if (ResolveAssetPath(asset).lexically_normal() == destination) {
				return asset.key;
			}
		}
	} else {
		path relative_destination{ destination_directory.lexically_normal() };
		path destination_root{
			(asset_directory_.value() / ProjectAssetFolderName(kind)).lexically_normal()
		};
		if (!relative_destination.empty() && relative_destination != "." &&
			AssetDirectoryKind(relative_destination) == kind) {
			destination_root = (asset_directory_.value() / relative_destination).lexically_normal();
		}
		EnsureDirectory(destination_root);
		destination = MakeUniqueDestinationPath(destination_root, resolved_source);

		std::error_code copy_error;
		std::filesystem::copy_file(
			resolved_source,
			destination,
			std::filesystem::copy_options::none,
			copy_error
		);
		if (copy_error) {
			PTGN_WARN("Failed to import asset: ", copy_error.message());
			return std::nullopt;
		}
	}

	std::error_code error;
	auto relative_to_project{
		std::filesystem::relative(destination, project_root_.value(), error)
	};
	if (error) {
		return std::nullopt;
	}
	auto relative_to_assets{
		std::filesystem::relative(destination, asset_directory_.value(), error)
	};
	if (error) {
		return std::nullopt;
	}

	auto key{ MakeUniqueAssetKey(kind, relative_to_assets) };
	SerializedAsset asset{
		.key = key,
		.kind = kind,
		.source_path = relative_to_project.lexically_normal(),
	};
	if (kind == AssetKind::Shader) {
		NormalizeShaderProgramConfiguration(asset, FileToString(destination));
	}

	auto storage_key{ MakeAssetStorageKey(key, kind) };
	catalog_.insert_or_assign(storage_key, asset);
	runtime_states_[storage_key].metadata = ProbeMetadata(asset);
	return key;
}

bool AssetManager::MoveAsset(const AssetKey& key, const path& destination_directory) {
	auto asset{ GetCatalogAsset(key) };
	return asset.has_value() && MoveAsset(key, asset->kind, destination_directory);
}

bool AssetManager::MoveAsset(
	const AssetKey& key,
	AssetKind kind,
	const path& destination_directory
) {
	if (!asset_directory_.has_value() || !project_root_.has_value()) {
		return false;
	}

	auto storage_key{ MakeAssetStorageKey(key, kind) };
	auto catalog_it{ catalog_.find(storage_key) };
	if (catalog_it == catalog_.end()) {
		return false;
	}


	path relative_destination{ destination_directory.lexically_normal() };
	if (relative_destination.empty() || relative_destination == ".") {
		relative_destination = path{ ProjectAssetFolderName(kind) };
	}
	if (AssetDirectoryKind(relative_destination) != kind) {
		return false;
	}

	path destination_directory_absolute{
		(asset_directory_.value() / relative_destination).lexically_normal()
	};
	if (!IsWithinDirectory(destination_directory_absolute, asset_directory_.value())) {
		return false;
	}
	EnsureDirectory(destination_directory_absolute);

	path source{ ResolveAssetPath(catalog_it->second) };
	if (!FileExists(source)) {
		return false;
	}
	if (source.parent_path() == destination_directory_absolute) {
		return true;
	}

	path destination{
		(destination_directory_absolute / source.filename()).lexically_normal()
	};
	if (FileExists(destination)) {
		return false;
	}

	path old_project_relative{ catalog_it->second.source_path.lexically_normal() };
	std::error_code error;
	std::filesystem::rename(source, destination, error);
	if (error) {
		return false;
	}

	path new_project_relative{
		std::filesystem::relative(destination, project_root_.value(), error)
	};
	if (error) {
		std::filesystem::rename(destination, source, error);
		return false;
	}

	catalog_it->second.source_path = new_project_relative.lexically_normal();
	for (auto& [_, shader_asset] : catalog_) {
		if (!shader_asset.shader.has_value()) {
			continue;
		}
		ReplaceShaderPathReference(
			shader_asset.shader->vertex,
			old_project_relative,
			catalog_it->second.source_path
		);
		ReplaceShaderPathReference(
			shader_asset.shader->fragment,
			old_project_relative,
			catalog_it->second.source_path
		);
	}
	if (kind == AssetKind::Shader) {
		NormalizeShaderProgramConfiguration(catalog_it->second, FileToString(destination));
	}
	runtime_states_[storage_key].metadata = ProbeMetadata(catalog_it->second);
	return true;
}

bool AssetManager::MoveAssetDirectory(
	const path& source_directory,
	const path& destination_directory
) {
	if (!asset_directory_.has_value() || !project_root_.has_value() ||
		!IsSafeAssetRelativePath(source_directory) ||
		!IsSafeAssetRelativePath(destination_directory)) {
		return false;
	}

	path source_relative{ source_directory.lexically_normal() };
	path destination_relative{ destination_directory.lexically_normal() };
	auto source_kind{ AssetDirectoryKind(source_relative) };
	auto destination_kind{ AssetDirectoryKind(destination_relative) };
	if (!source_kind.has_value() || source_kind != destination_kind ||
		source_relative == path{ ProjectAssetFolderName(source_kind.value()) } ||
		destination_relative == path{ ProjectAssetFolderName(source_kind.value()) }) {
		return false;
	}

	path source_absolute{ (asset_directory_.value() / source_relative).lexically_normal() };
	path destination_absolute{ (asset_directory_.value() / destination_relative).lexically_normal() };
	if (!IsWithinDirectory(source_absolute, asset_directory_.value()) ||
		!IsWithinDirectory(destination_absolute, asset_directory_.value())) {
		return false;
	}

	std::error_code error;
	if (!std::filesystem::is_directory(source_absolute, error) || error) {
		return false;
	}
	error.clear();
	if (std::filesystem::exists(destination_absolute, error) || error) {
		return false;
	}

	error.clear();
	path old_project_directory{
		std::filesystem::relative(source_absolute, project_root_.value(), error)
	};
	if (error) {
		return false;
	}
	error.clear();
	path new_project_directory{
		std::filesystem::relative(destination_absolute, project_root_.value(), error)
	};
	if (error) {
		return false;
	}

	EnsureDirectory(destination_absolute.parent_path());
	error.clear();
	std::filesystem::rename(source_absolute, destination_absolute, error);
	if (error) {
		return false;
	}

	for (auto& [_, asset] : catalog_) {
		path old_absolute{ (project_root_.value() / asset.source_path).lexically_normal() };
		if (!IsWithinDirectory(old_absolute, source_absolute)) {
			continue;
		}
		path tail{ old_absolute.lexically_relative(source_absolute) };
		asset.source_path = (new_project_directory / tail).lexically_normal();
	}

	for (auto& [_, shader_asset] : catalog_) {
		if (!shader_asset.shader.has_value()) {
			continue;
		}
		ReplaceShaderDirectoryReference(
			shader_asset.shader->vertex,
			old_project_directory,
			new_project_directory
		);
		ReplaceShaderDirectoryReference(
			shader_asset.shader->fragment,
			old_project_directory,
			new_project_directory
		);
	}
	return true;
}

bool AssetManager::RenameAssetKey(const AssetKey& key, AssetKey new_key) {
	auto asset{ GetCatalogAsset(key) };
	return asset.has_value() && RenameAssetKey(key, asset->kind, std::move(new_key));
}

bool AssetManager::RenameAssetKey(
	const AssetKey& key,
	AssetKind kind,
	AssetKey new_key
) {
	if (key.value.empty() || new_key.value.empty() || key == new_key ||
		HasCatalogAsset(new_key, kind)) {
		return false;
	}

	auto old_storage_key{ MakeAssetStorageKey(key, kind) };
	auto catalog_it{ catalog_.find(old_storage_key) };
	if (catalog_it == catalog_.end()) {
		return false;
	}
	const auto state_it{ runtime_states_.find(old_storage_key) };
	if (state_it != runtime_states_.end() &&
		(state_it->second.reference_count > 0 || state_it->second.globally_pinned ||
		 state_it->second.manually_pinned || state_it->second.load_state == AssetLoadState::Queued ||
		 state_it->second.load_state == AssetLoadState::Loading ||
		 state_it->second.load_state == AssetLoadState::Finalizing ||
		 state_it->second.load_state == AssetLoadState::Loaded)) {
		return false;
	}

	SerializedAsset asset{ catalog_it->second };
	RuntimeAssetState state{};
	if (state_it != runtime_states_.end()) {
		state = state_it->second;
	}
	catalog_.erase(catalog_it);
	runtime_states_.erase(old_storage_key);
	asset.key = new_key;
	auto new_storage_key{ MakeAssetStorageKey(new_key, kind) };
	catalog_.insert_or_assign(new_storage_key, std::move(asset));
	runtime_states_.insert_or_assign(new_storage_key, std::move(state));

	for (auto& dependency : project_asset_dependencies_) {
		if (dependency == key) {
			dependency = new_key;
		}
	}
	return true;
}

bool AssetManager::RestoreCatalogAsset(const SerializedAsset& asset) {
	if (asset.key.value.empty() || asset.kind == AssetKind::Unknown || asset.source_path.empty() ||
		HasCatalogAsset(asset.key, asset.kind) || !FileExists(ResolveAssetPath(asset))) {
		return false;
	}
	auto storage_key{ MakeAssetStorageKey(asset.key, asset.kind) };
	catalog_.insert_or_assign(storage_key, asset);
	auto& state{ runtime_states_[storage_key] };
	state = RuntimeAssetState{};
	state.metadata = ProbeMetadata(asset);
	return true;
}

bool AssetManager::DeleteAsset(const AssetKey& key, bool delete_file) {
	auto asset{ GetCatalogAsset(key) };
	return asset.has_value() && DeleteAsset(key, asset->kind, delete_file);
}

bool AssetManager::DeleteAsset(const AssetKey& key, AssetKind kind, bool delete_file) {
	auto storage_key{ MakeAssetStorageKey(key, kind) };
	auto catalog_it{ catalog_.find(storage_key) };
	if (catalog_it == catalog_.end() || kind == AssetKind::Scene) {
		return false;
	}

	auto state_it{ runtime_states_.find(storage_key) };
	if (state_it != runtime_states_.end()) {
		const auto& state{ state_it->second };
		if (state.reference_count > 0 || state.globally_pinned || state.manually_pinned ||
			state.load_state == AssetLoadState::Queued ||
			state.load_state == AssetLoadState::Loading ||
			state.load_state == AssetLoadState::Finalizing) {
			return false;
		}
	}

	ForceUnload(key, kind);

	if (delete_file) {
		std::error_code error;
		std::filesystem::remove(ResolveAssetPath(catalog_it->second), error);
		if (error) {
			return false;
		}
	}

	if (!std::ranges::any_of(catalog_, [&](const auto& entry) {
		return entry.first != storage_key && entry.second.key == key;
	})) {
		std::erase(project_asset_dependencies_, key);
	}
	catalog_.erase(catalog_it);
	runtime_states_.erase(storage_key);
	return true;
}

bool AssetManager::ConfigureShaderProgram(
	const ShaderKey& key,
	std::optional<std::string> vertex_source,
	std::optional<std::string> fragment_source
) {
	auto catalog_it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Shader)) };
	if (catalog_it == catalog_.end() || catalog_it->second.kind != AssetKind::Shader) {
		return false;
	}
	if (!vertex_source.has_value() && !fragment_source.has_value()) {
		catalog_it->second.shader.reset();
		return true;
	}
	if ((vertex_source.has_value() && vertex_source->empty()) ||
		(fragment_source.has_value() && fragment_source->empty())) {
		return false;
	}
	SerializedShaderProgram program;
	program.vertex = std::move(vertex_source);
	program.fragment = std::move(fragment_source);
	catalog_it->second.shader = std::move(program);
	return true;
}

std::optional<path> AssetManager::GetProjectRoot() const {
	return project_root_;
}

std::optional<path> AssetManager::GetAssetDirectory() const {
	return asset_directory_;
}

void AssetManager::Load(const SerializedAsset& asset) {
	const auto catalog_it{ catalog_.find(MakeAssetStorageKey(asset.key, asset.kind)) };
	const SerializedAsset& load_asset{
		catalog_it != catalog_.end() ? catalog_it->second : asset
	};
	auto source_path{ ResolveAssetPath(load_asset) };

	TrackAssetDependency(load_asset.key);

	if (load_asset.kind == AssetKind::Scene) {
		runtime_states_[MakeAssetStorageKey(load_asset.key, load_asset.kind)].load_state = AssetLoadState::Loaded;
		return;
	}

	if (load_asset.kind != AssetKind::Shader) {
		Load(load_asset.key, source_path, load_asset.kind);
		return;
	}
	
	auto source{ FileToString(source_path) };
	auto validation{ ValidateShaderSource(ShaderKey{ load_asset.key }, source) };
	auto& shader_state{ runtime_states_[MakeAssetStorageKey(load_asset.key, load_asset.kind)] };
	shader_state.compile_error = !validation.success;
	shader_state.compile_log = validation.log;
	if (!validation.success) {
		shader_state.load_state = AssetLoadState::Failed;
		shader_state.error = validation.log;
		PTGN_WARN("Shader failed to compile and was not loaded: ", load_asset.key);
		return;
	}
	if (!load_asset.shader.has_value()) {
		if (!HasVertexAndFragmentShader(source)) {
			PTGN_WARN(
				"Shader requires both stages or a configured engine stage: ",
				source_path.string()
			);
			return;
		}

		LoadShader(ShaderKey{ load_asset.key }, ShaderCode{ source }, load_asset.key.value);
		return;
	}

	const auto& program{ load_asset.shader.value() };
	if (program.vertex == std::optional<std::string>{ kShaderSourceToken } &&
		program.fragment == std::optional<std::string>{ kShaderSourceToken } &&
		HasVertexAndFragmentShader(source)) {
		LoadShader(ShaderKey{ load_asset.key }, ShaderCode{ source }, load_asset.key.value);
		return;
	}

	if (!program.vertex.has_value() || !program.fragment.has_value()) {
		PTGN_WARN(
			"Shader program requires both vertex and fragment stages: ",
			load_asset.key
		);
		return;
	}

	auto root{ project_root_.value_or(GetWorkingDirectory()) };
	LoadShader(
		ShaderKey{ load_asset.key },
		ShaderPair{
			.vertex = ResolveSerializedShaderStage(
				program.vertex.value(), source_path, root, ShaderStageMask::Vertex
			),
			.fragment = ResolveSerializedShaderStage(
				program.fragment.value(), source_path, root, ShaderStageMask::Fragment
			),
		},
		load_asset.key.value
	);
}

bool AssetManager::RegisterAsset(AssetKey key, const path& asset_path) {
	if (key.value.empty() || asset_path.empty()) {
		return false;
	}

	path source_path{ ResolvePathBackedAssetSource(asset_path) };
	if (!FileExists(source_path)) {
		PTGN_WARN("Cannot register asset because the file does not exist: ", source_path.string());
		return false;
	}

	auto kind{ DetectProjectAssetKind(source_path) };
	if (kind == AssetKind::Unknown || kind == AssetKind::Scene) {
		PTGN_WARN("Cannot register unsupported path-backed asset: ", source_path.string());
		return false;
	}

	auto storage_key{ MakeAssetStorageKey(key, kind) };
	auto& state{ runtime_states_[storage_key] };
	if (state.load_state == AssetLoadState::Loaded ||
		state.load_state == AssetLoadState::Queued ||
		state.load_state == AssetLoadState::Loading ||
		state.load_state == AssetLoadState::Finalizing) {
		PTGN_WARN("Cannot replace a resident or in-flight asset registration: ", key);
		return false;
	}

	TrackAssetDependency(key);

	path serialized_path{ source_path.lexically_normal() };
	if (source_path.is_absolute() && project_root_.has_value()) {
		std::error_code error;
		auto relative{ std::filesystem::relative(source_path, project_root_.value(), error) };
		if (!error && !relative.empty() && !relative.generic_string().starts_with("..")) {
			serialized_path = relative.lexically_normal();
		}
	}

	catalog_.insert_or_assign(
		storage_key,
		SerializedAsset{
			.key = key,
			.kind = kind,
			.source_path = std::move(serialized_path),
		}
	);

	state.load_state = AssetLoadState::Unloaded;
	state.error.clear();
	state.compile_error = false;
	state.compile_log.clear();
	state.metadata = impl::AssetMetadata{ .file_size = SafeFileSize(source_path) };
	return true;
}

void AssetManager::LoadAssetAsync(const AssetKey& key) {
	TrackAssetDependency(key);
	bool found{ false };
	for (const auto& [_, asset] : catalog_) {
		if (asset.key != key) {
			continue;
		}
		LoadAssetAsync(key, asset.kind);
		found = true;
	}
	if (!found) {
		PTGN_WARN("Cannot load asset because it is missing from the catalog: ", key);
	}
}

void AssetManager::LoadAssetAsync(const AssetKey& key, AssetKind kind) {
	TrackAssetDependency(key);
	auto storage_key{ MakeAssetStorageKey(key, kind) };
	auto catalog_it{ catalog_.find(storage_key) };
	if (catalog_it == catalog_.end()) {
		PTGN_WARN("Cannot load asset because it is missing from the catalog: ", key);
		return;
	}

	auto& state{ runtime_states_[storage_key] };
	state.manually_pinned = true;
	if (state.load_state == AssetLoadState::Loaded ||
		state.load_state == AssetLoadState::Queued ||
		state.load_state == AssetLoadState::Loading ||
		state.load_state == AssetLoadState::Finalizing) {
		return;
	}

	auto batch{ std::make_shared<impl::AssetLoadBatchState>() };
	{
		std::scoped_lock lock{ batch->mutex };
		batch->progress.total_assets = 1;
		batch->progress.total_bytes = state.metadata.file_size;
	}
	QueueAssetLoad(catalog_it->second, batch);
	active_batches_.emplace_back(batch);
	manual_load_batches_.emplace_back(std::move(batch));
}

void AssetManager::LoadDependencies(std::span<const AssetKey> dependencies) {
	for (const auto& key : ExpandDependencies(dependencies)) {
		TrackAssetDependency(key);
		bool found{ false };
		for (const auto& [_, asset] : catalog_) {
			if (asset.key != key) {
				continue;
			}
			found = true;
			if (!Has(key, asset.kind)) {
				Load(asset);
			}
		}
		if (!found) {
			PTGN_WARN("Asset dependency is missing from the catalog: ", key);
		}
	}
}

void AssetManager::LoadProjectAsset(AssetKey key, const path& asset_path) {
	AssetKey dependency{ key };
	Load(std::move(key), asset_path);

	if (!Has(dependency)) {
		PTGN_WARN("Project asset failed to load: ", dependency);
		return;
	}

	for (const auto& [storage_key, asset] : catalog_) {
		if (asset.key == dependency) {
			runtime_states_[storage_key].metadata = ProbeMetadata(asset);
		}
	}
	AddProjectAssetDependency(std::move(dependency));
}

impl::TextureObject AssetManager::CreateTexture(
	const impl::Surface& surface,
	TextureFormat storage_format,
	TextureParams params
) const {
	PTGN_ASSERT(
		surface.GetChannelCount() == GetChannelCount(storage_format),
		"Surface and texture storage format channel count must match"
	);
	return CreateTexture(
		surface.Data(),
		TextureDesc{
			.size{ surface.GetSize() },
			.format = storage_format,
			.params{ params },
		}
	);
}

impl::TextureObject AssetManager::CreateTexture(
	const std::uint8_t* pixel_data,
	TextureDesc desc
) const {
	return impl::RendererAccessor{ renderer_ }.CreateTexture(pixel_data, desc);
}

Texture AssetManager::CreateTexture(
	bool persistent,
	const path& asset_path,
	TextureFormat storage_format,
	TextureParams params
) {
	if (!FileExists(asset_path)) {
		PTGN_WARN("Cannot create texture from invalid path: ", asset_path.string());
		return Get<Texture>(AssetKey{ std::string{ kMissingTextureAssetKey } });
	}

	impl::Surface surface{ asset_path };

	Texture texture{ CreateAsset(), persistent };
	texture.GetEntity().Add<impl::TextureObject>(CreateTexture(surface, storage_format, params));
	return texture;
}

Texture AssetManager::CreateTexture(
	const path& asset_path,
	TextureFormat storage_format,
	TextureParams params
) {
	return CreateTexture(false, asset_path, storage_format, params);
}

Texture AssetManager::LoadTexture(
	TextureKey key,
	const path& asset_path,
	TextureFormat storage_format,
	TextureParams params
) {
	path source_path{ ResolvePathBackedAssetSource(asset_path) };

	AssetKind project_asset_kind{ AssetKind::Texture };
	if (FileExists(source_path) && impl::IsFontAtlasPng(source_path)) {
		project_asset_kind = AssetKind::Font;
	}

	if (project_root_ && asset_directory_) {
		if (auto localized{
				LocalizeProjectAsset(key, project_asset_kind, source_path)
			}) {
			source_path = localized.value();
		}
	}

	TrackAssetLoad(key, AssetKind::Texture, source_path);

	if (auto existing{ TryGet<Texture>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto texture{ CreateTexture(true, source_path, storage_format, params) };
	impl::AddAssetKey(texture.GetEntity(), key, source_path);
	runtime_states_[MakeAssetStorageKey(key, AssetKind::Texture)].load_state = AssetLoadState::Loaded;
	return texture;
}

impl::FontAtlasData AssetManager::PrepareFontAsset(const path& asset_path) {
	return FontSystem::PrepareFontAtlas(asset_path);
}

Font AssetManager::CreateFont(bool persistent, impl::FontAtlasData&& data) {
	PTGN_ASSERT(font_, "FontSystem must be connected before creating fonts");
	Font font{ CreateAsset(), persistent };
	font.GetEntity().Add<impl::FontAtlas>(renderer_, std::move(data));
	return font;
}

Font AssetManager::CreateFont(bool persistent, const path& asset_path) {
	return CreateFont(persistent, PrepareFontAsset(asset_path));
}

Font AssetManager::CreateFont(const path& asset_path) {
	return CreateFont(false, asset_path);
}

Font AssetManager::LoadFont(FontKey key, const path& asset_path) {
	path source_path{ ResolvePathBackedAssetSource(asset_path) };

	if (project_root_ && asset_directory_) {
		if (auto localized{
				LocalizeProjectAsset(key, AssetKind::Font, source_path)
			}) {
			source_path = localized.value();
		}
	}

	TrackAssetLoad(key, AssetKind::Font, source_path);

	if (auto existing{ TryGet<Font>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto font{ CreateFont(true, source_path) };
	impl::AddAssetKey(font.GetEntity(), key, source_path);
	runtime_states_[MakeAssetStorageKey(key, AssetKind::Font)].load_state = AssetLoadState::Loaded;
	return font;
}

Audio AssetManager::CreateAudio(bool persistent, const path& asset_path) {
	Audio audio{ CreateAsset(), persistent };
	audio.GetEntity().Add<impl::AudioObject>(asset_path);
	return audio;
}

Audio AssetManager::CreateAudio(const path& asset_path) {
	return CreateAudio(false, asset_path);
}

Audio AssetManager::LoadAudio(AudioKey key, const path& asset_path) {
	path source_path{ ResolvePathBackedAssetSource(asset_path) };

	if (project_root_ && asset_directory_) {
		if (auto localized{
				LocalizeProjectAsset(key, AssetKind::Audio, source_path)
			}) {
			source_path = localized.value();
		}
	}

	TrackAssetLoad(key, AssetKind::Audio, source_path);

	if (auto existing{ TryGet<Audio>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto audio{ CreateAudio(true, source_path) };
	impl::AddAssetKey(audio.GetEntity(), key, source_path);
	runtime_states_[MakeAssetStorageKey(key, AssetKind::Audio)].load_state = AssetLoadState::Loaded;
	return audio;
}

Shader AssetManager::CreateShader(
	bool persistent,
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::string_view shader_name
) {
	Shader shader{ CreateAsset(), persistent };
	shader.GetEntity().Add<impl::ShaderObject>(
		impl::RendererAccessor{ renderer_ }.CreateShader(source, shader_name)
	);
	return shader;
}

Shader AssetManager::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::string_view shader_name
) {
	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	auto validation{ ValidateShaderProgramSourceForLoad(source, max_texture_slots) };
	if (!validation.success) {
		PTGN_WARN(
			"Shader failed validation and was not created: ", shader_name, "\n", validation.log
		);
		return {};
	}
	return CreateShader(false, source, shader_name);
}

Shader AssetManager::LoadShader(
	ShaderKey key,
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::optional<std::string_view> shader_name
) {
	auto resolved_source{ source };
	std::optional<path> source_path;

	if (auto* shader_path{ std::get_if<ShaderPath>(&resolved_source) }) {
		path resolved_path{ ResolvePathBackedAssetSource(shader_path->path) };

		if (project_root_ && asset_directory_) {
			if (auto localized{
					LocalizeProjectAsset(key, AssetKind::Shader, resolved_path)
				}) {
				resolved_path = localized.value();
			}
		}

		shader_path->path = resolved_path;
		source_path = resolved_path;
		TrackAssetLoad(key, AssetKind::Shader, resolved_path);
	} else {
		TrackAssetDependency(key);
	}

	if (auto existing{ TryGet<Shader>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto shader{ CreateShader(true, resolved_source, shader_name.value_or(key.value)) };
	impl::AddAssetKey(shader.GetEntity(), key, source_path);
	runtime_states_[MakeAssetStorageKey(key, AssetKind::Shader)].load_state = AssetLoadState::Loaded;
	return shader;
}

Prefab& AssetManager::LoadPrefab(
	PrefabKey key,
	const path& file_path,
	const path& source_path
) {
	path resolved_source_path{ ResolvePathBackedAssetSource(source_path) };

	if (project_root_ && asset_directory_) {
		if (auto localized{
				LocalizeProjectAsset(key, AssetKind::Prefab, resolved_source_path)
			}) {
			resolved_source_path = localized.value();
		}
	}

	path resolved_file_path{
		file_path == source_path
			? resolved_source_path
			: ResolvePathBackedAssetSource(file_path)
	};

	TrackAssetLoad(key, AssetKind::Prefab, resolved_source_path);

	if (auto existing{ TryGet<Prefab>(key) }; existing.has_value()) {
		return existing->get();
	}

	auto prefab{ LoadPrefabFile(resolved_file_path) };
	prefab.key = key;

	auto [it, inserted]{ prefabs_.insert_or_assign(
		Hash(key),
		impl::PrefabAssetData{
			.key = key,
			.file_path = resolved_file_path,
			.source_path = resolved_source_path,
			.value = std::move(prefab),
		}
	) };
	(void)inserted;

	runtime_states_[MakeAssetStorageKey(key, AssetKind::Prefab)].load_state = AssetLoadState::Loaded;
	return it->second.value;
}

Prefab& AssetManager::SavePrefab(Prefab prefab, const path& prefab_path) {
	return SavePrefab(std::move(prefab), prefab_path, prefab_path);
}

Prefab& AssetManager::SavePrefab(
	Prefab prefab,
	const path& file_path,
	const path& source_path
) {
	SavePrefabFile(file_path, prefab);
	auto key{ prefab.key };

	auto [it, inserted]{ prefabs_.insert_or_assign(
		Hash(key),
		impl::PrefabAssetData{
			.key = key,
			.file_path = file_path,
			.source_path = source_path,
			.value = std::move(prefab),
		}
	) };
	(void)inserted;

	SerializedAsset serialized{
		.key = key,
		.kind = AssetKind::Prefab,
		.source_path = source_path,
	};
	auto storage_key{ MakeAssetStorageKey(key, AssetKind::Prefab) };
	catalog_.insert_or_assign(storage_key, serialized);
	runtime_states_[storage_key].load_state = AssetLoadState::Loaded;
	runtime_states_[storage_key].metadata = ProbeMetadata(serialized);
	return it->second.value;
}

bool AssetManager::SavePrefab(const PrefabKey& key) {
	auto it{ prefabs_.find(Hash(key)) };
	if (it == prefabs_.end()) {
		return false;
	}
	SavePrefabFile(it->second.file_path, it->second.value);
	return true;
}

bool AssetManager::RemovePrefab(const PrefabKey& key, bool remove_file) {
	return DeleteAsset(key, AssetKind::Prefab, remove_file);
}

std::vector<PrefabKey> AssetManager::GetPrefabKeys() const {
	std::vector<PrefabKey> keys;
	for (const auto& [_, asset] : catalog_) {
		if (asset.kind == AssetKind::Prefab) {
			keys.emplace_back(asset.key);
		}
	}
	std::ranges::sort(keys);
	return keys;
}

path AssetManager::GetPrefabPath(const PrefabKey& key) const {
	auto it{ catalog_.find(MakeAssetStorageKey(key, AssetKind::Prefab)) };
	if (it == catalog_.end()) {
		PTGN_WARN("Prefab is not in asset catalog: ", key);
		return {};
	}

	return ResolveAssetPath(it->second);
}

json& AssetManager::LoadJson(const JsonKey& key, const path& asset_path) {
	path source_path{ ResolvePathBackedAssetSource(asset_path) };

	if (project_root_ && asset_directory_) {
		if (auto localized{
				LocalizeProjectAsset(key, AssetKind::Json, source_path)
			}) {
			source_path = localized.value();
		}
	}

	TrackAssetLoad(key, AssetKind::Json, source_path);

	if (auto existing{ TryGet<json>(key) }; existing.has_value()) {
		return existing->get();
	}

	auto [it, inserted]{ jsons_.insert_or_assign(
		Hash(key),
		impl::JsonAssetData{
			.key = key,
			.source_path = source_path,
			.value = ptgn::LoadJson(source_path),
		}
	) };
	(void)inserted;

	runtime_states_[MakeAssetStorageKey(key, AssetKind::Json)].load_state = AssetLoadState::Loaded;
	return it->second.value;
}

json AssetManager::CreateJson(const path& json_path) const {
	return ptgn::LoadJson(json_path);
}

void AssetManager::LoadDirectory(const path& directory, bool recursive) {
	if (!IsDirectoryPath(directory.string())) {
		return;
	}

	auto load_entry = [this](const auto& entry) {
		if (!entry.is_regular_file()) {
			return;
		}
		auto kind{ impl::GetAssetKind(entry.path()) };
		if (kind == AssetKind::Unknown || kind == AssetKind::Scene) {
			return;
		}
		Load(AssetKey{ entry.path().stem().string() }, entry.path(), kind);
	};

	if (recursive) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
			load_entry(entry);
		}
	} else {
		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			load_entry(entry);
		}
	}
}

void AssetManager::LoadManifest(const path& asset_manifest_file) {
	auto manifest = ptgn::LoadJson(asset_manifest_file);
	PTGN_ASSERT(manifest.is_object(), "Asset manifest must be an object");

	for (const auto& [key, value] : manifest.items()) {
		if (value.is_string()) {
			Load(AssetKey{ key }, path{ value.get<std::string>() });
			continue;
		}

		if (value.is_array() && value.size() == 2) {
			Load(
				ShaderKey{ key },
				ShaderPair{
					.vertex = value[0].get<std::string>(),
					.fragment = value[1].get<std::string>(),
				}
			);
		}
	}
}

void AssetManager::Load(
	const std::vector<std::pair<AssetKey, std::variant<path, ShaderCode, ShaderPair>>>&
		asset_keys_and_paths
) {
	for (const auto& [asset_key, asset_variant] : asset_keys_and_paths) {
		std::visit([this, &asset_key](const auto& value) { Load(asset_key, value); }, asset_variant);
	}
}

void AssetManager::Load(ShaderKey key, const ShaderCode& shader_code) {
	TrackAssetDependency(key);

	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	auto validation{ impl::ValidateShaderSource(shader_code.content, max_texture_slots) };

	if (DetectShaderStages(shader_code.content) != ShaderStageMask::VertexFragment) {
		validation.success = false;
		validation.log =
			"A directly loaded ShaderCode program requires both vertex and fragment stages.";
	}

	if (!validation.success) {
		auto& state{ runtime_states_[MakeAssetStorageKey(key, AssetKind::Shader)] };
		state.load_state = AssetLoadState::Failed;
		state.error = validation.log;
		state.compile_error = true;
		state.compile_log = validation.log;

		PTGN_WARN(
			"Shader failed validation and was not loaded: ",
			key,
			"\n",
			validation.log
		);
		return;
	}

	LoadShader(std::move(key), shader_code, std::nullopt);
}

void AssetManager::Load(ShaderKey key, const ShaderPair& shader_pair) {
	TrackAssetDependency(key);

	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	auto validation{ ValidateShaderPairForLoad(shader_pair, max_texture_slots) };

	if (!validation.success) {
		auto& state{ runtime_states_[MakeAssetStorageKey(key, AssetKind::Shader)] };
		state.load_state = AssetLoadState::Failed;
		state.error = validation.log;
		state.compile_error = true;
		state.compile_log = validation.log;

		PTGN_WARN(
			"Shader pair failed validation and was not loaded: ",
			key,
			"\n",
			validation.log
		);
		return;
	}

	LoadShader(std::move(key), shader_pair, std::nullopt);
}

void AssetManager::Load(
	AssetKey key,
	const path& asset_path,
	AssetKind kind
) {
	path source_path{
		ResolvePathBackedAssetSource(
			asset_path
		)
	};

	AssetKind project_asset_kind{ kind };
	if (project_asset_kind == AssetKind::Texture &&
		FileExists(source_path) &&
		impl::IsFontAtlasPng(source_path)) {
		project_asset_kind = AssetKind::Font;
	}

	if (project_root_ &&
		asset_directory_ &&
		kind != AssetKind::Scene &&
		kind != AssetKind::Unknown) {
		if (auto localized{
				LocalizeProjectAsset(
					key,
					project_asset_kind,
					source_path
				)
			}) {
			source_path = localized.value();
		}
	}

	if (!FileExists(source_path)) {
		PTGN_WARN(
			"Cannot load nonexistent asset: ",
			asset_path.string()
		);
		return;
	}

	switch (kind) {
		using enum AssetKind;

		case Texture:
			if (impl::IsFontAtlasPng(
					source_path
				)) {
				LoadFont(
					FontKey{ std::move(key) },
					source_path
				);
			} else {
				LoadTexture(
					TextureKey{ std::move(key) },
					source_path
				);
			}
			break;

		case Audio:
			LoadAudio(
				AudioKey{ std::move(key) },
				source_path
			);
			break;

		case Font:
			LoadFont(
				FontKey{ std::move(key) },
				source_path
			);
			break;

		case Json:
			LoadJson(
				JsonKey{ std::move(key) },
				source_path
			);
			break;

		case Prefab:
			LoadPrefab(
				PrefabKey{ std::move(key) },
				source_path,
				source_path
			);
			break;

		case Shader: {
			TrackAssetLoad(key, AssetKind::Shader, source_path);

			auto source{
				FileToString(source_path)
			};

			auto max_texture_slots{
				impl::RendererAccessor{
					renderer_
				}.GetMaxTextureSlots()
			};
			auto validation{
				impl::ValidateShaderSource(
					source,
					max_texture_slots
				)
			};

			if (!HasVertexAndFragmentShader(
					source
				)) {
				validation.success = false;
				validation.log =
					"Shader requires both vertex and fragment stages or a configured pair.";
			}

			if (!validation.success) {
				auto& state{
					runtime_states_[MakeAssetStorageKey(key, AssetKind::Shader)]
				};
				state.load_state =
					AssetLoadState::Failed;
				state.error =
					validation.log;
				state.compile_error = true;
				state.compile_log =
					validation.log;

				PTGN_WARN(
					"Shader failed validation and was not loaded: ",
					source_path.string(),
					"\n",
					validation.log
				);
				return;
			}

			LoadShader(
				ShaderKey{ std::move(key) },
				ShaderCode{ source },
				std::nullopt
			);
			break;
		}

		case Scene:
			break;

		case Unknown:
			PTGN_WARN(
				"Unsupported asset file: ",
				source_path.string()
			);
			break;
	}
}

void AssetManager::Load(AssetKey key, const path& asset_path) {
	auto kind{ impl::GetAssetKind(asset_path) };
	Load(std::move(key), asset_path, kind);
}

ecs::Entity AssetManager::CreateAsset() {
	auto asset{ manager_.CreateEntity() };
	manager_.Refresh();
	return asset;
}

template <AssetType T>
bool HasAssetImpl(const ecs::Manager& manager, const AssetKey& key) {
	auto hash{ Hash(key) };
	using Object = typename impl::AssetInfo<T>::Object;
	return manager.EntitiesWith<Object, AssetKey>().AnyOf(
		[hash](auto, const auto&, const auto& asset_key) { return Hash(asset_key) == hash; }
	);
}

template <AssetType T>
std::optional<T> TryGetAssetImpl(const ecs::Manager& manager, const AssetKey& key) {
	auto hash{ Hash(key) };
	using Object = typename impl::AssetInfo<T>::Object;

	for (auto [entity, _asset, asset_key] : manager.EntitiesWith<Object, AssetKey>()) {
		if (Hash(asset_key) == hash) {
			return T{ entity, true };
		}
	}
	return std::nullopt;
}

template <AssetType T>
bool UnloadAssetImpl(ecs::Manager& manager, const AssetKey& key) {
	bool unloaded{ false };
	auto hash{ Hash(key) };
	using Object = typename impl::AssetInfo<T>::Object;

	for (auto [entity, _asset, asset_key] : manager.EntitiesWith<Object, AssetKey>()) {
		if (Hash(asset_key) == hash) {
			unloaded = true;
			entity.Destroy();
		}
	}

	manager.Refresh();
	return unloaded;
}

template <AssetType T>
bool AssetManager::Unload(const AssetKey& key) {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		return jsons_.erase(Hash(key)) != 0;
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, Prefab>) {
		return prefabs_.erase(Hash(key)) != 0;
	} else {
		return UnloadAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
std::optional<ConstAsset<T>> AssetManager::TryGet(const AssetKey& key) const {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		auto it{ jsons_.find(Hash(key)) };
		return it == jsons_.end()
			? std::nullopt
			: std::optional<ConstAsset<T>>{ std::cref(it->second.value) };
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, Prefab>) {
		auto it{ prefabs_.find(Hash(key)) };
		return it == prefabs_.end()
			? std::nullopt
			: std::optional<ConstAsset<T>>{ std::cref(it->second.value) };
	} else {
		return TryGetAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
std::optional<Asset<T>> AssetManager::TryGet(const AssetKey& key) {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		auto it{ jsons_.find(Hash(key)) };
		return it == jsons_.end() ? std::nullopt : std::optional<Asset<T>>{ std::ref(it->second.value) };
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, Prefab>) {
		auto it{ prefabs_.find(Hash(key)) };
		return it == prefabs_.end()
			? std::nullopt
			: std::optional<Asset<T>>{ std::ref(it->second.value) };
	} else {
		return TryGetAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
ConstAsset<T> AssetManager::Get(const AssetKey& key) const {
	if (auto asset{ TryGet<T>(key) }; asset.has_value()) {
		return asset.value();
	}

	PTGN_WARN("Asset not found for key: ", key);

	if constexpr (std::is_same_v<std::remove_cvref_t<T>, Texture>) {
		auto fallback{ TryGet<Texture>(AssetKey{ std::string{ kMissingTextureAssetKey } }) };
		if (fallback.has_value()) {
			return fallback.value();
		}

		PTGN_WARN("Built-in missing texture fallback is unavailable");
		return Texture{};
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		static const json empty = json::object();
		return std::cref(empty);
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, Prefab>) {
		static const Prefab empty;
		return std::cref(empty);
	} else {
		return T{};
	}
}

template <AssetType T>
Asset<T> AssetManager::Get(const AssetKey& key) {
	if (auto asset{ TryGet<T>(key) }; asset.has_value()) {
		return asset.value();
	}

	PTGN_WARN("Asset not found for key: ", key);

	if constexpr (std::is_same_v<std::remove_cvref_t<T>, Texture>) {
		auto fallback{ TryGet<Texture>(AssetKey{ std::string{ kMissingTextureAssetKey } }) };
		if (fallback.has_value()) {
			return fallback.value();
		}

		PTGN_WARN("Built-in missing texture fallback is unavailable");
		return Texture{};
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		static json empty = json::object();
		return std::ref(empty);
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, Prefab>) {
		static Prefab empty;
		return std::ref(empty);
	} else {
		return T{};
	}
}

bool AssetManager::Has(const AssetKey& key, AssetKind kind) const {
	switch (kind) {
		using enum AssetKind;
		case Texture: return Has<ptgn::Texture>(key);
		case Audio: return Has<ptgn::Audio>(key);
		case Font: return Has<ptgn::Font>(key);
		case Json: return Has<json>(key);
		case Shader: return Has<ptgn::Shader>(key);
		case Prefab: return Has<ptgn::Prefab>(key);
		case Scene: {
			auto storage_key{ MakeAssetStorageKey(key, AssetKind::Scene) };
			return runtime_states_.contains(storage_key) &&
				runtime_states_.at(storage_key).load_state == AssetLoadState::Loaded;
		}
		case Unknown: break;
	}
	return false;
}

template <AssetType T>
bool AssetManager::Has(const AssetKey& key) const {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		return jsons_.contains(Hash(key));
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, Prefab>) {
		return prefabs_.contains(Hash(key));
	} else {
		return HasAssetImpl<T>(manager_, key);
	}
}

std::size_t AssetManager::Size() const {
	return manager_.Size() + jsons_.size() + prefabs_.size();
}

V2_int AssetManager::GetTextureSize(const TextureKey& key) const {
	return Get<Texture>(key).GetEntity().Get<impl::TextureObject>().GetSize();
}

V2_int AssetManager::GetFontAtlasSize(const FontKey& key) const {
	return Get<Font>(key).GetEntity().Get<impl::FontAtlas>().GetSize();
}

impl::TextureId AssetManager::GetFontAtlasTexture(const FontKey& key) const {
	return Get<Font>(key).GetEntity().Get<impl::FontAtlas>().GetTexture();
}

std::vector<impl::AssetRecord> AssetManager::GetAssets() const {
	std::vector<impl::AssetRecord> records;
	records.reserve(catalog_.size() + manager_.Size());

	for (const auto& [storage_key, serialized] : catalog_) {
		const auto state_it{ runtime_states_.find(storage_key) };
		const RuntimeAssetState* state{ state_it == runtime_states_.end() ? nullptr : &state_it->second };

		impl::AssetRecord record{
			.key = serialized.key,
			.source_path = serialized.source_path,
			.kind = serialized.kind,
			.load_state = state ? state->load_state : AssetLoadState::Unloaded,
			.reference_count = state ? state->reference_count : 0,
			.globally_pinned = state && state->globally_pinned,
			.manually_pinned = state && state->manually_pinned,
			.cataloged = true,
			.compile_error = state && state->compile_error,
			.load_error = state ? state->error : std::string{},
			.compile_log = state ? state->compile_log : std::string{},
			.metadata = state ? state->metadata : impl::AssetMetadata{},
		};

		if (serialized.kind == AssetKind::Texture) {
			if (auto texture{ TryGet<Texture>(serialized.key) }; texture.has_value()) {
				record.preview = impl::AssetPreview{
					.texture = static_cast<impl::TextureId>(texture.value()),
					.size = texture->GetSize(),
				};
			}
		} else if (serialized.kind == AssetKind::Font) {
			if (auto font{ TryGet<Font>(serialized.key) }; font.has_value()) {
				record.preview = impl::AssetPreview{
					.texture = font->GetEntity().Get<impl::FontAtlas>().GetTexture(),
					.size = font->GetEntity().Get<impl::FontAtlas>().GetSize(),
				};
			}
		}

		records.emplace_back(std::move(record));
	}

	for (auto [asset, key] : manager_.EntitiesWith<AssetKey>()) {
		AssetKind runtime_kind{ GetAssetKindFromEntity(asset, {}) };
		if (key.value == kMissingTextureAssetKey || HasCatalogAsset(key, runtime_kind)) {
			continue;
		}

		path source_path;
		if (auto asset_path{ asset.TryGet<impl::AssetPath>() }) {
			source_path = asset_path->value;
		}

		impl::AssetRecord record{
			.key = key,
			.source_path = source_path,
			.kind = GetAssetKindFromEntity(asset, source_path),
			.load_state = AssetLoadState::Loaded,
		};

		if (record.kind == AssetKind::Font && key.value == kDefaultFont) {
			record.source_path = path{ "assets/fonts/LiberationSans-Regular.ttf" };
			path default_font_file{
				impl::GetBuildInfo().engine_directory / record.source_path
			};
			if (FileExists(default_font_file)) {
				record.metadata.file_size = SafeFileSize(default_font_file);
			}
		}

		if (auto texture{ asset.TryGet<impl::TextureObject>() }) {
			record.preview = impl::AssetPreview{
				.texture = static_cast<impl::TextureId>(*texture),
				.size = texture->GetSize(),
			};
		} else if (auto font{ asset.TryGet<impl::FontAtlas>() }) {
			record.preview = impl::AssetPreview{
				.texture = font->GetTexture(),
				.size = font->GetSize(),
			};
			record.metadata.dimensions = font->GetSize();
		}

		records.emplace_back(std::move(record));
	}

	return records;
}

bool AssetManager::Has(const AssetKey& key) const {
	return Has<Texture>(key) || Has<Audio>(key) || Has<Font>(key) || Has<Shader>(key) ||
		   Has<json>(key) || Has<Prefab>(key);
}

bool AssetManager::ForceUnload(const AssetKey& key, AssetKind kind) {
	bool unloaded{ false };
	switch (kind) {
		using enum AssetKind;
		case Texture: unloaded = Unload<ptgn::Texture>(key); break;
		case Audio: unloaded = Unload<ptgn::Audio>(key); break;
		case Font: unloaded = Unload<ptgn::Font>(key); break;
		case Json: unloaded = Unload<json>(key); break;
		case Shader: unloaded = Unload<ptgn::Shader>(key); break;
		case Prefab: unloaded = Unload<ptgn::Prefab>(key); break;
		case Scene: unloaded = true; break;
		case Unknown: break;
	}

	auto state_it{ runtime_states_.find(MakeAssetStorageKey(key, kind)) };
	if (state_it != runtime_states_.end()) {
		state_it->second.load_state = AssetLoadState::Unloaded;
		state_it->second.manually_pinned = false;
		state_it->second.error.clear();
	}
	return unloaded;
}

bool AssetManager::Unload(const AssetKey& key, AssetKind kind) {
	auto state_it{ runtime_states_.find(MakeAssetStorageKey(key, kind)) };
	if (state_it != runtime_states_.end()) {
		state_it->second.manually_pinned = false;
		if (state_it->second.reference_count > 0 || state_it->second.globally_pinned) {
			return false;
		}
	}
	return ForceUnload(key, kind);
}

std::vector<AssetKey> AssetManager::DiscoverDependencies(
	const json& value,
	std::span<const AssetKey> manual_dependencies
) const {
	std::vector<AssetKey> dependencies;
	for (const auto& key : manual_dependencies) {
		AddUnique(dependencies, key);
	}

	auto collect = [this, &dependencies](const json& root) {
		std::function<void(const json&)> visit = [&](const json& current) {
			if (current.is_string()) {
				AssetKey key{ current.get<std::string>() };
				if (HasCatalogAsset(key)) {
					AddUnique(dependencies, key);
				}
				return;
			}
			if (current.is_array()) {
				for (const auto& child : current) {
					visit(child);
				}
				return;
			}
			if (current.is_object()) {
				for (const auto& [_, child] : current.items()) {
					visit(child);
				}
			}
		};
		visit(root);
	};

	collect(value);

	for (std::size_t index{ 0 }; index < dependencies.size(); ++index) {
		for (const auto& [_, asset] : catalog_) {
			if (asset.key != dependencies[index] || asset.kind != AssetKind::Prefab) {
				continue;
			}

			const auto prefab_path{ ResolveAssetPath(asset) };
			if (!FileExists(prefab_path)) {
				continue;
			}

			try {
				collect(json::parse(FileToString(prefab_path)));
			} catch (const json::exception& error) {
				PTGN_WARN(
					"Could not inspect prefab dependencies for ",
					prefab_path.string(),
					": ",
					error.what()
				);
			}
		}
	}

	return dependencies;
}

std::vector<AssetKey> AssetManager::ExpandDependencies(
	std::span<const AssetKey> dependencies
) const {
	json values = json::array();
	for (const auto& key : dependencies) {
		values.emplace_back(key.value);
	}
	return DiscoverDependencies(values, dependencies);
}

template bool AssetManager::Unload<json>(const AssetKey&);
template bool AssetManager::Unload<Prefab>(const AssetKey&);
template bool AssetManager::Unload<Font>(const AssetKey&);
template bool AssetManager::Unload<Texture>(const AssetKey&);
template bool AssetManager::Unload<Audio>(const AssetKey&);
template bool AssetManager::Unload<Shader>(const AssetKey&);

template bool AssetManager::Has<json>(const AssetKey&) const;
template bool AssetManager::Has<Prefab>(const AssetKey&) const;
template bool AssetManager::Has<Font>(const AssetKey&) const;
template bool AssetManager::Has<Texture>(const AssetKey&) const;
template bool AssetManager::Has<Audio>(const AssetKey&) const;
template bool AssetManager::Has<Shader>(const AssetKey&) const;

template ConstAsset<json> AssetManager::Get<json>(const AssetKey&) const;
template ConstAsset<Prefab> AssetManager::Get<Prefab>(const AssetKey&) const;
template ConstAsset<Font> AssetManager::Get<Font>(const AssetKey&) const;
template ConstAsset<Texture> AssetManager::Get<Texture>(const AssetKey&) const;
template ConstAsset<Audio> AssetManager::Get<Audio>(const AssetKey&) const;
template ConstAsset<Shader> AssetManager::Get<Shader>(const AssetKey&) const;

template Asset<json> AssetManager::Get<json>(const AssetKey&);
template Asset<Prefab> AssetManager::Get<Prefab>(const AssetKey&);
template Asset<Font> AssetManager::Get<Font>(const AssetKey&);
template Asset<Texture> AssetManager::Get<Texture>(const AssetKey&);
template Asset<Audio> AssetManager::Get<Audio>(const AssetKey&);
template Asset<Shader> AssetManager::Get<Shader>(const AssetKey&);

namespace impl {

std::optional<std::size_t> DetectTexturePathCount(
	AssetManager& assets,
	const TextureKey& texture_key,
	std::string_view marker
) {
	std::optional<path> source_path;

	if (auto asset{ assets.GetCatalogAsset(texture_key, AssetKind::Texture) }) {
		source_path = asset->source_path;
	} else {
		AssetAccessor accessor{ assets };

		if (accessor.Has<Texture>(texture_key)) {
			const Texture texture{ accessor.Get<Texture>(texture_key) };

			if (auto asset_path{ texture.GetEntity().TryGet<AssetPath>() }) {
				source_path = asset_path->value;
			}
		}
	}

	if (!source_path) {
		return std::nullopt;
	}

	std::string stem_string{ source_path->stem().string() };
	const std::string_view stem{ stem_string };
	auto marker_position{ stem.rfind(marker) };

	if (marker_position == std::string_view::npos) {
		return std::nullopt;
	}

	const std::string_view digits{
		stem.substr(marker_position + marker.size())
	};

	if (digits.empty()) {
		return std::nullopt;
	}

	std::size_t count{ 0 };

	const auto [end, error]{
		std::from_chars(digits.data(), digits.data() + digits.size(), count)
	};

	if (error != std::errc{} || end != digits.data() + digits.size() || count == 0) {
		return std::nullopt;
	}

	return count;
}

} // namespace impl

} // namespace ptgn
