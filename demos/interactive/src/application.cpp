#include <string_view>
#include <type_traits>

#include "components/draw.h"
#include "components/drawable.h"
#include "components/input.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "rendering/batching/render_data.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

// TODO: Move these into the engine.

struct Circle : public Drawable<Circle> {
	Circle() = default;

	explicit Circle(float circle_radius) : radius{ circle_radius } {}

	float radius{ 0.0f };

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		PTGN_ASSERT(entity.Has<Circle>());

		auto transform{ entity.GetAbsoluteTransform() };
		auto depth{ entity.GetDepth() };
		auto blend_mode{ entity.GetBlendMode() };
		auto tint{ entity.GetTint().Normalized() };
		auto origin{ entity.GetOrigin() };

		auto circle{ entity.Get<Circle>() };
		auto line_width{ entity.GetOrDefault<LineWidth>() };

		auto offset_transform{ GetOffset(entity) };
		transform = transform.RelativeTo(offset_transform);

		auto camera{ entity.GetOrDefault<Camera>() };

		auto scaled_radius{ circle.radius * Abs(transform.scale) };

		ctx.AddEllipse(
			transform.position, V2_float{ scaled_radius }, line_width, depth, camera, blend_mode,
			tint, transform.rotation, false
		);
	}
};

struct Rect : public Drawable<Rect> {
	Rect() = default;

	explicit Rect(const V2_float& rect_size) : size{ rect_size } {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		PTGN_ASSERT(entity.Has<Rect>());

		auto transform{ entity.GetAbsoluteTransform() };
		auto depth{ entity.GetDepth() };
		auto blend_mode{ entity.GetBlendMode() };
		auto tint{ entity.GetTint().Normalized() };
		auto origin{ entity.GetOrigin() };

		auto rect{ entity.Get<Rect>() };
		auto line_width{ entity.GetOrDefault<LineWidth>() };

		auto offset_transform{ GetOffset(entity) };
		transform = transform.RelativeTo(offset_transform);

		auto camera{ entity.GetOrDefault<Camera>() };

		auto scaled_size{ rect.size * Abs(transform.scale) };

		ctx.AddQuad(
			transform.position, scaled_size, origin, line_width, depth, camera, blend_mode, tint,
			transform.rotation, false
		);
	}

	V2_float size;
};

