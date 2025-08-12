#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "components/interactive.h"
#include "core/entity.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class Camera;
class Button;

struct MouseInfo {
	MouseInfo();

	V2_int position;
	V2_int scroll_delta;

	bool left_pressed{ false };
	bool left_down{ false };
	bool left_up{ false };
};

struct DragState {
	V2_int drag_start_position;
};

class SceneInput {
public:
	bool IsDragging(Entity e) const {
		return dragging_entities.contains(e);
	}

	bool IsAnyDragging() const {
		return !dragging_entities.empty();
	}

	// If set to true, only the interactables in the scene will be triggered, i.e. if there are two
	// button on top of each other, only the top one will be able to be hovered or pressed.
	void SetTopOnly(bool top_only);

	// @return Mouse position relative to the top left of the scene camera.
	[[nodiscard]] V2_float GetMousePosition() const;

	// @return Mouse position during the previous frame relative to the top left of the scene
	// camera.
	[[nodiscard]] V2_float GetMousePositionPrevious() const;

	// @return Mouse position difference between the current and previous frames relative to the top
	// left of the scene camera.
	[[nodiscard]] V2_float GetMouseDifference() const;

	[[nodiscard]] V2_float TransformToCamera(const V2_float& screen_position) const;

	void SetDrawInteractives(bool draw_interactives);

private:
	friend class Scene;
	friend class Button;

	void Update(Scene& scene, const MouseInfo& mouse_state);

	[[nodiscard]] static std::vector<Entity> GetOverlappingDropzones(
		Scene& scene, const Entity& entity, const V2_float& world_pointer
	);

	void Init(std::size_t scene_key);
	void Shutdown();

	static std::vector<Entity> GetEntitiesUnderMouse(Scene& scene, const MouseInfo& mouse_state);
	static std::vector<Entity> GetDropzones(Scene& scene);
	void DispatchMouseEvents(const std::vector<Entity>& over, const MouseInfo& mouse);
	void UpdateMouseOverStates(const std::vector<Entity>& current);
	void HandleDragging(
		const std::vector<Entity>& over, const std::vector<Entity>& dropzones,
		const MouseInfo& mouse
	);
	void HandleDropzones(
		const std::vector<Entity>& mouse_over, const std::vector<Entity>& dropzones,
		const MouseInfo& mouse
	);

	// TODO: Add to serialization.

	std::unordered_map<Entity, DragState> dragging_entities;
	std::unordered_map<Entity, std::unordered_set<Entity>> last_dragged_dropzones;
	std::unordered_set<Entity> last_mouse_over_;
	std::unordered_set<Entity> last_dropzones_;

	std::size_t scene_key_{ 0 };

	bool top_only_{ false };

	bool draw_interactives_{ false };

public:
	// TODO: Potentially move away from serializing the entity vectors.

	PTGN_SERIALIZER_REGISTER_NAMED(
		SceneInput, KeyValue("scene_key", scene_key_), KeyValue("top_only", top_only_),
		KeyValue("draw_interactives", draw_interactives_)
	)
};

} // namespace ptgn