//#include "RandomizeEvent.h"
//
//#include "ecs/Components.h"
//#include "renderer/Color.h"
//
//RandomizeEvent::RandomizeEvent(ecs::Manager& manager) {
//	auto color_entities = manager.GetComponentTuple<RenderComponent>();
//	for (auto [entity2, render_component2] : color_entities) {
//		render_component2.original_color = engine::BLUE;
//		//entity2.AddComponent<SpriteComponent>("./resources/textures/moomin.png", V2_int{ 119, 140 });
//		//LOG("Setting color of " << entity2.GetId() << " to " << render_component2.color);
//	}
//}
//
//Randomize2Event::Randomize2Event(ecs::Manager& manager) {
//	auto color_entities = manager.GetComponentTuple<RenderComponent>();
//	for (auto [entity2, render_component2] : color_entities) {
//		render_component2.original_color = engine::RED;
//		//entity2.AddComponent<SpriteComponent>("./resources/textures/moomin.png", V2_int{ 119, 140 });
//		//LOG("Setting color of " << entity2.GetId() << " to " << render_component2.color);
//	}
//}