#pragma once

#include "ItemType.h"
#include "Vector3.h"
#include "Vector4.h"
#include "GpuParticleType.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace K4E = ::Ken4lowEngine;

class Item;

/// -------------------------------------------------------------
///						アイテム演出管理クラス
/// -------------------------------------------------------------
class ItemVisualEffect
{
public:
	struct Settings
	{
		bool itemEffectEnabled = true;
		bool idleEffectEnabled = true;
		bool pickupEffectEnabled = true;
		int idleParticleCount = 2;
		float idleEmitInterval = 0.28f;
		int pickupParticleCount = 18;
		float pickupParticleSpeed = 4.5f;
		float itemFloatHeight = 0.18f;
		float itemFloatSpeed = 2.2f;
		float itemRotationSpeed = 1.6f;
		K4E::Vector4 healEffectColor = { 0.25f, 1.0f, 0.45f, 1.0f };
		K4E::Vector4 ammoEffectColor = { 1.0f, 0.75f, 0.15f, 1.0f };
	};

	void StartIdle(const Item& item);
	void StopIdle(const Item& item);
	void UpdateIdle(const Item& item, float deltaTime);
	void PlayPickup(ItemType type, const K4E::Vector3& position);
	void Clear();
	void DrawImGui();

	Settings& GetSettings() { return settings_; }
	const Settings& GetSettings() const { return settings_; }
	K4E::Vector4 GetEffectColor(ItemType type) const;
	const std::string& GetLastPlayedEffectName() const { return lastPlayedEffectName_; }

private:
	struct IdleState
	{
		ItemType type = ItemType::None;
		float emitTimer = 0.0f;
		std::string emitterName;
	};

	std::string BuildIdleEmitterName(const Item& item) const;
	std::string BuildPickupEmitterName(ItemType type);
	std::string BuildPickupRingEmitterName(ItemType type);
	K4E::GpuParticleType GetIdleParticleType(ItemType type) const;
	K4E::GpuParticleType GetPickupParticleType(ItemType type) const;
	void EmitSpriteBurst(const std::string& emitterName, K4E::GpuParticleType particleType, const K4E::Vector3& position, uint32_t count, float radius);

private:
	Settings settings_{};
	std::unordered_map<const Item*, IdleState> idleStates_;
	std::string lastPlayedEffectName_ = "未再生";
};
