//#pragma once
//
//#include <string>
//
//#include "ecs/ECS.h"
//
//#include "inventory/InventoryComponent.h"
//#include "renderer/TextureManager.h"
//#include "renderer/FontManager.h"
//
//class InventorySystem : public ecs::System<InventoryComponent> {
//public:
//	virtual void Update() override final {
//		V2_int slot_size{ 64, 64 };
//		for (auto [entity, inventory] : entities) {
//			auto count{ 0 };
//			engine::TextureManager::DrawSolidRectangle(V2_int{ 20, 20 }, V2_int{ 2 * 10 + slot_size.x * inventory.tiles.size() + 10 * (inventory.tiles.size() - 1), 2 * 10 + slot_size.y }, { 0, 0, 0, 50 });
//			for (auto it{ inventory.tiles.begin() }; it != inventory.tiles.end(); ++it) {
//				V2_int pos{ 30 + count * (10 + slot_size.x), 30 };
//				engine::TextureManager::DrawRectangle(
//					it->second.texture_key,
//					V2_int{ 0, 0 },
//					V2_int{ 16, 16 },
//					pos,
//					slot_size
//				);
//				std::string num{ std::to_string(it->second.count) };
//				engine::FontManager::Load(num.c_str(), colors::CYAN, 24, "resources/fonts/oswald_regular.ttf");
//				engine::FontManager::Draw(num.c_str(), pos + V2_int{ 10, 10 }, slot_size - V2_int{ 20, 20 });
//				engine::FontManager::Unload(num.c_str());
//				++count;
//			}
//		}
//	}
//};