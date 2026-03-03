
#include "runtime/ui/interactive.h"

#include <utility>

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

struct ScriptC0 : public Script {
	/*
	void OnKeyPressed(Key key)  {
		PTGN_LOG("c0 Key pressed");
	}

	void OnKeyHeld(Key key)  {
		PTGN_LOG("c0 Key held");
	}

	void OnKeyReleased(Key key)  {
		PTGN_LOG("c0 Key released");
	}

	void OnMousePressedOver(Mouse mouse)  {
		PTGN_LOG("c0 Mouse pressed: ", mouse);
	}

	void OnMousePressedOut(Mouse mouse)  {
		PTGN_LOG("c0 Mouse pressed outside: ", mouse);
	}

	void OnMouseLeave()  {
		PTGN_LOG("c0 Mouse leave");
	}

	void OnMouseEnter()  {
		PTGN_LOG("c0 Mouse enter");
	}

	void OnMouseMoveOut()  { }

	void OnMouseMoveOver()  {
		PTGN_LOG("c0 Mouse over");
	}

	void OnMouseHeldOver(Mouse mouse)  {
		PTGN_LOG("c0 Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse)  {
		PTGN_LOG("c0 Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse)  {
		PTGN_LOG("c0 Mouse released: ", mouse);
	}

	void OnMouseReleasedOut(Mouse mouse)  {
		PTGN_LOG("c0 Mouse released outside: ", mouse);
	}
	*/
};

struct ScriptC1 : public Script {
	/*
	void OnKeyPressed(Key key)  {
		PTGN_LOG("c1 Key pressed");
	}

	void OnKeyHeld(Key key)  {
		PTGN_LOG("c1 Key held");
	}

	void OnKeyReleased(Key key)  {
		PTGN_LOG("c1 Key released");
	}

	void OnMousePressedOver(Mouse mouse)  {
		PTGN_LOG("c1 Mouse pressed: ", mouse);
	}

	void OnMousePressedOut(Mouse mouse)  {
		PTGN_LOG("c1 Mouse pressed outside: ", mouse);
	}

	void OnMouseLeave()  {
		PTGN_LOG("c1 Mouse leave");
	}

	void OnMouseEnter()  {
		PTGN_LOG("c1 Mouse enter");
	}

	void OnMouseMoveOut()  { }

	void OnMouseMoveOver()  {
		PTGN_LOG("c1 Mouse over");
	}

	void OnMouseHeldOver(Mouse mouse)  {
		PTGN_LOG("c1 Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse)  {
		PTGN_LOG("c1 Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse)  {
		PTGN_LOG("c1 Mouse released: ", mouse);
	}

	void OnMouseReleasedOut(Mouse mouse)  {
		PTGN_LOG("c1 Mouse released outside: ", mouse);
	}
	*/
};

struct ScriptR0 : public Script {
	/*
	void OnKeyPressed(Key key)  {
		PTGN_LOG("r0 Key pressed");
	}

	void OnKeyHeld(Key key)  {
		PTGN_LOG("r0 Key held");
	}

	void OnKeyReleased(Key key)  {
		PTGN_LOG("r0 Key released");
	}

	void OnMousePressedOver(Mouse mouse)  {
		PTGN_LOG("r0 Mouse pressed: ", mouse);
	}

	void OnMousePressedOut(Mouse mouse)  {
		PTGN_LOG("r0 Mouse pressed outside: ", mouse);
	}

	void OnMouseLeave()  {
		PTGN_LOG("r0 Mouse leave");
	}

	void OnMouseEnter()  {
		PTGN_LOG("r0 Mouse enter");
	}

	void OnMouseMoveOut()  {  }

	void OnMouseMoveOver()  {
		PTGN_LOG("r0 Mouse over");
	}

	void OnMouseHeldOver(Mouse mouse)  {
		PTGN_LOG("r0 Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse)  {
		PTGN_LOG("r0 Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse)  {
		PTGN_LOG("r0 Mouse released: ", mouse);
	}

	void OnMouseReleasedOut(Mouse mouse)  {
		PTGN_LOG("r0 Mouse released outside: ", mouse);
	}
	*/
};

