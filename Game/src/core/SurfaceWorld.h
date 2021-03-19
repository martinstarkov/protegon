#pragma once

#include <engine/Include.h>

class SurfaceWorld : public engine::World<SurfaceWorld> {
public:
	SurfaceWorld() : 
		chunk_manager_{ { 32, 32 }, { 16, 16 } } {}
	virtual engine::Chunk* MakeChunk() override final;
	virtual void Update() override final;
	virtual void Clear() override final {
		chunk_manager_.Clear();
		manager.DestroyEntities();
		manager.Refresh();
	}
	virtual void Render() override final {
		chunk_manager_.Render();
	}
	virtual void Reset() override final {
		chunk_manager_.Reset();
	}
private:
	engine::ChunkManager chunk_manager_;
};