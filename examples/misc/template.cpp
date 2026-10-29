
#include "app/application.h"
#include "core/event/event.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class TemplateScene : public Scene {
public:
	void OnEnter() override {}

	void OnUpdate() override {}

	void OnExit() override {}

	void OnEvent(Event d) override {}
};

int main(int, char**) {
	Application app{ "TemplateScene" };
	app.StartWith<TemplateScene>();
}