#include "ItemVisualEffect.h"

#include "Item.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cstdint>
#include <sstream>

namespace
{
	const char* ToItemEffectName(ItemType type)
	{
		switch (type)
		{
		case ItemType::HealSmall: return "HealSmall";
		case ItemType::AmmoSmall: return "AmmoSmall";
		case ItemType::NextStageKey: return "NextStageKey";
		case ItemType::None:
		default: return "None";
		}
	}
}

void ItemVisualEffect::StartIdle(const Item& item)
{
	if (item.GetType() == ItemType::None)
	{
		return;
	}

	IdleState& state = idleStates_[&item];
	state.type = item.GetType();
	state.emitTimer = 0.0f;
	state.emitterName = BuildIdleEmitterName(item);
}

void ItemVisualEffect::StopIdle(const Item& item)
{
	auto it = idleStates_.find(&item);
	if (it == idleStates_.end())
	{
		return;
	}

	if (auto* manager = K4E::GpuParticleManager::GetInstance())
	{
		manager->RemoveEmitter(it->second.emitterName);
	}
	idleStates_.erase(it);
}

void ItemVisualEffect::UpdateIdle(const Item& item, float deltaTime)
{
	if (!settings_.itemEffectEnabled || !settings_.idleEffectEnabled || settings_.idleParticleCount <= 0)
	{
		return;
	}

	auto it = idleStates_.find(&item);
	if (it == idleStates_.end())
	{
		StartIdle(item);
		it = idleStates_.find(&item);
		if (it == idleStates_.end())
		{
			return;
		}
	}

	IdleState& state = it->second;
	state.emitTimer += deltaTime;
	const float interval = std::max(0.05f, settings_.idleEmitInterval);
	if (state.emitTimer < interval)
	{
		return;
	}

	state.emitTimer = 0.0f;
	K4E::Vector3 emitPosition = item.GetPosition();
	emitPosition.y += 0.25f;
	// 常時演出は短い間隔の少量バーストにして、出しっぱなしでもGPU負荷が増えすぎないようにする。
	EmitSpriteBurst(state.emitterName, GetIdleParticleType(state.type), emitPosition, static_cast<uint32_t>(settings_.idleParticleCount), 0.16f);
	lastPlayedEffectName_ = std::string(ToItemEffectName(state.type)) + " 待機粒子";
}

void ItemVisualEffect::PlayPickup(ItemType type, const K4E::Vector3& position)
{
	if (!settings_.itemEffectEnabled || !settings_.pickupEffectEnabled || settings_.pickupParticleCount <= 0)
	{
		return;
	}

	K4E::Vector3 burstPosition = position;
	burstPosition.y += 0.2f;
	const std::string burstName = BuildPickupEmitterName(type);
	const std::string ringName = BuildPickupRingEmitterName(type);
	const float burstRadius = std::max(0.08f, settings_.pickupParticleSpeed * 0.035f);

	EmitSpriteBurst(burstName, GetPickupParticleType(type), burstPosition, static_cast<uint32_t>(settings_.pickupParticleCount), burstRadius);
	EmitSpriteBurst(ringName, K4E::GpuParticleType::Shockwave, burstPosition, 1, std::max(0.35f, settings_.pickupParticleSpeed * 0.12f));

	lastPlayedEffectName_ = std::string(ToItemEffectName(type)) + " 取得バースト";
}

void ItemVisualEffect::Clear()
{
	if (auto* manager = K4E::GpuParticleManager::GetInstance())
	{
		for (const auto& [item, state] : idleStates_)
		{
			(void)item;
			manager->RemoveEmitter(state.emitterName);
		}
	}
	idleStates_.clear();
	lastPlayedEffectName_ = "未再生";
}

void ItemVisualEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("アイテム演出");
	ImGui::Checkbox("アイテム演出有効", &settings_.itemEffectEnabled);
	ImGui::Checkbox("待機演出有効", &settings_.idleEffectEnabled);
	ImGui::Checkbox("取得演出有効", &settings_.pickupEffectEnabled);
	ImGui::DragInt("待機粒子数", &settings_.idleParticleCount, 1, 0, 32);
	ImGui::DragFloat("待機粒子発生間隔", &settings_.idleEmitInterval, 0.01f, 0.05f, 2.0f, "%.2f秒");
	ImGui::DragInt("取得時粒子数", &settings_.pickupParticleCount, 1, 0, 128);
	ImGui::DragFloat("取得時粒子速度", &settings_.pickupParticleSpeed, 0.1f, 0.1f, 20.0f, "%.2f");
	ImGui::DragFloat("アイテム浮遊高さ", &settings_.itemFloatHeight, 0.01f, 0.0f, 2.0f, "%.2f");
	ImGui::DragFloat("アイテム浮遊速度", &settings_.itemFloatSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::DragFloat("アイテム回転速度", &settings_.itemRotationSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::ColorEdit4("Heal演出色", &settings_.healEffectColor.x);
	ImGui::ColorEdit4("Ammo演出色", &settings_.ammoEffectColor.x);
	ImGui::Text("最後に再生したItem演出: %s", lastPlayedEffectName_.c_str());
#else
	(void)this;
#endif
}

K4E::Vector4 ItemVisualEffect::GetEffectColor(ItemType type) const
{
	switch (type)
	{
	case ItemType::HealSmall:
		return settings_.healEffectColor;
	case ItemType::AmmoSmall:
		return settings_.ammoEffectColor;
	case ItemType::NextStageKey:
		return { 0.25f, 1.0f, 1.0f, 1.0f };
	case ItemType::None:
	default:
		return { 1.0f, 1.0f, 1.0f, 0.5f };
	}
}

std::string ItemVisualEffect::BuildIdleEmitterName(const Item& item) const
{
	std::ostringstream oss;
	oss << "ItemIdle_" << ToItemEffectName(item.GetType()) << "_" << reinterpret_cast<std::uintptr_t>(&item);
	return oss.str();
}

std::string ItemVisualEffect::BuildPickupEmitterName(ItemType type)
{
	return std::string("ItemPickup_") + ToItemEffectName(type);
}

std::string ItemVisualEffect::BuildPickupRingEmitterName(ItemType type)
{
	return std::string("ItemPickupRing_") + ToItemEffectName(type);
}

K4E::GpuParticleType ItemVisualEffect::GetIdleParticleType(ItemType type) const
{
	switch (type)
	{
	case ItemType::HealSmall:
		return K4E::GpuParticleType::Heal;
	case ItemType::AmmoSmall:
		return K4E::GpuParticleType::Spark;
	case ItemType::NextStageKey:
		return K4E::GpuParticleType::Ambient;
	case ItemType::None:
	default:
		return K4E::GpuParticleType::Ambient;
	}
}

K4E::GpuParticleType ItemVisualEffect::GetPickupParticleType(ItemType type) const
{
	switch (type)
	{
	case ItemType::HealSmall:
		return K4E::GpuParticleType::Heal;
	case ItemType::AmmoSmall:
		return K4E::GpuParticleType::Spark;
	case ItemType::NextStageKey:
		return K4E::GpuParticleType::DeathBurstCore;
	case ItemType::None:
	default:
		return K4E::GpuParticleType::Spark;
	}
}

void ItemVisualEffect::EmitSpriteBurst(const std::string& emitterName, K4E::GpuParticleType particleType, const K4E::Vector3& position, uint32_t count, float radius)
{
	if (count == 0)
	{
		return;
	}

	auto* manager = K4E::GpuParticleManager::GetInstance();
	if (!manager)
	{
		return;
	}

	K4E::GpuParticleEmitter* emitter = manager->GetEmitter(emitterName);
	if (!emitter)
	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "Effects/white.dds";
		info.radius = radius;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.drawType = 0;
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = particleType;
		info.billboardFlags = K4E::BillboardMode::Camera;
		emitter = manager->CreateEmitter(emitterName, info);
	}

	if (!emitter)
	{
		return;
	}

	emitter->GetInfoMutable().radius = radius;
	emitter->SetPosition(position);
	emitter->RequestEmit(count);
}
