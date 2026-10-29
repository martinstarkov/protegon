#pragma once

#include <ecs/ecs.h>

#include "serialization/json/archiver.h"

namespace ptgn {

class Scene;

namespace impl {

class RenderData;

} // namespace impl

class Manager : public ecs::impl::BaseManager<JsonArchiver> {
private:
	using ManagerBase = ecs::impl::BaseManager<JsonArchiver>;

public:
	Manager()							   = default;
	Manager(const Manager&)				   = default;
	Manager& operator=(const Manager&)	   = default;
	Manager(Manager&&) noexcept			   = default;
	Manager& operator=(Manager&&) noexcept = default;
	~Manager() override					   = default;

	friend bool operator==(const Manager& a, const Manager& b) {
		return &a == &b;
	}

	template <typename T>
	void RegisterType() {
		ManagerBase::GetOrAddPool<T>(ManagerBase::GetId<T>());
	}

private:
	friend class Entity;
	friend class Scene;
	friend class impl::RenderData;

	void ClearEntities() final;

	explicit Manager(ManagerBase&& manager);
};

} // namespace ptgn