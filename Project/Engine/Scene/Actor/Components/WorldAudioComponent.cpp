#define NOMINMAX
#include "WorldAudioComponent.h"

#include "AssetPathSelector.h"
#include "CameraManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		float Length(const Vector3& value)
		{
			return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
		}
	}

	void WorldAudioComponent::Initialize()
	{
		SceneComponent::Initialize();
		sourcePosition_ = GetWorldPosition();
		UpdateRuntimeAudio();

		if (playOnStart_)
		{
			Play(); // Component開始時に3D音声を再生する
		}
	}

	void WorldAudioComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);

		if (!enabled_)
		{
			Stop(); // 無効化されているComponentの再生を停止する
			return;
		}

		UpdateRuntimeAudio();

		if (audioHandle_ != AudioManager::InvalidAudioHandle && !AudioManager::GetInstance()->IsPlaying(audioHandle_))
		{
			audioHandle_ = AudioManager::InvalidAudioHandle;
		}
	}

	void WorldAudioComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("ワールドオーディオコンポーネント");

		std::array<char, 256> soundPathBuffer{};
		std::snprintf(soundPathBuffer.data(), soundPathBuffer.size(), "%s", soundPath_.c_str());
		if (ImGui::InputText("サウンドパス", soundPathBuffer.data(), soundPathBuffer.size()))
		{
			SetSoundPath(soundPathBuffer.data());
		}

		std::string selectedSoundPath = soundPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##WorldAudioComponentSoundPath", selectedSoundPath, AssetType::Audio))
		{
			SetSoundPath(selectedSoundPath);
		}

		float volume = volume_;
		if (ImGui::DragFloat("音量", &volume, 0.01f, 0.0f, 4.0f))
		{
			SetVolume(volume);
		}

		float pitch = pitch_;
		if (ImGui::DragFloat("ピッチ", &pitch, 0.01f, 0.01f, 4.0f))
		{
			SetPitch(pitch);
		}

		bool loop = loop_;
		if (ImGui::Checkbox("ループ", &loop))
		{
			SetLoop(loop);
		}

		bool playOnStart = playOnStart_;
		if (ImGui::Checkbox("開始時に再生", &playOnStart))
		{
			SetPlayOnStart(playOnStart);
		}

		bool enabled = enabled_;
		if (ImGui::Checkbox("有効", &enabled))
		{
			SetEnabled(enabled);
		}

		float minDistance = minDistance_;
		if (ImGui::DragFloat("最小距離", &minDistance, 0.1f, 0.0f, 10000.0f))
		{
			SetMinDistance(minDistance);
		}

		float maxDistance = maxDistance_;
		if (ImGui::DragFloat("最大距離", &maxDistance, 0.1f, 0.01f, 10000.0f))
		{
			SetMaxDistance(maxDistance);
		}

		bool followOwner = followOwner_;
		if (ImGui::Checkbox("Actorに追従", &followOwner))
		{
			SetFollowOwner(followOwner);
		}

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
		if (ImGui::Button("再再生"))
		{
			Restart();
		}

		ImGui::Text("再生中: %s", IsPlaying() ? "はい" : "いいえ");
		ImGui::Text("Listenerまでの距離: %.2f", currentDistance_);
		ImGui::Text("減衰後の音量: %.2f", currentAttenuatedVolume_);
