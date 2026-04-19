#pragma once

#include <functional>
#include <memory>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "core/util/macro_loop.h"
#include "core/util/string.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class Application;

namespace impl {

template <typename SceneT, typename Tuple, std::size_t... I>
json BuildDefaultsImpl(const Tuple& fields, std::index_sequence<I...>) {
	SceneT scene{};
	json out = json::object();

	(
		[&] {
			const auto& f = std::get<I>(fields);
			using MemberT = std::remove_cvref_t<decltype(scene.*(f.member))>;
			if constexpr (JsonSerializable<MemberT>) {
				out[f.name] = scene.*(f.member);
			}
		}(),
		...
	);

	return out;
}

struct SceneRegistryEntry {
	std::string display_name;
	std::function<json()> default_params;
	std::function<std::function<std::unique_ptr<Scene>(Application&)>(const json&)> construct;
};

inline std::unordered_map<std::string, SceneRegistryEntry, StringHash, std::equal_to<>>&
GetSceneRegistry() {
	static std::unordered_map<std::string, SceneRegistryEntry, StringHash, std::equal_to<>> reg;
	return reg;
}

template <typename SceneT, typename MemberT>
struct FieldDesc {
	const char* name;
	MemberT SceneT::* member;
};

template <typename SceneT, typename MemberT>
constexpr auto MakeField(const char* name, MemberT SceneT::* member) {
	return FieldDesc<SceneT, MemberT>{ name, member };
}

template <typename TScene, typename Factory, typename... FieldTs>
void RegisterScene(
	std::string_view display_name, std::tuple<FieldTs...> fields, Factory&& factory
) {
	GetSceneRegistry()[type_name<TScene>()] = SceneRegistryEntry{
		.display_name = std::string{ display_name },
		.default_params =
			[fields]() {
				return BuildDefaultsImpl<TScene>(fields, std::index_sequence_for<FieldTs...>{});
			},
		.construct = std::forward<Factory>(factory)
	};
}

std::function<std::unique_ptr<Scene>(Application&)> GetSceneFactory(
	std::string_view scene_name, const json& scene_params
) {
	return GetSceneRegistry()[scene_name].construct(scene_params);
}

} // namespace impl

} // namespace ptgn

#define PTGN_IMPL_FIELD_OF(X, SceneType) ::ptgn::impl::MakeField<SceneType>(#X, &SceneType::X)

#define PTGN_REGISTER_SCENE(SceneType, DisplayName, ...)                                           \
	static void to_json(::ptgn::json& nlohmann_json_j, const SceneType& nlohmann_json_t) {         \
		__VA_OPT__(NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_TO, __VA_ARGS__) \
		))                                                                                         \
	}                                                                                              \
	static void from_json(const ::ptgn::json& nlohmann_json_j, SceneType& nlohmann_json_t) {       \
		__VA_OPT__(                                                                                \
			NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_FROM, __VA_ARGS__))     \
		)                                                                                          \
	}                                                                                              \
	namespace ptgn::impl {                                                                         \
	static const bool _ptgn_registered_##SceneType = [] {                                          \
		RegisterScene<SceneType>(                                                                  \
			DisplayName,                                                                           \
			std::make_tuple(                                                                       \
				__VA_OPT__(PTGN_MAP_LIST_DATA(PTGN_IMPL_FIELD_OF, SceneType, __VA_ARGS__))         \
			),                                                                                     \
			[](const ::ptgn::json& j                                                               \
			) -> std::function<std::unique_ptr<::ptgn::Scene>(::ptgn::Application&)> {             \
				return [j](::ptgn::Application& app) -> std::unique_ptr<::ptgn::Scene> {           \
					auto scene{ std::make_unique<SceneType>() };                                   \
					from_json(j, *scene);                                                          \
					scene->Init(app);                                                              \
					return scene;                                                                  \
				};                                                                                 \
			}                                                                                      \
		);                                                                                         \
		return true;                                                                               \
	}()                                                                                            \
	}