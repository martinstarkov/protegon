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

std::string MakeKeyFromRelativePath(path relative_path) {
	relative_path.replace_extension();

	std::string key;
	for (const auto& part : relative_path) {
		if (!key.empty()) {
			key += '/';
		}
		key += SanitizeKeySegment(part.string());
	}

	return key.empty() ? "asset" : key;
}

std::uintmax_t SafeFileSize(const path& file_path) {
	std::error_code error;
	const auto size{ std::filesystem::file_size(file_path, error) };
	return error ? 0 : size;
}

bool IsWithinDirectory(const path& candidate, const path& directory) {
	const auto normalized_candidate{ candidate.lexically_normal() };
	const auto normalized_directory{ directory.lexically_normal() };
	if (normalized_candidate == normalized_directory) {
		return true;
	}

	const auto relative{ normalized_candidate.lexically_relative(normalized_directory) };
	if (relative.empty() || relative.is_absolute()) {
		return false;
	}

	const auto first{ relative.begin() };
	return first != relative.end() && *first != "..";
}

std::optional<V2_int> ProbeImageDimensions(const path& file_path) {
	int width{ 0 };
	int height{ 0 };
	int channels{ 0 };

	const auto absolute_path{ GetAbsolutePath(file_path).string() };
	if (stbi_info(absolute_path.c_str(), &width, &height, &channels) == 0 || width <= 0 ||
		height <= 0) {
		return std::nullopt;
	}

	return V2_int{ width, height };
}

std::optional<double> ProbeAudioDuration(const path& file_path) {
	const auto absolute_path{ GetAbsolutePath(file_path).string() };

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
	const auto sample_rate{ decoder.outputSampleRate };
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
		const path file_path{ path_or_name };
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

	const path stage_path{
		reference == kShaderSourceToken
			? source_path
			: (project_root / path{ reference }).lexically_normal()
	};
	const std::string source{ FileToString(stage_path) };
	return ShaderCode{ impl::ExtractShaderStageSource(source, stage) };
}

} // namespace

