
#include "core/ecs/components/draw.h"
#include "core/ecs/components/interactive.h"
#include "core/ecs/components/movement.h"
#include "core/ecs/components/sprite.h"
#include "core/ecs/components/transform.h"
#include "core/app/game.h"
#include "core/scripting/script.h"
#include "core/app/window.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

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
		SetPosition(
			entity, entity.GetScene().input.GetMousePosition() + entity.Get<Draggable>().GetOffset()
		);
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
		// PTGN_LOG("c3 Drag: mouse: ", mouse, ", offset: ", entity.Get<Draggable>().offset);
		SetPosition(
			entity, entity.GetScene().input.GetMousePosition() + entity.Get<Draggable>().GetOffset()
		);
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
	Entity CreateInteractiveCircle(float radius) {
		auto entity = CreateEntity(*this);
		entity.Add<Circle>(radius);
		return entity;
	}

	Entity CreateInteractiveRect(const V2_float& size) {
		auto entity = CreateEntity(*this);
		entity.Add<Rect>(size);
		return entity;
	}

	void Enter() override {
		game.window.SetResizable();

		input.SetDrawInteractives(true);
		input.SetDrawInteractivesLineWidth(3.0f);

		LoadResource({ { "drag", "resources/drag.png" },
					   { "drag_circle", "resources/drag_circle.png" },
					   { "dropzone", "resources/dropzone.png" } });

		V2_float center{ GetTransform(camera).GetPosition() };

		V2_float offset{ 250, 250 };
		V2_float rsize{ 100, 50 };

		auto c0 = CreateCircle(
			*this, center + V2_float{ offset.x, -offset.y }, 90.0f, color::Green, 1.0f
		);
		auto c0_child = CreateInteractiveCircle(90.0f);
		AddInteractable(c0, std::move(c0_child));
		AddScript<ScriptC0>(c0);

		auto c1 = CreateCircle(
			*this, center + V2_float{ offset.x, offset.y }, 90.0f, color::LightGreen, 1.0f
		);
		auto c1_child = CreateInteractiveCircle(45.0f);
		AddInteractable(c1, std::move(c1_child));
		AddScript<ScriptC1>(c1);

		auto r0 = CreateRect(
			*this, center + V2_float{ -offset.x, -offset.y }, rsize * 2, color::Blue, 1.0f
		);
		auto r0_child = CreateInteractiveRect(rsize * 2);
		AddInteractable(r0, std::move(r0_child));
		AddScript<ScriptR0>(r0);

		auto r1 = CreateRect(
			*this, center + V2_float{ -offset.x, offset.y }, rsize, color::LightBlue, 1.0f
		);
		auto r1_child = CreateInteractiveRect(rsize * 2);
		AddInteractable(r1, std::move(r1_child));
		AddScript<ScriptR1>(r1);

		game.texture.Load("box", "resources/box.png");

		auto r2		  = CreateSprite(*this, "box", center + V2_float{ -offset.x, 0.0f });
		auto r2_child = CreateInteractiveRect(r2.GetDisplaySize());
		AddInteractable(r2, std::move(r2_child));
		AddScript<ScriptR2>(r2);

		auto r4		  = CreateSprite(*this, "dropzone", center + V2_float{ 0.0f, -offset.y });
		auto r4_child = CreateInteractiveRect(rsize * 2);
		AddInteractable(r4, std::move(r4_child));
		r4.Add<Dropzone>();

		PTGN_LOG("Dropzone id: ", r4.GetId());

		auto r3		  = CreateSprite(*this, "drag", center + V2_float{ offset.x, 0.0f });
		auto r3_child = CreateInteractiveRect(r3.GetDisplaySize());
		AddInteractable(r3, std::move(r3_child));
		r3.Add<Draggable>();
		AddScript<ScriptR3>(r3);

		PTGN_LOG("Rect drag id: ", r3.GetId());

		auto c3		  = CreateSprite(*this, "drag_circle", center + V2_float{ 0, 0 });
		auto c3_child = CreateInteractiveCircle(c3.GetDisplaySize().x * 0.5f);
		AddInteractable(c3, std::move(c3_child));
		c3.Add<Draggable>(); //.SetTrigger(CallbackTrigger::MouseOverlaps);
		AddScript<ScriptC3>(c3);

		PTGN_LOG("Circle drag id: ", c3.GetId());
	}

	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	void Update() override {
		if (input.KeyDown(Key::T)) {
			bool desired{ !input.IsTopOnly() };
			input.SetTopOnly(desired);
			PTGN_LOG("Top only input: ", desired);
		}

		MoveWASD(camera, { 3.0f, 3.0f });

		auto dt{ game.dt() };

		if (input.KeyPressed(Key::Q)) {
			Rotate(camera, rotation_speed * dt);
		}
		if (input.KeyPressed(Key::E)) {
			Rotate(camera, -rotation_speed * dt);
		}
		if (input.KeyPressed(Key::Z)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyPressed(Key::C)) {
			camera.Zoom(-zoom_speed * dt);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init(
		"InteractiveScene: T: Toggle Top Only Input, WASD/QE/ZC: "
		"Move/Rotate/Zoom Camera",
		{ 800, 800 }
	);
	game.scene.Enter<InteractiveScene>("");
	return 0;
}