struct ScriptC0 : public Script<ScriptC0> {
	void OnKeyDown(Key key) override {
		PTGN_LOG("c0 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("c0 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("c0 Key up");
	}

	void OnMouseDown(Mouse mouse) override {
		PTGN_LOG("c0 Mouse down: ", mouse);
	}

	void OnMouseDownOutside(Mouse mouse) override {
		PTGN_LOG("c0 Mouse down outside: ", mouse);
	}

	void OnMouseMove(V2_float mouse) override { /*PTGN_LOG("c0 Mouse move: ", mouse);*/ }

	void OnMouseLeave(V2_float mouse) override {
		PTGN_LOG("c0 Mouse leave: ", mouse);
	}

	void OnMouseEnter(V2_float mouse) override {
		PTGN_LOG("c0 Mouse enter: ", mouse);
	}

	void OnMouseOut(V2_float mouse) override { /*PTGN_LOG("c0 Mouse out: ", mouse);*/ }

	void OnMouseOver(V2_float mouse) override {
		PTGN_LOG("c0 Mouse over: ", mouse);
	}

	void OnMousePressed(Mouse mouse) override {
		PTGN_LOG("c0 Mouse pressed: ", mouse);
	}

	void OnMouseScroll(V2_int mouse) override {
		PTGN_LOG("c0 Mouse scroll: ", mouse);
	}

	void OnMouseUp(Mouse mouse) override {
		PTGN_LOG("c0 Mouse up: ", mouse);
	}

	void OnMouseUpOutside(Mouse mouse) override {
		PTGN_LOG("c0 Mouse up outside: ", mouse);
	}
};

struct ScriptC1 : public Script<ScriptC1> {
	void OnKeyDown(Key key) override {
		PTGN_LOG("c1 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("c1 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("c1 Key up");
	}

	void OnMouseDown(Mouse mouse) override {
		PTGN_LOG("c1 Mouse down: ", mouse);
	}

	void OnMouseDownOutside(Mouse mouse) override {
		PTGN_LOG("c1 Mouse down outside: ", mouse);
	}

	void OnMouseMove(V2_float mouse) override { /*PTGN_LOG("c1 Mouse move: ", mouse);*/ }

	void OnMouseLeave(V2_float mouse) override {
		PTGN_LOG("c1 Mouse leave: ", mouse);
	}

	void OnMouseEnter(V2_float mouse) override {
		PTGN_LOG("c1 Mouse enter: ", mouse);
	}

	void OnMouseOut(V2_float mouse) override { /*PTGN_LOG("c1 Mouse out: ", mouse);*/ }

	void OnMouseOver(V2_float mouse) override {
		PTGN_LOG("c1 Mouse over: ", mouse);
	}

	void OnMousePressed(Mouse mouse) override {
		PTGN_LOG("c1 Mouse pressed: ", mouse);
	}

	void OnMouseScroll(V2_int mouse) override {
		PTGN_LOG("c1 Mouse scroll: ", mouse);
	}

	void OnMouseUp(Mouse mouse) override {
		PTGN_LOG("c1 Mouse up: ", mouse);
	}

	void OnMouseUpOutside(Mouse mouse) override {
		PTGN_LOG("c1 Mouse up outside: ", mouse);
	}
};

struct ScriptR0 : public Script<ScriptR0> {
	void OnKeyDown(Key key) override {
		PTGN_LOG("r0 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("r0 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("r0 Key up");
	}

	void OnMouseDown(Mouse mouse) override {
		PTGN_LOG("r0 Mouse down: ", mouse);
	}

	void OnMouseDownOutside(Mouse mouse) override {
		PTGN_LOG("r0 Mouse down outside: ", mouse);
	}

	void OnMouseMove(V2_float mouse) override { /*PTGN_LOG("r0 Mouse move: ", mouse);*/ }

	void OnMouseLeave(V2_float mouse) override {
		PTGN_LOG("r0 Mouse leave: ", mouse);
	}

	void OnMouseEnter(V2_float mouse) override {
		PTGN_LOG("r0 Mouse enter: ", mouse);
	}

	void OnMouseOut(V2_float mouse) override { /*PTGN_LOG("r0 Mouse out: ", mouse);*/ }

	void OnMouseOver(V2_float mouse) override {
		PTGN_LOG("r0 Mouse over: ", mouse);
	}

	void OnMousePressed(Mouse mouse) override {
		PTGN_LOG("r0 Mouse pressed: ", mouse);
	}

	void OnMouseScroll(V2_int mouse) override {
		PTGN_LOG("r0 Mouse scroll: ", mouse);
	}

	void OnMouseUp(Mouse mouse) override {
		PTGN_LOG("r0 Mouse up: ", mouse);
	}

	void OnMouseUpOutside(Mouse mouse) override {
		PTGN_LOG("r0 Mouse up outside: ", mouse);
	}
};

struct ScriptR1 : public Script<ScriptR1> {
	void OnKeyDown(Key key) override {
		PTGN_LOG("r1 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("r1 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("r1 Key up");
	}

	void OnMouseDown(Mouse mouse) override {
		PTGN_LOG("r1 Mouse down: ", mouse);
	}

	void OnMouseDownOutside(Mouse mouse) override {
		PTGN_LOG("r1 Mouse down outside: ", mouse);
	}

	void OnMouseMove(V2_float mouse) override { /*PTGN_LOG("r1 Mouse move: ", mouse);*/ }

	void OnMouseLeave(V2_float mouse) override {
		PTGN_LOG("r1 Mouse leave: ", mouse);
	}

	void OnMouseEnter(V2_float mouse) override {
		PTGN_LOG("r1 Mouse enter: ", mouse);
	}

	void OnMouseOut(V2_float mouse) override { /*PTGN_LOG("r1 Mouse out: ", mouse);*/ }

	void OnMouseOver(V2_float mouse) override {
		PTGN_LOG("r1 Mouse over: ", mouse);
	}

	void OnMousePressed(Mouse mouse) override {
		PTGN_LOG("r1 Mouse pressed: ", mouse);
	}

	void OnMouseScroll(V2_int mouse) override {
		PTGN_LOG("r1 Mouse scroll: ", mouse);
	}

	void OnMouseUp(Mouse mouse) override {
		PTGN_LOG("r1 Mouse up: ", mouse);
	}

	void OnMouseUpOutside(Mouse mouse) override {
		PTGN_LOG("r1 Mouse up outside: ", mouse);
	}
};

struct ScriptR2 : public Script<ScriptR2> {
	void OnKeyDown(Key key) override {
		PTGN_LOG("r2 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("r2 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("r2 Key up");
	}

	void OnMouseDown(Mouse mouse) override {
		PTGN_LOG("r2 Mouse down: ", mouse);
	}

	void OnMouseDownOutside(Mouse mouse) override {
		PTGN_LOG("r2 Mouse down outside: ", mouse);
	}

	void OnMouseMove(V2_float mouse) override { /*PTGN_LOG("r2 Mouse move: ", mouse);*/ }

	void OnMouseLeave(V2_float mouse) override {
		PTGN_LOG("r2 Mouse leave: ", mouse);
	}

	void OnMouseEnter(V2_float mouse) override {
		PTGN_LOG("r2 Mouse enter: ", mouse);
	}

	void OnMouseOut(V2_float mouse) override { /*PTGN_LOG("r2 Mouse out: ", mouse);*/ }

	void OnMouseOver(V2_float mouse) override {
		PTGN_LOG("r2 Mouse over: ", mouse);
	}

	void OnMousePressed(Mouse mouse) override {
		PTGN_LOG("r2 Mouse pressed: ", mouse);
	}

	void OnMouseScroll(V2_int mouse) override {
		PTGN_LOG("r2 Mouse scroll: ", mouse);
	}

	void OnMouseUp(Mouse mouse) override {
		PTGN_LOG("r2 Mouse up: ", mouse);
	}

	void OnMouseUpOutside(Mouse mouse) override {
		PTGN_LOG("r2 Mouse up outside: ", mouse);
	}
};

struct ScriptR3 : public Script<ScriptR3> {
	void OnDrag(V2_float mouse) override {
		PTGN_LOG("r3 Drag: ", mouse);
		entity.Get<Transform>().position = mouse + entity.Get<Draggable>().offset;
	}

