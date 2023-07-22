#include "protegon/protegon.h"

using namespace ptgn;

enum class MouseEvent {
	MOVE,
	DOWN,
	UP,
	PRESSED,
};

struct MouseMoveEvent : public Event<MouseEvent> {
	MouseMoveEvent(const V2_int& previous, const V2_int& current) : 
		previous{ previous }, 
		current{ current }, 
		Event{ MouseEvent::MOVE } {}
	V2_int previous;
	V2_int current;
};

class MousePressedEvent : public Event<MouseEvent> {
public:
	MousePressedEvent(Mouse mouse, const V2_int& current) : 
		mouse{ mouse }, 
		current{ current },
		Event{ MouseEvent::PRESSED } {}
	Mouse mouse;
	V2_int current;
};

class MouseDownEvent : public Event<MouseEvent> {
public:
	MouseDownEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse }, 
		current{ current },
		Event{ MouseEvent::DOWN } {}
	Mouse mouse;
	V2_int current;
};

class MouseUpEvent : public Event<MouseEvent> {
public:
	MouseUpEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse },
		current{ current },
		Event{ MouseEvent::UP } {}
	Mouse mouse;
	V2_int current;
};

class IButton {
public:
	virtual ~IButton() = default;
	virtual void OnMouseEvent(const Event<MouseEvent>& event) = 0;
	virtual void OnActivate() = 0;
};

class Button : public IButton {
public:
	// TODO: Add disabled state.
	enum class State : std::size_t {
		IDLE_UP = 0,
		HOVER = 1,
		PRESSED = 2,
		HELD_OUTSIDE = 3,
		IDLE_DOWN = 4,
		HOVER_PRESSED = 5,
		FOCUSED = 6
	};
	Button(const Rectangle<int>& rect, const std::unordered_map<State, Texture>& textures) :
		rect{ rect },
		textures{ textures } {
	}
	virtual ~Button() = default;
	int index = 0;
	void OnActivate() {
		PrintLine("Button Activated: ", index);
		index++;
		if (state == State::FOCUSED)
			state = State::HOVER;
	}
	void OnMouseMove(const MouseMoveEvent& e) {
		if (state == State::IDLE_UP) {
			state = State::HOVER;
		}
	}
	void OnMouseMoveOutside(const MouseMoveEvent& e) {}
	void OnMouseEnter(const MouseMoveEvent& e) {
		if (state == State::IDLE_UP) {
			state = State::HOVER;
		} else if (state == State::IDLE_DOWN) {
			state = State::HOVER_PRESSED;
		} else if (state == State::HELD_OUTSIDE) {
			state = State::PRESSED;
		}
	}
	void OnMouseLeave(const MouseMoveEvent& e) {
		if (state == State::HOVER) {
			state = State::IDLE_UP;
		} else if (state == State::PRESSED) {
			state = State::HELD_OUTSIDE;
		} else if (state == State::HOVER_PRESSED) {
			state = State::IDLE_DOWN;
		}
	}
	void OnMouseDown(const MouseDownEvent& e) {
		if (e.mouse == Mouse::LEFT)
			if (state == State::HOVER) {
				state = State::PRESSED;
			}
	}
	void OnMouseDownOutside(const MouseDownEvent& e) {
		if (e.mouse == Mouse::LEFT)
			if (state == State::IDLE_UP) {
				state = State::IDLE_DOWN;
			}
	}
	void OnMouseUp(const MouseUpEvent& e) {
		if (e.mouse == Mouse::LEFT)
			if (state == State::PRESSED) {
				state = State::FOCUSED;
				OnActivate();
			} else if (state == State::HOVER_PRESSED) {
				state = State::HOVER;
			} else if (state == State::FOCUSED) {
				state = State::HOVER;
			}
	}
	void OnMouseUpOutside(const MouseUpEvent& e) {
		if (e.mouse == Mouse::LEFT) {
			if (state == State::IDLE_DOWN) {
				state = State::IDLE_UP;
			} else if (state == State::HELD_OUTSIDE) {
				state = State::IDLE_UP;
			} 
			/*else if (state == State::FOCUSED) { // TODO: Set button to idle if user left clicks outside it.
				state = State::IDLE_UP;
			}*/
		}
	}
	void OnMousePressed(const MousePressedEvent& e) {}
	void OnMousePressedOutside(const MousePressedEvent& e) {}
	void Draw() {
		auto it = textures.find(state);
		if (it != textures.end()) {
			auto& texture = it->second;
			if (texture.IsValid())
				texture.Draw(rect);
		} else {
			auto idle_it = textures.find(State::IDLE_UP);
			assert(idle_it != textures.end() && "Idle up state button texture must be added");
			assert(idle_it->second.IsValid() && "Idle up state button texture must be valid");
			idle_it->second.Draw(rect);
		}
	}
	void OnMouseEvent(const Event<MouseEvent>& event) {
		switch (event.Type()) {
			case MouseEvent::MOVE:
			{
				const MouseMoveEvent& e = static_cast<const MouseMoveEvent&>(event);
				if (overlap::PointRectangle(e.current, rect))
					OnMouseMove(e);
				else
					OnMouseMoveOutside(e);
				if (!overlap::PointRectangle(e.previous, rect) && overlap::PointRectangle(e.current, rect))
					OnMouseEnter(e);
				if (overlap::PointRectangle(e.previous, rect) && !overlap::PointRectangle(e.current, rect))
					OnMouseLeave(e);
				break;
			}
			case MouseEvent::PRESSED:
			{
				const MousePressedEvent& e = static_cast<const MousePressedEvent&>(event);
				if (overlap::PointRectangle(e.current, rect))
					OnMousePressed(e);
				else
					OnMousePressedOutside(e);
				break;
			}
			case MouseEvent::DOWN:
			{
				const MouseDownEvent& e = static_cast<const MouseDownEvent&>(event);
				if (overlap::PointRectangle(e.current, rect))
					OnMouseDown(e);
				else
					OnMouseDownOutside(e);
				break;
			}
			case MouseEvent::UP:
			{
				const MouseUpEvent& e = static_cast<const MouseUpEvent&>(event);
				if (overlap::PointRectangle(e.current, rect))
					OnMouseUp(e);
				else
					OnMouseUpOutside(e);
				break;
			}
			default:
				break;
		}
	}
	std::unordered_map<State, Texture> textures;
	Rectangle<int> rect;
	State state{ State::IDLE_UP };
};

