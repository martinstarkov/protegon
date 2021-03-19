#include "SurfaceWorld.h"

#include "procedural/chunks/BoxChunk.h"

engine::Chunk* SurfaceWorld::MakeChunk() {
	return new BoxChunk();
}

void SurfaceWorld::Update() {
	chunk_manager_.Update();
}