
#include "components/draw.h"
#include "components/interactive.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/script.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

struct ScriptC0 : public Script<ScriptC0, KeyScript, MouseScript> {
	/*
	void OnKeyDown(Key key) override {
		PTGN_LOG("c0 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("c0 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("c0 Key up");
	}

	void OnMouseDownOver(Mouse mouse) override {
		PTGN_LOG("c0 Mouse down: ", mouse);
	}

	void OnMouseDownOut(Mouse mouse) override {
		PTGN_LOG("c0 Mouse down outside: ", mouse);
	}

	void OnMouseLeave() override {
		PTGN_LOG("c0 Mouse leave");
	}

	void OnMouseEnter() override {
		PTGN_LOG("c0 Mouse enter");
	}

	void OnMouseMoveOut() override { }

	void OnMouseMoveOver() override {
		PTGN_LOG("c0 Mouse over");
	}

	void OnMousePressedOver(Mouse mouse) override {
		PTGN_LOG("c0 Mouse pressed: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse) override {
		PTGN_LOG("c0 Mouse scroll: ", mouse);
	}

	void OnMouseUpOver(Mouse mouse) override {
		PTGN_LOG("c0 Mouse up: ", mouse);
	}

	void OnMouseUpOut(Mouse mouse) override {
		PTGN_LOG("c0 Mouse up outside: ", mouse);
	}
	*/
};

struct ScriptC1 : public Script<ScriptC1, KeyScript, MouseScript> {
	/*
	void OnKeyDown(Key key) override {
		PTGN_LOG("c1 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("c1 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("c1 Key up");
	}

	void OnMouseDownOver(Mouse mouse) override {
		PTGN_LOG("c1 Mouse down: ", mouse);
	}

	void OnMouseDownOut(Mouse mouse) override {
		PTGN_LOG("c1 Mouse down outside: ", mouse);
	}

	void OnMouseLeave() override {
		PTGN_LOG("c1 Mouse leave");
	}

	void OnMouseEnter() override {
		PTGN_LOG("c1 Mouse enter");
	}

	void OnMouseMoveOut() override { }

	void OnMouseMoveOver() override {
		PTGN_LOG("c1 Mouse over");
	}

	void OnMousePressedOver(Mouse mouse) override {
		PTGN_LOG("c1 Mouse pressed: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse) override {
		PTGN_LOG("c1 Mouse scroll: ", mouse);
	}

	void OnMouseUpOver(Mouse mouse) override {
		PTGN_LOG("c1 Mouse up: ", mouse);
	}

	void OnMouseUpOut(Mouse mouse) override {
		PTGN_LOG("c1 Mouse up outside: ", mouse);
	}
	*/
};

struct ScriptR0 : public Script<ScriptR0, KeyScript, MouseScript> {
	/*
	void OnKeyDown(Key key) override {
		PTGN_LOG("r0 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("r0 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("r0 Key up");
	}

	void OnMouseDownOver(Mouse mouse) override {
		PTGN_LOG("r0 Mouse down: ", mouse);
	}

	void OnMouseDownOut(Mouse mouse) override {
		PTGN_LOG("r0 Mouse down outside: ", mouse);
	}

	void OnMouseLeave() override {
		PTGN_LOG("r0 Mouse leave");
	}

	void OnMouseEnter() override {
		PTGN_LOG("r0 Mouse enter");
	}

	void OnMouseMoveOut() override {  }

	void OnMouseMoveOver() override {
		PTGN_LOG("r0 Mouse over");
	}

	void OnMousePressedOver(Mouse mouse) override {
		PTGN_LOG("r0 Mouse pressed: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse) override {
		PTGN_LOG("r0 Mouse scroll: ", mouse);
	}

	void OnMouseUpOver(Mouse mouse) override {
		PTGN_LOG("r0 Mouse up: ", mouse);
	}

	void OnMouseUpOut(Mouse mouse) override {
		PTGN_LOG("r0 Mouse up outside: ", mouse);
	}
	*/
};

struct ScriptR1 : public Script<ScriptR1, KeyScript, MouseScript> {
	/*
	void OnKeyDown(Key key) override {
		PTGN_LOG("r1 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("r1 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("r1 Key up");
	}

	void OnMouseDownOver(Mouse mouse) override {
		PTGN_LOG("r1 Mouse down: ", mouse);
	}

	void OnMouseDownOut(Mouse mouse) override {
		PTGN_LOG("r1 Mouse down outside: ", mouse);
	}

	void OnMouseLeave() override {
		PTGN_LOG("r1 Mouse leave");
	}

	void OnMouseEnter() override {
		PTGN_LOG("r1 Mouse enter");
	}

	void OnMouseMoveOut() override { }

	void OnMouseMoveOver() override {
		PTGN_LOG("r1 Mouse over");
	}

	void OnMousePressedOver(Mouse mouse) override {
		PTGN_LOG("r1 Mouse pressed: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse) override {
		PTGN_LOG("r1 Mouse scroll: ", mouse);
	}

	void OnMouseUpOver(Mouse mouse) override {
		PTGN_LOG("r1 Mouse up: ", mouse);
	}

	void OnMouseUpOut(Mouse mouse) override {
		PTGN_LOG("r1 Mouse up outside: ", mouse);
	}

	*/
};

