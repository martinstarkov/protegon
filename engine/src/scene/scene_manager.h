#pragma once

#include <concepts>
#include <memory>
#include <vector>

#include "core/app/context.h"
#include "core/assert.h"
#include "core/util/time.h"
#include "math/hash.h"
#include "scene/scene.h"

namespace ptgn {

class Application;

// namespace impl {
//
// template <typename T>
// concept SceneType = IsOrDerivedFrom<T, Scene>;
//
// } // namespace impl

enum class Phase : std::uint8_t {
	Entering,
	Running,
	Exiting,
	Paused,
	Dead
};

struct SceneEntry {
	std::unique_ptr<Scene> ptr;
	Phase phase{ Phase::Entering };
	int z{ 0 };					// Draw order (higher = on top)
	bool blocks_input{ false }; // Modal
	bool updates{ true };		// Can manager flip this? Yes, during transitions
	bool renders{ true };		// Ditto
	std::size_t id{ 0 };		// Stable handle
	std::size_t key{ 0 };
};

struct TransitionContext {
	Scene& from;	 // Can be same as `to` if overlay-in
	Scene& to;
	float t{ 0.0f }; // 0..1 progress
	secondsf dt{ 0.0f };
};

class Transition {
public:
	virtual ~Transition() = default;

	// Runs the animation; returns true when finished.
	virtual bool Step(TransitionContext&) = 0;

	// Policies during the transition:
	virtual bool UpdatesFrom() const {
		return false;
	} // Does FROM update?

	virtual bool UpdatesTo() const {
		return true;
	} // Does TO update?

	virtual bool BlocksInput() const {
		return true;
	} // Block input below?

	virtual bool RendersBoth() const {
		return true;
	} // Draw FROM + TO?

	virtual bool Exclusive() const {
		return true;
	} // Exclusive transition
};

class SlideLeft : public Transition {
public:
	explicit SlideLeft(secondsf duration) : duration_(duration) {}

	bool Step(TransitionContext& context) override {
		accumulated_ += context.dt;
		context.t	  = std::min(1.0f, accumulated_ / duration_);
		PTGN_LOG("Sliding left: ", context.t);
		// Render: translate FROM by (-width * t), TO by (width * (1 - t))
		return context.t >= 1.0;
	}

	bool UpdatesFrom() const override {
		return false;
	}

	bool UpdatesTo() const override {
		return true;
	}

private:
	secondsf duration_{ 0.0f };
	secondsf accumulated_{ 0.0f };
};

enum class OperationKind {
	Switch,
	Push,
	Pop,
	Replace,
	Overlay
};

struct Operation {
	OperationKind kind{ OperationKind::Switch };
	std::function<std::unique_ptr<Scene>()> make_to; // TO scene factory
	std::unique_ptr<Transition> transition;			 // May be null (instant)
	std::size_t from_id{ 0 };						 // Optional: explicit source
	bool kill_from_on_end{ true };					 // Switch/Replace vs Overlay
	std::size_t key{ 0 };
};

class SceneManager {
public:
	SceneManager()									 = default;
	~SceneManager() noexcept						 = default;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager(SceneManager&&) noexcept			 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;
	SceneManager& operator=(SceneManager&&) noexcept = default;

	// High-level API (always enqueues; processed at frame boundary)
	// TODO: Add concept.
	template <typename TScene, typename... TArgs>
	void SwitchTo(
		std::string_view scene_key, std::unique_ptr<Transition> transition, TArgs&&... args
	) {
		Operation op;
		op.key	   = Hash(scene_key);
		op.kind	   = OperationKind::Switch;
		op.make_to = [this, &args...]() {
			auto scene{ std::make_unique<TScene>(args...) };
			scene->ctx_ = ctx_;
			return scene;
		};
		op.transition = std::move(transition);
		Enqueue(std::move(op));
	}

	// TODO: Add concept.
	template <typename TScene, typename... TArgs>
	void Overlay(
		std::string_view scene_key, std::unique_ptr<Transition> transition, int z, TArgs&&... args
	) {
		Operation op;
		op.key	   = Hash(scene_key);
		op.kind	   = OperationKind::Overlay;
		op.make_to = [this, &args...]() {
			auto scene{ std::make_unique<TScene>(args...) };
			scene->ctx_ = ctx_;
			return scene;
		};
		op.transition = std::move(transition);
		Enqueue(std::move(op));
		pending_overlay_z_ = z;
	}

