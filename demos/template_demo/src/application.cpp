#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

class TemplateScene : public Scene {
public:
	void Init() override {

	}

	void Update() override {
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("TemplateTitle", resolution);
	Application::Get().scene_.LoadActive<TemplateScene>("template_scene");
	return 0;
}