struct ScriptR1 : public Script {
	/*
	void OnKeyPressed(Key key)  {
		PTGN_LOG("r1 Key pressed");
	}

	void OnKeyHeld(Key key)  {
		PTGN_LOG("r1 Key held");
	}

	void OnKeyReleased(Key key)  {
		PTGN_LOG("r1 Key released");
	}

	void OnMousePressedOver(Mouse mouse)  {
		PTGN_LOG("r1 Mouse pressed: ", mouse);
	}

	void OnMousePressedOut(Mouse mouse)  {
		PTGN_LOG("r1 Mouse pressed outside: ", mouse);
	}

	void OnMouseLeave()  {
		PTGN_LOG("r1 Mouse leave");
	}

	void OnMouseEnter()  {
		PTGN_LOG("r1 Mouse enter");
	}

	void OnMouseMoveOut()  { }

	void OnMouseMoveOver()  {
		PTGN_LOG("r1 Mouse over");
	}

	void OnMouseHeldOver(Mouse mouse)  {
		PTGN_LOG("r1 Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse)  {
		PTGN_LOG("r1 Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse)  {
		PTGN_LOG("r1 Mouse released: ", mouse);
	}

	void OnMouseReleasedOut(Mouse mouse)  {
		PTGN_LOG("r1 Mouse released outside: ", mouse);
	}

	*/
};

struct ScriptR2 : public Script {
	/*
	void OnKeyPressed(Key key)  {
		PTGN_LOG("r2 Key pressed");
	}

	void OnKeyHeld(Key key)  {
		PTGN_LOG("r2 Key held");
	}

	void OnKeyReleased(Key key)  {
		PTGN_LOG("r2 Key released");
	}

	void OnMousePressedOver(Mouse mouse)  {
		PTGN_LOG("r2 Mouse pressed: ", mouse);
	}

	void OnMousePressedOut(Mouse mouse)  {
		PTGN_LOG("r2 Mouse pressed outside: ", mouse);
	}

	void OnMouseLeave()  {
		PTGN_LOG("r2 Mouse leave");
	}

	void OnMouseEnter()  {
		PTGN_LOG("r2 Mouse enter");
	}

	void OnMouseMoveOut()  { }

	void OnMouseMoveOver()  {
		PTGN_LOG("r2 Mouse over");
	}

	void OnMouseHeldOver(Mouse mouse)  {
		PTGN_LOG("r2 Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse)  {
		PTGN_LOG("r2 Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse)  {
		PTGN_LOG("r2 Mouse released: ", mouse);
	}

	void OnMouseReleasedOut(Mouse mouse)  {
		PTGN_LOG("r2 Mouse released outside: ", mouse);
	}

	*/
};

struct ScriptR3 : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<Dragging>([this](auto& e) { OnDrag(e.offset); });
	}

	void OnDrag(V2_float offset) {
		// PTGN_LOG("r3 Drag: ", mouse);
		SetPosition(entity, entity.GetScene().input.GetMousePosition() + offset);
	}

	/*
	void OnDragEnter(Entity dropzone)  {
		PTGN_LOG("r3 Drag enter: ", dropzone);
	}

	void OnDragLeave(Entity dropzone)  {
		PTGN_LOG("r3 Drag leave: ", dropzone);
	}

	void OnDragOut(Entity dropzone)  {
		PTGN_LOG("r3 Drag out: ", dropzone);
	}

	void OnDragOver(Entity dropzone)  {
		PTGN_LOG("r3 Drag over: ", dropzone);
	}

	void OnDragStart(V2_int start_position)  {
		PTGN_LOG("r3 Drag start: ", start_position);
	}

	void OnDragStop(V2_int stop_position)  {
		PTGN_LOG("r3 Drag stop: ", stop_position);
	}

	void OnDrop(Entity dropzone)  {
		PTGN_LOG("r3 dropped onto: ", dropzone);
	}

	void OnPickup(Entity dropzone)  {
		PTGN_LOG("r3 picked up from: ", dropzone);
	}
	*/
};

