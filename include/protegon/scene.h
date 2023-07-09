#pragma once

#include "manager.h"
#include "type_traits.h"

namespace ptgn {

class Scene {
public:
	virtual ~Scene() = default;
	virtual void Update(float dt) {}
	/*template <typename T>
	std::shared_ptr<T> Cast() {
		return std::static_pointer_cast<T>(this);
	}*/
private:
	friend class SceneManager;
	enum class Status : std::size_t {
		IDLE,
		DELETE
	};
	Status status_{ Status::IDLE };
};

class SceneManager : public ResourceManager<Scene> {
public:
	void Unload(std::size_t scene_key);
	void SetActive(std::size_t scene_key);
	void AddActive(std::size_t scene_key);
	void RemoveActive(std::size_t scene_key);
	std::vector<std::shared_ptr<Scene>> GetActive();
	void Update(float dt);
private:
	void UnloadFlagged();
	/*void ExitAllExcept(std::size_t scene_key) {
		for (auto other_key : active_scenes_) {
			if (other_key != scene_key && Has(other_key)) {
				auto scene = Get(other_key);
				scene->Exit();
			}
		}
	}*/
	bool ActiveScenesContain(std::size_t key) const;
	std::size_t flagged_{ 0 };
	std::vector<std::size_t> active_scenes_;
};

} // namespace ptgn