#endif // USE_IMGUI
	}

	void WorldAudioComponent::Finalize()
	{
		Stop();
	}

	void WorldAudioComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);

		outJson["Class"] = GetClassTypeName();
		outJson["SoundPath"] = soundPath_;
		outJson["Volume"] = volume_;
		outJson["Pitch"] = pitch_;
		outJson["Loop"] = loop_;
		outJson["PlayOnStart"] = playOnStart_;
		outJson["Enabled"] = enabled_;
		outJson["MinDistance"] = minDistance_;
		outJson["MaxDistance"] = maxDistance_;
		outJson["FollowOwner"] = followOwner_;
	}

	void WorldAudioComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);

		if (inJson.contains("SoundPath") && inJson["SoundPath"].is_string())
		{
			soundPath_ = inJson["SoundPath"].get<std::string>();
		}
		if (inJson.contains("Volume") && inJson["Volume"].is_number())
		{
			volume_ = std::clamp(inJson["Volume"].get<float>(), 0.0f, 4.0f);
		}
		if (inJson.contains("Pitch") && inJson["Pitch"].is_number())
		{
			pitch_ = std::clamp(inJson["Pitch"].get<float>(), 0.01f, 4.0f);
		}
		if (inJson.contains("Loop") && inJson["Loop"].is_boolean())
		{
			loop_ = inJson["Loop"].get<bool>();
		}
		if (inJson.contains("PlayOnStart") && inJson["PlayOnStart"].is_boolean())
		{
			playOnStart_ = inJson["PlayOnStart"].get<bool>();
		}
		if (inJson.contains("Enabled") && inJson["Enabled"].is_boolean())
		{
			enabled_ = inJson["Enabled"].get<bool>();
		}
		if (inJson.contains("MinDistance") && inJson["MinDistance"].is_number())
		{
			minDistance_ = std::max(0.0f, inJson["MinDistance"].get<float>());
		}
		if (inJson.contains("MaxDistance") && inJson["MaxDistance"].is_number())
		{
			maxDistance_ = std::max(0.01f, inJson["MaxDistance"].get<float>());
		}
		if (inJson.contains("FollowOwner") && inJson["FollowOwner"].is_boolean())
		{
			followOwner_ = inJson["FollowOwner"].get<bool>();
		}

		SanitizeDistanceRange();
	}

	void WorldAudioComponent::Play()
	{
		if (!enabled_ || soundPath_.empty())
		{
			return; // 無効またはパス未設定の場合は再生しない
		}

		Stop();

		sourcePosition_ = GetCurrentSourcePosition();
		UpdateRuntimeAudio();
		audioHandle_ = AudioManager::GetInstance()->PlaySEWithHandle(soundPath_, currentAttenuatedVolume_, pitch_, loop_);
	}

	void WorldAudioComponent::Stop()
	{
		if (audioHandle_ == AudioManager::InvalidAudioHandle)
		{
			return;
		}

		AudioManager::GetInstance()->Stop(audioHandle_);
		audioHandle_ = AudioManager::InvalidAudioHandle;
	}

	void WorldAudioComponent::Restart()
	{
		Stop();
		Play();
	}

	bool WorldAudioComponent::IsPlaying() const
	{
		return audioHandle_ != AudioManager::InvalidAudioHandle &&
			AudioManager::GetInstance()->IsPlaying(audioHandle_);
	}

	void WorldAudioComponent::SetSoundPath(const std::string& soundPath)
	{
		if (soundPath_ == soundPath)
		{
			return;
		}

		Stop();
		soundPath_ = soundPath;
	}

	void WorldAudioComponent::SetVolume(float volume)
	{
		volume_ = std::clamp(volume, 0.0f, 4.0f);
		UpdateRuntimeAudio();
	}

	void WorldAudioComponent::SetPitch(float pitch)
	{
		pitch_ = std::clamp(pitch, 0.01f, 4.0f);
	}

	void WorldAudioComponent::SetEnabled(bool enabled)
	{
		enabled_ = enabled;
		if (!enabled_)
		{
			Stop();
		}
	}

	void WorldAudioComponent::SetMinDistance(float minDistance)
	{
		minDistance_ = std::max(0.0f, minDistance);
		SanitizeDistanceRange();
		UpdateRuntimeAudio();
	}

	void WorldAudioComponent::SetMaxDistance(float maxDistance)
	{
		maxDistance_ = std::max(0.01f, maxDistance);
		SanitizeDistanceRange();
		UpdateRuntimeAudio();
	}

	void WorldAudioComponent::SetFollowOwner(bool followOwner)
	{
		followOwner_ = followOwner;
		if (followOwner_)
		{
			sourcePosition_ = GetWorldPosition();
			UpdateRuntimeAudio();
		}
	}

	void WorldAudioComponent::UpdateRuntimeAudio()
	{
		if (followOwner_)
		{
			sourcePosition_ = GetWorldPosition();
		}

		SanitizeDistanceRange();
		currentDistance_ = CalculateDistanceToListener();
		currentAttenuatedVolume_ = CalculateAttenuatedVolume(currentDistance_);

		if (audioHandle_ != AudioManager::InvalidAudioHandle)
		{
			AudioManager::GetInstance()->SetVoiceVolume(audioHandle_, currentAttenuatedVolume_);
		}
	}

	float WorldAudioComponent::CalculateDistanceToListener() const
	{
		const Vector3 listenerPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		const Vector3 toListener = {
			listenerPosition.x - sourcePosition_.x,
			listenerPosition.y - sourcePosition_.y,
			listenerPosition.z - sourcePosition_.z
		};

		const float distance = Length(toListener);
		return std::isfinite(distance) ? distance : 0.0f;
	}

	float WorldAudioComponent::CalculateAttenuatedVolume(float distance) const
	{
		if (!enabled_ || volume_ <= 0.0f)
		{
			return 0.0f;
		}

		if (distance <= minDistance_)
		{
			return volume_;
		}
		if (distance >= maxDistance_)
		{
			return 0.0f;
		}

		const float range = std::max(maxDistance_ - minDistance_, 0.0001f);
		const float rate = std::clamp((distance - minDistance_) / range, 0.0f, 1.0f);
		return volume_ * (1.0f - rate);
	}

	void WorldAudioComponent::SanitizeDistanceRange()
	{
		minDistance_ = std::max(0.0f, minDistance_);
		maxDistance_ = std::max(maxDistance_, minDistance_ + 0.01f);
	}

	Vector3 WorldAudioComponent::GetCurrentSourcePosition() const
	{
		return followOwner_ ? GetWorldPosition() : sourcePosition_;
	}
}
