#pragma once

#include <cstdint>

#include "runtime/ecs/entity.h"

namespace ptgn {

class AssetManager;

namespace impl {

struct RefCount {
	std::uint32_t value{ 0 };
};

struct PersistentTag {};

/// Reference counted asset CRTP class.
template <typename Derived>
class RefCountedAsset {
public:
	RefCountedAsset() = default;

	explicit RefCountedAsset(Entity e, AssetManager* asset_manager);

	RefCountedAsset(const RefCountedAsset& other);

	RefCountedAsset(RefCountedAsset&& other) noexcept;

	RefCountedAsset& operator=(const RefCountedAsset& other);

	RefCountedAsset& operator=(RefCountedAsset&& other) noexcept;

	~RefCountedAsset();

protected:
	Entity entity_;
	AssetManager* asset_manager_{ nullptr };

private:
	void AddRef();

	void Release();

	bool Valid() const;
};

} // namespace impl

} // namespace ptgn