	void PopTop(std::string_view scene_key, std::unique_ptr<Transition> transition) {
		Operation op;
		op.key		  = Hash(scene_key);
		op.kind		  = OperationKind::Pop;
		op.transition = std::move(transition);
		queue_.push_back(std::move(op));
	}

	void Update(secondsf dt) {
		// TODO: Move this function to private.

		FlushOps();
		StepTransitions(dt);
		UpdateScenes(dt);
		Draw();
	}

	Scene* GetCurrent() {
		PTGN_ASSERT(current_scene_ != nullptr);
		return current_scene_;
	}

	const Scene* GetCurrent() const {
		PTGN_ASSERT(current_scene_ != nullptr);
		return current_scene_;
	}

	bool Has(std::string_view scene_key) const {
		auto key{ Hash(scene_key) };
		return std::any_of(entries_.begin(), entries_.end(), [&](const SceneEntry& e) {
			return e.key == key && e.phase != Phase::Dead;
		});
	}

	Scene* Get(std::string_view scene_key) {
		auto key{ Hash(scene_key) };
		for (auto& e : entries_) {
			if (e.key == key && e.phase != Phase::Dead) {
				return e.ptr.get();
			}
		}
		return nullptr;
	}

	const Scene* Get(std::string_view scene_key) const {
		auto key{ Hash(scene_key) };
		for (auto& e : entries_) {
			if (e.key == key && e.phase != Phase::Dead) {
				return e.ptr.get();
			}
		}
		return nullptr;
	}

private:
	friend class Application;

	void Draw() {
		DrawScenes();
	}

	void SetContext(const std::shared_ptr<ApplicationContext>& ctx) {
		ctx_ = ctx;
	}

	std::shared_ptr<ApplicationContext> ctx_;

	struct TransitionRun {
		std::size_t from_index{ 0 };
		std::size_t to_index{ 0 };
		std::unique_ptr<Transition> transition;
		float progress{ 0.0f };
		bool kill_from_on_end{ true };
	};

	Scene* current_scene_{ nullptr };

	std::vector<SceneEntry> entries_;
	std::vector<Operation> queue_;
	std::vector<Operation> deferred_queue_;
	bool is_flushing_ = false;
	std::vector<TransitionRun> runs_;
	int pending_overlay_z_{ 0 };
	std::size_t next_id_{ 1 };

	void ProcessOperations(std::vector<Operation>& queue) {
		auto is_scene_locked = [&](std::size_t index) -> bool {
			for (auto& run : runs_) {
				if (run.from_index == index || run.to_index == index) {
					return true;
				}
			}
			return false;
		};

		for (auto& op : queue) {
			if (op.kind == OperationKind::Switch || op.kind == OperationKind::Replace) {
				std::size_t from_index = TopRunningIndex();

				// Handle first scene (no from)
				bool has_from = from_index != SIZE_MAX;

				if (has_from && is_scene_locked(from_index)) {
					continue; // still respect locked scenes
				}

				// Create new scene entry
				SceneEntry to;
				to.ptr	 = op.make_to();
				to.phase = has_from ? Phase::Entering
									: Phase::Running; // no entering transition if first scene
				to.z	 = entries_.empty() ? 0 : entries_.back().z + 1;
				to.id	 = next_id_++;
				to.key	 = op.key; // if you added SceneKey support
				entries_.push_back(std::move(to));

				std::size_t to_index = entries_.size() - 1;

				if (has_from && op.transition) {
					// Normal transition path
					runs_.push_back(TransitionRun{ from_index, to_index, std::move(op.transition),
												   0.0, op.kill_from_on_end });
					ApplyPoliciesOnStart(runs_.back());
				} else if (!has_from) {
					// First scene: start immediately
					entries_[to_index].phase = Phase::Running;
					entries_[to_index].ptr->Enter();
				} else {
					// Instant swap
					entries_[from_index].ptr->Exit();
					entries_[from_index].phase = Phase::Dead;
					entries_[to_index].ptr->Enter();
					entries_[to_index].phase = Phase::Running;
				}
			} else if (op.kind == OperationKind::Overlay) {
				SceneEntry to;
				to.ptr	 = op.make_to();
				to.phase = Phase::Entering;
				to.z	 = pending_overlay_z_;
				to.id	 = next_id_++;
				to.key	 = op.key;
				entries_.push_back(std::move(to));
				ResortByZ();
				std::size_t to_index   = IndexById(entries_.back().id);
				std::size_t from_index = (entries_.size() >= 2) ? to_index - 1 : SIZE_MAX;
				if (op.transition && from_index != SIZE_MAX) {
					runs_.push_back(TransitionRun{ from_index, to_index, std::move(op.transition),
												   0.0, false });
					ApplyPoliciesOnStart(runs_.back());
				} else {
					entries_[to_index].phase = Phase::Running;
				}
			} else if (op.kind == OperationKind::Pop) {
				std::size_t from_index = TopIndex();
				if (from_index == SIZE_MAX || is_scene_locked(from_index)) {
					continue;
				}
				std::size_t to_index = (from_index > 0) ? from_index - 1 : from_index;
				if (op.transition && from_index != SIZE_MAX) {
					runs_.push_back(TransitionRun{ from_index, to_index, std::move(op.transition),
												   0.0, true });
					ApplyPoliciesOnStart(runs_.back());
				} else {
					entries_[from_index].phase = Phase::Dead;
					if (to_index != SIZE_MAX) {
						entries_[to_index].phase = Phase::Running;
					}
				}
			}
		}
	}