struct ScriptR2 : public Script<ScriptR2, KeyScript, MouseScript> {
	/*
	void OnKeyDown(Key key) override {
		PTGN_LOG("r2 Key down");
	}

	void OnKeyPressed(Key key) override {
		PTGN_LOG("r2 Key pressed");
	}

	void OnKeyUp(Key key) override {
		PTGN_LOG("r2 Key up");
	}

	void OnMouseDownOver(Mouse mouse) override {
		PTGN_LOG("r2 Mouse down: ", mouse);
	}

	void OnMouseDownOut(Mouse mouse) override {
		PTGN_LOG("r2 Mouse down outside: ", mouse);
	}

	void OnMouseLeave() override {
		PTGN_LOG("r2 Mouse leave");
	}

	void OnMouseEnter() override {
		PTGN_LOG("r2 Mouse enter");
	}

	void OnMouseMoveOut() override { }

	void OnMouseMoveOver() override {
		PTGN_LOG("r2 Mouse over");
	}

	void OnMousePressedOver(Mouse mouse) override {
		PTGN_LOG("r2 Mouse pressed: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse) override {
		PTGN_LOG("r2 Mouse scroll: ", mouse);
	}

	void OnMouseUpOver(Mouse mouse) override {
		PTGN_LOG("r2 Mouse up: ", mouse);
	}

	void OnMouseUpOut(Mouse mouse) override {
		PTGN_LOG("r2 Mouse up outside: ", mouse);
	}

	*/
};

struct ScriptR3 : public Script<ScriptR3, KeyScript, MouseScript, DragScript> {
	void OnDrag() override {
		// PTGN_LOG("r3 Drag: ", mouse);
		V2_float mouse{ game.input.GetMousePosition() };
		GetPosition(entity) = mouse + entity.Get<Draggable>().offset;
	}

	/*
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

	void OnDragStart(V2_int start_position) override {
		PTGN_LOG("r3 Drag start: ", start_position);
	}

	void OnDragStop(V2_int stop_position) override {
		PTGN_LOG("r3 Drag stop: ", stop_position);
	}

	void OnDrop(Entity dropzone) override {
		PTGN_LOG("r3 dropped onto: ", dropzone.GetId());
	}

	void OnPickup(Entity dropzone) override {
		PTGN_LOG("r3 picked up from: ", dropzone.GetId());
	}
	*/
};

struct ScriptC3 : public Script<ScriptC3, KeyScript, MouseScript, DragScript> {
	void OnDrag() override {
		V2_float mouse{ game.input.GetMousePosition() };
		// PTGN_LOG("c3 Drag: mouse: ", mouse, ", offset: ", entity.Get<Draggable>().offset);
		GetPosition(entity) = mouse + entity.Get<Draggable>().offset;
	}

	void OnMouseDownOver(Mouse mouse) override {
		PTGN_LOG("c3 Mouse down: ", mouse);
	}

	void OnMouseDownOut(Mouse mouse) override {
		PTGN_LOG("c3 Mouse down outside: ", mouse);
	}

	void OnMousePressedOver(Mouse mouse) override {
		// PTGN_LOG("c3 Mouse pressed: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse) override {
		PTGN_LOG("c3 Mouse scroll: ", mouse);
	}

	void OnMouseUpOver(Mouse mouse) override {
		PTGN_LOG("c3 Mouse up: ", mouse);
	}

	void OnMouseUpOut(Mouse mouse) override {
		PTGN_LOG("c3 Mouse up outside: ", mouse);
	}

	void OnDragEnter(Entity dropzone) override {
		PTGN_LOG("c3 Drag enter: ", dropzone.GetId());
	}

	void OnDragLeave(Entity dropzone) override {
		PTGN_LOG("c3 Drag leave: ", dropzone.GetId());
	}

	void OnDragOut(Entity dropzone) override {
		// PTGN_LOG("c3 Drag out: ", dropzone.GetId());
	}

	void OnDragOver(Entity dropzone) override {
		// PTGN_LOG("c3 Drag over: ", dropzone.GetId());
	}

