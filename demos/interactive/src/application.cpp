
#include "components/draw.h"
#include "components/input.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/texture.h"
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
		// PTGN_LOG("r3 Drag: ", mouse);
		entity.GetPosition() = mouse + entity.Get<Draggable>().offset;
	}

	void OnDragEnter(Entity dropzone) override {
		PTGN_LOG("r3 Drag enter: ", dropzone.GetId());
	}

	void OnDragLeave(Entity dropzone) override {
		PTGN_LOG("r3 Drag leave: ", dropzone.GetId());
	}

	void OnDragOut(Entity dropzone) override {
		PTGN_LOG("r3 Drag out: ", dropzone.GetId());
	}

	void OnDragOver(Entity dropzone) override {
		PTGN_LOG("r3 Drag over: ", dropzone.GetId());
	}

	/*void OnDragStart(V2_float mouse) override {
		PTGN_LOG("r3 Drag start: ", mouse);
	}

	void OnDragStop(V2_float mouse) override {
		PTGN_LOG("r3 Drag stop: ", mouse);
	}*/

	void OnDrop(Entity dropzone) override {
		PTGN_LOG("r3 dropped onto: ", dropzone.GetId());
	}

	void OnPickup(Entity dropzone) override {
		PTGN_LOG("r3 picked up from: ", dropzone.GetId());
	}
};

struct ScriptC3 : public Script<ScriptC3> {
	void OnDrag(V2_float mouse) override {
		// PTGN_LOG("c3 Drag: ", mouse);
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

	void OnDragEnter(Entity dropzone) override {
		PTGN_LOG("c3 Drag enter: ", dropzone.GetId());
	}

	void OnDragLeave(Entity dropzone) override {
		PTGN_LOG("c3 Drag leave: ", dropzone.GetId());
	}

	void OnDragOut(Entity dropzone) override {
		PTGN_LOG("c3 Drag out: ", dropzone.GetId());
	}

	void OnDragOver(Entity dropzone) override {
		PTGN_LOG("c3 Drag over: ", dropzone.GetId());
	}

	/*void OnDragStart(V2_float mouse) override {
		PTGN_LOG("c3 Drag start: ", mouse);
	}

	void OnDragStop(V2_float mouse) override {
		PTGN_LOG("c3 Drag stop: ", mouse);
	}*/

	void OnDrop(Entity dropzone) override {
		PTGN_LOG("c3 dropped onto: ", dropzone.GetId());
	}

	void OnPickup(Entity dropzone) override {
		PTGN_LOG("c3 picked up from: ", dropzone.GetId());
	}
};

struct InteractiveScene : public Scene {
	void Enter() override {
		V2_float ws{ game.window.GetSize() };
		V2_float center{ game.window.GetCenter() };

		V2_float offset{ 250, 250 };
		V2_float rsize{ 100, 50 };

		/*auto c0 = CreateCircle(
			*this, center + V2_float{ offset.x, -offset.y }, 90.0f, color::Green, 1.0f
		);
		auto c0_child = CreateCircle(*this, {}, 90.0f, color::Magenta, 1.0f);
		c0.AddInteractable(c0_child);
		c0.AddScript<ScriptC0>();

		auto c1 = CreateCircle(
			*this, center + V2_float{ offset.x, offset.y }, 90.0f, color::LightGreen, 1.0f
		);
		auto c1_child = CreateCircle(*this, {}, 45.0f, color::Magenta, 1.0f);
		c1.AddInteractable(c1_child);
		c1.AddScript<ScriptC1>();

		auto r0 = CreateRect(
			*this, center + V2_float{ -offset.x, -offset.y }, rsize * 2, color::Blue, 1.0f
		);
		auto r0_child = CreateRect(*this, {}, rsize * 2, color::Magenta, 1.0f);
		r0.AddInteractable(r0_child);
		r0.AddScript<ScriptR0>();

		auto r1 = CreateRect(
			*this, center + V2_float{ -offset.x, offset.y }, rsize, color::LightBlue, 1.0f
		);
		auto r1_child = CreateRect(*this, {}, rsize * 2, color::Magenta, 1.0f);
		r1.AddInteractable(r1_child);
		r1.AddScript<ScriptR1>();

		game.texture.Load("box", "resources/box.png");

		auto r2 = CreateSprite(*this, "box");
		r2.SetPosition(center + V2_float{ -offset.x, 0.0f });
		auto r2_child = CreateRect(*this, {}, r2.GetDisplaySize(), color::Magenta, 1.0f);
		r2.AddInteractable(r2_child);
		r2.AddScript<ScriptR2>();*/

		game.texture.Load("drag", "resources/drag.png");
		game.texture.Load("drag_circle", "resources/drag_circle.png");
		game.texture.Load("dropzone", "resources/dropzone.png");

		auto r4 = CreateSprite(*this, "dropzone");
		r4.SetPosition(center + V2_float{ 0.0f, -offset.y });
		auto r4_child = CreateRect(*this, {}, rsize * 2, color::Magenta, 1.0f);
		r4.AddInteractable(r4_child);
		r4.Add<Dropzone>().trigger = DropTrigger::MouseOverlaps;

		PTGN_LOG("Dropzone id: ", r4.GetId());

		auto r3 = CreateSprite(*this, "drag");
		r3.SetPosition(center + V2_float{ offset.x, 0.0f });
		auto r3_child = CreateRect(*this, {}, r3.GetDisplaySize(), color::Magenta, 1.0f);
		r3.AddInteractable(r3_child);
		r3.Add<Draggable>();
		r3.AddScript<ScriptR3>();

		PTGN_LOG("Rect drag id: ", r3.GetId());

		auto c3 = CreateSprite(*this, "drag_circle");
		c3.SetPosition(center + V2_float{ 0, 0 });
		auto c3_child = CreateCircle(*this, {}, c3.GetDisplaySize().x * 0.5f, color::Magenta, 1.0f);
		c3.AddInteractable(c3_child);
		c3.Add<Draggable>();
		c3.AddScript<ScriptC3>();

		PTGN_LOG("Circle drag id: ", c3.GetId());
	}

	void Update() override {
		if (game.input.KeyDown(Key::Q)) {
			input.SetTopOnly(false);
		} else if (game.input.KeyDown(Key::E)) {
			input.SetTopOnly(true);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("InteractiveScene");
	game.scene.Enter<InteractiveScene>("");
	return 0;
}