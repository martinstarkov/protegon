
#include "runtime/ui/interactive.h"

#include <chrono>
#include <optional>
#include <utility>

#include "app/application.h"
#include "app/context.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

struct DragScript : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<Dragging>([this](auto& e) { OnDrag(e.position); });
	}

	void OnDrag(V2_float pos) {
		// PTGN_LOG(entity, " drag pos: ", pos);
		SetPosition(entity, pos);
	}
};

struct DropzoneScript : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<DropIntoDropzone>([this](auto& e) { OnDrop(e.draggable); });
		d.Dispatch<PickupFromDropzone>([this](auto& e) { OnPickup(e.draggable); });
	}

	void OnDrop(Entity draggable) {
		PTGN_LOG("dropped ", draggable, " onto dropzone ", entity);
	}

	void OnPickup(Entity draggable) {
		PTGN_LOG("picked up ", draggable, " from dropzone ", entity);
	}
};

struct DraggableScript : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<Dragging>([this](auto& e) { OnDrag(e.position); });
		d.Dispatch<MousePressedOver>([this](auto& e) { OnMousePressedOver(e.button); });
		d.Dispatch<MousePressedOut>([this](auto& e) { OnMousePressedOut(e.button); });
		d.Dispatch<MouseHeldOver>([this](auto& e) { OnMouseHeldOver(e.button); });
		d.Dispatch<MouseScrollOver>([this](auto& e) { OnMouseScrollOver(e.scroll_delta); });
		d.Dispatch<MouseReleasedOver>([this](auto& e) { OnMouseReleasedOver(e.button); });
		d.Dispatch<MouseReleasedOut>([this](auto& e) { OnMouseReleasedOut(e.button); });
		d.Dispatch<DragEnter>([this](auto& e) { OnDragEnter(e.dropzone); });
		d.Dispatch<DragLeave>([this](auto& e) { OnDragLeave(e.last_dropzone); });
		d.Dispatch<DragOut>([this](auto& e) { OnDragOut(e.dropzone); });
		d.Dispatch<DragOver>([this](auto& e) { OnDragOver(e.dropzone); });
		d.Dispatch<DragStart>([this](auto& e) { OnDragStart(e.start_position); });
		d.Dispatch<DragStop>([this](auto& e) { OnDragStop(e.stop_position); });
		d.Dispatch<DropIntoDropzone>([this](auto& e) { OnDrop(e.draggable); });
		d.Dispatch<PickupFromDropzone>([this](auto& e) { OnPickup(e.draggable); });
	}

	void OnDrag(V2_float pos) {
		// PTGN_LOG(entity, " drag pos: ", pos);
		SetPosition(entity, pos);
	}

	void OnMousePressedOver(Mouse mouse) {
		PTGN_LOG(entity, " Mouse pressed: ", mouse);
	}

	void OnMousePressedOut(Mouse mouse) {
		PTGN_LOG(entity, " Mouse pressed outside: ", mouse);
	}

	void OnMouseHeldOver(Mouse mouse) {
		// PTGN_LOG(entity, " Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_float mouse) {
		PTGN_LOG(entity, " Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse) {
		PTGN_LOG(entity, " Mouse released: ", mouse);
	}

	void OnMouseReleasedOut(Mouse mouse) {
		PTGN_LOG(entity, " Mouse released outside: ", mouse);
	}

	void OnDragEnter(Entity dropzone) {
		PTGN_LOG(entity, " Drag enter: ", dropzone);
	}

	void OnDragLeave(Entity last_dropzone) {
		PTGN_LOG(entity, " Drag leave: ", last_dropzone);
	}

	void OnDragOut(Entity dropzone) {
		// PTGN_LOG(entity, " Drag out: ", dropzone);
	}

	void OnDragOver(Entity dropzone) {
		// PTGN_LOG(entity, " Drag over: ", dropzone);
	}

	void OnDragStart(V2_int start_position) {
		PTGN_LOG(entity, " Drag start: ", start_position);
	}

	void OnDragStop(V2_int stop_position) {
		PTGN_LOG(entity, " Drag stop: ", stop_position);
	}

	void OnDrop(Entity draggable) {
		PTGN_ERROR("Logic error, draggable should not trigger this event");
	}

	void OnPickup(Entity draggable) {
		PTGN_ERROR("Logic error, draggable should not trigger this event");
	}
};

