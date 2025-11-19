#pragma once

#include "ecs/entity.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/util/move_direction.h"
#include "math/vector2.h"
#include "physics/collider.h"
#include "serialization/json/fwd.h"

namespace ptgn {

enum class ScriptType {
	Base,
	Draw,
	Window,
	GameSize,
	DisplaySize,
	Key,
	GlobalMouse,
	Mouse,
	Drag,
	Dropzone,
	Animation,
	PlayerMove,
	Overlap,
	Collision,
	Button,
	Tween
};

namespace impl {

class IScript {
public:
	Entity entity;

	virtual ~IScript() = default;

	// Called when the script is created, after entity is populated.
	virtual void OnCreate() { /* user implementation */ }

	virtual void OnUpdate() { /* user implementation */ }

	// TODO: Consider implementing?
	// virtual void OnFixedUpdate([[maybe_unused]] float fixed_dt) {} // Called at fixed intervals
	// (physics).

	// Serialization (do not override these, as this is handled automatically by the
	// ScriptRegistry).
	virtual json Serialize() const			= 0;
	virtual void Deserialize(const json& j) = 0;

	constexpr virtual bool HasScriptType(ScriptType type) const = 0;

	constexpr virtual std::size_t GetHash() const = 0;

	constexpr static ScriptType GetScriptType() {
		return ScriptType::Base;
	}
};

template <ScriptType type>
struct BaseScript {
public:
	constexpr static ScriptType GetScriptType() {
		return type;
	}
};

} // namespace impl

template <typename TDerived, typename... TScripts>
class Script;

struct DrawScript : public impl::BaseScript<ScriptType::Draw> {
	virtual ~DrawScript() = default;

	// Called when entity is shown.
	virtual void OnShow() { /* user implementation */ }

	// Called when entity is hidden.
	virtual void OnHide() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct GameSizeScript : public impl::BaseScript<ScriptType::GameSize> {
	virtual ~GameSizeScript() = default;

	virtual void OnGameSizeChanged() { /* user implementation */ }
};

struct DisplaySizeScript : public impl::BaseScript<ScriptType::DisplaySize> {
	virtual ~DisplaySizeScript() = default;

	virtual void OnDisplaySizeChanged() { /* user implementation */ }
};

struct WindowScript : public impl::BaseScript<ScriptType::Window> {
	virtual ~WindowScript() = default;

	virtual void OnWindowResized() { /* user implementation */ }

	virtual void OnWindowMoved() { /* user implementation */ }

	virtual void OnWindowMaximized() { /* user implementation */ }

	virtual void OnWindowMinimized() { /* user implementation */ }

	virtual void OnWindowFocusLost() { /* user implementation */ }

