
#include "components/input.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "math/vector2.h"
#include "rendering/graphics/circle.h"
#include "rendering/graphics/rect.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

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
		entity.GetPosition() = mouse + entity.Get<Draggable>().offset;
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
		entity.GetPosition() = mouse + entity.Get<Draggable>().offset;
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

		auto c0 = CreateCircle(*this, center + V2_float{ 200, -200 }, 90.0f, color::Green);
		c0.SetInteractive();
		c0.AddScript<ScriptC0>();

		auto c1 = CreateCircle(*this, center + V2_float{ 200, 200 }, 45.0f, color::LightGreen);
		c1.Add<InteractiveCircles>(90.0f);
		c1.SetInteractive();
		c1.AddScript<ScriptC1>();

		auto r0 =
			CreateRect(*this, center + V2_float{ -200, -200 }, V2_float{ 200, 100 }, color::Blue);
		r0.SetInteractive();
		r0.AddScript<ScriptR0>();

		V2_float r1size{ 100, 50 };
		auto r1 = CreateRect(*this, center + V2_float{ -200, 200 }, r1size, color::LightBlue);
		r1.SetInteractive();
		r1.Add<InteractiveRects>(r1size * 2.0f);
		r1.AddScript<ScriptR1>();

		game.texture.Load("box", "resources/box.png");

		auto r2 = CreateSprite(*this, "box");
		r2.SetPosition(center + V2_float{ -200, 0 });
		r2.SetInteractive();
		r2.AddScript<ScriptR2>();

		game.texture.Load("drag", "resources/drag.png");
		game.texture.Load("drag_circle", "resources/drag_circle.png");

		auto r3 = CreateSprite(*this, "drag");
		r3.SetPosition(center + V2_float{ 200, 0 });
		r3.SetInteractive();
		r3.Add<Draggable>();
		r3.AddScript<ScriptR3>();

		auto c3 = CreateSprite(*this, "drag_circle");
		c3.SetPosition(center + V2_float{ 0, 0 });
		c3.Add<InteractiveCircles>(game.texture.GetSize("drag_circle").x * 0.5f);
		c3.SetInteractive();
		c3.Add<Draggable>();
		c3.AddScript<ScriptC3>();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("InteractiveScene");
	game.scene.Enter<InteractiveScene>("");
	return 0;
}