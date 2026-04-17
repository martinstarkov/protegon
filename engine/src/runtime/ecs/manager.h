#pragma once

#include <ecs/ecs.h>
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"

namespace ptgn {

class Scene;

namespace impl {

class RenderData;

} // namespace impl

class Manager : public ecs::impl::Manager<JsonArchiver> {
private:
	using ManagerBase = ecs::impl::Manager<JsonArchiver>;

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

	friend void to_json(json& j, const Manager& manager);
	friend void from_json(const json& j, Manager& manager);

private:
	friend class Entity;
	friend class Scene;
	friend class impl::RenderData;

	void ClearEntities() final;

	explicit Manager(ManagerBase&& manager);
};

} // namespace ptgn

namespace ecs::impl {

void to_json(ptgn::json& j, const DynamicBitset& bitset);

void from_json(const ptgn::json& j, DynamicBitset& bitset);

} // namespace ecs::impl