	void OnDragEnter(V2_float mouse) override {
		PTGN_LOG("r3 Drag enter: ", mouse);
	}

	void OnDragLeave(V2_float mouse) override {
		PTGN_LOG("r3 Drag leave: ", mouse);
	}

	void OnDragOut(V2_float mouse) override {
		PTGN_LOG("r3 Drag out: ", mouse);
	}

	void OnDragOver(V2_float mouse) override {
		PTGN_LOG("r3 Drag over: ", mouse);
	}

	void OnDragStart(V2_float mouse) override {
		PTGN_LOG("r3 Drag start: ", mouse);
	}

	void OnDragStop(V2_float mouse) override {
		PTGN_LOG("r3 Drag stop: ", mouse);
	}
};

struct ScriptC3 : public Script<ScriptC3> {
	void OnDrag(V2_float mouse) override {
		PTGN_LOG("c3 Drag: ", mouse);
		entity.Get<Transform>().position = mouse + entity.Get<Draggable>().offset;
	}

	// void OnMouseDown(Mouse mouse) override {
	//	PTGN_LOG("c3 Mouse down: ", mouse);
	// }
	// void OnMouseDownOutside(Mouse mouse) override {
	//	PTGN_LOG("c3 Mouse down outside: ", mouse);
	// }
	// void OnMousePressed(Mouse mouse) override {
	//	PTGN_LOG("c3 Mouse pressed: ", mouse);
	// }
	// void OnMouseScroll(V2_int mouse) override {
	//	PTGN_LOG("c3 Mouse scroll: ", mouse);
	// }
	// void OnMouseUp(Mouse mouse) override {
	//	PTGN_LOG("c3 Mouse up: ", mouse);
	// }
	// void OnMouseUpOutside(Mouse mouse) override {
	//	PTGN_LOG("c3 Mouse up outside: ", mouse);
	// }

