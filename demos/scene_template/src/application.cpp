#include "core/game.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

void ApplyVerticalLayout(
	std::vector<Entity>& entities, const V2_float& origin, float spacing, bool center_items
) {
	float total_height = static_cast<float>(entities.size()) * spacing;
	float start_y	   = origin.y;

	if (center_items) {
		start_y = origin.y - (total_height - spacing) / 2.0f;
	}

	for (std::size_t i = 0; i < entities.size(); ++i) {
		SetPosition(entities[i], { origin.x, start_y + i * spacing });
	}
}

void ApplyHorizontalLayout(
	std::vector<Entity>& entities, const V2_float& origin, float spacing, bool center_items
) {
	float total_width = static_cast<float>(entities.size()) * spacing;
	float start_x	  = origin.x;

	if (center_items) {
		start_x = origin.x - (total_width - spacing) / 2.0f;
	}

	for (std::size_t i = 0; i < entities.size(); ++i) {
		SetPosition(entities[i], { start_x + i * spacing, origin.y });
	}
}

void ApplyGridLayout(
	std::vector<Entity>& entities, const V2_float& origin, const V2_float& spacing,
	const V2_int& grid_size
) {
	int rows = std::max(1, grid_size.y);
	int cols = std::max(1, grid_size.x);

	if (rows == 1 && cols == 1 && entities.size() > 1) {
		cols = static_cast<int>(entities.size());
	}

	V2_float total{ cols * spacing.x, rows * spacing.y };

	V2_float start{ origin - (total - spacing) / 2.0f };

	for (std::size_t i = 0; i < entities.size(); ++i) {
		int r = static_cast<int>(i) / cols; // entities[i].row.value_or
		int c = static_cast<int>(i) % cols; // entities[i].col.value_or
		SetPosition(entities[i], { start + V2_float{ c, r } * spacing });
	}
}

void LoadScene(const path& scene_file) {
	json j{ LoadJson(scene_file) };
	PTGN_LOG(j.dump(4));
}

// --------------------------------------------

class SceneTemplateExample : public Scene {
public:
	SceneTemplateExample() {
		LoadResource({ { "bg1", "resources/bg1.png" },
					   { "bg2", "resources/bg2.png" },
					   { "bg3", "resources/bg3.png" } });

		LoadScene("resources/scenes.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SceneTemplateExample", resolution);
	game.scene.Enter<SceneTemplateExample>("");
	return 0;
}