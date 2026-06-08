#pragma once
#include <array>
#include <string_view>

#include "Collider.h"
#include "CollisionTypes.h"

namespace K4E = ::Ken4lowEngine;

/// Collider用途ごとのObjectChannelとResponse設定を再利用するためのプリセット。
struct CollisionPreset
{
	static constexpr uint32_t kMaxObjectChannels = static_cast<uint32_t>(EObjectChannel::Count);

	std::string_view name{};
	EObjectChannel objectChannel = EObjectChannel::Default;
	bool queryEnabled = true;
	bool physicsEnabled = true;
	std::array<ECollisionResponse, kMaxObjectChannels> responses{};

	ECollisionResponse GetResponse(EObjectChannel other) const
	{
		const uint32_t index = ToCollisionTypeId(other);
		if (index >= kMaxObjectChannels) return ECollisionResponse::Ignore;
		return responses[index];
	}
};

enum class ECollisionPresetId : uint8_t
{
	WorldStatic,
	WorldDynamic,
	Player,
	Enemy,
	Item,
	Projectile,
	Trigger,
};

inline CollisionPreset MakeCollisionPreset(std::string_view name, EObjectChannel objectChannel, bool queryEnabled, bool physicsEnabled)
{
	CollisionPreset preset{};
	preset.name = name;
	preset.objectChannel = objectChannel;
	preset.queryEnabled = queryEnabled;
	preset.physicsEnabled = physicsEnabled;
	preset.responses.fill(ECollisionResponse::Ignore);
	return preset;
}

inline void SetPresetResponse(CollisionPreset& preset, EObjectChannel other, ECollisionResponse response)
{
	const uint32_t index = ToCollisionTypeId(other);
	if (index >= CollisionPreset::kMaxObjectChannels) return;
	preset.responses[index] = response;
}

inline CollisionPreset GetCollisionPreset(ECollisionPresetId presetId)
{
	switch (presetId)
	{
	case ECollisionPresetId::WorldStatic:
	{
		CollisionPreset preset = MakeCollisionPreset("WorldStatic", EObjectChannel::WorldStatic, true, true);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Enemy, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Boss, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::PlayerProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::EnemyProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::BossProjectile, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::WorldDynamic:
	{
		// WorldDynamic用ObjectChannelは未定義のため、既存挙動を壊さないDefault予約として残す。
		return MakeCollisionPreset("WorldDynamic", EObjectChannel::Default, true, true);
	}
	case ECollisionPresetId::Player:
	{
		CollisionPreset preset = MakeCollisionPreset("Player", EObjectChannel::Player, true, true);
		SetPresetResponse(preset, EObjectChannel::Enemy, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::Boss, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::EnemyProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::BossProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Item, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::Enemy:
	{
		CollisionPreset preset = MakeCollisionPreset("Enemy", EObjectChannel::Enemy, true, true);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::PlayerProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::Item:
	{
		CollisionPreset preset = MakeCollisionPreset("Item", EObjectChannel::Item, true, false);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Overlap);
		return preset;
	}
	case ECollisionPresetId::Projectile:
	{
		// Projectileは現行のPlayerProjectile寄りに定義し、敵弾/ボス弾は後続Phaseで個別化する。
		CollisionPreset preset = MakeCollisionPreset("Projectile", EObjectChannel::PlayerProjectile, true, false);
		SetPresetResponse(preset, EObjectChannel::Enemy, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Boss, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Crystal, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::Trigger:
	{
		// Trigger専用ObjectChannelは未定義のため、Queryのみ有効なDefault予約として残す。
		CollisionPreset preset = MakeCollisionPreset("Trigger", EObjectChannel::Default, true, false);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Overlap);
		return preset;
	}
	default:
		return MakeCollisionPreset("Default", EObjectChannel::Default, true, true);
	}
}

inline void ApplyCollisionPreset(K4E::Collider& collider, const CollisionPreset& preset)
{
	// 現段階ではColliderが保持できるTypeIDだけを反映し、Response配列は将来の個別設定用に残す。
	collider.SetTypeID(ToCollisionTypeId(preset.objectChannel));
}

inline void ApplyCollisionPreset(K4E::Collider& collider, ECollisionPresetId presetId)
{
	// JsonやImGuiからPreset名を選ぶ段階では、この入口に名前解決を足す。
	ApplyCollisionPreset(collider, GetCollisionPreset(presetId));
}
