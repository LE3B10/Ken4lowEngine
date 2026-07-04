#define NOMINMAX
#include "GpuParticleComponent.h"

#include "GpuParticleEmitter.h"
#include "GpuParticleEmitterPreset.h"
#include "GpuParticleManager.h"
#include "GpuParticleType.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		GpuParticleType ParseGpuParticleType(const std::string& name)
		{
			for (uint32_t index = 0; index < GpuParticleEmitterPresetTable::GetSpritePresetCount(); ++index)
			{
				const GpuParticleType type = GpuParticleEmitterPresetTable::GetSpriteTypeByIndex(index);
				if (name == GpuParticleEmitterPresetTable::GetSpriteDisplayName(type))
				{
					return type;
				}
			}

			return GpuParticleType::Default;
		}

		Vector3 ReadVector3FromJson(const nlohmann::json& json, const char* key, const Vector3& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>()
			};
		}

		Vector4 ReadVector4FromJson(const nlohmann::json& json, const char* key, const Vector4& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 4)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>(),
				json[key][3].get<float>()
			};
		}
	}

	void GpuParticleComponent::Initialize()
	{
		SceneComponent::Initialize();

		if (playOnStart_)
		{
			Play(); // Component開始時にGPUパーティクルを再生する
		}
	}

	void GpuParticleComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);

		if (!visible_ || !IsActiveInHierarchy())
		{
			Stop(); // 非表示または無効なComponentのEmitterを停止する
			return;
		}

		if (!playing_)
		{
			return;
		}

		if (followOwner_)
		{
			SyncEmitterPosition();
		}

		if (!loop_ && !IsPlaying())
		{
			playing_ = false;
			activeEmitterName_.clear();
		}
	}

	void GpuParticleComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("GPUパーティクルコンポーネント");

		std::array<char, 128> effectNameBuffer{};
		std::snprintf(effectNameBuffer.data(), effectNameBuffer.size(), "%s", effectName_.c_str());
		if (ImGui::InputText("エフェクト名", effectNameBuffer.data(), effectNameBuffer.size()))
		{
			effectName_ = effectNameBuffer.data();
		}

		std::array<char, 128> emitterNameBuffer{};
		std::snprintf(emitterNameBuffer.data(), emitterNameBuffer.size(), "%s", emitterName_.c_str());
		if (ImGui::InputText("エミッター名", emitterNameBuffer.data(), emitterNameBuffer.size()))
		{
			emitterName_ = emitterNameBuffer.data();
		}

		ImGui::Checkbox("開始時に再生", &playOnStart_);
		ImGui::Checkbox("ループ", &loop_);

		bool visible = visible_;
		if (ImGui::Checkbox("表示", &visible))
		{
			SetVisible(visible);
		}

		ImGui::Checkbox("Actorに追従", &followOwner_);
		ImGui::DragFloat3("ローカルオフセット", &localOffset_.x, 0.01f);
		ImGui::DragFloat3("スケール", &scale_.x, 0.01f, 0.0f, 100.0f);
		ImGui::DragFloat("発生レート", &emissionRate_, 0.1f, 0.0f, 10000.0f);
		ImGui::DragFloat("寿命", &lifeTime_, 0.01f, 0.01f, 60.0f);
		ImGui::ColorEdit4("開始色", &startColor_.x);
		ImGui::ColorEdit4("終了色", &endColor_.x);
		ImGui::DragFloat("開始サイズ", &startSize_, 0.01f, 0.0f, 100.0f);
		ImGui::DragFloat("終了サイズ", &endSize_, 0.01f, 0.0f, 100.0f);
		ImGui::DragFloat("速度倍率", &velocityScale_, 0.01f, 0.0f, 100.0f);

		if (ImGui::Button("再生"))
		{
			Play();
		}

		ImGui::SameLine();

		if (ImGui::Button("停止"))
		{
			Stop();
		}

		ImGui::SameLine();

		if (ImGui::Button("再開"))
		{
			Restart();
		}

		ImGui::Text("再生中: %s", IsPlaying() ? "はい" : "いいえ");
