#include "runtime/event/state_machine.h"

#include <functional>
#include <iostream>
#include <ostream>
#include <utility>
#include <vector>

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/interactive.h"

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

			.Transition<Normal, MouseEnter, Hovered>()
			.Action([](Entity e) { std::cout << "Hover start\n"; })

			.Transition<Hovered, MouseLeave, Normal>()
			.Action([](Entity e) { std::cout << "Hover end\n"; })

			.Transition<Hovered, MousePressedOver, Pressed>()
			.Action([](Entity e) { std::cout << "Pressed\n"; })

			.Transition<Pressed, MouseReleasedOver, Hovered>()
			.Action([](Entity e) { std::cout << "Click\n"; });

		AddStateMachine(r)
			.Initial<Normal>()

			.Transition<Normal, MouseEnter, Hovered>()
			.Action([](Entity e) { std::cout << "Hover animation (infinite loop)\n"; })

			.Transition<Hovered, MouseLeave, Normal>()

			.Transition<Hovered, MousePressedOver, Pressed>()
			.Action([](Entity e) { std::cout << "Pressed animation (infinite loop)\n"; })

			.Transition<Pressed, MouseReleasedOver, Hovered>()
			.Action([](Entity e) { std::cout << "Click animation (once)\n"; });
	}
};

int main(int, char**) {
	Application app{ "StateMachineScene" };
	app.StartWith<StateMachineScene>();
}