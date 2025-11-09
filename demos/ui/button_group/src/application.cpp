#include "core/app/application.h"

#include "core/app/window.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/transform.h"
#include "core/log.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "ui/button.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

struct ToggleButtonScript : public Script<ToggleButtonScript, ButtonScript> {
	ToggleButtonScript() {}

	explicit ToggleButtonScript(int number) : number{ number } {}

	void OnButtonActivate() override {
		PTGN_LOG("Clicked ", number);
	}

	int number{ 0 };
};

class ToggleButtonGroupScene : public Scene {
	ToggleButtonGroup group;

	ToggleButton CreateToggleButtonGroupItem(const V2_float& position, int number) {
		ToggleButton b{ CreateToggleButton(*this, false) };
		SetPosition(b, position);
		b.SetSize({ 200, 130 });
		SetDrawOrigin(b, Origin::TopLeft);
		b.SetBackgroundColor(color::LightRed);
		b.SetBackgroundColor(color::Red, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
		b.SetBackgroundColorToggled(color::LightBlue);
		b.SetBackgroundColorToggled(color::Blue, ButtonState::Hover);
		b.SetBackgroundColorToggled(color::DarkBlue, ButtonState::Pressed);
		AddScript<ToggleButtonScript>(b, number);
		return b;
	}

	void Enter() override {
		Application::Get().window_.SetResizable();
		group = CreateToggleButtonGroup(*this);
		group.Load("1", CreateToggleButtonGroupItem(V2_float{ -100, -300 - 130 / 2 }, 1));
		group.Load("2", CreateToggleButtonGroupItem(V2_float{ -100, -100 - 130 / 2 }, 2));
		group.Load("3", CreateToggleButtonGroupItem(V2_float{ -100, 100 - 130 / 2 }, 3));
		group.Load("4", CreateToggleButtonGroupItem(V2_float{ -100, 300 - 130 / 2 }, 4));
		group.SetActive("3");
	}

	void Update() override {
		if (input.KeyDown(Key::I)) {
			PTGN_LOG("Active Toggle Button ID: ", group.GetActive().GetId());
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ToggleButtonGroupScene");
	Application::Get().scene_.Enter<ToggleButtonGroupScene>("");
	return 0;
}