class Sandbox : public Engine {
	Dispatcher<MouseEvent> mouse_event;
	/*Button button{ Rectangle<int>{ { 60, 60 }, { 300, 300 } }, {
		{ Button::State::IDLE_UP,       Texture{ "resources/ui/q0.png" } },
		{ Button::State::HOVER,         Texture{ "resources/ui/q1.png" } },
		{ Button::State::PRESSED,       Texture{ "resources/ui/q2.png" } },
		{ Button::State::HELD_OUTSIDE,  Texture{ "resources/ui/q3.png" } },
		{ Button::State::IDLE_DOWN,     Texture{ "resources/ui/q4.png" } },
		{ Button::State::HOVER_PRESSED, Texture{ "resources/ui/q5.png" } },
		{ Button::State::FOCUSED,       Texture{ "resources/ui/q6.png" } }
	}};*/
	Button button{ Rectangle<int>{ { 60, 60 }, { 300, 300 } }, {
		{ Button::State::IDLE_UP,       Texture{ "resources/ui/idle.png" } },
		{ Button::State::HOVER,         Texture{ "resources/ui/hover.png" } },
		{ Button::State::HOVER_PRESSED, Texture{ "resources/ui/hover.png" } },
		{ Button::State::PRESSED,       Texture{ "resources/ui/pressed.png" } },
		{ Button::State::FOCUSED,       Texture{ "resources/ui/focused.png" } }
	} };
	Button button2{ Rectangle<int>{ { 390, 390 }, { 30, 30 } }, {
		{ Button::State::IDLE_UP,       Texture{ "resources/ui/idle.png" } },
		{ Button::State::HOVER,         Texture{ "resources/ui/hover.png" } },
		{ Button::State::HOVER_PRESSED, Texture{ "resources/ui/hover.png" } },
		{ Button::State::PRESSED,       Texture{ "resources/ui/pressed.png" } },
		{ Button::State::FOCUSED,       Texture{ "resources/ui/focused.png" } }
	} };
	void Create() final {
		mouse_event.Subscribe((void*)&button,  std::bind(&Button::OnMouseEvent, &button, std::placeholders::_1));
		mouse_event.Subscribe((void*)&button2, std::bind(&Button::OnMouseEvent, &button2, std::placeholders::_1));
	}
	void Update(float dt) final {
		static V2_int previous_mouse = input::GetMousePosition();
		V2_int current_mouse = input::GetMousePosition();
		if (current_mouse != previous_mouse)
			mouse_event.Post(MouseMoveEvent{ previous_mouse, current_mouse });
		for (auto mouse : std::array<Mouse, 2>{ Mouse::LEFT, Mouse::RIGHT }) {
			if (input::MouseDown(mouse))
				mouse_event.Post(MouseDownEvent{ mouse, current_mouse });
			else if (input::MousePressed(mouse))
				mouse_event.Post(MousePressedEvent{ mouse, current_mouse });
			else if (input::MouseUp(mouse))
				mouse_event.Post(MouseUpEvent{ mouse, current_mouse });
		}

		button.Draw();
		button2.Draw();
		previous_mouse = current_mouse;
	}
};

int main(int c, char** v) {
	Sandbox game;
	game.Construct("sandbox", { 720, 720 });
	return 0;
}