struct ScriptC3 : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<Dragging>([this](auto& e) { OnDrag(e.offset); });
		d.Dispatch<MousePressedOver>([this](auto& e) { OnMousePressedOver(e.mouse); });
		d.Dispatch<MousePressedOut>([this](auto& e) { OnMousePressedOut(e.mouse); });
		d.Dispatch<MouseHeldOver>([this](auto& e) { OnMouseHeldOver(e.mouse); });
		d.Dispatch<MouseScrollOver>([this](auto& e) { OnMouseScrollOver(e.mouse); });
		d.Dispatch<MouseReleasedOver>([this](auto& e) { OnMouseReleasedOver(e.mouse); });
		d.Dispatch<MouseReleasedOut>([this](auto& e) { OnMouseReleasedOut(e.mouse); });
		d.Dispatch<DragEnter>([this](auto& e) { OnDragEnter(e.dropzone); });
		d.Dispatch<DragLeave>([this](auto& e) { OnDragLeave(e.dropzone); });
		d.Dispatch<DragOut>([this](auto& e) { OnDragOut(e.dropzone); });
		d.Dispatch<DragOver>([this](auto& e) { OnDragOver(e.dropzone); });
		d.Dispatch<DragStart>([this](auto& e) { OnDragStart(e.start_position); });
		d.Dispatch<DragStop>([this](auto& e) { OnDragStop(e.stop_position); });
		d.Dispatch<DropIntoDropzone>([this](auto& e) { OnDrop(e.dropzone); });
		d.Dispatch<PickupFromDropzone>([this](auto& e) { OnPickup(e.dropzone); });
	}

	void OnDrag(V2_float offset) {
		// PTGN_LOG("c3 Drag: mouse: ", mouse, ", offset: ", offset);
		SetPosition(entity, entity.GetScene().input.GetMousePosition() + offset);
	}

	void OnMousePressedOver(Mouse mouse) {
		PTGN_LOG("c3 Mouse pressed: ", mouse);
	}

	void OnMousePressedOut(Mouse mouse) {
		PTGN_LOG("c3 Mouse pressed outside: ", mouse);
	}

	void OnMouseHeldOver(Mouse mouse) {
		// PTGN_LOG("c3 Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_int mouse) {
		PTGN_LOG("c3 Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse) {
		PTGN_LOG("c3 Mouse released: ", mouse);
	}

	void OnMouseReleasedOut(Mouse mouse) {
		PTGN_LOG("c3 Mouse released outside: ", mouse);
	}

	void OnDragEnter(Entity dropzone) {
		PTGN_LOG("c3 Drag enter: ", dropzone);
	}

	void OnDragLeave(Entity dropzone) {
		PTGN_LOG("c3 Drag leave: ", dropzone);
	}

	void OnDragOut(Entity dropzone) {
		// PTGN_LOG("c3 Drag out: ", dropzone);
	}

	void OnDragOver(Entity dropzone) {
		// PTGN_LOG("c3 Drag over: ", dropzone);
	}

	void OnDragStart(V2_int start_position) {
		PTGN_LOG("c3 Drag start: ", start_position);
	}

	void OnDragStop(V2_int stop_position) {
		PTGN_LOG("c3 Drag stop: ", stop_position);
	}

	void OnDrop(Entity dropzone) {
		PTGN_LOG("c3 dropped onto: ", dropzone);
	}

	void OnPickup(Entity dropzone) {
		PTGN_LOG("c3 picked up from: ", dropzone);
	}
};

struct InteractiveScene : public Scene {
	Entity CreateInteractiveCircle(float radius) {
		auto entity = CreateEntity();
		entity.Add<Circle>(radius);
		return entity;
	}

	Entity CreateInteractiveRect(const V2_float& size) {
		auto entity = CreateEntity();
		entity.Add<Rect>(size);
		return entity;
	}

