#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/hash.h"
#include "core/util/string.h"
#include "runtime/scene/scene.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class Application;

namespace impl {

struct SceneFactory {
	using Construct = std::function<std::unique_ptr<Scene>(Application&, SceneData&&)>;
	using Preload = std::function<std::vector<AssetKey>(Application&)>;

	SceneFactory() = default;
	SceneFactory(std::nullptr_t) {}
	SceneFactory(Construct scene_construct) : construct{ std::move(scene_construct) } {} // NOSONAR
	SceneFactory(Construct scene_construct, Preload scene_preload) :
		construct{ std::move(scene_construct) }, preload{ std::move(scene_preload) } {}

	[[nodiscard]] explicit operator bool() const {
		return static_cast<bool>(construct);
	}

	std::unique_ptr<Scene> operator()(Application& app, SceneData&& scene_data) const {
		PTGN_ASSERT(construct, "Cannot invoke an empty scene factory");
		return construct(app, std::move(scene_data));
	}

	[[nodiscard]] std::vector<AssetKey> GetPreloadDependencies(Application& app) const {
		return preload ? preload(app) : std::vector<AssetKey>{};
	}

	Construct construct{};
	Preload preload{};
};

struct SceneRegistryEntry {
	std::string type{};
	std::size_t type_id{ 0 };

	std::function<json()> default_parameters{};
	std::function<std::unique_ptr<Scene>(const json& parameters)> construct{};
	std::function<json(const Scene& scene)> serialize_parameters{};
	std::function<void(const json& parameters, Scene& scene)> deserialize_parameters{};
	std::function<std::vector<AssetKey>(const json& parameters)> preload_dependencies{};
};

inline auto& GetSceneRegistry() {
	static std::unordered_map<std::string, SceneRegistryEntry, StringHash, std::equal_to<>> registry;
	return registry;
}

inline auto& GetSceneCppTypeRegistry() {
	static std::unordered_map<std::size_t, std::string> registry;
	return registry;
}

template <typename TScene>
[[nodiscard]] json SerializeSceneParameters(const TScene& scene) {
	if constexpr (JsonSerializable<TScene>) {
		json parameters = scene;
		return parameters;
	} else {
		return json::object();
	}
}

template <typename TScene>
void DeserializeSceneParameters(const json& parameters, TScene& scene) {
	if constexpr (JsonDeserializable<TScene>) {
		PTGN_ASSERT(
			parameters.is_object(),
			"Scene parameters must be a JSON object for scene type: ",
			type_name<TScene>()
		);

		json complete_parameters = SerializeSceneParameters(scene);
		for (const auto& [key, value] : parameters.items()) {
			std::string normalized_key{ key };
			if (!normalized_key.empty() && normalized_key.back() == '_') {
				normalized_key.pop_back();
			}
			complete_parameters[normalized_key] = value;
		}

		try {
			complete_parameters.get_to(scene);
		} catch (const json::exception& error) {
			PTGN_ERROR(
				"Failed to deserialize parameters for scene type ",
				type_name<TScene>(),
				": ",
				error.what(),
				"\nParameters: ",
				complete_parameters.dump(4)
			);
		}
	} else {
		PTGN_ASSERT(
			parameters.empty(),
			"A scene without PTGN_REFLECT cannot have serialized parameters"
		);
	}
}

template <typename TScene, typename Serialize, typename Deserialize>
void DeserializeSceneParameters(
	const json& parameters,
	TScene& scene,
	const Serialize& serialize,
	const Deserialize& deserialize
) {
	PTGN_ASSERT(
		parameters.is_object(),
		"Scene parameters must be a JSON object for scene type: ",
		type_name<TScene>()
	);

	json complete_parameters{ std::invoke(serialize, std::as_const(scene)) };
	for (const auto& [key, value] : parameters.items()) {
		std::string normalized_key{ key };
		if (!normalized_key.empty() && normalized_key.back() == '_') {
			normalized_key.pop_back();
		}
		complete_parameters[normalized_key] = value;
	}

	try {
		std::invoke(deserialize, complete_parameters, scene);
	} catch (const json::exception& error) {
		PTGN_ERROR(
			"Failed to deserialize parameters for scene type ",
			type_name<TScene>(),
			": ",
			error.what(),
			"\nParameters: ",
			complete_parameters.dump(4)
		);
	}
}

template <SceneType TScene, typename Serialize, typename Deserialize>
	requires std::default_initializable<TScene>
bool RegisterSceneWithSerialization(Serialize serialize, Deserialize deserialize) {
	auto& registry{ GetSceneRegistry() };
	constexpr auto type_id{ Hash<TScene>() };
	constexpr std::string_view type{ type_name<TScene>() };

	if (auto existing{ registry.find(type) }; existing != registry.end()) {
		PTGN_ASSERT(
			existing->second.type_id == type_id,
			"Scene registration key is already used by another C++ type: ",
			type
		);
		return false;
	}

	SceneRegistryEntry entry{
		.type = std::string{ type },
		.type_id = type_id,
		.default_parameters = [serialize] {
			TScene scene;
			return std::invoke(serialize, std::as_const(scene));
		},
		.construct = [serialize, deserialize](const json& parameters) -> std::unique_ptr<Scene> {
			auto scene{ std::make_unique<TScene>() };
			DeserializeSceneParameters(parameters, *scene, serialize, deserialize);
			return scene;
		},
		.serialize_parameters = [serialize](const Scene& scene) {
			return std::invoke(serialize, static_cast<const TScene&>(scene));
		},
		.deserialize_parameters = [serialize, deserialize](const json& parameters, Scene& scene) {
			DeserializeSceneParameters(
				parameters, static_cast<TScene&>(scene), serialize, deserialize
			);
		},
		.preload_dependencies = [serialize, deserialize](const json& parameters) {
			TScene scene;
			DeserializeSceneParameters(parameters, scene, serialize, deserialize);
			AssetPreloadContext preload;
			scene.OnPreload(preload);
			for (const auto& key : scene.GetExplicitAssetDependencies()) {
				preload.Add(key);
			}
			return preload.GetDependencies();
		},
	};

	auto [it, inserted]{ registry.emplace(entry.type, std::move(entry)) };
	PTGN_ASSERT(inserted, "Failed to register scene type: ", type);
	GetSceneCppTypeRegistry().emplace(type_id, it->first);
	return true;
}

template <SceneType TScene>
	requires std::default_initializable<TScene>
bool RegisterScene() {
	return RegisterSceneWithSerialization<TScene>(
		[](const TScene& scene) { return SerializeSceneParameters(scene); },
		[](const json& parameters, TScene& scene) {
			DeserializeSceneParameters(parameters, scene);
		}
	);
}

template <SceneType TScene, typename Serialize, typename Deserialize>
	requires std::default_initializable<TScene>
bool RegisterScene(Serialize serialize, Deserialize deserialize) {
	return RegisterSceneWithSerialization<TScene>(std::move(serialize), std::move(deserialize));
}

[[nodiscard]] inline const SceneRegistryEntry& GetSceneRegistration(std::string_view type) {
	auto it{ GetSceneRegistry().find(type) };
	PTGN_ASSERT(it != GetSceneRegistry().end(), "Scene type is not registered: ", type);
	return it->second;
}

template <SceneType TScene>
[[nodiscard]] const SceneRegistryEntry& GetSceneRegistration() {
	constexpr auto type_id{ Hash<TScene>() };
	auto it{ GetSceneCppTypeRegistry().find(type_id) };
	PTGN_ASSERT(
		it != GetSceneCppTypeRegistry().end(),
		"Scene type has not been registered with PTGN_REGISTER_SCENE"
	);
	return GetSceneRegistration(it->second);
}

template <SceneType TScene>
[[nodiscard]] std::string GetRegisteredSceneType() {
	constexpr auto type_id{ Hash<TScene>() };

	const auto it{ GetSceneCppTypeRegistry().find(type_id) };
	return it == GetSceneCppTypeRegistry().end() ? std::string{} : it->second;
}

} // namespace impl

} // namespace ptgn