	void OnDragEnter(V2_float mouse) override {
		PTGN_LOG("c3 Drag enter: ", mouse);
	}

	void OnDragLeave(V2_float mouse) override {
		PTGN_LOG("c3 Drag leave: ", mouse);
	}

	void OnDragOut(V2_float mouse) override {
		PTGN_LOG("c3 Drag out: ", mouse);
	}

	void OnDragOver(V2_float mouse) override {
		PTGN_LOG("c3 Drag over: ", mouse);
	}

	void OnDragStart(V2_float mouse) override {
		PTGN_LOG("c3 Drag start: ", mouse);
	}

	void OnDragStop(V2_float mouse) override {
		PTGN_LOG("c3 Drag stop: ", mouse);
	}
};

struct InteractiveScene : public Scene {
	void Enter() override {
		V2_float ws{ game.window.GetSize() };
		V2_float center{ game.window.GetCenter() };

		auto c0 = manager.CreateEntity();
		c0.Add<Circle>(90.0f);
		c0.SetDraw<Circle>();
		c0.Add<Transform>(center + V2_float{ 200, -200 });
		c0.Add<Tint>(color::Green);
		c0.Add<Visible>();
		c0.Add<Interactive>();
		c0.Add<Enabled>();
		c0.AddScript<ScriptC0>();

		auto c1 = manager.CreateEntity();
		c1.Add<Circle>(45.0f);
		c1.SetDraw<Circle>();
		c1.Add<Transform>(center + V2_float{ 200, 200 });
		c1.Add<InteractiveCircles>(90.0f);
		c1.Add<Tint>(color::LightGreen);
		c1.Add<Visible>();
		c1.Add<Interactive>();
		c1.Add<Enabled>();
		c1.AddScript<ScriptC1>();

		auto r0 = manager.CreateEntity();
		r0.Add<Rect>(V2_float{ 200, 100 });
		r0.SetDraw<Rect>();
		r0.Add<Origin>(Origin::Center);
		r0.Add<Transform>(center + V2_float{ -200, -200 });
		r0.Add<Tint>(color::Blue);
		r0.Add<Visible>();
		r0.Add<Interactive>();
		r0.Add<Enabled>();
		r0.AddScript<ScriptR0>();

		auto r1 = manager.CreateEntity();
		V2_float r1size{ r1.Add<Rect>(V2_float{ 100, 50 }).size };
		r1.Add<Origin>(Origin::Center);
		r1.SetDraw<Rect>();
		r1.Add<Transform>(center + V2_float{ -200, 200 });
		r1.Add<InteractiveRects>(r1size * 2.0f);
		r1.Add<Tint>(color::LightBlue);
		r1.Add<Visible>();
		r1.Add<Interactive>();
		r1.Add<Enabled>();
		r1.AddScript<ScriptR1>();

		game.texture.Load("box", "resources/box.png");

		auto r2 = CreateSprite(manager, "box");
		r2.Add<Transform>(center + V2_float{ -200, 0 });
		r2.Add<Origin>(Origin::Center);
		r2.Add<Interactive>();
		r2.Add<Enabled>();
		r2.AddScript<ScriptR2>();

		game.texture.Load("drag", "resources/drag.png");
		game.texture.Load("drag_circle", "resources/drag_circle.png");

		auto r3 = CreateSprite(manager, "drag");
		r3.Add<Transform>(center + V2_float{ 200, 0 });
		r3.Add<Origin>(Origin::Center);
		r3.Add<Interactive>();
		r3.Add<Enabled>();
		r3.Add<Draggable>();
		r3.AddScript<ScriptR3>();

		auto c3 = CreateSprite(manager, "drag_circle");
		c3.Add<Transform>(center + V2_float{ 0, 0 });
		c3.Add<Origin>(Origin::Center);
		c3.Add<InteractiveCircles>(game.texture.GetSize("drag_circle").x * 0.5f);
		c3.Add<Interactive>();
		c3.Add<Enabled>();
		c3.Add<Draggable>();
		c3.AddScript<ScriptC3>();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("InteractiveScene");
	game.scene.Enter<InteractiveScene>("");
	return 0;
}