	void OnEnter() override {
		input.SetInteractiveDebugDraw({ .enabled = true, .line_width = 3.0f });

		app().asset.LoadMany({ { "drag", "assets/drag.png" },
							   { "drag_circle", "assets/drag_circle.png" },
							   { "dropzone", "assets/dropzone.png" } });

		V2_float center{ GetTransform(camera).GetPosition() };

		V2_float offset{ 250, 250 };
		V2_float rsize{ 100, 50 };

		auto c0 = CreateCircle(
			*this, center + V2_float{ offset.x, -offset.y }, 90.0f, color::Green, 1.0f
		);
		auto c0_child = CreateInteractiveCircle(90.0f);
		AddInteractiveShape(c0, GameObject{ std::move(c0_child) });
		AddScript<ScriptC0>(c0);

		auto c1 = CreateCircle(
			*this, center + V2_float{ offset.x, offset.y }, 90.0f, color::LightGreen, 1.0f
		);
		auto c1_child = CreateInteractiveCircle(45.0f);
		AddInteractiveShape(c1, GameObject{ std::move(c1_child) });
		AddScript<ScriptC1>(c1);

		auto r0 = CreateRect(
			*this, center + V2_float{ -offset.x, -offset.y }, rsize * 2, color::Blue, 1.0f
		);
		auto r0_child = CreateInteractiveRect(rsize * 2);
		AddInteractiveShape(r0, GameObject{ std::move(r0_child) });
		AddScript<ScriptR0>(r0);

		auto r1 = CreateRect(
			*this, center + V2_float{ -offset.x, offset.y }, rsize, color::LightBlue, 1.0f
		);
		auto r1_child = CreateInteractiveRect(rsize * 2);
		AddInteractiveShape(r1, GameObject{ std::move(r1_child) });
		AddScript<ScriptR1>(r1);

		app().asset.Load("box", "assets/box.png");

		auto r2		  = CreateSprite(*this, "box", center + V2_float{ -offset.x, 0.0f });
		auto r2_child = CreateInteractiveRect(GetDisplaySize(r2));
		AddInteractiveShape(r2, GameObject{ std::move(r2_child) });
		AddScript<ScriptR2>(r2);

		auto r4		  = CreateSprite(*this, "dropzone", center + V2_float{ 0.0f, -offset.y });
		auto r4_child = CreateInteractiveRect(rsize * 2);
		AddInteractiveShape(r4, GameObject{ std::move(r4_child) });
		SetDropzone(r4);
		PTGN_LOG("Dropzone: ", r4);

		auto r3		  = CreateSprite(*this, "drag", center + V2_float{ offset.x, 0.0f });
		auto r3_child = CreateInteractiveRect(GetDisplaySize(r3));
		AddInteractiveShape(r3, GameObject{ std::move(r3_child) });
		SetDraggable(r3);
		AddScript<ScriptR3>(r3);

		PTGN_LOG("Rect drag: ", r3);

		auto c3		  = CreateSprite(*this, "drag_circle", center + V2_float{ 0, 0 });
		auto c3_child = CreateInteractiveCircle(GetDisplaySize(c3).x * 0.5f);
		AddInteractiveShape(c3, GameObject{ std::move(c3_child) });
		SetDraggable(c3); //.SetTrigger(CallbackTrigger::MouseOverlaps);
		AddScript<ScriptC3>(c3);

		PTGN_LOG("Circle drag: ", c3);
	}

	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	void OnUpdate() override {
		if (input.KeyPressed(Key::T)) {
			bool desired{ !input.IsTopOnly() };
			input.SetTopOnly(desired);
			PTGN_LOG("Top only input: ", desired);
		}

		MoveWASD(camera, { 3.0f, 3.0f });

		float dt{ app().DeltaTime().count() };

		if (input.KeyHeld(Key::Q)) {
			Rotate(camera, rotation_speed * dt);
		}
		if (input.KeyHeld(Key::E)) {
			Rotate(camera, -rotation_speed * dt);
		}
		if (input.KeyHeld(Key::Z)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyHeld(Key::C)) {
			camera.Zoom(-zoom_speed * dt);
		}
	}
};

int main(int, char**) {
	Application app{ "InteractiveScene: T: Toggle Top Only Input, WASD/QE/ZC: "
					 "Move/Rotate/Zoom Camera" };
	app.StartWith<InteractiveScene>();
}