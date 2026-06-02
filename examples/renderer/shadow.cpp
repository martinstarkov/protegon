
#include "app/application.h"
#include "core/graphics/color.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

impl::VisibilityPolygon CreateSampleVisibilityPolygon(float radius) {
	impl::VisibilityPolygon polygon;

	auto r{ radius };

	// Light-local, clockwise, non-self-intersecting concave polygon.
	// This gives you a "room with cutouts/corridors" style mask.
	polygon.vertices = {
		{ -0.90f * r, -0.70f * r }, { -0.35f * r, -0.70f * r }, { -0.35f * r, -0.38f * r },
		{ 0.05f * r, -0.38f * r },	{ 0.05f * r, -0.62f * r },	{ 0.78f * r, -0.62f * r },
		{ 0.78f * r, -0.08f * r },	{ 0.42f * r, -0.08f * r },	{ 0.42f * r, 0.34f * r },
		{ 0.85f * r, 0.34f * r },	{ 0.85f * r, 0.72f * r },	{ 0.18f * r, 0.72f * r },
		{ 0.18f * r, 0.45f * r },	{ -0.22f * r, 0.45f * r },	{ -0.22f * r, 0.68f * r },
		{ -0.82f * r, 0.68f * r },	{ -0.82f * r, 0.18f * r },	{ -0.52f * r, 0.18f * r },
		{ -0.52f * r, -0.16f * r }, { -0.90f * r, -0.16f * r },
	};

	// Test occluder body that stays black in the mask.
	polygon.occluder_interiors.emplace_back(impl::ShadowMaskInterior{
		.vertices = {
			{ -0.18f * r, -0.12f * r },
			{  0.12f * r, -0.12f * r },
			{  0.12f * r,  0.16f * r },
			{ -0.18f * r,  0.16f * r },
		},
		.masks_light_inside = true,
	});

	// Test occluder body that gets painted back into the mask.
	polygon.occluder_interiors.emplace_back(impl::ShadowMaskInterior{
		.vertices = {
			{  0.38f * r, -0.46f * r },
			{  0.58f * r, -0.46f * r },
			{  0.58f * r, -0.24f * r },
			{  0.38f * r, -0.24f * r },
		},
		.masks_light_inside = false,
	});

	return polygon;
}

class ShadowScene : public Scene {
public:
	Light light1;
	Light light2;
	Light light3;

	int mouse_light{ 0 };

	void OnEnter() override {
		constexpr LightProperties properties{
			.radius = 400.0f, .color = color::Cyan, .intensity = 0.5f, .falloff = 2.0f
		};
		constexpr V2_int light_starting_pos{ 130, 0 };

		ctx().asset.Load("sprite", "assets/jpg.jpg");

		auto properties1{ properties };
		properties1.color = color::Red;
		light1			  = CreateLight(*this, light_starting_pos, properties1);

		light1.Add<impl::VisibilityPolygon>(CreateSampleVisibilityPolygon(500.0f));
		// SetPosition(light1, light_starting_pos);

		// CreateSprite(*this, "sprite", { 200, -250 });
		// auto sprite2{ CreateSprite(*this, "sprite", { -200, -250 }) };
		//// SetOccluder(sprite2);

		// CreateRect(*this, { -300, 300 }, { 100, 100 }, color::LightGray);
		// auto rect2{ CreateRect(*this, { 0, 300 }, { 100, 100 }, color::Gray) };
		//// SetOccluder(rect2);

		// auto properties2{ properties };
		// properties2.color = color::Green;
		// light2			  = CreateLight(*this, light_starting_pos, properties2);

		// auto rect3{ CreateRect(*this, { 300, 300 }, { 100, 100 }, color::DarkGray) };
		//// SetOccluder(rect3);

		// auto properties3{ properties };
		// properties3.color = color::Blue;
		// light3			  = CreateLight(*this, light_starting_pos, properties3);
	}

	void OnUpdate() override {
		if (ctx().input.MousePressed(Mouse::Left)) {
			mouse_light++;
			mouse_light = Mod(mouse_light, 3);
		}
		SetPosition(light1, ctx().input.GetMousePosition());
		// switch (mouse_light) {
		//	case 0:	 SetPosition(light1, ctx().input.GetMousePosition()); break;
		//	case 1:	 SetPosition(light2, ctx().input.GetMousePosition()); break;
		//	case 2:	 SetPosition(light3, ctx().input.GetMousePosition()); break;
		//	default: PTGN_ERROR("Mouse light index out of range");
		// }
	}
};

int main(int, char**) {
	Application app{ "ShadowScene: Left click to switch mouse light" };
	app.StartWith<ShadowScene>();
}