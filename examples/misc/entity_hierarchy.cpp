#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"
#include "core/editor.h"

using namespace ptgn;

class EntityHierarchyScene : public Scene {
	void OnEnter() override {
		// Independent reference rects. These do not have parents.
		CreateRect(*this, { -260.0f, -150.0f }, { 180.0f, 40.0f }, Color{ 80, 80, 80, 255 }, 0.0f, Origin::Center);
		CreateRect(*this, { 0.0f, -150.0f }, { 180.0f, 40.0f }, Color{ 80, 80, 80, 255 }, 0.0f, Origin::Center);
		CreateRect(*this, { 260.0f, -150.0f }, { 180.0f, 40.0f }, Color{ 80, 80, 80, 255 }, 0.0f, Origin::Center);

		// ---------------------------------------------------------------------
		// 1. Transform hierarchy.
		// ---------------------------------------------------------------------
		auto transform_parent{
			CreateRect(
				*this,
				{ -260.0f, 60.0f },
				{ 180.0f, 120.0f },
				Color{ 220, 220, 220, 180 },
				0.0f,
				Origin::Center
			)
		};

		SetRotation(transform_parent, 25.0f);
		SetScale(transform_parent, { 1.1f, 1.0f });

		auto transform_child_a{
			CreateRect(
				*this,
				{ -45.0f, 0.0f },
				{ 70.0f, 45.0f },
				Color{ 80, 160, 255, 255 },
				1.0f,
				Origin::Center
			)
		};

		auto transform_child_b{
			CreateRect(
				*this,
				{ 45.0f, 0.0f },
				{ 70.0f, 45.0f },
				Color{ 255, 180, 80, 255 },
				1.0f,
				Origin::Center
			)
		};

		SetParent(transform_child_a, transform_parent);
		SetParent(transform_child_b, transform_parent);

		// These rotations are local. They are applied on top of the parent rotation.
		SetRotation(transform_child_a, -15.0f);
		SetRotation(transform_child_b, 15.0f);

		// ---------------------------------------------------------------------
		// 2. Tint hierarchy.
		// ---------------------------------------------------------------------
		auto tint_parent{
			CreateRect(
				*this,
				{ 0.0f, 60.0f },
				{ 180.0f, 120.0f },
				Color{ 255, 160, 160, 255 },
				0.0f,
				Origin::Center
			)
		};

		auto tint_child_a{
			CreateRect(
				*this,
				{ -40.0f, 0.0f },
				{ 80.0f, 55.0f },
				Color{ 80, 160, 255, 255 },
				1.0f,
				Origin::Center
			)
		};

		auto tint_child_b{
			CreateRect(
				*this,
				{ 40.0f, 0.0f },
				{ 80.0f, 55.0f },
				Color{ 120, 255, 120, 255 },
				1.0f,
				Origin::Center
			)
		};

		SetParent(tint_child_a, tint_parent);
		SetParent(tint_child_b, tint_parent);

		// Parent tint should multiply into both children.
		// So the children keep their own tint, but become affected by the warm parent tint.
		SetTint(tint_parent, Color{ 255, 150, 150, 255 });

		// ---------------------------------------------------------------------
		// 3. Relative depth hierarchy.
		// ---------------------------------------------------------------------
		auto depth_parent{
			CreateRect(
				*this,
				{ 260.0f, 60.0f },
				{ 180.0f, 120.0f },
				Color{ 180, 180, 180, 220 },
				10.0f,
				Origin::Center
			)
		};

		auto depth_child_back{
			CreateRect(
				*this,
				{ -25.0f, 0.0f },
				{ 100.0f, 70.0f },
				Color{ 80, 160, 255, 255 },
				-1.0f,
				Origin::Center
			)
		};

		auto depth_child_front{
			CreateRect(
				*this,
				{ 25.0f, 0.0f },
				{ 100.0f, 70.0f },
				Color{ 255, 220, 80, 255 },
				1.0f,
				Origin::Center
			)
		};

		SetParent(depth_child_back, depth_parent);
		SetParent(depth_child_front, depth_parent);

		// With parent-relative depth:
		// depth_parent      = 10
		// depth_child_back  = 10 + -1 = 9
		// depth_child_front = 10 +  1 = 11
		//
		// So the yellow child should draw in front of the blue child.

		SetRotation(depth_parent, -15.0f);
	}
};

int main(int, char**) {
	Application app{ "entity_hierarchy" };
	PTGN_WITH_EDITOR(app);
	app.StartWith<EntityHierarchyScene>();
}