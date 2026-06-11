#include "runtime/ecs/entity_hierarchy.h"

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class EntityHierarchyScene : public Scene {
	void OnEnter() override {
		auto transform_parent{ CreateRect(
			*this, { -260.0f, 60.0f }, { 180.0f, 120.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(transform_parent, Color{ 220, 220, 220, 180 });
		SetDepth(transform_parent, 0.0f);
		SetRotation(transform_parent, 25.0f);
		SetScale(transform_parent, { 1.1f, 1.0f });

		auto transform_child_a{ CreateRect(
			*this, { -45.0f, 0.0f }, { 70.0f, 45.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(transform_child_a, Color{ 80, 160, 255, 255 });
		SetDepth(transform_child_a, 1.0f);
		SetRotation(transform_child_a, -15.0f);
		SetParent(transform_child_a, transform_parent);

		auto transform_child_b{ CreateRect(
			*this, { 45.0f, 0.0f }, { 70.0f, 45.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(transform_child_b, Color{ 255, 180, 80, 255 });
		SetDepth(transform_child_b, 1.0f);
		SetRotation(transform_child_b, 15.0f);
		SetParent(transform_child_b, transform_parent);

		auto tint_parent{ CreateRect(
			*this, { 0.0f, 60.0f }, { 180.0f, 120.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(tint_parent, Color{ 255, 150, 150, 255 });
		SetDepth(tint_parent, 0.0f);

		auto tint_child_a{ CreateRect(
			*this, { -40.0f, 0.0f }, { 80.0f, 55.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(tint_child_a, Color{ 80, 160, 255, 255 });
		SetDepth(tint_child_a, 1.0f);
		SetParent(tint_child_a, tint_parent);

		auto tint_child_b{ CreateRect(
			*this, { 40.0f, 0.0f }, { 80.0f, 55.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(tint_child_b, Color{ 120, 255, 120, 255 });
		SetDepth(tint_child_b, 1.0f);
		SetParent(tint_child_b, tint_parent);

		auto depth_parent{ CreateRect(
			*this, { 260.0f, 60.0f }, { 180.0f, 120.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(depth_parent, Color{ 180, 180, 180, 220 });
		SetDepth(depth_parent, 10.0f);
		SetRotation(depth_parent, -15.0f);

		auto depth_child_back{ CreateRect(
			*this, { -25.0f, 0.0f }, { 100.0f, 70.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(depth_child_back, Color{ 0, 255, 255, 255 });
		SetDepth(depth_child_back, -1.0f);
		SetParent(depth_child_back, depth_parent);

		auto depth_child_front{ CreateRect(
			*this, { 25.0f, 0.0f }, { 100.0f, 70.0f }, color::White, Solid{}, Origin::Center
		) };

		SetTint(depth_child_front, Color{ 255, 220, 80, 255 });
		SetDepth(depth_child_front, 1.0f);
		SetParent(depth_child_front, depth_parent);
	}
};

int main(int, char**) {
	Application app{ "entity_hierarchy" };
	PTGN_WITH_EDITOR(app);
	app.StartWith<EntityHierarchyScene>();
}