#include <iostream>
#include "event/Event.h"

using namespace ptgn;


enum WindowEvent {
	QUIT,
	FOCUS
};

struct QuitEvent : public event::Event<QuitEvent> {
};

int main(int c, char** v) {

	event::Dispatcher dispatcher;
	auto listener1 = dispatcher.Subscribe<QuitEvent>([](const event::Event<QuitEvent>& event) {
		std::cout << "WINDOW QUIT SOUND!" << std::endl;
	});
	auto listener2 = dispatcher.Subscribe<QuitEvent>([](const event::Event<QuitEvent>& event) {
		std::cout << "WINDOW QUIT GRAPHIC!" << std::endl;
	});
	QuitEvent window;
	dispatcher.Post(window);
	dispatcher.Unsubscribe<QuitEvent>(listener2);
	//dispatcher.Unsubscribe<QuitEvent>(listener1);
	dispatcher.Post(window);
	std::cin.get();
	return 0;
}