struct InteractiveScene : public Scene {
	Entity CreateInteractiveCircle(float radius) {
		auto entity = CreateEntity();
		entity.Add<Circle>(radius);
		return entity;
	}

	Entity CreateInteractiveRect(V2_float size) {
		auto entity = CreateEntity();
		entity.Add<Rect>(size);
		return entity;
	}

	void OnEnter() override {
		SetBackgroundColor(color::DarkGray);

		input.SetSettings({ .debug_draw_enabled = true, .debug_draw_line_width = 3.0f });

		app().asset.LoadMany({ { "circle", "assets/circle.png" },
							   { "drag", "assets/drag.png" },
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

		auto c1 = CreateCircle(
			*this, center + V2_float{ offset.x, offset.y }, 90.0f, color::LightGreen, 1.0f
		);
		auto c1_child = CreateInteractiveCircle(45.0f);
		AddInteractiveShape(c1, GameObject{ std::move(c1_child) });

		auto r0 = CreateRect(
			*this, center + V2_float{ -offset.x, -offset.y }, rsize * 2, color::Blue, 1.0f
		);
		auto r0_child = CreateInteractiveRect(rsize * 2);
		AddInteractiveShape(r0, GameObject{ std::move(r0_child) });

		auto r1 = CreateRect(
			*this, center + V2_float{ -offset.x, offset.y }, rsize, color::LightBlue, 1.0f
		);
		auto r1_child = CreateInteractiveRect(rsize * 2);
		AddInteractiveShape(r1, GameObject{ std::move(r1_child) });

		app().asset.Load("box", "assets/box.png");

		auto r2		  = CreateSprite(*this, "box", center + V2_float{ -offset.x, 0.0f });
		auto r2_child = CreateInteractiveRect(*GetDisplaySize(r2));
		AddInteractiveShape(r2, GameObject{ std::move(r2_child) });

		auto r4		  = CreateSprite(*this, "dropzone", center + V2_float{ 0.0f, -offset.y });
		auto r4_child = CreateInteractiveRect(rsize * 2);
		AddInteractiveShape(r4, GameObject{ std::move(r4_child) });
		SetDropzone(r4);
		AddScript<DropzoneScript>(r4);
		PTGN_LOG("Dropzone: ", r4);

		auto r3		  = CreateSprite(*this, "drag", center + V2_float{ offset.x, 0.0f });
		auto r3_child = CreateInteractiveRect(*GetDisplaySize(r3));
		AddInteractiveShape(r3, GameObject{ std::move(r3_child) });
		SetDraggable(r3);
		AddScript<DragScript>(r3);

		PTGN_LOG("Rect drag: ", r3);

		auto c3		  = CreateSprite(*this, "drag_circle", center + V2_float{ 0, 0 });
		auto c3_child = CreateInteractiveCircle(GetDisplaySize(c3)->x * 0.5f);
		AddInteractiveShape(c3, GameObject{ std::move(c3_child) });
		SetDraggable(c3); //.SetTrigger(CallbackTrigger::MouseOverlaps);
		AddScript<DraggableScript>(c3);

		PTGN_LOG("Circle drag: ", c3);

		auto c4		  = CreateSprite(*this, "circle", center + V2_float{ 0, offset.y });
		auto c4_child = CreateInteractiveCircle(GetDisplaySize(c4)->x * 0.5f);
		AddInteractiveShape(c4, GameObject{ std::move(c4_child) });
		SetDraggable(c4, ComponentState::Disabled);
		AddScript<DraggableScript>(c4);

		PTGN_LOG("Disabled circle drag: ", c4);
	}

	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	void OnUpdate() override {
		if (input.KeyPressed(Key::T)) {
			bool desired{ !input.IsTopOnly() };
			input.SetTopOnly(desired);
			PTGN_LOG("Top only input: ", desired);
		}

		constexpr V2_float speed{ 300.0f };
		float dt{ app().DeltaTime().count() };
		MoveWASD(camera, speed * dt);

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