	void FlushOps() {
		if (queue_.empty()) {
			return;
		}

		is_flushing_ = true;

		while (!queue_.empty()) {
			std::vector<Operation> current = std::move(queue_);
			queue_.clear();

			ProcessOperations(current);
		}

		is_flushing_ = false;

		// Move deferred operations into main queue for next frame
		if (!deferred_queue_.empty()) {
			queue_ = std::move(deferred_queue_);
			deferred_queue_.clear();
		}

		Compact();
	}

	void Enqueue(Operation op) {
		if (is_flushing_) {
			deferred_queue_.push_back(std::move(op));
		} else {
			queue_.push_back(std::move(op));
		}
	}

	void ApplyPoliciesOnStart(TransitionRun& run) {
		auto& from = entries_[run.from_index];
		auto& to   = entries_[run.to_index];
		from.phase = Phase::Exiting;
		from.ptr->Exit();
		to.phase = Phase::Entering;
	}

	void StepTransitions(secondsf dt) {
		std::vector<std::size_t> done;
		for (std::size_t i = 0; i < runs_.size(); ++i) {
			auto& run = runs_[i];
			TransitionContext context{ *entries_[run.from_index].ptr, *entries_[run.to_index].ptr,
									   run.progress, dt };
			bool finished = run.transition->Step(context);
			run.progress  = context.t;
			if (finished) {
				done.push_back(i);
			}
		}

		for (std::size_t i = done.size(); i-- > 0;) {
			auto& run = runs_[done[i]];
			entries_[run.to_index].ptr->Enter();
			entries_[run.to_index].phase = Phase::Running;

			if (run.kill_from_on_end) {
				entries_[run.from_index].ptr->Exit();
				entries_[run.from_index].phase = Phase::Dead;
			} else {
				entries_[run.from_index].phase = Phase::Paused;
			}
			runs_.erase(runs_.begin() + done[i]);
		}
		Compact();
	}

	void UpdateScenes(secondsf dt) {
		bool input_blocked = false;

		current_scene_ = nullptr;

		for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
			auto& entry = entries_[i];
			if (entry.phase == Phase::Dead) {
				continue;
			}

			bool involved	  = IsInvolvedInTransition(i);
			bool allow_update = entry.phase == Phase::Running ||
								(involved && AllowUpdateByPolicy(i)) && entry.updates;

			if (allow_update) {
				current_scene_ = entry.ptr.get();
				entry.ptr->Update();
			}

			if (!input_blocked && IsBlockingInput(i)) {
				input_blocked = true;
			}
		}
	}

