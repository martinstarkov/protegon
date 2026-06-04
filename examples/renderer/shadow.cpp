
#include <vector>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/input/mouse.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

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

		auto sprite2{ CreateSprite(*this, "sprite", { -200, -250 }) };
		SetOccluder(sprite2);

		auto properties1{ properties };
		properties1.color = color::Red;
		light1			  = CreateLight(*this, light_starting_pos, properties1);

		// SetPosition(light1, light_starting_pos);

		// CreateSprite(*this, "sprite", { 200, -250 });

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