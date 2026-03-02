#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/math/vector2.h"
#include "core/util/file.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "serialization/json/fwd.h"

namespace ptgn {

class ApplicationContext;

namespace impl {

void ApplyVerticalLayout(
	std::vector<Entity>& entities, V2_float origin, float spacing, bool center_items
);

void ApplyHorizontalLayout(
	std::vector<Entity>& entities, V2_float origin, float spacing, bool center_items
);

void ApplyGridLayout(
	std::vector<Entity>& entities, V2_float origin, V2_float spacing, V2_int grid_size
);

class TemplateMenuScene : public Scene {
public:
	TemplateMenuScene() = default;

	TemplateMenuScene(const std::string& key, const json& scene_json_arg);

	std::string key;
	json scene_json;

	void OnEnter() override;
};

} // namespace impl

class SceneAction {
public:
	static void Register(
		const Scene& scene, const std::string& name, const std::function<void()>& action
	);

private:
	friend class impl::TemplateMenuScene;

	void SetContext(const std::shared_ptr<ApplicationContext>& ctx);

	static SceneAction& GetInstance(const std::shared_ptr<ApplicationContext>& ctx);

	static std::function<void()> Get(
		const Scene& scene, const std::string& from_key, const json& scene_json,
		const std::string& action_name
	);

	using PrefixFunc = std::function<void(const std::string&, const json&, const std::string&)>;

	std::shared_ptr<ApplicationContext> ctx_;

	std::unordered_map<std::size_t, std::function<void()>> actions_;

	// Prefix string is used to compare the scene action.
	std::unordered_map<std::string, PrefixFunc> prefix_handlers_;
};

void EnterSceneConfig(Scene& scene, const path& template_json_path);

} // namespace ptgn