	void DrawScenes() {
		current_scene_ = nullptr;

		for (auto& entry : entries_) {
			if (entry.phase == Phase::Dead || !entry.renders) {
				continue;
			}
			current_scene_ = entry.ptr.get();
			// PTGN_LOG("Drawing scene: ", entry.id);
			//  TODO: Draw scene.
			//  entry.ptr->Draw();
		}
	}

	// --- Policy helpers ---
	bool IsInvolvedInTransition(std::size_t index) const {
		for (auto& run : runs_) {
			if (run.from_index == index || run.to_index == index) {
				return true;
			}
		}
		return false;
	}

	bool AllowUpdateByPolicy(std::size_t index) const {
		for (auto& run : runs_) {
			if (run.from_index == index) {
				return run.transition->UpdatesFrom();
			}
			if (run.to_index == index) {
				return run.transition->UpdatesTo();
			}
		}
		return false;
	}

	bool IsBlockingInput(std::size_t index) const {
		for (auto& run : runs_) {
			if (run.from_index == index || run.to_index == index) {
				return run.transition->BlocksInput();
			}
		}
		return entries_[index].blocks_input;
	}

	// --- Bookkeeping ---
	std::size_t TopIndex() const {
		return entries_.empty() ? SIZE_MAX : entries_.size() - 1;
	}

	std::size_t TopRunningIndex() const {
		for (std::size_t i = entries_.size(); i-- > 0;) {
			if (entries_[i].phase == Phase::Running) {
				return i;
			}
		}
		return SIZE_MAX;
	}

	std::size_t IndexById(std::size_t id) const {
		for (std::size_t i = 0; i < entries_.size(); ++i) {
			if (entries_[i].id == id) {
				return i;
			}
		}
		return SIZE_MAX;
	}

	void ResortByZ() {
		std::stable_sort(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) {
			return a.z < b.z;
		});
	}

	void Compact() {
		entries_.erase(
			std::remove_if(
				entries_.begin(), entries_.end(), [](auto& e) { return e.phase == Phase::Dead; }
			),
			entries_.end()
		);
	}
};

