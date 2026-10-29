#include "runtime/ecs/entity_hierarchy.h"

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class EntityHierarchyScene : public Scene {
	void OnEnter() override {
		auto transform_parent{
			CreateRect(*this, { 0, 0 }, { 180, 120 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(transform_parent, Color{ 220, 220, 220, 180 });
		SetDepth(transform_parent, 0);
		SetRotation(transform_parent, 25);
		SetScale(transform_parent, { 1.1f, 1 });

		auto transform_child_a{
			CreateRect(*this, { -45, 0 }, { 70, 45 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(transform_child_a, Color{ 80, 160, 255, 255 });
		SetDepth(transform_child_a, 1);
		SetRotation(transform_child_a, -15);
		SetParent(transform_child_a, transform_parent);

		auto transform_child_b{
			CreateRect(*this, { 45, 0 }, { 70, 45 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(transform_child_b, Color{ 255, 180, 80, 255 });
		SetDepth(transform_child_b, 1);
		SetRotation(transform_child_b, 15);
		SetParent(transform_child_b, transform_parent);

		auto tint_parent{
			CreateRect(*this, { 400, 0 }, { 180, 120 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(tint_parent, Color{ 255, 150, 150, 255 });
		SetDepth(tint_parent, 0);

		auto tint_child_a{
			CreateRect(*this, { -40, 0 }, { 80, 55 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(tint_child_a, Color{ 80, 160, 255, 255 });
		SetDepth(tint_child_a, 1);
		SetParent(tint_child_a, tint_parent);

		auto tint_child_b{
			CreateRect(*this, { 40, 0 }, { 80, 55 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(tint_child_b, Color{ 120, 255, 120, 255 });
		SetDepth(tint_child_b, 1);
		SetParent(tint_child_b, tint_parent);

		auto depth_parent{
			CreateRect(*this, { -400, 0 }, { 180, 120 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(depth_parent, Color{ 180, 180, 180, 220 });
		SetDepth(depth_parent, 10);
		SetRotation(depth_parent, -15);

		auto depth_child_back{
			CreateRect(*this, { -25, 0 }, { 100, 70 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(depth_child_back, Color{ 0, 255, 255, 255 });
		SetDepth(depth_child_back, -1);
		SetParent(depth_child_back, depth_parent);

		auto depth_child_front{
			CreateRect(*this, { 25, 0 }, { 100, 70 }, color::White, Solid{}, Origin::Center)
		};

		SetTint(depth_child_front, Color{ 255, 220, 80, 255 });
		SetDepth(depth_child_front, 1);
		SetParent(depth_child_front, depth_parent);
	}
};

int main(int, char**) {
	Application app{ "entity_hierarchy" };
	PTGN_WITH_EDITOR(app);
	app.StartWith<EntityHierarchyScene>();
}