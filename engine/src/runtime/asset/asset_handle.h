#pragma once

#include <cstdint>

#include "ecs/ecs.h"

namespace ptgn {

class AssetManager;

namespace impl {

struct RefCount {
	std::uint32_t value{ 0 };
};

struct PersistentTag {};

class Asset {
public:
	Asset() = default;

	explicit Asset(ecs::Entity entity);

	Asset(const Asset& other);

	Asset(Asset&& other) noexcept;

	Asset& operator=(const Asset& other);

	Asset& operator=(Asset&& other) noexcept;

	~Asset();

protected:
	friend class AssetManager;

	ecs::Entity entity_;

private:
	void AddRef();

	void Release();

	bool Valid() const;
};

} // namespace impl

} // namespace ptgn