	virtual void OnWindowFocusGained() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct KeyScript : public impl::BaseScript<ScriptType::Key> {
	virtual ~KeyScript() = default;

	virtual void OnKeyDown([[maybe_unused]] Key key) { /* user implementation */ }

	virtual void OnKeyPressed([[maybe_unused]] Key key) { /* user implementation */ }

	virtual void OnKeyUp([[maybe_unused]] Key key) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct GlobalMouseScript : public impl::BaseScript<ScriptType::GlobalMouse> {
	virtual ~GlobalMouseScript() = default;

	virtual void OnMouseMove() { /* user implementation */ }

	virtual void OnMouseDown([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMousePressed([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseUp([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseScroll([[maybe_unused]] V2_int scroll_amount) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct MouseScript : public impl::BaseScript<ScriptType::Mouse> {
	virtual ~MouseScript() = default;

	virtual void OnMouseEnter() { /* user implementation */ }

	virtual void OnMouseLeave() { /* user implementation */ }

	virtual void OnMouseMoveOut() { /* user implementation */ }

	virtual void OnMouseMoveOver() { /* user implementation */ }

	virtual void OnMouseDownOver([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseDownOut([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMousePressedOver([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMousePressedOut([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseUpOver([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseUpOut([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseScrollOver([[maybe_unused]] V2_int scroll_amount
	) { /* user implementation */ }

	virtual void OnMouseScrollOut([[maybe_unused]] V2_int scroll_amount) { /* user implementation */
	}

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct DragScript : public impl::BaseScript<ScriptType::Drag> {
	virtual ~DragScript() = default;

	// Triggered when the user start holding left click over a draggable interactive object.
	virtual void OnDragStart([[maybe_unused]] V2_int start_position) { /* user implementation */ }

	// Triggered when the user lets go of left click while dragging a draggable interactive object.
	virtual void OnDragStop([[maybe_unused]] V2_int stop_position) { /* user implementation */ }

	// Triggered every frame while the user is holding left click over a draggable interactive
	// object.
	virtual void OnDrag() { /* user implementation */ }

	virtual void OnDragEnter([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragLeave([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragOver([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragOut([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	// Triggered when the user lets go of (by releasing left click) a draggable interactive object
	// while it overlaps with a dropzone interactive object.
	virtual void OnDrop([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	// Triggered when the user picks up (by pressing left click) a draggable interactive object
	// while it overlaps with a dropzone interactive object.
	virtual void OnPickup([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct DropzoneScript : public impl::BaseScript<ScriptType::Dropzone> {
	virtual ~DropzoneScript() = default;

	virtual void OnDraggableEnter([[maybe_unused]] Entity draggable) { /* user implementation */ }

	virtual void OnDraggableLeave([[maybe_unused]] Entity draggable) { /* user implementation */ }

	virtual void OnDraggableOver([[maybe_unused]] Entity draggable) { /* user implementation */ }

	virtual void OnDraggableOut([[maybe_unused]] Entity draggable) { /* user implementation */ }

	virtual void OnDraggableDrop([[maybe_unused]] Entity draggable) { /* user implementation */ }

	virtual void OnDraggablePickup([[maybe_unused]] Entity draggable) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct AnimationScript : public impl::BaseScript<ScriptType::Animation> {
	virtual ~AnimationScript() = default;

	virtual void OnAnimationStart() { /* user implementation */ }

	virtual void OnAnimationUpdate() { /* user implementation */ }

	// Called for each repeat of the full animation.
	virtual void OnAnimationRepeat() { /* user implementation */ }

	// Called when the frame of the animation changes
	virtual void OnAnimationFrameChange() { /* user implementation */ }

	// Called once when the animation goes through its first full cycle.
	virtual void OnAnimationComplete() { /* user implementation */ }

	virtual void OnAnimationPause() { /* user implementation */ }

	virtual void OnAnimationResume() { /* user implementation */ }

	virtual void OnAnimationStop() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct PlayerMoveScript : public impl::BaseScript<ScriptType::PlayerMove> {
	virtual ~PlayerMoveScript() = default;

	// Called on the first frame of player movement.
	virtual void OnMoveStart() { /* user implementation */ }

	// Called every frame that the player is moving.
	virtual void OnMove() { /* user implementation */ }

	// Called on the first frame of player stopping their movement.
	virtual void OnMoveStop() { /* user implementation */ }

	// Called when the movement direction changes. Passed parameter is the difference in direction.
	// If not moving, this is simply the new direction. If moving already, this is the newly added
	// component of movement. To get the current direction instead, simply use GetDirection().
	virtual void OnDirectionChange([[maybe_unused]] MoveDirection direction_difference
	) { /* user implementation */ }

	virtual void OnMoveUpStart() { /* user implementation */ }

	virtual void OnMoveUp() { /* user implementation */ }

	virtual void OnMoveUpStop() { /* user implementation */ }

	virtual void OnMoveDownStart() { /* user implementation */ }

	virtual void OnMoveDown() { /* user implementation */ }

	virtual void OnMoveDownStop() { /* user implementation */ }

	virtual void OnMoveLeftStart() { /* user implementation */ }

	virtual void OnMoveLeft() { /* user implementation */ }

	virtual void OnMoveLeftStop() { /* user implementation */ }

	virtual void OnMoveRightStart() { /* user implementation */ }

	virtual void OnMoveRight() { /* user implementation */ }

	virtual void OnMoveRightStop() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct OverlapScript : public impl::BaseScript<ScriptType::Overlap> {
	virtual ~OverlapScript() = default;

	// Must return true for overlap to be checked.
	// Defaults to true.
	// Note: Modifying the state of either entity in this function may lead to undefined behavior.
	virtual bool PreOverlapCheck([[maybe_unused]] const Entity& other) const {
		return true;
	}

	virtual void OnOverlapStart([[maybe_unused]] Entity other) { /* user implementation */ }

	virtual void OnOverlap([[maybe_unused]] Entity other) { /* user implementation */ }

	virtual void OnOverlapStop([[maybe_unused]] Entity other) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct CollisionScript : public impl::BaseScript<ScriptType::Collision> {
	virtual ~CollisionScript() = default;

	virtual void OnCollision([[maybe_unused]] Collision collision) { /* user implementation */ }

	// Must return true for collision to be checked.
	// Defaults to true.
	// Note: Modifying the state of either entity in this function may lead to undefined behavior.
	virtual bool PreCollisionCheck([[maybe_unused]] const Entity& other) const {
		return true;
	}

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct ButtonScript : public impl::BaseScript<ScriptType::Button> {
	virtual ~ButtonScript() = default;

	virtual void OnButtonHoverStart() { /* user implementation */ }

	virtual void OnButtonHover() { /* user implementation */ }

	virtual void OnButtonHoverStop() { /* user implementation */ }

	virtual void OnButtonActivate() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct TweenScript : public impl::BaseScript<ScriptType::Tween> {
	// TODO: Consider adding access to tween:
	// Tween tween;

	virtual ~TweenScript() = default;

	// Tween has started.
	virtual void OnStart() { /* user implementation */ }

	// Entire tween has completed.
	virtual void OnComplete() { /* user implementation */ }

	// Tween point has started.
	virtual void OnPointStart() { /* user implementation */ }

	// Tween point has completed.
	virtual void OnPointComplete() { /* user implementation */ }

	virtual void OnRepeat() { /* user implementation */ }

	virtual void OnYoyo() { /* user implementation */ }

	virtual void OnStop() { /* user implementation */ }

	virtual void OnProgress([[maybe_unused]] float progress) { /* user implementation */ }

	virtual void OnPause() { /* user implementation */ }

	virtual void OnResume() { /* user implementation */ }

	virtual void OnReset() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

} // namespace ptgn