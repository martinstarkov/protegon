#include <iostream>
#include "event/Observer.h"

using namespace ptgn;

struct QuitEvent : public event::Event<QuitEvent> {

};
struct CollisionEvent : public event::Event<CollisionEvent> {
public:
	CollisionEvent(int i) : i{ i } {}
	int i;
};

int main(int c, char** v) {

	event::Dispatcher dispatcher;
	/*auto listener1 = dispatcher.Subscribe([](QuitEvent& event) {
		std::cout << "WINDOW QUIT SOUND!" << std::endl;
	});
	auto listener2 = dispatcher.Subscribe([](const QuitEvent& event) {
		std::cout << "WINDOW QUIT GRAPHIC!" << std::endl;
	});*/
	auto listener3 = dispatcher.Subscribe([](CollisionEvent& event) {

		// Equivalent to state enter, listens to things and reacts, does not handle state transitions.
		std::cout << "COLLISION1 OCCURED! -> " << event.i << std::endl;
	});
	auto listener4 = dispatcher.Subscribe([](CollisionEvent event) {
		std::cout << "COLLISION2 OCCURED! -> " << event.i << std::endl;
	});
	/*auto listener5 = dispatcher.Subscribe([](QuitEvent& event) {
		std::cout << "COLLISION3 OCCURED!" << std::endl;
	});*/
	//QuitEvent window;
	//dispatcher.Post(window);
	//listener4.Unsubscribe<CollisionEvent>();
	//dispatcher.Post(window);

	// If statement here that checks for a condition and that the previous state is a specific thing
	dispatcher.Post(CollisionEvent{ 1 });



	//listener4.Post(CollisionEvent{ 2 });
	//listener5.Post(CollisionEvent{ 3 });
	std::cin.get();
	return 0;
}