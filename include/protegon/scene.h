#pragma once

namespace ptgn {

class Scene {
public:
	virtual ~Scene() = default;

	virtual void Update([[maybe_unused]] float dt) {}

	virtual void Update() {}

	/*template <typename T>
	std::shared_ptr<T> Cast() {
		return std::static_pointer_cast<T>(this);
	}*/
private:
	friend class SceneManager;
	enum class Status : std::size_t {
		Idle,
		Delete
	};
	Status status_{ Status::Idle };
};

} // namespace ptgn