	void OnDragStart(V2_int start_position) override {
		PTGN_LOG("c3 Drag start: ", start_position);
	}

	void OnDragStop(V2_int stop_position) override {
		PTGN_LOG("c3 Drag stop: ", stop_position);
	}

	void OnDrop(Entity dropzone) override {
		PTGN_LOG("c3 dropped onto: ", dropzone.GetId());
	}

	void OnPickup(Entity dropzone) override {
		PTGN_LOG("c3 picked up from: ", dropzone.GetId());
	}
};

struct InteractiveScene : public Scene {
	Sprite r4;

	void Enter() override {
		V2_float ws{ game.window.GetSize() };
		V2_float center{ game.window.GetCenter() };

		V2_float offset{ 250, 250 };
		V2_float rsize{ 100, 50 };

		auto c0 = CreateCircle(
			*this, center + V2_float{ offset.x, -offset.y }, 90.0f, color::Green, 1.0f
		);
		auto c0_child = CreateCircle(*this, {}, 90.0f, color::Magenta, 1.0f);
		AddInteractable(c0, c0_child);
		AddScript<ScriptC0>(c0);

		auto c1 = CreateCircle(
			*this, center + V2_float{ offset.x, offset.y }, 90.0f, color::LightGreen, 1.0f
		);
		auto c1_child = CreateCircle(*this, {}, 45.0f, color::Magenta, 1.0f);
		AddInteractable(c1, c1_child);
		AddScript<ScriptC1>(c1);

		auto r0 = CreateRect(
			*this, center + V2_float{ -offset.x, -offset.y }, rsize * 2, color::Blue, 1.0f
		);
		auto r0_child = CreateRect(*this, {}, rsize * 2, color::Magenta, 1.0f);
		AddInteractable(r0, r0_child);
		AddScript<ScriptR0>(r0);

		auto r1 = CreateRect(
			*this, center + V2_float{ -offset.x, offset.y }, rsize, color::LightBlue, 1.0f
		);
		auto r1_child = CreateRect(*this, {}, rsize * 2, color::Magenta, 1.0f);
		AddInteractable(r1, r1_child);
		AddScript<ScriptR1>(r1);

		game.texture.Load("box", "resources/box.png");

		auto r2		  = CreateSprite(*this, "box", center + V2_float{ -offset.x, 0.0f });
		auto r2_child = CreateRect(*this, {}, r2.GetDisplaySize(), color::Magenta, 1.0f);
		AddInteractable(r2, r2_child);
		AddScript<ScriptR2>(r2);

		LoadResources({ { "drag", "resources/drag.png" },
						{ "drag_circle", "resources/drag_circle.png" },
						{ "dropzone", "resources/dropzone.png" } });

		r4			  = CreateSprite(*this, "dropzone", center + V2_float{ 0.0f, -offset.y });
		auto r4_child = CreateRect(*this, {}, rsize * 2, color::Magenta, 1.0f);
		AddInteractable(r4, r4_child);
		r4.Add<Dropzone>();

		PTGN_LOG("Dropzone id: ", r4.GetId());

		auto r3		  = CreateSprite(*this, "drag", center + V2_float{ offset.x, 0.0f });
		auto r3_child = CreateRect(*this, {}, r3.GetDisplaySize(), color::Magenta, 1.0f);
		AddInteractable(r3, r3_child);
		r3.Add<Draggable>();
		AddScript<ScriptR3>(r3);

		PTGN_LOG("Rect drag id: ", r3.GetId());

		auto c3		  = CreateSprite(*this, "drag_circle", center + V2_float{ 0, 0 });
		auto c3_child = CreateCircle(*this, {}, c3.GetDisplaySize().x * 0.5f, color::Magenta, 1.0f);
		AddInteractable(c3, c3_child);
		c3.Add<Draggable>(); //.SetTrigger(CallbackTrigger::MouseOverlaps);
		AddScript<ScriptC3>(c3);

		PTGN_LOG("Circle drag id: ", c3.GetId());
	}

	void Update() override {
		if (game.input.KeyDown(Key::Q)) {
			input.SetTopOnly(false);
			PTGN_LOG("Setting top input only: false");
		} else if (game.input.KeyDown(Key::E)) {
			input.SetTopOnly(true);
			PTGN_LOG("Setting top input only: true");
		}
		/*const auto& dropped{ r4.Get<Dropzone>().dropped_entities };
		Print("Dropped: ");
		for (auto d : dropped) {
			Print(d.GetId(), ", ");
		}
		PrintLine();*/
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("InteractiveScene");
	game.scene.Enter<InteractiveScene>("");
	return 0;
}