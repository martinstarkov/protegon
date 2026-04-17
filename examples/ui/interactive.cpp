
#include "runtime/interaction/interactive.h"

#include <chrono>
#include <optional>
#include <utility>

#include "app/application.h"
#include "core/editor.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/interaction/trigger_condition.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "serialization/json/fwd.h"

using namespace ptgn;

struct DragScript : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::Drag>([this](auto& e) { OnDrag(e.position); });
	}

	void OnDrag(V2_float pos) {
		// PTGN_LOG(entity, " drag pos: ", pos);
		SetPosition(entity, pos);
	}
};

struct DropzoneScript : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::DropIntoDropzone>([this](auto& e) { OnDrop(e.draggable); });
		d.Dispatch<event::PickupFromDropzone>([this](auto& e) { OnPickup(e.draggable); });
	}

	void OnDrop(Entity draggable) {
		PTGN_LOG("dropped ", draggable, " onto dropzone ", entity);
	}

	void OnPickup(Entity draggable) {
		PTGN_LOG("picked up ", draggable, " from dropzone ", entity);
	}
};

struct DraggableScript : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::Drag>(&DraggableScript::OnDrag, this);
		d.Dispatch<event::MousePressedOver>(&DraggableScript::OnMousePressedOver, this);
		d.Dispatch<event::MousePressedOut>(&DraggableScript::OnMousePressedOut, this);
		d.Dispatch<event::MouseHeldOver>(&DraggableScript::OnMouseHeldOver, this);
		d.Dispatch<event::MouseScrollOver>(&DraggableScript::OnMouseScrollOver, this);
		d.Dispatch<event::MouseReleasedOver>(&DraggableScript::OnMouseReleasedOver, this);
		d.Dispatch<event::MouseReleasedOut>(&DraggableScript::OnMouseReleasedOut, this);
		d.Dispatch<event::DragEnter>(&DraggableScript::OnDragEnter, this);
		d.Dispatch<event::DragLeave>(&DraggableScript::OnDragLeave, this);
		d.Dispatch<event::DragOut>(&DraggableScript::OnDragOut, this);
		d.Dispatch<event::DragOver>(&DraggableScript::OnDragOver, this);
		d.Dispatch<event::DragStart>(&DraggableScript::OnDragStart, this);
		d.Dispatch<event::DragStop>(&DraggableScript::OnDragStop, this);
		d.Dispatch<event::DropIntoDropzone>(&DraggableScript::OnDrop, this);
		d.Dispatch<event::PickupFromDropzone>(&DraggableScript::OnPickup, this);
	}

	void OnDrag(V2_float pos) {
		// PTGN_LOG("Setting draggable entity position to: ", pos);
		SetPosition(entity, pos);
	}

	void OnMousePressedOver(Mouse mouse) {
		PTGN_LOG(entity, " Mouse pressed: ", json(mouse));
	}

	void OnMousePressedOut(Mouse mouse) {
		PTGN_LOG(entity, " Mouse pressed outside: ", json(mouse));
	}

	void OnMouseHeldOver(Mouse mouse) {
		// PTGN_LOG(entity, " Mouse held: ", mouse);
	}

	void OnMouseScrollOver(V2_float mouse) {
		PTGN_LOG(entity, " Mouse scroll: ", mouse);
	}

	void OnMouseReleasedOver(Mouse mouse) {
		PTGN_LOG(entity, " Mouse released: ", json(mouse));
	}

	void OnMouseReleasedOut(Mouse mouse) {
		PTGN_LOG(entity, " Mouse released outside: ", json(mouse));
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

	void OnDragStart(V2_float start_position) {
		PTGN_LOG(entity, " Drag start: ", start_position);
	}

	void OnDragStop(V2_float stop_position) {
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

		ctx().interaction.SetDebugSettings({ .draw_enabled = true, .draw_line_width = 3.0f });

		ctx().asset.LoadMany({ { "circle", "assets/circle.png" },
							   { "drag", "assets/drag.png" },
							   { "drag_circle", "assets/drag_circle.png" },
							   { "dropzone", "assets/dropzone.png" },
							   { "box", "assets/box.png" } });

		V2_float center{ GetTransform(ctx().camera).GetPosition() };

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
		if (ctx().input.KeyPressed(Key::T)) {
			bool desired{ !ctx().interaction.IsTopOnly() };
			ctx().interaction.SetTopOnly(desired);
			PTGN_LOG("Top only input: ", desired);
		}

		constexpr V2_float speed{ 300.0f };
		float dt{ ctx().dt().count() };
		MoveWASD(ctx().camera, speed * dt);

		if (ctx().input.KeyHeld(Key::Q)) {
			Rotate(ctx().camera, Radians{ rotation_speed } * dt);
		}
		if (ctx().input.KeyHeld(Key::E)) {
			Rotate(ctx().camera, Radians{ -rotation_speed } * dt);
		}
		if (ctx().input.KeyHeld(Key::Z)) {
			ctx().camera.Zoom(zoom_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::C)) {
			ctx().camera.Zoom(-zoom_speed * dt);
		}
	}
};

int main(int, char**) {
	Application app{ "InteractiveScene: T: Toggle Top Only Input, WASD/QE/ZC: "
					 "Move/Rotate/Zoom Camera" };
	PTGN_WITH_EDITOR(app);
	app.StartWith<InteractiveScene>();
}