#define PTGN_IMPL_SCENE_CONCAT_INNER(a, b) a##b
#define PTGN_IMPL_SCENE_CONCAT(a, b) PTGN_IMPL_SCENE_CONCAT_INNER(a, b)

#define PTGN_IMPL_SCENE_JSON_TO(member)                                                   \
	::ptgn::impl::extended_to_json(                                                       \
		::ptgn::impl::StripTrailingUnderscore(#member), parameters, scene.member          \
	);

#define PTGN_IMPL_SCENE_JSON_FROM(member)                                                 \
	::ptgn::impl::extended_from_json(                                                     \
		::ptgn::impl::StripTrailingUnderscore(#member), parameters, scene.member          \
	);

/// @brief Registers a scene using its C++ type name as the registry key.
///
/// Optional member names explicitly define the scene parameters serialized by the registry:
/// PTGN_REGISTER_SCENE(MyScene, level, seed);
///
/// With no member names, the scene's normal JSON serialization is used when available.
#define PTGN_REGISTER_SCENE(SceneTypeName, ...)                                           \
	namespace {                                                                            \
	[[maybe_unused]] const bool PTGN_IMPL_SCENE_CONCAT(_ptgn_registered_scene_, __COUNTER__) = \
		::ptgn::impl::RegisterScene<SceneTypeName>(                                        \
			__VA_OPT__(                                                                     \
				[](const SceneTypeName& scene) {                                             \
					::ptgn::json parameters{ ::ptgn::json::object() };                       \
					NLOHMANN_JSON_EXPAND(                                                    \
						NLOHMANN_JSON_PASTE(PTGN_IMPL_SCENE_JSON_TO, __VA_ARGS__)           \
					)                                                                          \
					return parameters;                                                        \
				},                                                                             \
				[](const ::ptgn::json& parameters, SceneTypeName& scene) {                    \
					NLOHMANN_JSON_EXPAND(                                                    \
						NLOHMANN_JSON_PASTE(PTGN_IMPL_SCENE_JSON_FROM, __VA_ARGS__)         \
					)                                                                          \
				}                                                                              \
			)                                                                                \
		);                                                                                  \
	}