/*
class SceneManager {
public:
	SceneManager()									 = default;
	~SceneManager()									 = default;
	SceneManager(SceneManager&&) noexcept			 = delete;
	SceneManager& operator=(SceneManager&&) noexcept = delete;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	template <impl::SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& TryLoad(const SceneKey& scene_key, TArgs&&... constructor_args) {
		scene_index_++;
		auto [inserted, scene] = VectorTryEmplaceIf<TScene>(
			scenes_, [scene_key](const auto& s) { return s->GetKey() == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		scene->SetKey(scene_key);
		return *static_cast<TScene*>(scene.get());
	}

	template <impl::SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Load(const SceneKey& scene_key, TArgs&&... constructor_args) {
		scene_index_++;
		auto [replaced, scene] = VectorReplaceOrEmplaceIf<TScene>(
			scenes_, [scene_key](const auto& s) { return s->GetKey() == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		scene->SetKey(scene_key);
		return *static_cast<TScene*>(scene.get());
	}

	template <impl::SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Enter(const SceneKey& scene_key, TArgs&&... constructor_args) {
		auto& scene{ Load<TScene>(scene_key, std::forward<TArgs>(constructor_args)...) };
		[[maybe_unused]] auto keep_alive{ GetImpl(scene_key) };
		Enter(scene_key);
		return scene;
	}

	// @param from_scene_key If nullopt, will transition from all currently active scenes.
	template <
		impl::SceneType TScene, impl::SceneTransitionType TransitionIn = NoTransition,
		impl::SceneTransitionType TransitionOut = NoTransition, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Transition(
		const std::optional<SceneKey>& from_scene_key, const SceneKey& to_scene_key,
		TransitionIn&& in = {}, TransitionOut&& out = {}, TArgs&&... args
	) {
		auto& scene{ Load<TScene>(to_scene_key, std::forward<TArgs>(args)...) };

		if constexpr (!std::same_as<std::remove_cvref_t<TransitionOut>, NoTransition>) {
			auto set_out = [&](auto& scene_from) {
				auto t = std::make_shared<std::remove_cvref_t<TransitionOut>>(
					std::forward<TransitionOut>(out)
				);
				t->scene				= scene_from.get();
				scene_from->transition_ = std::move(t);
			};
			if (from_scene_key) {
				set_out(GetImpl(*from_scene_key));
			} else {
				for (auto& s : active_scenes_) {
					set_out(s);
				}
			}
		}

		if constexpr (!std::same_as<std::remove_cvref_t<TransitionIn>, NoTransition>) {
			auto scene_to = GetImpl(to_scene_key);
			auto t =
				std::make_shared<std::remove_cvref_t<TransitionIn>>(std::forward<TransitionIn>(in));
			t->scene			  = scene_to.get();
			scene_to->transition_ = std::move(t);
		}

		if (from_scene_key) {
			Exit(*from_scene_key);
		} else {
			ExitAll();
		}
		Enter(to_scene_key);
		return scene;
	}

	void Enter(const SceneKey& scene_key);

	void EnterConfig(const path& scene_json_file);

	void Unload(const SceneKey& scene_key);

	void Exit(const SceneKey& scene_key);

	[[nodiscard]] std::shared_ptr<Scene> Get(std::string_view scene_key) {
		auto p = GetImpl(key);
		PTGN_ASSERT(p, "Cannot retrieve scene which does not exist in the scene manager");
		return p;
	}

	[[nodiscard]] std::shared_ptr<const Scene> Get(std::string_view scene_key) const {
		auto p = GetImpl(key);
		PTGN_ASSERT(p, "Cannot retrieve scene which does not exist in the scene manager");
		return std::static_pointer_cast<const Scene>(p);
	}

	template <impl::SceneType TScene>
	[[nodiscard]] std::shared_ptr<TScene> Get(std::string_view scene_key) {
		auto base = GetImpl(key);
		PTGN_ASSERT(base, "Cannot retrieve scene which does not exist in the scene manager");

		if constexpr (std::is_same_v<std::remove_cv_t<TScene>, Scene>) {
			return std::static_pointer_cast<Scene>(base);
		} else {
			auto d = std::dynamic_pointer_cast<TScene>(base);
			PTGN_ASSERT(d, "Requested scene type does not match stored type for key");
			return d;
		}
	}

	template <impl::SceneType TScene>
	[[nodiscard]] std::shared_ptr<const TScene> Get(std::string_view scene_key) const {
		auto base = GetImpl(key);
		PTGN_ASSERT(base, "Cannot retrieve scene which does not exist in the scene manager");

		if constexpr (std::is_same_v<std::remove_cv_t<TScene>, Scene>) {
			return std::static_pointer_cast<const Scene>(base);
		} else {
			auto d = std::dynamic_pointer_cast<const TScene>(base);
			PTGN_ASSERT(d, "Requested scene type does not match stored type for key");
			return d;
		}
	}

	[[nodiscard]] std::shared_ptr<const Scene> GetCurrent() const;
	[[nodiscard]] std::shared_ptr<Scene> GetCurrent();

	[[nodiscard]] bool Has(const SceneKey& scene_key) const;

	[[nodiscard]] bool IsActive(const SceneKey& scene_key) const;

	// Move a scene up in the scene list.
	void MoveUp(const SceneKey& scene_key);

	// Move a scene down in the scene list.
	void MoveDown(const SceneKey& scene_key);

	// Bring a scene to the top of the scene list.
	void BringToTop(const SceneKey& scene_key);

	// Move a scene to the bottom of the scene list.
	void MoveToBottom(const SceneKey& scene_key);

	// Move a scene above another scene in the scene list.
	void MoveAbove(const SceneKey& source_key, const SceneKey& target_key);

	// Move a scene below another scene in the scene list.
	void MoveBelow(const SceneKey& source_key, const SceneKey& target_key);

private:
	friend class Application;

	void ExitAll();

	std::shared_ptr<Scene> GetImpl(const SceneKey& scene_key) const;

	// Updates all the active scenes.
	void Update();

	// Clears the frame buffers of each scene.
	// void ClearSceneTargets();

	void Reset();
	void Shutdown();

	void HandleSceneEvents();

	std::vector<std::shared_ptr<Scene>> active_scenes_;
	std::vector<std::shared_ptr<Scene>> scenes_;
	std::shared_ptr<Scene> current_;

	std::size_t scene_index_{ 0 };
};
*/

} // namespace ptgn