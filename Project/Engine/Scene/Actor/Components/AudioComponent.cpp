#define NOMINMAX
#include "AudioComponent.h"
#include "AssetPathSelector.h"

#include <algorithm>
#include <array>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void AudioComponent::Initialize()
	{
		if (playOnStart_)
		{
			Play(); // Component開始時に指定された音声を再生する
		}
	}

	void AudioComponent::Update([[maybe_unused]] float deltaTime)
	{
		if (!enabled_)
		{
			Stop(); // 無効化されているComponentの再生を停止する
			return;
		}

		if (audioHandle_ != AudioManager::InvalidAudioHandle && !AudioManager::GetInstance()->IsPlaying(audioHandle_))
		{
			audioHandle_ = AudioManager::InvalidAudioHandle;
		}
	}

	void AudioComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("オーディオコンポーネント");

		std::array<char, 256> soundPathBuffer{};
		std::snprintf(soundPathBuffer.data(), soundPathBuffer.size(), "%s", soundPath_.c_str());
		if (ImGui::InputText("サウンドパス", soundPathBuffer.data(), soundPathBuffer.size()))
		{
			SetSoundPath(soundPathBuffer.data());
		}

		std::string selectedSoundPath = soundPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##AudioComponentSoundPath", selectedSoundPath, AssetType::Audio))
		{
			SetSoundPath(selectedSoundPath);
		}

		float volume = volume_;
		if (ImGui::DragFloat("音量", &volume, 0.01f, 0.0f, 4.0f))
		{
			SetVolume(volume);
		}

		ImGui::DragFloat("ピッチ", &pitch_, 0.01f, 0.01f, 4.0f);
		ImGui::Checkbox("ループ", &loop_);
		ImGui::Checkbox("開始時に再生", &playOnStart_);

		bool enabled = enabled_;
		if (ImGui::Checkbox("有効", &enabled))
		{
			SetEnabled(enabled);
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

		ImGui::Text("再生中: %s", IsPlaying() ? "はい" : "いいえ");
#endif // USE_IMGUI
	}

	void AudioComponent::Finalize()
	{
		Stop();
	}

	void AudioComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // ActorComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // AudioComponentとして保存する
		outJson["SoundPath"] = soundPath_;
		outJson["Volume"] = volume_;
		outJson["Pitch"] = pitch_;
		outJson["Loop"] = loop_;
		outJson["PlayOnStart"] = playOnStart_;
		outJson["Enabled"] = enabled_;
	}

	void AudioComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // ActorComponent共通情報をJSONから復元する

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
	}

	void AudioComponent::Play()
	{
		if (!enabled_ || soundPath_.empty())
		{
			return; // 無効またはパス未設定の場合は再生しない
		}

		Stop();

		audioHandle_ = AudioManager::GetInstance()->PlaySEWithHandle(soundPath_, volume_, pitch_, loop_);
	}

	void AudioComponent::Stop()
	{
		if (audioHandle_ == AudioManager::InvalidAudioHandle)
		{
			return;
		}

		AudioManager::GetInstance()->Stop(audioHandle_);
		audioHandle_ = AudioManager::InvalidAudioHandle;
	}

	bool AudioComponent::IsPlaying() const
	{
		return audioHandle_ != AudioManager::InvalidAudioHandle &&
			AudioManager::GetInstance()->IsPlaying(audioHandle_);
	}

	void AudioComponent::SetSoundPath(const std::string& soundPath)
	{
		if (soundPath_ == soundPath)
		{
			return;
		}

		Stop();
		soundPath_ = soundPath;
	}

	void AudioComponent::SetVolume(float volume)
	{
		volume_ = std::clamp(volume, 0.0f, 4.0f);
		if (audioHandle_ != AudioManager::InvalidAudioHandle)
		{
			AudioManager::GetInstance()->SetVoiceVolume(audioHandle_, volume_);
		}
	}

	void AudioComponent::SetEnabled(bool enabled)
	{
		enabled_ = enabled;
		if (!enabled_)
		{
			Stop();
		}
	}
}
