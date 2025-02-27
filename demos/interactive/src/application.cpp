#include <string_view>
#include <type_traits>

#include "components/draw.h"
#include "components/input.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "vfx/light.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };

struct InteractiveComponentScene : public Scene {
	void Enter() override {
		V2_float ws{ game.window.GetSize() };
		V2_float center{ game.window.GetCenter() };

		auto c0 = manager.CreateEntity();
		c0.Add<Circle>(90.0f);
		c0.Add<Transform>(center + V2_float{ 200, -200 });
		c0.Add<Tint>(color::Green);
		c0.Add<Visible>();
		c0.Add<Interactive>();
		c0.Add<callback::KeyDown>([](auto key) { PTGN_LOG("c0 Key down"); });
		c0.Add<callback::KeyPressed>([](auto key) { PTGN_LOG("c0 Key pressed"); });
		c0.Add<callback::KeyUp>([](auto key) { PTGN_LOG("c0 Key up"); });
		c0.Add<callback::MouseDown>([](auto mouse) { PTGN_LOG("c0 Mouse down: ", mouse); });
		c0.Add<callback::MouseDownOutside>([](auto mouse) {
			PTGN_LOG("c0 Mouse down outside: ", mouse);
		});
		c0.Add<callback::MouseMove>([](auto mouse) { /*PTGN_LOG("c0 Mouse move: ", mouse);*/ });
		c0.Add<callback::MouseLeave>([](auto mouse) { PTGN_LOG("c0 Mouse leave: ", mouse); });
		c0.Add<callback::MouseEnter>([](auto mouse) { PTGN_LOG("c0 Mouse enter: ", mouse); });
		c0.Add<callback::MouseOut>([](auto mouse) { /*PTGN_LOG("c0 Mouse out: ", mouse);*/ });
		c0.Add<callback::MouseOver>([](auto mouse) { PTGN_LOG("c0 Mouse over: ", mouse); });
		c0.Add<callback::MousePressed>([](auto mouse) { PTGN_LOG("c0 Mouse pressed: ", mouse); });
		c0.Add<callback::MouseScroll>([](auto mouse) { PTGN_LOG("c0 Mouse scroll: ", mouse); });
		c0.Add<callback::MouseUp>([](auto mouse) { PTGN_LOG("c0 Mouse up: ", mouse); });
		c0.Add<callback::MouseUpOutside>([](auto mouse) {
			PTGN_LOG("c0 Mouse up outside: ", mouse);
		});

		auto c1 = manager.CreateEntity();
		c1.Add<Circle>(45.0f);
		c1.Add<Transform>(center + V2_float{ 200, 200 });
		c1.Add<InteractiveRadius>(90.0f);
		c1.Add<Tint>(color::LightGreen);
		c1.Add<Visible>();
		c1.Add<Interactive>();
		c1.Add<callback::KeyDown>([](auto key) { PTGN_LOG("c1 Key down"); });
		c1.Add<callback::KeyPressed>([](auto key) { PTGN_LOG("c1 Key pressed"); });
		c1.Add<callback::KeyUp>([](auto key) { PTGN_LOG("c1 Key up"); });
		c1.Add<callback::MouseDown>([](auto mouse) { PTGN_LOG("c1 Mouse down: ", mouse); });
		c1.Add<callback::MouseDownOutside>([](auto mouse) {
			PTGN_LOG("c1 Mouse down outside: ", mouse);
		});
		c1.Add<callback::MouseMove>([](auto mouse) { /*PTGN_LOG("c1 Mouse move: ", mouse);*/ });
		c1.Add<callback::MouseLeave>([](auto mouse) { PTGN_LOG("c1 Mouse leave: ", mouse); });
		c1.Add<callback::MouseEnter>([](auto mouse) { PTGN_LOG("c1 Mouse enter: ", mouse); });
		c1.Add<callback::MouseOut>([](auto mouse) { /*PTGN_LOG("c1 Mouse out: ", mouse);*/ });
		c1.Add<callback::MouseOver>([](auto mouse) { PTGN_LOG("c1 Mouse over: ", mouse); });
		c1.Add<callback::MousePressed>([](auto mouse) { PTGN_LOG("c1 Mouse pressed: ", mouse); });
		c1.Add<callback::MouseScroll>([](auto mouse) { PTGN_LOG("c1 Mouse scroll: ", mouse); });
		c1.Add<callback::MouseUp>([](auto mouse) { PTGN_LOG("c1 Mouse up: ", mouse); });
		c1.Add<callback::MouseUpOutside>([](auto mouse) {
			PTGN_LOG("c1 Mouse up outside: ", mouse);
		});

		auto r0 = manager.CreateEntity();
		r0.Add<Rect>(V2_float{ 200, 100 }, Origin::Center);
		r0.Add<Transform>(center + V2_float{ -200, -200 });
		r0.Add<Tint>(color::Blue);
		r0.Add<Visible>();
		r0.Add<Interactive>();
		r0.Add<callback::KeyDown>([](auto key) { PTGN_LOG("r0 Key down"); });
		r0.Add<callback::KeyPressed>([](auto key) { PTGN_LOG("r0 Key pressed"); });
		r0.Add<callback::KeyUp>([](auto key) { PTGN_LOG("r0 Key up"); });
		r0.Add<callback::MouseDown>([](auto mouse) { PTGN_LOG("r0 Mouse down: ", mouse); });
		r0.Add<callback::MouseDownOutside>([](auto mouse) {
			PTGN_LOG("r0 Mouse down outside: ", mouse);
		});
		r0.Add<callback::MouseMove>([](auto mouse) { /*PTGN_LOG("r0 Mouse move: ", mouse);*/ });
		r0.Add<callback::MouseLeave>([](auto mouse) { PTGN_LOG("r0 Mouse leave: ", mouse); });
		r0.Add<callback::MouseEnter>([](auto mouse) { PTGN_LOG("r0 Mouse enter: ", mouse); });
		r0.Add<callback::MouseOut>([](auto mouse) { /*PTGN_LOG("r0 Mouse out: ", mouse);*/ });
		r0.Add<callback::MouseOver>([](auto mouse) { PTGN_LOG("r0 Mouse over: ", mouse); });
		r0.Add<callback::MousePressed>([](auto mouse) { PTGN_LOG("r0 Mouse pressed: ", mouse); });
		r0.Add<callback::MouseScroll>([](auto mouse) { PTGN_LOG("r0 Mouse scroll: ", mouse); });
		r0.Add<callback::MouseUp>([](auto mouse) { PTGN_LOG("r0 Mouse up: ", mouse); });
		r0.Add<callback::MouseUpOutside>([](auto mouse) {
			PTGN_LOG("r0 Mouse up outside: ", mouse);
		});

		auto r1 = manager.CreateEntity();
		V2_float r1size{ r1.Add<Rect>(V2_float{ 100, 50 }, Origin::Center).size };
		r1.Add<Transform>(center + V2_float{ -200, 200 });
		r1.Add<InteractiveSize>(r1size * 2.0f);
		r1.Add<Tint>(color::LightBlue);
		r1.Add<Visible>();
		r1.Add<Interactive>();
		r1.Add<callback::KeyDown>([](auto key) { PTGN_LOG("r1 Key down"); });
		r1.Add<callback::KeyPressed>([](auto key) { PTGN_LOG("r1 Key pressed"); });
		r1.Add<callback::KeyUp>([](auto key) { PTGN_LOG("r1 Key up"); });
		r1.Add<callback::MouseDown>([](auto mouse) { PTGN_LOG("r1 Mouse down: ", mouse); });
		r1.Add<callback::MouseDownOutside>([](auto mouse) {
			PTGN_LOG("r1 Mouse down outside: ", mouse);
		});
		r1.Add<callback::MouseMove>([](auto mouse) { /*PTGN_LOG("r1 Mouse move: ", mouse);*/ });
		r1.Add<callback::MouseLeave>([](auto mouse) { PTGN_LOG("r1 Mouse leave: ", mouse); });
		r1.Add<callback::MouseEnter>([](auto mouse) { PTGN_LOG("r1 Mouse enter: ", mouse); });
		r1.Add<callback::MouseOut>([](auto mouse) { /*PTGN_LOG("r1 Mouse out: ", mouse);*/ });
		r1.Add<callback::MouseOver>([](auto mouse) { PTGN_LOG("r1 Mouse over: ", mouse); });
		r1.Add<callback::MousePressed>([](auto mouse) { PTGN_LOG("r1 Mouse pressed: ", mouse); });
		r1.Add<callback::MouseScroll>([](auto mouse) { PTGN_LOG("r1 Mouse scroll: ", mouse); });
		r1.Add<callback::MouseUp>([](auto mouse) { PTGN_LOG("r1 Mouse up: ", mouse); });
		r1.Add<callback::MouseUpOutside>([](auto mouse) {
			PTGN_LOG("r1 Mouse up outside: ", mouse);
		});

		game.texture.Load("box", "resources/box.png");

		auto r2 = CreateSprite(manager, "box");
		r2.Add<Transform>(center + V2_float{ -200, 0 });
		r2.Add<Origin>(Origin::Center);
		r2.Add<Interactive>();
		r2.Add<callback::KeyDown>([](auto key) { PTGN_LOG("r2 Key down"); });
		r2.Add<callback::KeyPressed>([](auto key) { PTGN_LOG("r2 Key pressed"); });
		r2.Add<callback::KeyUp>([](auto key) { PTGN_LOG("r2 Key up"); });
		r2.Add<callback::MouseDown>([](auto mouse) { PTGN_LOG("r2 Mouse down: ", mouse); });
		r2.Add<callback::MouseDownOutside>([](auto mouse) {
			PTGN_LOG("r2 Mouse down outside: ", mouse);
		});
		r2.Add<callback::MouseMove>([](auto mouse) { /*PTGN_LOG("r2 Mouse move: ", mouse);*/ });
		r2.Add<callback::MouseLeave>([](auto mouse) { PTGN_LOG("r2 Mouse leave: ", mouse); });
		r2.Add<callback::MouseEnter>([](auto mouse) { PTGN_LOG("r2 Mouse enter: ", mouse); });
		r2.Add<callback::MouseOut>([](auto mouse) { /*PTGN_LOG("r2 Mouse out: ", mouse);*/ });
		r2.Add<callback::MouseOver>([](auto mouse) { PTGN_LOG("r2 Mouse over: ", mouse); });
		r2.Add<callback::MousePressed>([](auto mouse) { PTGN_LOG("r2 Mouse pressed: ", mouse); });
		r2.Add<callback::MouseScroll>([](auto mouse) { PTGN_LOG("r2 Mouse scroll: ", mouse); });
		r2.Add<callback::MouseUp>([](auto mouse) { PTGN_LOG("r2 Mouse up: ", mouse); });
		r2.Add<callback::MouseUpOutside>([](auto mouse) {
			PTGN_LOG("r2 Mouse up outside: ", mouse);
		});

		game.texture.Load("drag", "resources/drag.png");
		game.texture.Load("drag_circle", "resources/drag_circle.png");

		auto r3 = CreateSprite(manager, "drag");
		r3.Add<Transform>(center + V2_float{ 200, 0 });
		r3.Add<Origin>(Origin::Center);
		r3.Add<Interactive>();
		r3.Add<Draggable>();
		r3.Add<callback::Drag>([=](auto mouse) mutable {
			PTGN_LOG("r3 Drag: ", mouse);
			r3.Get<Transform>().position = mouse + r3.Get<Draggable>().offset;
		});
		r3.Add<callback::DragEnter>([](auto mouse) { PTGN_LOG("r3 Drag enter: ", mouse); });
		r3.Add<callback::DragLeave>([](auto mouse) { PTGN_LOG("r3 Drag leave: ", mouse); });
		r3.Add<callback::DragOut>([](auto mouse) { PTGN_LOG("r3 Drag out: ", mouse); });
		r3.Add<callback::DragOver>([](auto mouse) { PTGN_LOG("r3 Drag over: ", mouse); });
		r3.Add<callback::DragStart>([](auto mouse) { PTGN_LOG("r3 Drag start: ", mouse); });
		r3.Add<callback::DragStop>([](auto mouse) { PTGN_LOG("r3 Drag stop: ", mouse); });

		auto c3 = CreateSprite(manager, "drag_circle");
		c3.Add<Transform>(center + V2_float{ 0, 0 });
		c3.Add<Origin>(Origin::Center);
		c3.Add<InteractiveRadius>(game.texture.GetSize("drag_circle").x * 0.5f);
		c3.Add<Interactive>();
		c3.Add<Draggable>();
		c3.Add<callback::Drag>([=](auto mouse) mutable {
			PTGN_LOG("c3 Drag: ", mouse);
			c3.Get<Transform>().position = mouse + c3.Get<Draggable>().offset;
		});
		c3.Add<callback::DragEnter>([](auto mouse) { PTGN_LOG("c3 Drag enter: ", mouse); });
		c3.Add<callback::DragLeave>([](auto mouse) { PTGN_LOG("c3 Drag leave: ", mouse); });
		c3.Add<callback::DragOut>([](auto mouse) { PTGN_LOG("c3 Drag out: ", mouse); });
		c3.Add<callback::DragOver>([](auto mouse) { PTGN_LOG("c3 Drag over: ", mouse); });
		c3.Add<callback::DragStart>([](auto mouse) { PTGN_LOG("c3 Drag start: ", mouse); });
		c3.Add<callback::DragStop>([](auto mouse) { PTGN_LOG("c3 Drag stop: ", mouse); });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Interactive Component", window_size, color::Transparent);
	game.scene.Enter<InteractiveComponentScene>("interactive_component_scene");
	return 0;
}