// #pragma once
//
// #include <functional>
// #include <string>
// #include <unordered_map>
// #include <vector>
//
// #include "ecs/entity.h"
// #include "math/vector2.h"
// #include "serialization/json/fwd.h"
// #include "scene/scene.h"
//
// namespace ptgn {
//
// namespace impl {
//
// void ApplyVerticalLayout(
//	std::vector<Entity>& entities, const V2_float& origin, float spacing, bool center_items
//);
//
// void ApplyHorizontalLayout(
//	std::vector<Entity>& entities, const V2_float& origin, float spacing, bool center_items
//);
//
// void ApplyGridLayout(
//	std::vector<Entity>& entities, const V2_float& origin, const V2_float& spacing,
//	const V2_int& grid_size
//);
//
// class TemplateMenuScene : public Scene {
// public:
//	TemplateMenuScene() = default;
//
//	TemplateMenuScene(const std::string& key, const json& scene_json_arg);
//
//	std::string key;
//	json scene_json;
//
//	void Enter() override;
// };
//
// } // namespace impl
//
// class SceneAction {
// public:
//	static void Register(const std::string& name, const std::function<void()>& action);
//
// private:
//	friend class impl::TemplateMenuScene;
//
//	SceneAction();
//
//	static SceneAction& GetInstance();
//
//	static std::function<void()> Get(
//		const std::string& from_key, const json& scene_json, const std::string& action_name
//	);
//
//	using PrefixFunc = std::function<void(const std::string&, const json&, const std::string&)>;
//
//	std::unordered_map<std::size_t, std::function<void()>> actions_;
//
//	// Prefix string is used to compare the scene action.
//	std::unordered_map<std::string, PrefixFunc> prefix_handlers_;
// };
//
// } // namespace ptgn