#endif // USE_IMGUI
	}

	void GpuParticleComponent::Finalize()
	{
		Stop();
	}

	void GpuParticleComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // GpuParticleComponentとして保存する
		outJson["EffectName"] = effectName_;
		outJson["EmitterName"] = emitterName_;
		outJson["PlayOnStart"] = playOnStart_;
		outJson["Loop"] = loop_;
		outJson["Visible"] = visible_;
		outJson["FollowOwner"] = followOwner_;
		outJson["LocalOffset"] = { localOffset_.x, localOffset_.y, localOffset_.z };
		outJson["Scale"] = { scale_.x, scale_.y, scale_.z };
		outJson["EmissionRate"] = emissionRate_;
		outJson["LifeTime"] = lifeTime_;
		outJson["StartColor"] = { startColor_.x, startColor_.y, startColor_.z, startColor_.w };
		outJson["EndColor"] = { endColor_.x, endColor_.y, endColor_.z, endColor_.w };
		outJson["StartSize"] = startSize_;
		outJson["EndSize"] = endSize_;
		outJson["VelocityScale"] = velocityScale_;
	}

	void GpuParticleComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		if (inJson.contains("EffectName") && inJson["EffectName"].is_string())
		{
			effectName_ = inJson["EffectName"].get<std::string>();
		}

		if (inJson.contains("EmitterName") && inJson["EmitterName"].is_string())
		{
			emitterName_ = inJson["EmitterName"].get<std::string>();
		}

		if (inJson.contains("PlayOnStart") && inJson["PlayOnStart"].is_boolean())
		{
			playOnStart_ = inJson["PlayOnStart"].get<bool>();
		}

		if (inJson.contains("Loop") && inJson["Loop"].is_boolean())
		{
			loop_ = inJson["Loop"].get<bool>();
		}

		if (inJson.contains("Visible") && inJson["Visible"].is_boolean())
		{
			visible_ = inJson["Visible"].get<bool>();
		}

		if (inJson.contains("FollowOwner") && inJson["FollowOwner"].is_boolean())
		{
			followOwner_ = inJson["FollowOwner"].get<bool>();
		}

		localOffset_ = ReadVector3FromJson(inJson, "LocalOffset", localOffset_);
		scale_ = ReadVector3FromJson(inJson, "Scale", scale_);
		startColor_ = ReadVector4FromJson(inJson, "StartColor", startColor_);
		endColor_ = ReadVector4FromJson(inJson, "EndColor", endColor_);

		if (inJson.contains("EmissionRate") && inJson["EmissionRate"].is_number())
		{
			emissionRate_ = inJson["EmissionRate"].get<float>();
		}

		if (inJson.contains("LifeTime") && inJson["LifeTime"].is_number())
		{
			lifeTime_ = inJson["LifeTime"].get<float>();
		}

		if (inJson.contains("StartSize") && inJson["StartSize"].is_number())
		{
			startSize_ = inJson["StartSize"].get<float>();
		}

		if (inJson.contains("EndSize") && inJson["EndSize"].is_number())
		{
			endSize_ = inJson["EndSize"].get<float>();
		}

		if (inJson.contains("VelocityScale") && inJson["VelocityScale"].is_number())
		{
			velocityScale_ = inJson["VelocityScale"].get<float>();
		}
	}

	void GpuParticleComponent::Play()
	{
		if (!visible_ || !IsActiveInHierarchy() || effectName_.empty())
		{
			return; // 無効な状態やEffect名未設定では再生しない
		}

		Stop();
		lockedPosition_ = CalculateEmitterPosition();

		GpuParticleEmitter* emitter = EnsureEmitter();
		if (!emitter)
		{
			return;
		}

		emitter->SetPosition(lockedPosition_);
		const uint32_t emitCount = std::max(1u, static_cast<uint32_t>(std::max(emissionRate_, 1.0f)));
		emitter->RequestEmit(emitCount);
		playing_ = true;
	}

	void GpuParticleComponent::Stop()
	{
		if (!activeEmitterName_.empty())
		{
			GpuParticleManager::GetInstance()->RemoveEmitter(activeEmitterName_);
		}

		activeEmitterName_.clear();
		playing_ = false;
	}

	void GpuParticleComponent::Restart()
	{
		Stop();
		Play();
	}

	bool GpuParticleComponent::IsPlaying() const
	{
		if (activeEmitterName_.empty())
		{
			return false;
		}

		GpuParticleEmitter* emitter = GpuParticleManager::GetInstance()->GetEmitter(activeEmitterName_);
		if (!emitter)
		{
			return false;
		}

		return loop_ || emitter->HasActiveParticles();
	}

	void GpuParticleComponent::SetVisible(bool visible)
	{
		visible_ = visible;
		if (!visible_)
		{
			Stop();
		}
	}

	std::string GpuParticleComponent::ResolveEmitterName() const
	{
		if (!emitterName_.empty())
		{
			return emitterName_;
		}

		char buffer[128]{};
		std::snprintf(buffer, sizeof(buffer), "ActorGpuParticle_%p", static_cast<const void*>(this));
		return buffer;
	}

	Vector3 GpuParticleComponent::CalculateEmitterPosition() const
	{
		return GetWorldPosition() + localOffset_;
	}

	GpuParticleEmitter* GpuParticleComponent::EnsureEmitter()
	{
		const GpuParticleType type = ParseGpuParticleType(effectName_);
		if (type == GpuParticleType::Default && effectName_ != "Default")
		{
			return nullptr; // 未登録のEffect名ではEmitterを作成しない
		}

		GpuParticleEmitter::EmitterInfo info = GpuParticleEmitterPresetTable::MakeEmitterInfo(type);
		const float scaleAverage = std::max((scale_.x + scale_.y + scale_.z) / 3.0f, 0.0f);
		info.radius *= scaleAverage;
		info.speedScale = std::max(velocityScale_, 0.0f);
		info.useDescSpawnOverride = true;
		info.lifeTime = std::max(lifeTime_, 0.01f);
		info.startColor = startColor_;
		info.endColor = endColor_;
		info.startSize = { std::max(startSize_, 0.0f), std::max(startSize_, 0.0f) };
		info.endSize = { std::max(endSize_, 0.0f), std::max(endSize_, 0.0f) };
		info.startScale3D = scale_;
		info.endScale3D = scale_;

		if (loop_)
		{
			info.loopCount = std::max(1u, static_cast<uint32_t>(std::max(emissionRate_, 1.0f)));
			info.loopFrequency = emissionRate_ > 0.0f ? 1.0f : 0.0f;
		}
		else
		{
			info.loopCount = 0;
			info.loopFrequency = 0.0f;
		}

		activeEmitterName_ = ResolveEmitterName();
		return GpuParticleManager::GetInstance()->CreateRuntimeEmitter(activeEmitterName_, info);
	}

	void GpuParticleComponent::SyncEmitterPosition()
	{
		if (activeEmitterName_.empty())
		{
			return;
		}

		if (GpuParticleEmitter* emitter = GpuParticleManager::GetInstance()->GetEmitter(activeEmitterName_))
		{
			emitter->SetPosition(CalculateEmitterPosition()); // Actorの現在位置をEmitterへ反映する
		}
	}
}
