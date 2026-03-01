#include <iostream>
#include <ostream>

#include "app/application.h"
#include "app/context.h"
#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/shape.h"
#include "platform/input/events.h"
#include "platform/input/mouse.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/ecs/components/text_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class TemplateScene : public Scene {
public:
	void OnEnter() override {}

	void OnUpdate() override {}

	void OnExit() override {}

	void OnEvent(EventDispatcher d) override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{ "TemplateScene" };
	app.StartWith<TemplateScene>();
}