#include "runtime/animation/animation.h"

#include <algorithm>
#include <chrono>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/animation/animation_event.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/visible.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"

namespace ptgn {

namespace {

void DispatchFrameChange(
	Animation animation,
	impl::AnimationData& data,
	impl::TextureCrop& crop
) {
	auto texture_size{ GetTextureSize(animation) };
	crop.Update(data, texture_size);
	data.frame_dirty = false;

	PushEvent<event::AnimationFrameChange>(
		animation,
		animation
	);
}

[[nodiscard]] bool IsFinalFrameBeforeComplete(
	const impl::AnimationData& data
) {
	if (data.config.frame_count == 0 ||
		!data.config.play_count.has_value()) {
		return false;
	}

	const std::size_t total_frames{
		data.config.play_count.value() *
		data.config.frame_count
	};

	if (total_frames == 0) {
		return false;
	}

	return data.frames_played ==
			   total_frames - 1 &&
		   data.current_frame ==
			   data.config.frame_count - 1;
}

void DispatchFinalFrameIfNeeded(
	Animation animation,
	const impl::AnimationData& data
) {
	if (!IsFinalFrameBeforeComplete(data)) {
		return;
	}

	PushEvent<event::AnimationFinalFrame>(
		animation,
		animation
	);
}

/// @return Whether the animation completed.
bool AdvanceAnimationFrame(
	Animation animation,
	impl::AnimationData& data,
	impl::TextureCrop& crop
) {
	if (data.config.frame_count == 0) {
		return false;
	}

	const std::size_t next_frames_played{
		data.frames_played + 1
	};

	if (data.config.play_count.has_value()) {
		const std::size_t total_frames{
			data.config.play_count.value() *
			data.config.frame_count
		};

		if (next_frames_played >=
			total_frames) {
			PushEvent<event::AnimationComplete>(
				animation,
				animation
			);

			if (data.config.reset_on_complete) {
				data.SetCurrentFrame(0);

				DispatchFrameChange(
					animation,
					data,
					crop
				);
			}

			data.frame_timer.Stop();

			PushEvent<event::AnimationStop>(
				animation,
				animation
			);

			return true;
		}
	}

	data.frames_played =
		next_frames_played;

	data.IncrementFrame();

	DispatchFrameChange(
		animation,
		data,
		crop
	);

	DispatchFinalFrameIfNeeded(
		animation,
		data
	);

	if (data.frames_played %
			data.config.frame_count ==
		0) {
		PushEvent<event::AnimationLoopComplete>(
			animation,
			animation
		);
	}

	return false;
}

} // namespace

Animation::Animation(Entity entity) : Entity{ entity } {}

Animation& Animation::SetConfig(AnimationConfig config) {
	auto texture_size{ GetTextureSize(*this) };
	std::size_t automatic_row_count{ 1 };

	if (const auto texture_key{ TryGet<TextureKey>() }) {
		if (const auto layout{
				impl::DetectAnimationTextureLayout(
					GetScene().ctx().asset,
					*texture_key
				)
			}) {
			config.frame_count = layout->frame_count;
			config.frame_size.reset();
			automatic_row_count = layout->row_count;
		}
	}

	if (auto anim_data{ TryGet<impl::AnimationData>() };
		anim_data &&
		anim_data->config.IsIdentical(config, texture_size, automatic_row_count) &&
		anim_data->GetAutomaticRowCount() == automatic_row_count) {
		return *this;
	}

	const auto& anim{ Add<impl::AnimationData>(
		std::move(config), texture_size, automatic_row_count
	) };

	auto& crop{ TryAdd<impl::TextureCrop>() };
	crop.Update(anim, texture_size);

	return Reset();
}

Animation& Animation::Start(bool force) {
	if (!Has<
			impl::AnimationData,
			impl::TextureCrop
		>()) {
		return *this;
	}

	auto& anim{
		Get<impl::AnimationData>()
	};

	anim.current_frame = 0;
	anim.frames_played = 0;

	auto& crop{
		Get<impl::TextureCrop>()
	};

	auto texture_size{ GetTextureSize(*this) };

	crop.Update(anim, texture_size);

	if (const bool started{
			anim.frame_timer.Start(force)
		};
		started) {
		PushEvent<event::AnimationStart>(
			*this,
			*this
		);

		DispatchFinalFrameIfNeeded(
			*this,
			anim
		);
	}

	return *this;
}

Animation& Animation::Reset() {
	if (!Has<impl::AnimationData, impl::TextureCrop>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop{ Get<impl::TextureCrop>() };
	auto texture_size{ GetTextureSize(*this) };
	crop.Update(anim, texture_size);
	anim.frame_timer.Reset();
	PushEvent<event::AnimationStop>(*this, *this);
	return *this;
}

Animation& Animation::Stop(bool reset) {
	if (reset) {
		Reset();
		return *this;
	}
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Stop();
	PushEvent<event::AnimationStop>(*this, *this);
	return *this;
}

Animation& Animation::Toggle() {
	if (IsPlaying()) {
		Stop();
	} else {
		Start();
	}
	return *this;
}

Animation& Animation::Pause() {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Pause();
	PushEvent<event::AnimationPause>(*this, *this);
	return *this;
}

Animation& Animation::Resume() {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.frame_timer.Resume();
	PushEvent<event::AnimationResume>(*this, *this);
	return *this;
}

bool Animation::IsPaused() const {
	if (!Has<impl::AnimationData>()) {
		return false;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frame_timer.IsPaused();
}

bool Animation::IsPlaying() const {
	if (!Has<impl::AnimationData>()) {
		return false;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frame_timer.IsRunning();
}

std::size_t Animation::GetPlayCount() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.GetPlayCount();
}

std::size_t Animation::GetFramePlayCount() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.frames_played;
}

milliseconds Animation::GetDuration() const {
	if (!Has<impl::AnimationData>()) {
		return 0ms;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.config.duration;
}

milliseconds Animation::GetFrameDuration() const {
	if (!Has<impl::AnimationData>()) {
		return 0ms;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	milliseconds frame_duration{ anim.config.duration / anim.config.frame_count };
	return frame_duration;
}

std::size_t Animation::GetFrameCount() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.config.frame_count;
}

Animation& Animation::SetCurrentFrame(std::size_t new_frame) {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.SetCurrentFrame(new_frame);
	return *this;
}

Animation& Animation::SetTexture(TextureKey texture_key) {
	Sprite{ *this }.SetTexture(std::move(texture_key));

	auto data{ TryGet<impl::AnimationData>() };

	if (!data) {
		return *this;
	}

	data->SetAutomaticRowCount(1);

	if (const auto key{ TryGet<TextureKey>() }) {
		if (const auto layout{
				impl::DetectAnimationTextureLayout(
					GetScene().ctx().asset,
					*key
				)
			}) {
			data->config.frame_count = layout->frame_count;
			data->config.frame_size.reset();
			data->SetAutomaticRowCount(layout->row_count);
		}
	}

	const auto texture_size{ GetTextureSize(*this) };

	if (data->config.frame_count == 0) {
		data->current_frame = 0;
	} else {
		data->current_frame %= data->config.frame_count;
	}

	data->frame_dirty = true;

	if (auto crop{ TryGet<impl::TextureCrop>() }) {
		crop->Update(*data, texture_size);
	}

	return *this;
}

Animation& Animation::IncrementFrame() {
	if (!Has<
			impl::AnimationData,
			impl::TextureCrop
		>()) {
		return *this;
	}

	auto& data{
		Get<impl::AnimationData>()
	};
	auto& crop{
		Get<impl::TextureCrop>()
	};

	AdvanceAnimationFrame(
		*this,
		data,
		crop
	);

	return *this;
}

Animation& Animation::SetResetOnComplete(bool reset_on_complete) {
	if (!Has<impl::AnimationData>()) {
		return *this;
	}
	auto& anim{ Get<impl::AnimationData>() };
	anim.config.reset_on_complete = reset_on_complete;
	return *this;
}

std::size_t Animation::GetCurrentFrame() const {
	if (!Has<impl::AnimationData>()) {
		return 0;
	}
	const auto& anim{ Get<impl::AnimationData>() };
	return anim.current_frame;
}

V2_int Animation::GetCurrentFramePosition() const {
	if (!Has<impl::AnimationData>()) {
		return {};
	}
	const auto& anim{ Get<impl::AnimationData>() };
	auto texture_size{ GetTextureSize(*this) };
	return anim.GetCurrentFramePosition(texture_size);
}

V2_int Animation::GetFrameSize() const {
	if (!Has<impl::AnimationData>()) {
		return {};
	}
	const auto& anim{ Get<impl::AnimationData>() };
	auto texture_size{ GetTextureSize(*this) };
	return anim.GetFrameSize(texture_size);
}

namespace impl {

namespace {

[[nodiscard]] std::optional<path> ResolveAnimationTextureSourcePath(
	AssetManager& assets, const TextureKey& texture_key
) {
	if (const auto asset{ assets.GetCatalogAsset(texture_key, AssetKind::Texture) }) {
		return asset->source_path;
	}

	AssetAccessor accessor{ assets };
	if (!accessor.Has<Texture>(texture_key)) {
		return std::nullopt;
	}

	const Texture texture{ accessor.Get<Texture>(texture_key) };
	if (const auto asset_path{ texture.GetEntity().TryGet<AssetPath>() }) {
		return asset_path->value;
	}

	return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> ParsePositiveSize(std::string_view text) {
	if (text.empty()) {
		return std::nullopt;
	}

	std::size_t value{ 0 };
	const auto [end, error]{
		std::from_chars(text.data(), text.data() + text.size(), value)
	};

	if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
		return std::nullopt;
	}

	return value;
}

[[nodiscard]] std::optional<AnimationTextureLayout> ParseAnimationTextureLayout(
	std::string_view stem
) {
	constexpr std::string_view marker{ "_frames" };
	const auto marker_position{ stem.rfind(marker) };
	if (marker_position == std::string_view::npos) {
		return std::nullopt;
	}

	const std::string_view suffix{
		stem.substr(marker_position + marker.size())
	};
	if (suffix.empty()) {
		return std::nullopt;
	}

	const auto separator{ suffix.find('x') };
	if (separator == std::string_view::npos) {
		const auto frame_count{ ParsePositiveSize(suffix) };
		return frame_count
			? std::optional<AnimationTextureLayout>{ AnimationTextureLayout{
				.frame_count = *frame_count,
				.row_count = 1,
			} }
			: std::nullopt;
	}

	if (suffix.find('x', separator + 1) != std::string_view::npos) {
		return std::nullopt;
	}

	const auto frame_count{ ParsePositiveSize(suffix.substr(0, separator)) };
	const auto row_count{ ParsePositiveSize(suffix.substr(separator + 1)) };
	if (!frame_count || !row_count) {
		return std::nullopt;
	}

	return AnimationTextureLayout{
		.frame_count = *frame_count,
		.row_count = *row_count,
	};
}

} // namespace

std::optional<AnimationTextureLayout> DetectAnimationTextureLayout(
	AssetManager& assets, const TextureKey& texture_key
) {
	const auto source_path{ ResolveAnimationTextureSourcePath(assets, texture_key) };
	if (!source_path) {
		return std::nullopt;
	}

	const std::string stem{ source_path->stem().string() };
	return ParseAnimationTextureLayout(stem);
}

std::optional<std::size_t> DetectAnimationFrameCount(
	AssetManager& assets,
	const TextureKey& texture_key
) {
	const auto layout{ DetectAnimationTextureLayout(assets, texture_key) };
	return layout ? std::optional<std::size_t>{ layout->frame_count } : std::nullopt;
}

AnimationData::AnimationData(
	AnimationConfig&& anim_config, std::optional<V2_int> texture_size,
	std::size_t row_count
) :
	config{ std::move(anim_config) },
	automatic_row_count{ std::max<std::size_t>(1, row_count) } {
	(void)texture_size;
}

milliseconds AnimationData::GetFrameDuration() const {
	if (!config.frame_count) {
		return 0ms;
	}
	return config.duration / config.frame_count;
}

V2_int AnimationData::GetFrameSize(std::optional<V2_int> texture_size) const {
	return config.frame_size.value_or(
		impl::GetFrameSize(texture_size, config.frame_count, automatic_row_count)
			.value_or(V2_int{})
	);
}

void AnimationData::SetAutomaticRowCount(std::size_t row_count) {
	automatic_row_count = std::max<std::size_t>(1, row_count);
}

std::size_t AnimationData::GetAutomaticRowCount() const {
	return automatic_row_count;
}

V2_int AnimationData::GetCurrentFramePosition(std::optional<V2_int> texture_size) const {
	auto frame_size{ GetFrameSize(texture_size) };
	return { config.start_pixel.x + frame_size.x * static_cast<int>(current_frame),
			 config.start_pixel.y };
}

std::size_t AnimationData::GetPlayCount() const {
	if (!config.frame_count) {
		return 0;
	}
	return frames_played / config.frame_count;
}

void AnimationData::SetCurrentFrame(std::size_t new_frame) {
	current_frame = new_frame % config.frame_count;
	frame_dirty	  = true;
}

void AnimationData::IncrementFrame() {
	SetCurrentFrame(current_frame + 1);
}

void AnimationSystem::Prepare(Scene& scene) {
	for (auto [entity, anim, crop] : scene.EntitiesWith<AnimationData, TextureCrop>()) {
		auto texture_size{ GetTextureSize(entity) };

		bool layout_detected{ false };

		if (const auto texture_key{ entity.TryGet<TextureKey>() }) {
			if (const auto layout{
					DetectAnimationTextureLayout(
						scene.ctx().asset,
						*texture_key
					)
				}) {
				layout_detected = true;
				const bool layout_changed{
					anim.config.frame_count != layout->frame_count ||
					anim.GetAutomaticRowCount() != layout->row_count ||
					anim.config.frame_size.has_value()
				};

				anim.config.frame_count = layout->frame_count;
				anim.config.frame_size.reset();
				anim.SetAutomaticRowCount(layout->row_count);

				if (layout_changed) {
					anim.current_frame %= anim.config.frame_count;
					anim.frame_dirty = true;
				}
			}
		}

		if (!layout_detected && anim.GetAutomaticRowCount() != 1) {
			anim.SetAutomaticRowCount(1);
			anim.frame_dirty = true;
		}

		crop.Update(anim, texture_size);
	}
}

void AnimationSystem::Update(
	Scene& scene,
	secondsf dt
) {
	for (auto [entity, data, crop] :
		 scene.EntitiesWith<
			 AnimationData,
			 TextureCrop
		 >()) {
		data.frame_timer.Update(dt);

		Animation animation{
			entity
		};

		// Handles direct SetCurrentFrame calls that did not go through
		// IncrementFrame.
		if (data.frame_dirty) {
			DispatchFrameChange(
				animation,
				data,
				crop
			);
		}

		if (data.config.frame_count == 0 ||
			data.config.duration <= 0ms ||
			!data.frame_timer.IsRunning() ||
			data.frame_timer.IsPaused()) {
			continue;
		}

		PushEvent<event::AnimationUpdate>(
			animation,
			animation
		);

		const auto frame_duration{
			data.GetFrameDuration()
		};

		if (!data.frame_timer.Completed(
				frame_duration
			)) {
			continue;
		}

		const bool completed{
			AdvanceAnimationFrame(
				animation,
				data,
				crop
			)
		};

		if (!completed) {
			data.frame_timer.Start(true);
		}
	}
}

} // namespace impl

AnimationMap::AnimationMap(Entity entity) : Entity{ entity } {}

Animation AnimationMap::Find(impl::AnimationMapKey key) const {
	if (!HasChildren(*this)) {
		return {};
	}

	auto hash{ Hash(key) };

	auto children{ GetChildren(*this) };
	auto it{ std::ranges::find_if(children, [hash](Entity child) {
		auto child_key{ child.TryGet<impl::AnimationMapKey>() };
		return child_key && Hash(*child_key) == hash;
	}) };

	return it != children.end() ? Animation{ *it } : Animation{};
}

Animation AnimationMap::Add(std::string_view animation_key, Animation animation, bool hide) {
	if (!Has<impl::AnimationMapData>()) {
		return animation;
	}

	impl::AnimationMapKey key{ animation_key };

	if (auto existing{ Find(key) }; existing && existing != animation) {
		existing.Destroy();
	}

	if (hide) {
		Hide(animation);
	}

	animation.Add<impl::AnimationMapKey>(key);
	SetParent(animation, *this);

	return animation;
}

void AnimationMap::Remove(std::string_view animation_key) {
	if (!Has<impl::AnimationMapData>()) {
		return;
	}

	impl::AnimationMapKey key{ animation_key };

	if (auto animation{ Find(key) }) {
		animation.Destroy();
	}

	auto& info{ Get<impl::AnimationMapData>() };

	if (info.active == key) {
		info.active = {};
	}
}

std::optional<Animation> AnimationMap::GetActive() const {
	if (!Has<impl::AnimationMapData>()) {
		return std::nullopt;
	}

	auto animation{ Find(Get<impl::AnimationMapData>().active) };

	return animation ? std::optional<Animation>{ animation } : std::nullopt;
}

Animation AnimationMap::GetAnimation(std::string_view animation_key) const {
	return Find(impl::AnimationMapKey{ animation_key });
}

bool AnimationMap::SetActive(std::string_view animation_key) {
	if (!Has<impl::AnimationMapData>()) {
		return false;
	}

	auto& info{ Get<impl::AnimationMapData>() };

	impl::AnimationMapKey key{ animation_key };

	if (info.active == key) {
		return false;
	}

	auto animation{ Find(key) };

	if (!animation) {
		PTGN_WARN("Attempting to set non-existent animation key to active: ", animation_key);
		return false;
	}

	if (auto previous{ Find(info.active) }) {
		Hide(previous);
		previous.Pause();
	}

	info.active = key;
	Show(animation);

	return true;
}

Animation CreateAnimation(
	Scene& scene, Transform transform, TextureKey texture_key, AnimationConfig config, Origin origin
) {
	Animation animation{ CreateSprite(scene, transform, std::move(texture_key), origin) };
	animation.SetConfig(std::move(config));
	animation.Add<Tag>("Animation");
	return animation;
}

Animation PlayTemporaryAnimation(
	Scene& scene, Transform transform, TextureKey texture_key, AnimationConfig config,
	milliseconds destroy_delay, Origin origin
) {
	Animation animation{
		CreateAnimation(scene, transform, std::move(texture_key), std::move(config), origin)
	};
	animation.Add<Tag>("Temporary Animation");

	animation.OnComplete([destroy_delay](auto& event) mutable {
		if (destroy_delay == 0ms) {
			event.animation.Destroy();
			return;
		}

		After(
			event.animation.GetScene(),
			destroy_delay,
			[animation = event.animation]() mutable {
				animation.Destroy();
			}
		);
	});

	animation.Start(true);
	return animation;
}

AnimationMap CreateAnimationMap(Scene& scene) {
	AnimationMap animation_map{ scene.CreateEntity() };

	animation_map.Entity::Add<Tag>("Animation Map");
	animation_map.Entity::Add<impl::AnimationMapData>();

	return animation_map;
}

} // namespace ptgn