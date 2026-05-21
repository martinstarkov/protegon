#include "runtime/ui/menu_template.h"

#include <algorithm>
#include <functional>
#include <list>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "runtime/ecs/entity.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "serialization/json/fwd.h"

namespace ptgn {

namespace impl {

void ApplyVerticalLayout(
	std::vector<Entity>& entities, V2_float origin, float spacing, bool center_items
) {
	float total_height = static_cast<float>(entities.size()) * spacing;
	float start_y	   = origin.y;

	if (center_items) {
		start_y = origin.y - (total_height - spacing) / 2.0f;
	}

	for (auto i{ 0uz }; i < entities.size(); ++i) {
		SetPosition(entities[i], { origin.x, start_y + static_cast<float>(i) * spacing });
	}
}

void ApplyHorizontalLayout(
	std::vector<Entity>& entities, V2_float origin, float spacing, bool center_items
) {
	float total_width = static_cast<float>(entities.size()) * spacing;
	float start_x	  = origin.x;

	if (center_items) {
		start_x = origin.x - (total_width - spacing) / 2.0f;
	}

	for (auto i{ 0uz }; i < entities.size(); ++i) {
		SetPosition(entities[i], { start_x + static_cast<float>(i) * spacing, origin.y });
	}
}

void ApplyGridLayout(
	std::vector<Entity>& entities, V2_float origin, V2_float spacing, V2_int grid_size
) {
	int rows = std::max(1, grid_size.y);
	int cols = std::max(1, grid_size.x);

	if (rows == 1 && cols == 1 && entities.size() > 1) {
		cols = static_cast<int>(entities.size());
	}

	V2_float total{ static_cast<float>(cols) * spacing.x, static_cast<float>(rows) * spacing.y };

	V2_float start{ origin - (total - spacing) / 2.0f };

	for (auto i{ 0uz }; i < entities.size(); ++i) {
		int r = static_cast<int>(i) / cols; // entities[i].row.value_or
		int c = static_cast<int>(i) % cols; // entities[i].col.value_or
		SetPosition(entities[i], { start + V2_float{ c, r } * spacing });
	}
}

TemplateMenuScene::TemplateMenuScene(const std::string& key, const json& scene_json_arg) :
	key{ key }, scene_json(scene_json_arg) {}

void TemplateMenuScene::OnEnter() {
	ctx().interaction.SetDebugSettings({ .draw_enabled = true });

	PTGN_ASSERT(scene_json.contains(key));

	const auto& config = scene_json.at(key);

	std::vector<Entity> buttons;

	const auto& j_buttons = config.at("buttons");

	for (const auto& j_button : j_buttons) {
		const V2_float button_size{ 100, 50 };
		const Color button_text_color{ color::White };

		const auto& label = j_button.at("label");
		auto button{ CreateButton(*this, {}) };
		button.SetShape(button_size);
		auto label_string{ label.get<std::string>() };
		// TODO: Fix.
		// button.SetText(label_string, button_text_color);
		const auto& action_name{ j_button.at("action").get<std::string>() };
		button.OnPress([key = key, scene_json = scene_json, action_name, button]() mutable {
			std::invoke(SceneAction::Get(key, scene_json, action_name), button.GetScene());
		});
		buttons.emplace_back(button);
	}

	const auto& layout = config.at("layout");

	PTGN_ASSERT(layout.contains("origin"));
	PTGN_ASSERT(layout.contains("spacing"));

	V2_float origin{ layout.at("origin").get<V2_float>() };

	auto template_type{ config.at("template").get<std::string>() };

	if (template_type == "VerticalList") {
		const auto& j_spacing = layout.at("spacing");
		PTGN_ASSERT(j_spacing.is_number());
		bool center_items{ layout.contains("center_items") ? layout.at("center_items").get<bool>()
														   : true };
		ApplyVerticalLayout(buttons, origin, j_spacing.get<float>(), center_items);
	} else if (template_type == "HorizontalList") {
		const auto& j_spacing = layout.at("spacing");
		PTGN_ASSERT(j_spacing.is_number());
		bool center_items{ layout.contains("center_items") ? layout.at("center_items").get<bool>()
														   : true };
		ApplyHorizontalLayout(buttons, origin, j_spacing.get<float>(), center_items);
	} else if (template_type == "Grid") {
		const auto& j_spacing = layout.at("spacing");
		PTGN_ASSERT(j_spacing.is_array());
		PTGN_ASSERT(layout.contains("rows"));
		PTGN_ASSERT(layout.contains("columns"));
		V2_float grid_size{ layout.at("columns").get<float>(), layout.at("rows").get<float>() };
		V2_float spacing{ j_spacing.get<V2_float>() };
		ApplyGridLayout(buttons, origin, spacing, grid_size);
	}
}

} // namespace impl

void SceneAction::Register(const std::string& name, const std::function<void(Scene&)>& action) {
	auto& registry{ GetInstance() };
	auto key{ Hash(name) };
	PTGN_ASSERT(!registry.actions_.contains(key), "Action name: ", name, " already registered");
	registry.actions_.try_emplace(key, action);
}

SceneAction::SceneAction() {
	actions_ = { { Hash("quit"), [](Scene& scene) mutable {
					  scene.ctx().Stop();
				  } } };

	prefix_handlers_ = {
		{ "enter:",
		  [](const std::string&, const json& scenes, const std::string& to, Scene& scene) mutable {
			  scene.ctx().scene.Switch<impl::TemplateMenuScene>(to, scenes, scenes);
		  } },
		{ "transition:",
		  [](const std::string& from, const json& scenes, const std::string& to,
			 Scene& scene) mutable {
			  scene.ctx().scene.Switch<impl::TemplateMenuScene>(from, to, scenes);
		  } }
	};
}

SceneAction& SceneAction::GetInstance() {
	static SceneAction registry;
	return registry;
}

std::function<void(Scene&)> SceneAction::Get(
	const std::string& from_key, const json& scene_json, const std::string& action_name
) {
	const auto& registry{ GetInstance() };

	if (auto action_it{ registry.actions_.find(Hash(action_name)) };
		action_it != registry.actions_.end()) {
		return action_it->second;
	}

	// Check for known prefix.
	for (const auto& [prefix, handler] : registry.prefix_handlers_) {
		if (action_name.rfind(prefix, 0) == 0) { // starts with prefix
			std::string to_key{ action_name.substr(prefix.length()) };
			return [from_key, handler, to_key, scene_json](auto& scene) {
				handler(from_key, scene_json, to_key, scene);
			};
		}
	}

	PTGN_ERROR("Unknown action: ", action_name);
}

void EnterSceneConfig(Scene& scene, const json& template_json) {
	PTGN_ASSERT(template_json.contains("scenes"), "Scene config must contain a scenes dictionary");
	PTGN_ASSERT(template_json.contains("start_scene"), "Scene config must specify a start scene");

	const json& scene_json = template_json.at("scenes");

	std::string start_scene{ template_json.at("start_scene").get<std::string>() };

	PTGN_ASSERT(scene_json.contains(start_scene), "Start scene must be in the scenes dictionary");

	scene.ctx().scene.Switch<impl::TemplateMenuScene>(start_scene, start_scene, scene_json);
}

} // namespace ptgn