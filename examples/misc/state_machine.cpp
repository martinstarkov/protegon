#include "runtime/event/state_machine.h"

#include <utility>

#include "app/application.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/interaction/interactive.h"

using namespace ptgn;

struct StateMachineScene : public Scene {
	Entity CreateInteractiveRect(V2_float size) {
		auto entity = CreateEntity();
		entity.Add<Rect>(size);
		return entity;
	}

	void OnEnter() override {
		V2_float rsize{ 100, 50 };

		auto r		= CreateRect(*this, {}, rsize, color::Green, 1.0f);
		auto rchild = CreateInteractiveRect(rsize);
		AddInteractiveShape(r, GameObject{ std::move(rchild) });

		struct Normal {};

		struct Hovered {};

		struct Pressed {};

		AddStateMachine(r)
			.Initial<Normal>()

			.Transition<Normal, event::MouseEnter, Hovered>()
			.Action([](Entity e) { PTGN_LOG("Hover start"); })

			.Transition<Hovered, event::MouseLeave, Normal>()
			.Action([](Entity e) { PTGN_LOG("Hover end"); })

			.Transition<Hovered, event::MousePressedOver, Pressed>()
			.Action([](Entity e) { PTGN_LOG("Pressed"); })

			.Transition<Pressed, event::MouseReleasedOver, Hovered>()
			.Action([](Entity e) { PTGN_LOG("Released"); });

		AddStateMachine(r)
			.Initial<Normal>()

			.Transition<Normal, event::MouseEnter, Hovered>()
			.Action([](Entity e) { PTGN_LOG("Hover animation (infinite loop)"); })

			.Transition<Hovered, event::MouseLeave, Normal>()

			.Transition<Hovered, event::MousePressedOver, Pressed>()
			.Action([](Entity e) { PTGN_LOG("Pressed animation (infinite loop)"); })

			.Transition<Pressed, event::MouseReleasedOver, Hovered>()
			.Action([](Entity e) { PTGN_LOG("Released animation (once)"); });
	}
};

int main(int, char**) {
	Application app{ "StateMachineScene" };
	app.StartWith<StateMachineScene>();
}