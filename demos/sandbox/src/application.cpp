#include "protegon/protegon.h"

using namespace ptgn;

class Sandbox : public Engine {
	Button button0{ Rectangle<int>{ { 5, 5 }, { 50, 50 } },
		Texture{ "resources/ui/idle.png" },
		Texture{ "resources/ui/hover.png" },
		Texture{ "resources/ui/pressed.png" }, 
	[&]() {
		PrintLine(bye);
	} };
	Button button1{ Rectangle<int>{ { 60, 60 }, { 300, 300 } },
		Texture{ "resources/ui/q0.png" },
		Texture{ "resources/ui/q1.png" },
		Texture{ "resources/ui/q2.png" }
	};
	std::string bye{ "bye" };
	Button button2{ Rectangle<int>{ { 460, 460 }, { 30, 30 } },
		Texture{ "resources/ui/idle.png" },
		Texture{ "resources/ui/hover.png" },
		Texture{ "resources/ui/pressed.png" }
	};
	ToggleButton button3{ Rectangle<int>{ { 390, 390 }, { 50, 50 } },
		std::pair<Texture, Texture>{ Texture{ "resources/ui/idle.png" }, Texture{ "resources/ui/mute_grey.png" } },
		std::pair<Texture, Texture>{ Texture{ "resources/ui/hover.png" }, Texture{ "resources/ui/mute_grey_hover.png" } },
		std::pair<Texture, Texture>{ Texture{ "resources/ui/pressed.png" }, Texture{ "resources/ui/mute_grey_pressed.png" } },
		[&]() {
			 PrintLine("Toggling!");
	    }
	};
	void Create() final {
		button2.SetOnActivate([]() {
			PrintLine("Hi!");
		});
	}
	void Update(float dt) final {
		button0.Draw();
		button1.GetRectangle().Draw(color::BLACK, 10);
		button1.Draw();
		button2.Draw();
		button3.Draw();
		if (input::KeyDown(Key::T)) {
			button3.SetToggleStatus(!button3.GetToggleStatus());
			button3.Activate();
		}
	}
};

int main(int c, char** v) {
	Sandbox game;
	game.Construct("sandbox", { 720, 720 });
	return 0;
}