namespace impl {

struct AssetLoadBatchState {
	mutable std::mutex mutex;
	AssetLoadProgress progress;
};

void AddAssetKey(ecs::Entity asset, AssetKey key, const std::optional<path>& path) {
	asset.Add<AssetKey>(std::move(key));
	if (path.has_value()) {
		asset.Add<impl::AssetPath>(path.value());
	}
}

AssetKind GetAssetKind(const path& asset_path) {
	const auto extension{ ToLower(GetExtension(asset_path)) };

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

AssetAccessor::AssetAccessor(AssetManager& assets) : assets{ assets } {}

std::vector<AssetRecord> AssetAccessor::GetAssets() const {
	return assets.GetAssets();
}

bool AssetAccessor::Unload(const AssetKey& key, AssetKind kind) {
	return assets.Unload(key, kind);
}

AssetCaptureScope::AssetCaptureScope(
	AssetManager& assets,
	std::vector<AssetKey>& dependencies
) :
	assets_{ assets }, dependencies_{ dependencies } {
	assets_.BeginAssetCapture(dependencies_);
}

AssetCaptureScope::~AssetCaptureScope() noexcept {
	assets_.EndAssetCapture(dependencies_);
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
		std::unique_ptr<impl::Surface> surface;
	};

	struct PreparedAudio {
		path file_path;
	};

	struct PreparedFont {
		path file_path;
	};

	struct PreparedJson {
		json value;
	};

	struct PreparedPrefab {
		Prefab value;
	};

	struct PreparedShader {
		std::variant<ShaderCode, ShaderPath, ShaderPair> source;
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
		SerializedAsset asset;
		path absolute_path;
		path project_root;
	};

	struct Result {
		SerializedAsset asset;
		std::optional<Payload> payload;
		std::string error;
	};

	AsyncLoader() {
#ifndef __EMSCRIPTEN__
		const auto hardware_threads{ std::max(1u, std::thread::hardware_concurrency()) };
		const auto worker_count{ std::clamp(hardware_threads > 1 ? hardware_threads - 1 : 1u, 1u, 4u) };

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

		const path stage_path{
			reference == kShaderSourceToken
				? source_path
				: (project_root / path{ reference }).lexically_normal()
		};
		const std::string source{ FileToString(stage_path) };
		return ShaderCode{ impl::ExtractShaderStageSource(source, stage) };
	}

	static Result Prepare(Job job) {
		Result result{ .asset = std::move(job.asset) };

		try {
			switch (result.asset.kind) {
				using enum AssetKind;
				case Texture: {
					if (impl::IsFontAtlasPng(job.absolute_path)) {
						result.payload = PreparedFont{ job.absolute_path };
					} else {
						result.payload = PreparedTexture{
							.surface = std::make_unique<impl::Surface>(job.absolute_path),
						};
					}
					break;
				}
				case Audio: result.payload = PreparedAudio{ job.absolute_path }; break;
				case Font: result.payload = PreparedFont{ job.absolute_path }; break;
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
						const auto source{ FileToString(job.absolute_path) };
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
		const std::string name{ filename.stem().string() };
		const auto stages{ DetectShaderStages(shader_file.source) };
		engine_shader_sources_.push_back(impl::EngineShaderSource{
			.key = AssetKey{ "$engine/shaders/" + filename.string() },
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
	const auto it{ std::ranges::find(engine_shader_sources_, key, &impl::EngineShaderSource::key) };
	if (it == engine_shader_sources_.end()) {
		return std::nullopt;
	}
	return it->source;
}

std::span<const std::string> AssetManager::GetEngineVertexShaderNames() const {
	return engine_vertex_shader_names_;
}

std::span<const std::string> AssetManager::GetEngineFragmentShaderNames() const {
	return engine_fragment_shader_names_;
}

std::optional<std::string> AssetManager::GetShaderSource(const ShaderKey& key) const {
	if (auto engine{ GetEngineShaderSource(key) }) {
		return engine;
	}
	const auto it{ catalog_.find(Hash(key)) };
	if (it == catalog_.end() || it->second.kind != AssetKind::Shader) {
		return std::nullopt;
	}
	const auto path{ ResolveAssetPath(it->second) };
	if (!FileExists(path)) {
		return std::nullopt;
	}
	return FileToString(path);
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
		const auto path{ ResolveAssetPath(owner) };
		return FileExists(path) ? std::optional<std::string>{ FileToString(path) } : std::nullopt;
	}
	if (reference.starts_with(kBuiltinShaderPrefix)) {
		const std::string name{ reference.substr(kBuiltinShaderPrefix.size()) };
		const auto it{ std::ranges::find(engine_shader_sources_, name, &impl::EngineShaderSource::name) };
		if (it == engine_shader_sources_.end()) {
			return std::nullopt;
		}
		return it->source;
	}
	const auto root{ project_root_.value_or(GetWorkingDirectory()) };
	const path stage_path{ (root / path{ reference }).lexically_normal() };
	if (!FileExists(stage_path)) {
		return std::nullopt;
	}
	return FileToString(stage_path);
}

ShaderCompileResult AssetManager::ValidateShaderSource(
	const ShaderKey& key,
	std::string_view source
) const {
	const auto catalog_it{ catalog_.find(Hash(key)) };
	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	if (catalog_it == catalog_.end()) {
		return impl::ValidateShaderSource(source, max_texture_slots);
	}
	const auto& asset{ catalog_it->second };
	const auto stages{ DetectShaderStages(source) };
	if (!asset.shader.has_value() && stages == ShaderStageMask::VertexFragment) {
		return impl::ValidateShaderSource(source, max_texture_slots);
	}
	if (!asset.shader.has_value()) {
		return impl::ValidateShaderSource(source, max_texture_slots);
	}
	const auto& program{ asset.shader.value() };
	if (!program.vertex.has_value() || !program.fragment.has_value()) {
		return { false, "Shader program configuration must provide both vertex and fragment stages." };
	}
	auto vertex{ ResolveShaderStageSourceText(program.vertex.value(), asset, source) };
	auto fragment{ ResolveShaderStageSourceText(program.fragment.value(), asset, source) };
	if (!vertex.has_value() || !fragment.has_value()) {
		return { false, "One or more configured shader stage sources could not be resolved." };
	}
	return impl::ValidateShaderProgram(
		vertex.value(), fragment.value(), max_texture_slots
	);
}

bool AssetManager::SaveShaderSource(
	const ShaderKey& key,
	std::string_view source,
	const ShaderCompileResult& validation
) {
	auto catalog_it{ catalog_.find(Hash(key)) };
	if (catalog_it == catalog_.end() || catalog_it->second.kind != AssetKind::Shader) {
		return false;
	}
	const auto file_path{ ResolveAssetPath(catalog_it->second) };
	std::ofstream output{ file_path, std::ios::binary | std::ios::trunc };
	if (!output) {
		return false;
	}
	output.write(source.data(), static_cast<std::streamsize>(source.size()));
	if (!output) {
		return false;
	}
	auto& state{ runtime_states_[Hash(key)] };
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
	std::optional<std::string_view> source_override
) const {
	const auto source_path{ ResolveAssetPath(asset) };
	std::string source;
	if (source_override.has_value()) {
		source = std::string{ source_override.value() };
	} else if (FileExists(source_path)) {
		source = FileToString(source_path);
	} else {
		return std::nullopt;
	}
	if (!asset.shader.has_value()) {
		return std::variant<ShaderCode, ShaderPath, ShaderPair>{ ShaderCode{ source } };
	}
	const auto& program{ asset.shader.value() };
	if (program.vertex == std::optional<std::string>{ kShaderSourceToken } &&
		program.fragment == std::optional<std::string>{ kShaderSourceToken }) {
		return std::variant<ShaderCode, ShaderPath, ShaderPair>{ ShaderCode{ source } };
	}
	if (!program.vertex.has_value() || !program.fragment.has_value()) {
		return std::nullopt;
	}
	const auto root{ project_root_.value_or(GetWorkingDirectory()) };
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
	auto validation{ ValidateShaderSource(key, source) };
	if (!validation.success) {
		return validation;
	}
	auto catalog_it{ catalog_.find(Hash(key)) };
	if (catalog_it == catalog_.end()) {
		return { false, "Shader is not in the project catalog." };
	}
	auto& state{ runtime_states_[Hash(key)] };
	if (state.load_state != AssetLoadState::Loaded || !Has<ptgn::Shader>(key)) {
		validation.log += "\nShader is not resident; source was validated but no runtime program was replaced.";
		return validation;
	}
	auto prepared{ BuildShaderProgramSource(catalog_it->second, source) };
	if (!prepared.has_value()) {
		return { false, "Could not resolve shader program sources for reload." };
	}

	const RuntimeAssetState retained_state{ state };
	ForceUnload(key, AssetKind::Shader);
	LoadShader(ShaderKey{ key }, prepared.value(), key.value);
	auto& refreshed_state{ runtime_states_[Hash(key)] };
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

		const auto key_hash{ Hash(result->asset.key) };
		auto state_it{ runtime_states_.find(key_hash) };
		if (state_it == runtime_states_.end()) {
			continue;
		}

		state_it->second.load_state = AssetLoadState::Finalizing;

		if (!result->error.empty() || !result->payload.has_value()) {
			CompleteAssetLoad(key_hash, false, std::move(result->error));
			continue;
		}

		bool success{ true };
		std::string error;

		try {
			std::visit(
				[this, &result, &success, &error]<typename T>(T&& prepared) {
					using Value = std::remove_cvref_t<T>;
					const auto& asset{ result->asset };
					const auto absolute_path{ ResolveAssetPath(asset) };

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
						auto font{ CreateFont(true, prepared.file_path) };
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
						const auto disk_source{ FileToString(absolute_path) };
						auto validation{ ValidateShaderSource(ShaderKey{ asset.key }, disk_source) };
						auto& shader_state{ runtime_states_[Hash(asset.key)] };
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

		CompleteAssetLoad(key_hash, success, std::move(error));
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
}

void AssetManager::CompleteAssetLoad(
	std::size_t key_hash,
	bool success,
	std::string error
) {
	auto state_it{ runtime_states_.find(key_hash) };
	if (state_it == runtime_states_.end()) {
		return;
	}

	auto& state{ state_it->second };
	state.load_state = success ? AssetLoadState::Loaded : AssetLoadState::Failed;
	state.error = std::move(error);

	const auto catalog_it{ catalog_.find(key_hash) };
	const auto file_size{
		catalog_it == catalog_.end() ? 0 : state.metadata.file_size
	};
	const auto active_asset{
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
	const auto key_hash{ Hash(asset.key) };
	auto& state{ runtime_states_[key_hash] };
	state.waiters.emplace_back(batch);

	if (state.load_state == AssetLoadState::Loaded) {
		CompleteAssetLoad(key_hash, true);
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
	std::vector<AssetKey> available_dependencies;
	available_dependencies.reserve(expanded_dependencies.size());

	for (const auto& key : expanded_dependencies) {
		if (!HasCatalogAsset(key)) {
			PTGN_WARN(
				"Asset dependency is missing from the project catalog: ",
				key,
				". Texture requests use the purple fallback; other asset requests remain unavailable."
			);
			continue;
		}

		available_dependencies.emplace_back(key);
	}

	auto batch{ std::make_shared<impl::AssetLoadBatchState>() };

	{
		std::scoped_lock lock{ batch->mutex };
		batch->progress.total_assets = available_dependencies.size();
	}

	for (const auto& key : available_dependencies) {
		auto catalog_it{ catalog_.find(Hash(key)) };
		if (catalog_it == catalog_.end()) {
			continue;
		}

		auto& state{ runtime_states_[Hash(key)] };
		++state.reference_count;

		{
			std::scoped_lock lock{ batch->mutex };
			batch->progress.total_bytes += state.metadata.file_size;
		}

		QueueAssetLoad(catalog_it->second, batch);
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
	const auto progress{ GetActiveLoadProgress() };
	return progress.total_assets > 0 && !progress.IsComplete();
}

void AssetManager::ReleaseDependencies(std::span<const AssetKey> dependencies) noexcept {
	for (const auto& key : dependencies) {
		auto state_it{ runtime_states_.find(Hash(key)) };
		if (state_it == runtime_states_.end()) {
			continue;
		}

		auto& state{ state_it->second };
		if (state.reference_count > 0) {
			--state.reference_count;
		}

		if (state.reference_count == 0 && !state.globally_pinned &&
			!state.manually_pinned && state.load_state == AssetLoadState::Loaded) {
			auto catalog_it{ catalog_.find(Hash(key)) };
			if (catalog_it != catalog_.end()) {
				ForceUnload(key, catalog_it->second.kind);
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

void AssetManager::TrackAssetLoad(
	const AssetKey& key,
	AssetKind kind,
	const path& source_path
) {
	if (key.value.empty() || kind == AssetKind::Unknown || source_path.empty()) {
		return;
	}

	path serialized_path{ source_path.lexically_normal() };

	if (source_path.is_absolute() && project_root_.has_value()) {
		std::error_code error;
		auto relative{
			std::filesystem::relative(source_path, project_root_.value(), error)
		};
		if (!error && !relative.empty() && !relative.generic_string().starts_with("..")) {
			serialized_path = relative.lexically_normal();
		}
	}

	SerializedAsset asset{
		.key = key,
		.kind = kind,
		.source_path = std::move(serialized_path),
	};

	if (auto existing{ catalog_.find(Hash(key)) };
		existing != catalog_.end() && existing->second.kind == kind) {
		asset.shader = existing->second.shader;
	}

	catalog_.insert_or_assign(Hash(key), asset);
	runtime_states_[Hash(key)].metadata = ProbeMetadata(asset);

	if (captured_asset_dependencies_) {
		AddUnique(*captured_asset_dependencies_, key);
	}
}

void AssetManager::RegisterCatalog(
	std::span<const SerializedAsset> assets,
	const Project& project
) {
	project_load_tickets_.clear();
	manual_load_tickets_.clear();
	active_batches_.clear();
	async_loader_.reset();
	for (const auto& [_, asset] : catalog_) {
		ForceUnload(asset.key, asset.kind);
	}

	project_root_ = project.file_path.parent_path().lexically_normal();
	asset_directory_ = (project_root_.value() / project.asset_directory).lexically_normal();

	catalog_.clear();
	runtime_states_.clear();
	project_asset_dependencies_.clear();
	async_loader_ = std::make_unique<AsyncLoader>();

	EnsureDirectory(asset_directory_.value());

	for (const auto& asset : assets) {
		PTGN_ASSERT(!asset.key.value.empty(), "Serialized asset key cannot be empty");
		PTGN_ASSERT(
			asset.kind != AssetKind::Unknown,
			"Serialized asset kind cannot be Unknown for key: ",
			asset.key
		);
		PTGN_ASSERT(
			!asset.source_path.empty(),
			"Serialized asset path cannot be empty for key: ",
			asset.key
		);

		catalog_.insert_or_assign(Hash(asset.key), asset);
		runtime_states_[Hash(asset.key)].metadata = ProbeMetadata(asset);
	}

	RefreshCatalogFromDisk();
	SetProjectAssetDependencies(project.preload_assets);
}

void AssetManager::RefreshCatalogFromDisk() {
	if (!asset_directory_.has_value() || !project_root_.has_value() ||
		!IsDirectoryPath(asset_directory_->string())) {
		return;
	}

	for (auto it{ catalog_.begin() }; it != catalog_.end();) {
		auto& state{ runtime_states_[it->first] };
		if (FileExists(ResolveAssetPath(it->second))) {
			state.metadata = ProbeMetadata(it->second);
			if (state.load_state != AssetLoadState::Failed || state.error == "Source file is missing") {
				state.error.clear();
				if (state.load_state == AssetLoadState::Failed) {
					state.load_state = AssetLoadState::Unloaded;
				}
			}
			++it;
			continue;
		}

		const bool in_use{
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

		ForceUnload(it->second.key, it->second.kind);
		std::erase(project_asset_dependencies_, it->second.key);
		runtime_states_.erase(it->first);
		it = catalog_.erase(it);
	}

	std::unordered_set<std::string> catalog_paths;
	for (const auto& [_, asset] : catalog_) {
		catalog_paths.insert(asset.source_path.lexically_normal().generic_string());
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(asset_directory_.value())) {
		if (!entry.is_regular_file()) {
			continue;
		}

		auto kind{ impl::GetAssetKind(entry.path()) };
		if (kind == AssetKind::Unknown) {
			continue;
		}
		if (kind == AssetKind::Texture && impl::IsFontAtlasPng(entry.path())) {
			kind = AssetKind::Font;
		}

		std::error_code error;
		auto relative_to_project{
			std::filesystem::relative(entry.path(), project_root_.value(), error)
		};
		if (error) {
			continue;
		}

		const auto normalized_path{ relative_to_project.lexically_normal().generic_string() };
		if (catalog_paths.contains(normalized_path)) {
			continue;
		}

		std::error_code asset_error;
		auto relative_to_assets{
			std::filesystem::relative(entry.path(), asset_directory_.value(), asset_error)
		};
		if (asset_error) {
			continue;
		}

		auto key{ MakeUniqueAssetKey(relative_to_assets) };
		SerializedAsset asset{
			.key = key,
			.kind = kind,
			.source_path = relative_to_project,
		};

		if (kind == AssetKind::Shader) {
			const auto stages{ DetectShaderStages(FileToString(entry.path())) };
			if (stages != ShaderStageMask::VertexFragment) {
				SerializedShaderProgram program;
				if (HasShaderStage(stages, ShaderStageMask::Vertex)) {
					program.vertex = std::string{ kShaderSourceToken };
				}
				if (HasShaderStage(stages, ShaderStageMask::Fragment)) {
					program.fragment = std::string{ kShaderSourceToken };
				}
				asset.shader = std::move(program);
			}
		}

		catalog_.emplace(Hash(key), asset);
		runtime_states_[Hash(key)].metadata = ProbeMetadata(asset);
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
		return lhs.key < rhs.key;
	});

	return assets;
}

std::optional<SerializedAsset> AssetManager::GetCatalogAsset(const AssetKey& key) const {
	auto it{ catalog_.find(Hash(key)) };
	if (it == catalog_.end()) {
		return std::nullopt;
	}
	return it->second;
}

void AssetManager::PinProjectDependency(const AssetKey& key) {
	auto& state{ runtime_states_[Hash(key)] };
	state.globally_pinned = true;
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

	auto catalog_it{ catalog_.find(Hash(key)) };
	if (catalog_it == catalog_.end()) {
		return;
	}

	const auto state_it{ runtime_states_.find(Hash(key)) };
	if (state_it != runtime_states_.end() &&
		(state_it->second.load_state == AssetLoadState::Loaded ||
		 state_it->second.load_state == AssetLoadState::Queued ||
		 state_it->second.load_state == AssetLoadState::Loading ||
		 state_it->second.load_state == AssetLoadState::Finalizing)) {
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

	auto state_it{ runtime_states_.find(Hash(key)) };
	if (state_it == runtime_states_.end()) {
		return;
	}

	auto& state{ state_it->second };
	state.globally_pinned = false;
	if (state.reference_count == 0 && !state.manually_pinned &&
		state.load_state == AssetLoadState::Loaded) {
		auto catalog_it{ catalog_.find(Hash(key)) };
		if (catalog_it != catalog_.end()) {
			ForceUnload(key, catalog_it->second.kind);
		}
	}
}

void AssetManager::AddProjectAssetDependencies(std::span<const AssetKey> dependencies) {
	for (const auto& key : dependencies) {
		AddProjectAssetDependency(key);
	}
}

void AssetManager::SetProjectAssetDependencies(std::span<const AssetKey> dependencies) {
	for (const auto& key : project_asset_dependencies_) {
		auto state_it{ runtime_states_.find(Hash(key)) };
		if (state_it == runtime_states_.end()) {
			continue;
		}

		auto& state{ state_it->second };
		state.globally_pinned = false;
		if (state.reference_count == 0 && !state.manually_pinned &&
			state.load_state == AssetLoadState::Loaded) {
			auto catalog_it{ catalog_.find(Hash(key)) };
			if (catalog_it != catalog_.end()) {
				ForceUnload(key, catalog_it->second.kind);
			}
		}
	}

	project_asset_dependencies_.clear();
	AddProjectAssetDependencies(dependencies);
}

const std::vector<AssetKey>& AssetManager::GetProjectAssetDependencies() const {
	return project_asset_dependencies_;
}

bool AssetManager::HasCatalogAsset(const AssetKey& key) const {
	return catalog_.contains(Hash(key));
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
	const auto file_path{ ResolveAssetPath(asset) };
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

AssetKey AssetManager::MakeUniqueAssetKey(const path& source_path) const {
	const auto base{ MakeKeyFromRelativePath(source_path) };
	AssetKey key{ base };

	for (std::size_t suffix{ 2 }; HasCatalogAsset(key); ++suffix) {
		key = base + "_" + std::to_string(suffix);
	}

	return key;
}

std::optional<AssetKey> AssetManager::ImportAsset(
	const path& source_file,
	const path& destination_directory
) {
	if (!asset_directory_.has_value() || !project_root_.has_value() || !FileExists(source_file)) {
		return std::nullopt;
	}

	auto kind{ impl::GetAssetKind(source_file) };
	if (kind == AssetKind::Unknown) {
		return std::nullopt;
	}
	if (kind == AssetKind::Texture && impl::IsFontAtlasPng(source_file)) {
		kind = AssetKind::Font;
	}

	path destination_root{
		destination_directory.is_absolute()
			? destination_directory
			: asset_directory_.value() / destination_directory
	};
	destination_root = destination_root.lexically_normal();
	if (!IsWithinDirectory(destination_root, asset_directory_.value())) {
		return std::nullopt;
	}
	EnsureDirectory(destination_root);

	path destination{ destination_root / source_file.filename() };
	for (std::size_t suffix{ 2 }; FileExists(destination); ++suffix) {
		destination = destination_root /
			(source_file.stem().string() + "_" + std::to_string(suffix) +
			 source_file.extension().string());
	}

	std::error_code error;
	std::filesystem::copy_file(
		source_file,
		destination,
		std::filesystem::copy_options::none,
		error
	);
	if (error) {
		PTGN_WARN("Failed to import asset: ", error.message());
		return std::nullopt;
	}

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

	auto key{ MakeUniqueAssetKey(relative_to_assets) };
	SerializedAsset asset{
		.key = key,
		.kind = kind,
		.source_path = relative_to_project,
	};

	if (kind == AssetKind::Shader) {
		const auto stages{ DetectShaderStages(FileToString(destination)) };
		if (stages != ShaderStageMask::VertexFragment) {
			SerializedShaderProgram program;
			if (HasShaderStage(stages, ShaderStageMask::Vertex)) {
				program.vertex = std::string{ kShaderSourceToken };
			}
			if (HasShaderStage(stages, ShaderStageMask::Fragment)) {
				program.fragment = std::string{ kShaderSourceToken };
			}
			asset.shader = std::move(program);
		}
	}

	catalog_.insert_or_assign(Hash(key), asset);
	runtime_states_[Hash(key)].metadata = ProbeMetadata(asset);
	return key;
}

bool AssetManager::MoveAsset(const AssetKey& key, const path& destination_directory) {
	if (!asset_directory_.has_value() || !project_root_.has_value()) {
		return false;
	}

	auto catalog_it{ catalog_.find(Hash(key)) };
	if (catalog_it == catalog_.end() || catalog_it->second.kind == AssetKind::Scene) {
		return false;
	}

	const auto state_it{ runtime_states_.find(Hash(key)) };
	if (state_it != runtime_states_.end() &&
		(state_it->second.reference_count > 0 || state_it->second.globally_pinned ||
		 state_it->second.load_state == AssetLoadState::Queued ||
		 state_it->second.load_state == AssetLoadState::Loading ||
		 state_it->second.load_state == AssetLoadState::Finalizing)) {
		return false;
	}

	auto source{ ResolveAssetPath(catalog_it->second) };
	if (!FileExists(source)) {
		return false;
	}

	path destination_root{
		destination_directory.is_absolute()
			? destination_directory
			: asset_directory_.value() / destination_directory
	};
	destination_root = destination_root.lexically_normal();
	if (!IsWithinDirectory(destination_root, asset_directory_.value())) {
		return false;
	}
	EnsureDirectory(destination_root);

	path destination{ destination_root / source.filename() };
	if (source.lexically_normal() == destination.lexically_normal()) {
		return true;
	}

	for (std::size_t suffix{ 2 }; FileExists(destination); ++suffix) {
		destination = destination_root /
			(source.stem().string() + "_" + std::to_string(suffix) + source.extension().string());
	}

	std::error_code error;
	std::filesystem::rename(source, destination, error);
	if (error) {
		return false;
	}

	catalog_it->second.source_path =
		std::filesystem::relative(destination, project_root_.value(), error);
	if (error) {
		return false;
	}

	if (Has(key)) {
		ForceUnload(key, catalog_it->second.kind);
	}
	catalog_it->second.source_path = catalog_it->second.source_path.lexically_normal();
	runtime_states_[Hash(key)].metadata = ProbeMetadata(catalog_it->second);
	return true;
}

bool AssetManager::DeleteAsset(const AssetKey& key, bool delete_file) {
	auto catalog_it{ catalog_.find(Hash(key)) };
	if (catalog_it == catalog_.end() || catalog_it->second.kind == AssetKind::Scene) {
		return false;
	}

	auto state_it{ runtime_states_.find(Hash(key)) };
	if (state_it != runtime_states_.end()) {
		const auto& state{ state_it->second };
		if (state.reference_count > 0 || state.globally_pinned ||
			state.load_state == AssetLoadState::Queued ||
			state.load_state == AssetLoadState::Loading ||
			state.load_state == AssetLoadState::Finalizing) {
			return false;
		}
	}

	ForceUnload(key, catalog_it->second.kind);

	if (delete_file) {
		std::error_code error;
		std::filesystem::remove(ResolveAssetPath(catalog_it->second), error);
		if (error) {
			return false;
		}
	}

	std::erase(project_asset_dependencies_, key);
	catalog_.erase(catalog_it);
	runtime_states_.erase(Hash(key));
	return true;
}

bool AssetManager::ConfigureShaderProgram(
	const ShaderKey& key,
	std::optional<std::string> vertex_source,
	std::optional<std::string> fragment_source
) {
	auto catalog_it{ catalog_.find(Hash(key)) };
	if (catalog_it == catalog_.end() || catalog_it->second.kind != AssetKind::Shader) {
		return false;
	}
	if (!vertex_source.has_value() && !fragment_source.has_value()) {
		catalog_it->second.shader.reset();
		return true;
	}
	if (!vertex_source.has_value() || !fragment_source.has_value() ||
		vertex_source->empty() || fragment_source->empty()) {
		return false;
	}
	SerializedShaderProgram program;
	program.vertex = std::move(vertex_source.value());
	program.fragment = std::move(fragment_source.value());
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
	const auto source_path{ ResolveAssetPath(asset) };

	if (asset.kind == AssetKind::Scene) {
		runtime_states_[Hash(asset.key)].load_state = AssetLoadState::Loaded;
		return;
	}

	if (asset.kind != AssetKind::Shader) {
		Load(asset.key, source_path, asset.kind);
		return;
	}

	const auto source{ FileToString(source_path) };
	auto validation{ ValidateShaderSource(ShaderKey{ asset.key }, source) };
	auto& shader_state{ runtime_states_[Hash(asset.key)] };
	shader_state.compile_error = !validation.success;
	shader_state.compile_log = validation.log;
	if (!validation.success) {
		shader_state.load_state = AssetLoadState::Failed;
		shader_state.error = validation.log;
		PTGN_WARN("Shader failed to compile and was not loaded: ", asset.key);
		return;
	}
	if (!asset.shader.has_value()) {
		if (!HasVertexAndFragmentShader(source)) {
			PTGN_WARN(
				"Shader requires both stages or a configured engine stage: ",
				source_path.string()
			);
			return;
		}

		LoadShader(ShaderKey{ asset.key }, ShaderCode{ source }, asset.key.value);
		return;
	}

	const auto& program{ asset.shader.value() };
	if (program.vertex == std::optional<std::string>{ kShaderSourceToken } &&
		program.fragment == std::optional<std::string>{ kShaderSourceToken } &&
		HasVertexAndFragmentShader(source)) {
		LoadShader(ShaderKey{ asset.key }, ShaderCode{ source }, asset.key.value);
		return;
	}

	if (!program.vertex.has_value() || !program.fragment.has_value()) {
		PTGN_WARN(
			"Shader program requires both vertex and fragment stages: ",
			asset.key
		);
		return;
	}

	const auto root{ project_root_.value_or(GetWorkingDirectory()) };
	LoadShader(
		ShaderKey{ asset.key },
		ShaderPair{
			.vertex = ResolveSerializedShaderStage(
				program.vertex.value(), source_path, root, ShaderStageMask::Vertex
			),
			.fragment = ResolveSerializedShaderStage(
				program.fragment.value(), source_path, root, ShaderStageMask::Fragment
			),
		},
		asset.key.value
	);
}

void AssetManager::LoadAssetAsync(const AssetKey& key) {
	auto catalog_it{ catalog_.find(Hash(key)) };
	if (catalog_it == catalog_.end()) {
		PTGN_WARN("Cannot load asset because it is missing from the catalog: ", key);
		return;
	}

	auto& state{ runtime_states_[Hash(key)] };
	state.manually_pinned = true;
	if (state.load_state == AssetLoadState::Loaded ||
		state.load_state == AssetLoadState::Queued ||
		state.load_state == AssetLoadState::Loading ||
		state.load_state == AssetLoadState::Finalizing) {
		return;
	}

	const AssetKey dependency{ key };
	manual_load_tickets_.emplace_back(AcquireDependenciesAsync(
		std::span<const AssetKey>{ &dependency, 1 }
	));
}

void AssetManager::LoadDependencies(std::span<const AssetKey> dependencies) {
	for (const auto& key : ExpandDependencies(dependencies)) {
		auto it{ catalog_.find(Hash(key)) };
		if (it == catalog_.end()) {
			PTGN_WARN("Asset dependency is missing from the catalog: ", key);
			continue;
		}
		if (!Has(key, it->second.kind)) {
			Load(it->second);
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

	auto kind{ impl::GetAssetKind(asset_path) };
	if (kind == AssetKind::Texture && impl::IsFontAtlasPng(asset_path)) {
		kind = AssetKind::Font;
	}

	SerializedAsset asset{
		.key = dependency,
		.kind = kind,
		.source_path = asset_path,
	};
	catalog_.insert_or_assign(Hash(dependency), asset);
	runtime_states_[Hash(dependency)].metadata = ProbeMetadata(asset);
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
	if (auto existing{ TryGet<Texture>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto texture{ CreateTexture(true, asset_path, storage_format, params) };
	impl::AddAssetKey(texture.GetEntity(), key, asset_path);
	runtime_states_[Hash(key)].load_state = AssetLoadState::Loaded;
	return texture;
}

Font AssetManager::CreateFont(bool persistent, const path& asset_path) {
	PTGN_ASSERT(font_, "FontSystem must be connected before creating fonts");
	Font font{ CreateAsset(), persistent };
	font.GetEntity().Add<impl::FontAtlas>(FontSystem::CreateFontAtlas(renderer_, asset_path));
	return font;
}

Font AssetManager::CreateFont(const path& asset_path) {
	return CreateFont(false, asset_path);
}

Font AssetManager::LoadFont(FontKey key, const path& asset_path) {
	if (auto existing{ TryGet<Font>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto font{ CreateFont(true, asset_path) };
	impl::AddAssetKey(font.GetEntity(), key, asset_path);
	runtime_states_[Hash(key)].load_state = AssetLoadState::Loaded;
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
	if (auto existing{ TryGet<Audio>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto audio{ CreateAudio(true, asset_path) };
	impl::AddAssetKey(audio.GetEntity(), key, asset_path);
	runtime_states_[Hash(key)].load_state = AssetLoadState::Loaded;
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
	const auto validation{ ValidateShaderProgramSourceForLoad(source, max_texture_slots) };
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
	if (auto existing{ TryGet<Shader>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto shader{ CreateShader(true, source, shader_name.value_or(key.value)) };
	std::optional<path> source_path;
	if (const auto* shader_path{ std::get_if<ShaderPath>(&source) }) {
		source_path = shader_path->path;
	}
	impl::AddAssetKey(shader.GetEntity(), key, source_path);
	runtime_states_[Hash(key)].load_state = AssetLoadState::Loaded;
	return shader;
}

Prefab& AssetManager::LoadPrefab(
	PrefabKey key,
	const path& file_path,
	const path& source_path
) {
	if (auto existing{ TryGet<Prefab>(key) }; existing.has_value()) {
		return existing->get();
	}

	auto prefab{ LoadPrefabFile(file_path) };
	prefab.key = key;

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
	runtime_states_[Hash(key)].load_state = AssetLoadState::Loaded;
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
	const auto key{ prefab.key };

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
	catalog_.insert_or_assign(Hash(key), serialized);
	runtime_states_[Hash(key)].load_state = AssetLoadState::Loaded;
	runtime_states_[Hash(key)].metadata = ProbeMetadata(serialized);
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
	return DeleteAsset(key, remove_file);
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
	auto it{ catalog_.find(Hash(key)) };
	if (it == catalog_.end()) {
		PTGN_WARN("Prefab is not in asset catalog: ", key);
		return {};
	}

	return ResolveAssetPath(it->second);
}

json& AssetManager::LoadJson(const JsonKey& key, const path& asset_path) {
	if (auto existing{ TryGet<json>(key) }; existing.has_value()) {
		return existing->get();
	}

	auto [it, inserted]{ jsons_.insert_or_assign(
		Hash(key),
		impl::JsonAssetData{
			.key = key,
			.source_path = asset_path,
			.value = ptgn::LoadJson(asset_path),
		}
	) };
	(void)inserted;
	runtime_states_[Hash(key)].load_state = AssetLoadState::Loaded;
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
	const auto manifest = ptgn::LoadJson(asset_manifest_file);
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
	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	auto validation{ impl::ValidateShaderSource(shader_code.content, max_texture_slots) };
	if (DetectShaderStages(shader_code.content) != ShaderStageMask::VertexFragment) {
		validation.success = false;
		validation.log = "A directly loaded ShaderCode program requires both vertex and fragment stages.";
	}
	if (!validation.success) {
		auto& state{ runtime_states_[Hash(key)] };
		state.load_state = AssetLoadState::Failed;
		state.error = validation.log;
		state.compile_error = true;
		state.compile_log = validation.log;
		PTGN_WARN("Shader failed validation and was not loaded: ", key, "\n", validation.log);
		return;
	}
	LoadShader(std::move(key), shader_code, std::nullopt);
}

void AssetManager::Load(ShaderKey key, const ShaderPair& shader_pair) {
	auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
	auto validation{ ValidateShaderPairForLoad(shader_pair, max_texture_slots) };
	if (!validation.success) {
		auto& state{ runtime_states_[Hash(key)] };
		state.load_state = AssetLoadState::Failed;
		state.error = validation.log;
		state.compile_error = true;
		state.compile_log = validation.log;
		PTGN_WARN("Shader pair failed validation and was not loaded: ", key, "\n", validation.log);
		return;
	}
	LoadShader(std::move(key), shader_pair, std::nullopt);
}

void AssetManager::Load(AssetKey key, const path& asset_path, AssetKind kind) {
	if (!FileExists(asset_path)) {
		PTGN_WARN("Cannot load nonexistent asset: ", asset_path.string());
		return;
	}

	switch (kind) {
		using enum AssetKind;
		case Texture:
			if (impl::IsFontAtlasPng(asset_path)) {
				TrackAssetLoad(key, AssetKind::Font, asset_path);
				LoadFont(FontKey{ std::move(key) }, asset_path);
			} else {
				TrackAssetLoad(key, AssetKind::Texture, asset_path);
				LoadTexture(TextureKey{ std::move(key) }, asset_path);
			}
			break;
		case Audio:
			TrackAssetLoad(key, AssetKind::Audio, asset_path);
			LoadAudio(AudioKey{ std::move(key) }, asset_path);
			break;
		case Font:
			TrackAssetLoad(key, AssetKind::Font, asset_path);
			LoadFont(FontKey{ std::move(key) }, asset_path);
			break;
		case Json:
			TrackAssetLoad(key, AssetKind::Json, asset_path);
			LoadJson(JsonKey{ std::move(key) }, asset_path);
			break;
		case Prefab:
			TrackAssetLoad(key, AssetKind::Prefab, asset_path);
			LoadPrefab(PrefabKey{ std::move(key) }, asset_path, asset_path);
			break;
		case Shader: {
			const auto source{ FileToString(asset_path) };
			TrackAssetLoad(key, AssetKind::Shader, asset_path);
			auto max_texture_slots{ impl::RendererAccessor{ renderer_ }.GetMaxTextureSlots() };
			auto validation{ impl::ValidateShaderSource(source, max_texture_slots) };
			if (!HasVertexAndFragmentShader(source)) {
				validation.success = false;
				validation.log = "Shader requires both vertex and fragment stages or a configured pair.";
			}
			if (!validation.success) {
				auto& state{ runtime_states_[Hash(key)] };
				state.load_state = AssetLoadState::Failed;
				state.error = validation.log;
				state.compile_error = true;
				state.compile_log = validation.log;
				PTGN_WARN(
					"Shader failed validation and was not loaded: ", asset_path.string(), "\n",
					validation.log
				);
				return;
			}
			LoadShader(ShaderKey{ std::move(key) }, ShaderCode{ source }, std::nullopt);
			break;
		}
		case Scene: break;
		case Unknown:
			PTGN_WARN("Unsupported asset file: ", asset_path.string());
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
	const auto hash{ Hash(key) };
	using Object = typename impl::AssetInfo<T>::Object;
	return manager.EntitiesWith<Object, AssetKey>().AnyOf(
		[hash](auto, const auto&, const auto& asset_key) { return Hash(asset_key) == hash; }
	);
}

template <AssetType T>
std::optional<T> TryGetAssetImpl(const ecs::Manager& manager, const AssetKey& key) {
	const auto hash{ Hash(key) };
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
	const auto hash{ Hash(key) };
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
		case Scene:
			return runtime_states_.contains(Hash(key)) &&
				   runtime_states_.at(Hash(key)).load_state == AssetLoadState::Loaded;
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

	for (const auto& [key_hash, serialized] : catalog_) {
		const auto state_it{ runtime_states_.find(key_hash) };
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
		if (key.value == kMissingTextureAssetKey || catalog_.contains(Hash(key))) {
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

	auto state_it{ runtime_states_.find(Hash(key)) };
	if (state_it != runtime_states_.end()) {
		state_it->second.load_state = AssetLoadState::Unloaded;
		state_it->second.manually_pinned = false;
		state_it->second.error.clear();
	}
	return unloaded;
}

bool AssetManager::Unload(const AssetKey& key, AssetKind kind) {
	auto state_it{ runtime_states_.find(Hash(key)) };
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
		auto catalog_it{ catalog_.find(Hash(dependencies[index])) };
		if (catalog_it == catalog_.end() || catalog_it->second.kind != AssetKind::Prefab) {
			continue;
		}

		const auto prefab_path{ ResolveAssetPath(catalog_it->second) };
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

	if (const auto asset{ assets.GetCatalogAsset(texture_key) }) {
		source_path = asset->source_path;
	} else {
		AssetAccessor accessor{ assets };

		if (accessor.Has<Texture>(texture_key)) {
			const Texture texture{ accessor.Get<Texture>(texture_key) };

			if (const auto asset_path{ texture.GetEntity().TryGet<AssetPath>() }) {
				source_path = asset_path->value;
			}
		}
	}

	if (!source_path) {
		return std::nullopt;
	}

	const std::string stem_string{ source_path->stem().string() };
	const std::string_view stem{ stem_string };
	const auto marker_position{ stem.rfind(marker) };

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
