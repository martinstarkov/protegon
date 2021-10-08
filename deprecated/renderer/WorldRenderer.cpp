#include "WorldRenderer.h"

#include "core/SceneManager.h"
#include "math/Math.h"
#include "renderer/text/Text.h"

namespace ptgn {

V2_int WorldRenderer::WorldToScreen(const V2_double& world_coordinate) {
	return WorldToScreen(world_coordinate, SceneManager::GetActiveCamera());
}

V2_int WorldRenderer::ScreenToWorld(const V2_double& screen_coordinate) {
	return ScreenToWorld(screen_coordinate, SceneManager::GetActiveCamera());
}

V2_int WorldRenderer::Scale(const V2_double& size) {
	return Scale(size, SceneManager::GetActiveCamera());
}

int WorldRenderer::ScaleX(double value) {
	return ScaleX(value, SceneManager::GetActiveCamera());
}

int WorldRenderer::ScaleY(double value) {
	return ScaleY(value, SceneManager::GetActiveCamera());
}

V2_int WorldRenderer::WorldToScreen(const V2_double& world_coordinate, const Camera& active_camera) {
	return math::Ceil((world_coordinate - active_camera.position) * active_camera.scale);
}

V2_int WorldRenderer::ScreenToWorld(const V2_double& screen_coordinate, const Camera& active_camera) {
	return math::Ceil(screen_coordinate / active_camera.scale + active_camera.position);
}

V2_int WorldRenderer::Scale(const V2_double& size, const Camera& active_camera) {
	return math::Ceil(size * active_camera.scale);
}

int WorldRenderer::ScaleX(double value, const Camera& active_camera) {
	return math::Ceil(value * active_camera.scale.x);
}

int WorldRenderer::ScaleY(double value, const Camera& active_camera) {
	return math::Ceil(value * active_camera.scale.y);
}

void WorldRenderer::DrawTexture(const Texture& texture, 
								const V2_double& position, 
								const V2_double& size, 
								const V2_int source_position, 
								const V2_int source_size) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawTexture(texture, WorldToScreen(position, active_camera),
								Scale(size, active_camera), source_position, source_size);
}

void WorldRenderer::DrawTexture(const char* texture_key,
								const V2_double& position,
								const V2_double& size,
								const V2_int source_position,
								const V2_int source_size) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawTexture(texture_key, WorldToScreen(position, active_camera), 
								Scale(size, active_camera), source_position, source_size, 
								nullptr, 0.0, Flip::NONE);
}

void WorldRenderer::DrawTexture(const char* texture_key,
								const V2_double& position, 
								const V2_double& size,
								const V2_int source_position, 
								const V2_int source_size,
								const V2_int* center_of_rotation, 
								const double angle,
								Flip flip) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawTexture(texture_key, WorldToScreen(position), Scale(size), 
								source_position, source_size, nullptr, 0.0, Flip::NONE);
}

void WorldRenderer::DrawText(const Text& text,
							 const V2_double& position,
							 const V2_double& size) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawText(text, WorldToScreen(position, active_camera),
							 Scale(size, active_camera));
}

void WorldRenderer::DrawPoint(const V2_double& point, const Color& color) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawPoint(WorldToScreen(point, active_camera), color);
}

void WorldRenderer::DrawLine(const V2_double& origin,
							 const V2_double& destination, 
							 const Color& color) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawLine(WorldToScreen(origin, active_camera),
							 WorldToScreen(destination, active_camera), color);
}

void WorldRenderer::DrawCircle(const V2_double& center,
							   const double radius, 
							   const Color& color) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawCircle(WorldToScreen(center, active_camera),
							   ScaleX(radius, active_camera), color);
}

void WorldRenderer::DrawSolidCircle(const V2_double& center,
									const double radius,
									const Color& color) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawSolidCircle(WorldToScreen(center, active_camera),
									ScaleX(radius, active_camera), color);
}

void WorldRenderer::DrawRectangle(const V2_double& position,
								  const V2_double& size,
								  const Color& color) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawRectangle(WorldToScreen(position, active_camera),
								  Scale(size, active_camera), color);
}

void WorldRenderer::DrawSolidRectangle(const V2_double& position,
									   const V2_double& size,
									   const Color& color) {
	const auto& active_camera{ SceneManager::GetActiveCamera() };
	ScreenRenderer::DrawSolidRectangle(WorldToScreen(position, active_camera), 
									   Scale(size, active_camera), color);
}

} // namespace ptgn