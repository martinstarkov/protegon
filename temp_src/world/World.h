//#pragma once
//
//#include "procedural/ChunkManager.h"
//
//namespace engine {
//
//class BaseWorld {
//public:
//	virtual void Update() = 0;
//	virtual void Clear() = 0;
//	virtual void Render() = 0;
//	virtual void Reset() = 0;
//	virtual void SetPlayer(const ecs::Entity& new_player) = 0;
//	virtual ecs::Entity& GetPlayer() = 0;
//	virtual ecs::Manager& GetManager() = 0;
//	virtual Chunk* MakeChunk() { return nullptr; }
//};
//
//template <typename T>
//class World : public BaseWorld {
//public:
//	World() = default;
//	virtual ecs::Entity& GetPlayer() override final {
//		return player;
//	}
//	virtual void SetPlayer(const ecs::Entity& new_player) override final {
//		player = new_player;
//	}
//	virtual ecs::Manager& GetManager() override final {
//		return manager;
//	}
//protected:
//	ecs::Manager manager;
//	ecs::Entity player;
//};
//
//} // namespace engine